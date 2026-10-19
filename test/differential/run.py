#!/usr/bin/env python3
"""Runs the differential tests of the generated matrix against a pinned baseline.

The matrix test executables are built twice from the same working-tree test
sources, as described in docs/architecture/spec-driven-design.md under
Differential Testing:

- current: against the headers in include/
- baseline: against the include/ directory of the commit pinned in
  test/differential/baseline-commit, extracted to <build-root>/baseline-source

Every executable runs with DINGO_MATRIX_RECORD_FILE pointing at
<build-root>/records/<side>/<target>.jsonl, and test/differential/compare.py
compares the records of both sides. The exit code is the one of compare.py:
0 for no unexpected differences, 1 for differences, 2 for an input error.
"""

import argparse
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
import tarfile
import time
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
DIFFERENTIAL_ROOT = Path(__file__).resolve().parent
BASELINE_COMMIT_FILE = DIFFERENTIAL_ROOT / "baseline-commit"
COMPARE = DIFFERENTIAL_ROOT / "compare.py"

SIDES = ("current", "baseline")

# Targets that cannot take part. The constructor-detection family tests
# internal names that the new implementation does not preserve.
EXCLUDED_TARGETS = frozenset({"dingo_matrix_test_constructor_detection"})

TARGETS_RE = re.compile(r"set\(\s*DINGO_MATRIX_TEST_TARGETS\b(.*?)\)", re.DOTALL)


class Failure(Exception):
    """An error that ends the run with exit code 2."""


def step(message):
    print(f"==> {message}", flush=True)


def run(args, **kwargs):
    return subprocess.run([str(arg) for arg in args], **kwargs)


def read_baseline_commit():
    commit = BASELINE_COMMIT_FILE.read_text().strip()
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise Failure(f"{BASELINE_COMMIT_FILE} must hold a full commit hash")
    return commit


def extract_baseline(build_root, commit):
    """Extracts include/ of the commit into <build-root>/baseline-source."""
    source = build_root / "baseline-source"
    marker = source / ".commit"
    include = source / "include"
    if (
        marker.exists()
        and marker.read_text().strip() == commit
        and (include / "dingo" / "container.h").exists()
    ):
        step(f"baseline sources of {commit[:12]} are up to date")
        return include
    step(f"extracting include/ of {commit[:12]} to {source}")
    if source.exists():
        shutil.rmtree(source)
    source.mkdir(parents=True)
    archive = subprocess.Popen(
        ["git", "archive", commit, "include"],
        cwd=PROJECT_ROOT,
        stdout=subprocess.PIPE,
    )
    try:
        with tarfile.open(fileobj=archive.stdout, mode="r|") as tar:
            if hasattr(tarfile, "data_filter"):
                tar.extractall(source, filter="data")
            else:
                tar.extractall(source)
    except tarfile.TarError as error:
        archive.kill()
        raise Failure(f"cannot read the git archive of {commit}: {error}")
    finally:
        archive.stdout.close()
        returncode = archive.wait()
    if returncode != 0:
        raise Failure(
            f"git archive {commit} failed; fetch the full history "
            "(git fetch --unshallow) so that the pinned commit is available"
        )
    marker.write_text(commit + "\n")
    return include


def configure(args, side, build_dir, include_dir):
    cmake_args = [
        "cmake",
        "-S", PROJECT_ROOT,
        "-B", build_dir,
        "-DDINGO_DEVELOPMENT_MODE=ON",
        f"-DDINGO_MATRIX_PROFILE={args.profile}",
        "-DDINGO_MATRIX_RECORD=ON",
        "-DDINGO_BENCHMARK_ENABLED=OFF",
        "-DDINGO_EXAMPLES_ENABLED=OFF",
        "-DDINGO_LIT_TESTS_ENABLED=OFF",
        "-DCMAKE_BUILD_TYPE=Release",
    ]
    if include_dir is not None:
        cmake_args.append(f"-DDINGO_DIFFERENTIAL_INCLUDE_DIR={include_dir}")
    if args.cxx:
        cmake_args.append(f"-DCMAKE_CXX_COMPILER={args.cxx}")
    if args.generator:
        cmake_args += ["-G", args.generator]
    elif shutil.which("ninja"):
        cmake_args += ["-G", "Ninja"]
    step(f"configuring {side} in {build_dir}")
    if run(cmake_args).returncode != 0:
        raise Failure(f"configuring the {side} side failed")


def read_targets(build_dir):
    path = build_dir / "generated" / "matrix-tests.cmake"
    try:
        text = path.read_text()
    except OSError as error:
        raise Failure(f"cannot read {path}: {error}")
    match = TARGETS_RE.search(text)
    if match is None:
        raise Failure(f"{path} does not define DINGO_MATRIX_TEST_TARGETS")
    return [t for t in match.group(1).split() if t not in EXCLUDED_TARGETS]


def executable_candidates(build_dir, target):
    for config in ("", "Release"):
        for suffix in ("", ".exe"):
            yield build_dir / "test" / config / f"{target}{suffix}"


def find_executable(build_dir, target):
    return next((p for p in executable_candidates(build_dir, target) if p.is_file()), None)


def build(args, side, build_dir, targets):
    """Builds the targets; returns the targets that produced an executable."""
    # A failed build must not leave an executable of an earlier run behind.
    for target in targets:
        for candidate in executable_candidates(build_dir, target):
            candidate.unlink(missing_ok=True)
    command = [
        "cmake", "--build", build_dir,
        "--config", "Release",
        "--target", *targets,
        "-j", args.jobs,
    ]
    generator = cmake_generator(build_dir)
    # Keep building the other targets after one fails.
    if generator == "Ninja":
        command += ["--", "-k", "0"]
    elif "Makefiles" in generator:
        command += ["--", "-k"]
    step(f"building {len(targets)} targets of {side}")
    started = time.monotonic()
    returncode = run(command).returncode
    elapsed = time.monotonic() - started
    built = [t for t in targets if find_executable(build_dir, t) is not None]
    status = "ok" if returncode == 0 else f"FAILED (exit {returncode})"
    print(
        f"    {side} build {status} in {elapsed:.0f}s: "
        f"{len(built)}/{len(targets)} executables",
        flush=True,
    )
    for target in targets:
        if target not in built:
            print(f"    no executable for {side}/{target}; it produces no records")
    return built


def cmake_generator(build_dir):
    cache = build_dir / "CMakeCache.txt"
    match = re.search(r"^CMAKE_GENERATOR:INTERNAL=(.*)$", cache.read_text(), re.MULTILINE)
    return match.group(1) if match else ""


def run_tests(side, build_dir, records_dir, targets, jobs):
    side_records = records_dir / side
    if side_records.exists():
        shutil.rmtree(side_records)
    side_records.mkdir(parents=True)

    def run_one(target):
        env = dict(os.environ)
        env["DINGO_MATRIX_RECORD_FILE"] = str(side_records / f"{target}.jsonl")
        env.setdefault("ASAN_OPTIONS", "use_sigaltstack=false")
        with open(side_records / f"{target}.log", "w") as log:
            result = run(
                [find_executable(build_dir, target)],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        return target, result.returncode

    step(f"running {len(targets)} executables of {side}")
    started = time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        results = list(pool.map(run_one, targets))
    failed = [(t, code) for t, code in results if code != 0]
    print(
        f"    {side}: {len(results) - len(failed)}/{len(results)} executables "
        f"passed in {time.monotonic() - started:.0f}s",
        flush=True,
    )
    for target, code in failed:
        print(f"    {side}/{target} exited with {code}; log: "
              f"{side_records / (target + '.log')}")
    return dict(results)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--build-root",
        type=Path,
        required=True,
        help="directory for both builds, the baseline sources, and the records",
    )
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 8))
    parser.add_argument("--profile", choices=("portable", "full"), default="portable")
    parser.add_argument("--generator", help="CMake generator; Ninja when available")
    parser.add_argument("--cxx", help="C++ compiler for both builds")
    args = parser.parse_args()
    build_root = args.build_root.resolve()

    try:
        commit = read_baseline_commit()
        baseline_include = extract_baseline(build_root, commit)

        build_dirs = {side: build_root / side for side in SIDES}
        configure(args, "current", build_dirs["current"], None)
        configure(args, "baseline", build_dirs["baseline"], baseline_include)

        targets = {side: read_targets(build_dirs[side]) for side in SIDES}
        if targets["current"] != targets["baseline"]:
            only = set(targets["current"]) ^ set(targets["baseline"])
            raise Failure(
                "the sides list different matrix targets: " + ", ".join(sorted(only))
            )
        step(f"{len(targets['current'])} matrix targets (excluded: "
             f"{', '.join(sorted(EXCLUDED_TARGETS))})")

        # A build failure of one side is reported and does not stop the other.
        built = {
            side: build(args, side, build_dirs[side], targets[side]) for side in SIDES
        }

        records = build_root / "records"
        exit_codes = {
            side: run_tests(side, build_dirs[side], records, built[side], args.jobs)
            for side in SIDES
        }
    except Failure as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    unbuilt = sum(len(targets[side]) - len(built[side]) for side in SIDES)
    failing = sum(1 for side in SIDES for code in exit_codes[side].values() if code)
    print(f"    {unbuilt} targets not built, {failing} executables failed")

    if not COMPARE.exists():
        print(
            f"error: {COMPARE} does not exist; the records are in {records}",
            file=sys.stderr,
        )
        return 2
    step("comparing the records")
    return run(
        [
            sys.executable, COMPARE,
            "--baseline", records / "baseline",
            "--current", records / "current",
            "--spec",
            "--report", build_root / "report.json",
        ]
    ).returncode


if __name__ == "__main__":
    sys.exit(main())

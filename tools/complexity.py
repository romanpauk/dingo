"""Reports where the complexity of the headers is.

The report has five sections, each a ranking:

- functions: function bodies by cyclomatic complexity (lizard)
- headers: template machinery per header: template declarations,
  specializations, `if constexpr` branches, SFINAE constructs, and the type
  definitions that are named at most once outside their definition
- modules: non-comment lines per module directory
- duplicates: repeated code blocks (lizard)
- compile: template instantiation time per template, aggregated from clang
  -ftime-trace of the translation units selected with --profile

The heuristics are coarse. A definition named once is a candidate for
inlining, not a defect; a costly template is costly for every translation unit
that includes it. The sections are meant to be compared between revisions.
"""

import argparse
import collections
import concurrent.futures
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

COMMENTS = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
TEMPLATE = re.compile(r"\btemplate\s*<")
SPECIALIZATION = re.compile(r"\b(?:struct|class)\s+\w+\s*<")
IF_CONSTEXPR = re.compile(r"\bif\s+constexpr\b")
SFINAE = re.compile(r"\b(?:enable_if|enable_if_t|void_t|declval)\b")
TYPE_DEFINITION = re.compile(
    r"^\s*(?:template\s*<[^;{]*?>\s*)?(?:struct|class)\s+(\w+)\s*(?:final\s*)?"
    r"(?::[^;{]*)?\{"
    r"|^\s*(?:template\s*<[^;{]*?>\s*)?using\s+(\w+)\s*="
    r"|\binline\s+constexpr\s+[\w:<>]+\s+(\w+)\s*[=<]",
    re.M,
)
IDENTIFIER = re.compile(r"\b[A-Za-z_]\w*\b")


def headers(include_directory):
    root = Path(include_directory)
    return sorted(
        path for path in root.rglob("*") if path.suffix in (".h", ".hpp")
    )


def strip_comments(text):
    return COMMENTS.sub("", text)


def code_lines(text):
    return sum(1 for line in text.splitlines() if line.strip())


def header_metrics(sources):
    """Returns per-header template metrics for {path: comment-free text}."""
    uses = collections.Counter()
    for text in sources.values():
        uses.update(IDENTIFIER.findall(text))
    rows = []
    for path, text in sources.items():
        names = {
            name
            for match in TYPE_DEFINITION.finditer(text)
            for name in match.groups()
            if name
        }
        rows.append(
            {
                "header": path,
                "lines": code_lines(text),
                "templates": len(TEMPLATE.findall(text)),
                "specializations": len(SPECIALIZATION.findall(text)),
                "if_constexpr": len(IF_CONSTEXPR.findall(text)),
                "sfinae": len(SFINAE.findall(text)),
                "types": len(names),
                "single_use": sum(1 for name in names if uses[name] <= 2),
            }
        )
    return rows


def header_weight(row):
    return (
        row["templates"]
        + 2 * row["specializations"]
        + row["if_constexpr"]
        + row["sfinae"]
    )


def module_lines(sources, root):
    totals = collections.Counter()
    files = collections.Counter()
    for path, text in sources.items():
        parts = Path(path).relative_to(root).parts
        module = parts[0] if len(parts) > 1 else "."
        totals[module] += code_lines(text)
        files[module] += 1
    return [(module, files[module], lines) for module, lines in totals.items()]


def template_name(detail):
    """The name of an instantiated template, without arguments or scope."""
    name = []
    depth = 0
    for character in detail:
        if character == "<":
            depth += 1
        elif character == ">":
            depth -= 1
        elif depth == 0:
            name.append(character)
    name = "".join(name).split("(")[0].strip()
    return re.sub(r"^(?:dingo::)?(?:detail::)?", "", name)


def instantiation_times(events, scope="dingo"):
    """Returns {template: (self microseconds, count)} of a -ftime-trace trace.

    The self time of an instantiation excludes the instantiations nested in
    it, so a template is charged for its own work only.
    """
    instantiations = sorted(
        (
            event
            for event in events
            if event.get("ph") == "X"
            and event.get("name") in ("InstantiateClass", "InstantiateFunction")
        ),
        key=lambda event: (event.get("tid"), event["ts"], -event["dur"]),
    )
    self_time = {}
    stack = []
    for index, event in enumerate(instantiations):
        while stack and (
            instantiations[stack[-1]].get("tid") != event.get("tid")
            or instantiations[stack[-1]]["ts"] + instantiations[stack[-1]]["dur"]
            <= event["ts"]
        ):
            stack.pop()
        self_time[index] = event["dur"]
        if stack:
            self_time[stack[-1]] -= event["dur"]
        stack.append(index)
    totals = collections.defaultdict(lambda: [0, 0])
    for index, event in enumerate(instantiations):
        detail = event.get("args", {}).get("detail", "")
        if scope not in detail:
            continue
        entry = totals[template_name(detail)]
        entry[0] += self_time[index]
        entry[1] += 1
    return {name: tuple(entry) for name, entry in totals.items()}


def profile_command(entry, compiler, output):
    """The command of a compile_commands.json entry, retargeted to clang."""
    arguments = entry.get("arguments") or shlex.split(entry["command"])
    kept = [compiler]
    skip = False
    for argument in arguments[1:]:
        if skip:
            skip = False
            continue
        if argument == "-o":
            skip = True
            continue
        if argument.startswith(("-W", "-fdiagnostics")) or argument == "-c":
            continue
        if argument == entry["file"] or argument.endswith(".cpp"):
            continue
        kept.append(argument)
    return kept + [
        "-c",
        entry["file"],
        "-o",
        str(output),
        "-ftime-trace",
        "-ftime-trace-granularity=50",
    ]


def profile_unit(entry, compiler, output):
    subprocess.run(
        profile_command(entry, compiler, output), cwd=entry["directory"], check=True
    )
    trace = json.loads(output.with_suffix(".json").read_text())
    return instantiation_times(trace["traceEvents"])


def profile(build_directory, patterns, compiler, jobs):
    database = json.loads(
        (Path(build_directory) / "compile_commands.json").read_text()
    )
    entries = [
        entry
        for entry in database
        if any(pattern in entry["file"] for pattern in patterns)
    ]
    if not entries:
        raise SystemExit(f"no translation unit matches {patterns}")
    totals = collections.defaultdict(lambda: [0, 0])
    with tempfile.TemporaryDirectory() as directory:
        with concurrent.futures.ThreadPoolExecutor(jobs) as executor:
            units = executor.map(
                lambda item: profile_unit(
                    item[1], compiler, Path(directory) / f"unit{item[0]}.o"
                ),
                enumerate(entries),
            )
            for times in units:
                for name, (time, count) in times.items():
                    totals[name][0] += time
                    totals[name][1] += count
    return [entry["file"] for entry in entries], totals


def lizard_command(*arguments):
    return [sys.executable, "-m", "lizard", "-l", "cpp", *arguments]


def function_rows(include_directory):
    import lizard

    rows = []
    for path in headers(include_directory):
        for function in lizard.analyze_file(str(path)).function_list:
            rows.append(
                (
                    function.cyclomatic_complexity,
                    function.nloc,
                    # Attribute macros before a class name are part of it.
                    re.sub(r"\bDINGO_[A-Z_]+(?=[a-z])", "", function.name),
                    f"{os.path.relpath(path, include_directory)}:"
                    f"{function.start_line}",
                )
            )
    return sorted(set(rows), reverse=True)


def duplicate_files(include_directory):
    result = subprocess.run(
        lizard_command("-Eduplicate", str(include_directory)),
        capture_output=True,
        text=True,
        check=False,
    )
    report = result.stdout[result.stdout.find("Duplicates") :]
    blocks = collections.Counter(re.findall(r"^(\S+\.hp?p?):\d+ ~ \d+$", report, re.M))
    rate = re.search(r"Total duplicate rate: ([\d.]+%)", report)
    return blocks, rate.group(1) if rate else "unknown"


def print_table(title, header, rows):
    print(f"\n## {title}\n")
    widths = [
        max(len(str(value)) for value in column)
        for column in zip(header, *rows)
    ]
    numeric = [
        all(re.fullmatch(r"[\d.]+%?", str(row[index])) for row in rows)
        for index in range(len(header))
    ]
    for row in (header, *rows):
        cells = [
            str(value).rjust(width) if numeric[index] else str(value).ljust(width)
            for index, (value, width) in enumerate(zip(row, widths))
        ]
        print("  ".join(cells).rstrip())


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("include_directory", nargs="?", default="include")
    parser.add_argument("--top", type=int, default=20)
    parser.add_argument(
        "--sections",
        default="functions,headers,modules,duplicates",
        help="comma-separated sections; compile is added by --profile",
    )
    parser.add_argument(
        "--profile",
        action="append",
        default=[],
        metavar="PATTERN",
        help="profile the translation units whose path contains PATTERN",
    )
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--clang", default="clang++")
    parser.add_argument("--jobs", type=int, default=os.cpu_count())
    options = parser.parse_args()

    root = Path(options.include_directory)
    sources = {
        str(path): strip_comments(path.read_text()) for path in headers(root)
    }
    sections = set(options.sections.split(","))
    top = options.top

    if "functions" in sections:
        rows = function_rows(root)
        print_table(
            "Functions by cyclomatic complexity",
            ("ccn", "nloc", "function", "location"),
            [(ccn, nloc, name, location) for ccn, nloc, name, location in rows[:top]],
        )

    if "headers" in sections:
        rows = header_metrics(sources)
        rows.sort(key=header_weight, reverse=True)
        print_table(
            "Headers by template machinery",
            ("lines", "tmpl", "spec", "ifcx", "sfinae", "types", "1use", "header"),
            [
                (
                    row["lines"],
                    row["templates"],
                    row["specializations"],
                    row["if_constexpr"],
                    row["sfinae"],
                    row["types"],
                    row["single_use"],
                    os.path.relpath(row["header"], root),
                )
                for row in rows[:top]
            ],
        )
        totals = {
            key: sum(row[key] for row in rows)
            for key in ("lines", "types", "single_use", "specializations")
        }
        print(
            f"\n{totals['lines']} lines, {totals['types']} type definitions, "
            f"{totals['single_use']} named at most once, "
            f"{totals['specializations']} specializations"
        )

    if "modules" in sections:
        rows = sorted(module_lines(sources, root / "dingo"), key=lambda row: -row[2])
        print_table("Modules by lines", ("lines", "files", "module"),
                    [(lines, files, module) for module, files, lines in rows])

    if "duplicates" in sections:
        blocks, rate = duplicate_files(root)
        print_table(
            f"Duplicate blocks by header (duplicate rate {rate})",
            ("blocks", "header"),
            [(count, os.path.relpath(path, root)) for path, count in blocks.most_common(top)],
        )

    if options.profile:
        units, totals = profile(
            options.build_dir, options.profile, options.clang, options.jobs
        )
        ranked = sorted(totals.items(), key=lambda item: -item[1][0])
        print_table(
            f"Instantiation self time over {len(units)} translation units",
            ("ms", "count", "template"),
            [(f"{time / 1000:.1f}", count, name) for name, (time, count) in ranked[:top]],
        )


if __name__ == "__main__":
    main()

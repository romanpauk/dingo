import argparse
import html
import mdformat
import os
from pathlib import Path
import re
import subprocess
import sys

CURRENT_MARKDOWN_FILE = None


def file_type(file):
    if os.path.splitext(file)[1].lower() in [".cpp", ".hpp", ".h"]:
        return "c++"

    if os.path.split(file)[1] == "CMakeLists.txt":
        return "CMake"

    raise Exception("unsupported file type: {}".format(file))


def include(file, scope=None, summary=None, expanded=False):
    global CURRENT_MARKDOWN_FILE

    display_file = str(file)
    file = Path(file)
    if not file.is_absolute() and CURRENT_MARKDOWN_FILE is not None:
        resolved_file = (Path(CURRENT_MARKDOWN_FILE).parent / file).resolve()
    else:
        resolved_file = file.resolve()

    lines = []
    with open(resolved_file, "r") as f:
        if not scope:
            lines = f.readlines()
        else:
            in_scope = False
            for line in f.readlines():
                if line.strip().startswith(scope):
                    in_scope = not in_scope
                elif in_scope:
                    lines.append(line)

    if file_type(str(resolved_file)) == "c++":
        p = subprocess.run(
            ["clang-format"],
            stdout=subprocess.PIPE,
            input=str("".join(lines)).encode("ascii"),
        )
        lines = [line + "\n" for line in p.stdout.decode().split("\n")]
        if lines[-1].strip() == "":
            lines.pop()

    result = []
    if summary:
        details_tag = "<details open>\n" if expanded else "<details>\n"
        result.append(details_tag)
        result.append("<summary>{}</summary>\n".format(html.escape(summary)))
        result.append("\n")
    result.append("Example code included from [{0}]({0}):\n".format(display_file))
    result.append("```{}\n".format(file_type(str(resolved_file))))
    result.extend(lines)
    result.append("```\n")
    if summary:
        result.append("\n")
        result.append("</details>\n")

    return result


def process(lines):
    stack = []
    result = []
    for i, line in enumerate(lines):
        m = re.search(r"<!--\s*{(.*)-->", line)
        if m:
            result.append(line)
            stack.append((i, m.group(1).strip(), []))
            continue

        m = re.search(r"<!--\s*}\s*-->", line)
        if m:
            data = stack.pop()
            (code, buffer) = (data[1].strip(), data[2])
            for l in eval(code):
                result.append(l)
            result.append(line)
            continue

        if stack:
            stack[-1][2].append(line)
        else:
            result.append(line)

    return result


def format(lines):
    return mdformat.text("".join(lines), options={"number": True, "wrap": 80})


def iter_non_code_lines(lines):
    in_fence = False
    fence = None

    for lineno, line in enumerate(lines, 1):
        stripped = line.lstrip()
        m = re.match(r"^(```+|~~~+)", stripped)
        if m:
            marker = m.group(1)
            if not in_fence:
                in_fence = True
                fence = marker[:3]
            elif marker.startswith(fence):
                in_fence = False
                fence = None
            continue

        if not in_fence:
            yield lineno, line


def heading_to_anchor(text, counts):
    text = re.sub(r"`([^`]*)`", r"\1", text)
    text = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", text)
    text = text.strip().lower()
    text = "".join(ch for ch in text if ch.isalnum() or ch in (" ", "-", "_"))
    text = re.sub(r"\s+", "-", text)
    text = re.sub(r"-+", "-", text).strip("-")

    count = counts.get(text, 0)
    counts[text] = count + 1
    if count:
        return "{}-{}".format(text, count)
    return text


def collect_anchors(path, cache):
    if path in cache:
        return cache[path]

    counts = {}
    anchors = set()
    with open(path, "r") as f:
        for _, line in iter_non_code_lines(f.readlines()):
            m = re.match(r"^\s{0,3}(#{1,6})\s+(.*?)\s*$", line)
            if not m:
                continue

            heading = re.sub(r"\s+#+\s*$", "", m.group(2)).strip()
            if heading:
                anchors.add(heading_to_anchor(heading, counts))

    cache[path] = anchors
    return anchors


def parse_link_target(target):
    target = target.strip()
    if target.startswith("<") and target.endswith(">"):
        target = target[1:-1].strip()

    if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target):
        return None

    if "#" in target:
        path_part, anchor = target.split("#", 1)
    else:
        path_part, anchor = target, None

    return path_part, anchor


def verify_links(filenames):
    link_re = re.compile(r"(?<!!)\[[^\]]+\]\(([^)]+)\)")
    anchor_cache = {}
    errors = []

    for filename in filenames:
        source = Path(filename)
        with open(source, "r") as f:
            for lineno, line in iter_non_code_lines(f.readlines()):
                for match in link_re.finditer(line):
                    parsed = parse_link_target(match.group(1))
                    if parsed is None:
                        continue

                    path_part, anchor = parsed
                    if path_part == "":
                        target = source
                    else:
                        target = (source.parent / path_part).resolve()

                    if not target.exists():
                        errors.append(
                            "{}:{}: missing link target '{}'".format(
                                source, lineno, path_part
                            )
                        )
                        continue

                    if anchor is None or anchor == "":
                        continue

                    if not target.is_file() or target.suffix.lower() != ".md":
                        errors.append(
                            "{}:{}: anchor '{}' points to non-markdown target '{}'".format(
                                source, lineno, anchor, target
                            )
                        )
                        continue

                    anchors = collect_anchors(target, anchor_cache)
                    if anchor not in anchors:
                        errors.append(
                            "{}:{}: missing anchor '#{}' in '{}'".format(
                                source, lineno, anchor, target
                            )
                        )

    if errors:
        for error in errors:
            print(error)
        sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("filenames", help="file(s) to process", nargs="+")
    parser.add_argument(
        "--mode",
        choices=["verify", "update"],
        default="verify",
        help="'update' the file or 'verify' that it is updated",
    )
    args = parser.parse_args()

    for file in args.filenames:
        CURRENT_MARKDOWN_FILE = file
        with open(file, "r") as f:
            content = f.readlines()

        result = format(process(content))
        if result != "".join(content):
            if args.mode == "update":
                with open(file, "w") as f:
                    f.write(result)
                print("{} updated".format(file))
            elif args.mode == "verify":
                print("{} requires update, run md.py in update mode".format(file))
                sys.exit(1)
        else:
            print("{} is up-to-date".format(file))

    if args.mode == "verify":
        verify_links(args.filenames)

    sys.exit(0)

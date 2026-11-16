"""Where a compiler spends its work on a program, as it reports it: GCC by
-ftime-report, the time and the collected memory of each of its phases;
clang by -ftime-trace, the events of its front end and its back end, and the
templates it instantiates by the area of the library they are of. Counts and
memory are the same in every compile of the same program; times are not."""

import json
import re
from pathlib import Path

# The areas of the library a template is of, by its name without its
# arguments: the first that matches.
AREAS = (
    ("standard library", r"^(std|__gnu_cxx|__cxx)::"),
    ("test harness", r"^dingo::model::"),
    ("request chain", r"request_resolution::|container_requests::|"
                      r"::(resolve_selected|select_request|resolve_request|"
                      r"resolve_local|resolve_entry|resolve_transacted|"
                      r"unresolved)$|returned_value|resolved_value|"
                      r"resolution_context::resolve|constructor_dependency|"
                      r"request_type|runtime_request"),
    ("execution", r"execution::|semantics::|layer_universe|wrapper_kind|"
                  r"possible_kind|stack_type|stack_quals|requested_category"),
    ("registration", r"registration|commit_|prepare_binding|lookup_|"
                     r"construct_cell|register_type|binding_model|"
                     r"materialization_facts"),
    ("bindings and storages", r"binding|storage|cell|constructor|factory|"
                              r"materializ|retention"),
    ("other dingo", r"^dingo::"),
)


def library_area(name):
    """The area of the library a template is of, by its name."""
    for area, pattern in AREAS:
        if re.search(pattern, name):
            return area
    return "other"


def template_name(detail):
    """The name of a template an event names, without its arguments, its
    parameters and its return type."""
    detail = re.sub(r"\(.*", "", detail)
    out = []
    depth = 0
    for character in detail:
        if character == "<":
            depth += 1
        elif character == ">":
            depth -= 1
        elif depth == 0:
            out.append(character)
    return "".join(out).split(" ")[-1]


SIZE = re.compile(r"([\d.]+)([kMG]?)\s*\(\s*[\d.]+%\)")
TIME = re.compile(r"^([\d.]+)\s*\(\s*[\d.]+%\)")
UNITS = {"": 1, "k": 1024, "M": 1024 ** 2, "G": 1024 ** 3}


def gcc_report(text):
    """The phases and the time variables of -ftime-report: the seconds and
    the collected bytes of each."""
    found = {}
    for line in text.splitlines():
        name, colon, rest = line.partition(":")
        if not colon or not rest.strip() or name.strip() == "TOTAL":
            continue
        rest = rest.strip()
        time = TIME.match(rest)
        sizes = SIZE.findall(rest)
        if time is None or not sizes:
            continue
        number, unit = sizes[-1]
        found[name.strip()] = {
            "seconds": float(time.group(1)),
            "collected": int(float(number) * UNITS[unit])}
    return found


def clang_trace(path):
    """The events of -ftime-trace by kind, with how many there were and how
    many milliseconds they took apart from the events they hold; and the
    templates instantiated by the area of the library they are of."""
    events = [event for event in json.loads(Path(path).read_text())
              ["traceEvents"]
              if event.get("ph") == "X" and
              not event["name"].startswith("Total ")]
    events.sort(key=lambda event: (event["ts"], -event["dur"]))
    own = [event["dur"] for event in events]
    open_events = []
    for index, event in enumerate(events):
        while (open_events and
               events[open_events[-1]]["ts"] + events[open_events[-1]]["dur"]
               <= event["ts"]):
            open_events.pop()
        if open_events:
            own[open_events[-1]] -= event["dur"]
        open_events.append(index)
    kinds = {}
    areas = {}
    for event, duration in zip(events, own):
        kind = kinds.setdefault(event["name"], {"count": 0, "milliseconds": 0})
        kind["count"] += 1
        kind["milliseconds"] += duration / 1000
        if event["name"].startswith("Instantiate"):
            area = areas.setdefault(
                library_area(template_name(event.get("args", {}).get("detail", ""))),
                {"count": 0, "milliseconds": 0})
            area["count"] += 1
            area["milliseconds"] += duration / 1000
    for found in (kinds, areas):
        for value in found.values():
            value["milliseconds"] = round(value["milliseconds"], 1)
    return {"events": kinds, "instantiations": areas}

"""The baseline of a family: its cells, counted by dimension, its defects,
and the cells whose outcomes have to agree and do not."""

import json
import sys

from .dimensions import Outcome

MARKS = {
    Outcome.RESOLVED: "ok",
    Outcome.TYPE_NOT_CONVERTIBLE: "nc",
    Outcome.TYPE_NOT_FOUND: "nf",
    Outcome.TYPE_AMBIGUOUS: "amb",
    Outcome.REJECTED: "rej",
    Outcome.NOT_REGISTRABLE: "-",
    Outcome.INVALID: "inv",
    Outcome.DOES_NOT_COMPILE: "DNC",
    Outcome.CRASHED: "CRASH",
    Outcome.VIOLATED: "BAD",
    Outcome.UNPROBED: "UNPROBED",
    Outcome.UNREACHABLE: "UNREACHABLE",
    Outcome.EXCEPTION: "exc",
    Outcome.DETECTED: "det",
    Outcome.MISDETECTED: "MIS",
    Outcome.LIMITED: "lim",
    Outcome.STALE_LIMITATION: "STALE",
    Outcome.UNEXPECTED: "UNEXP",
    Outcome.EXCESS: "excess",
    Outcome.PUBLISHED: "pub",
    Outcome.DEFINED: "def",
    Outcome.COMPOSED: "comp",
    Outcome.DERIVED: "deriv",
    Outcome.ADOPTED: "adopt",
    Outcome.PLACED: "place",
    Outcome.UNCONVERTIBLE: "unconv",
    Outcome.UNPUBLISHED: "unpub",
    Outcome.DIVERGED: "DIVERGED",
    Outcome.SELECTS_ANOTHER: "other",
}


def mark(outcome):
    return MARKS.get(Outcome(outcome), outcome)


def is_defect(cell):
    return Outcome(cell["outcome"]).is_defect


def counts_table(cells, dimensions, dimension):
    """The outcomes counted by the values of a dimension of the cells."""
    outcomes = [o for o in MARKS if any(c["outcome"] == o.value for c in cells)]
    totals = {}
    for cell in cells:
        value = cell["cell"].split("/")[dimensions.index(dimension)]
        row = totals.setdefault(value, {})
        row[cell["outcome"]] = row.get(cell["outcome"], 0) + 1
    lines = [f"| {dimension} | " + " | ".join(o.value for o in outcomes) +
             " |", "|---|" + "---:|" * len(outcomes)]
    for value, row in totals.items():
        lines.append(f"| `{value}` | " + " | ".join(
            str(row.get(o.value, "")) for o in outcomes) + " |")
    return lines + [""]


def compile_time_outcomes(cells):
    """The messages of the cells rejected or not compiled, with the cells."""
    errors = {}
    for cell in cells:
        if cell["outcome"] in (Outcome.DOES_NOT_COMPILE, Outcome.REJECTED):
            errors.setdefault((cell["outcome"], cell["error"]), []).append(
                cell["cell"])
    lines = ["## Compile-time outcomes", ""]
    for (outcome, error), where in sorted(
            errors.items(), key=lambda item: (item[0][0], -len(item[1]))):
        lines.append(f"- {outcome}, {len(where)} cells: `{error[:200]}`")
        lines += [f"  - {cell}" for cell in where[:4]]
        if len(where) > 4:
            lines.append(f"  - ... {len(where) - 4} more")
    return lines


def write(output, cells):
    """Writes the cells, one per cell, sorted, and returns them so."""
    cells = sorted({cell["cell"]: cell for cell in cells}.values(),
                   key=lambda cell: cell["cell"])
    output.write_text(json.dumps(cells, indent=1) + "\n")
    return cells


def summarize(cells, disagreements, report):
    """Prints the counts of the outcomes, the defects and the disagreements;
    returns the exit status: 1 on a defect or a disagreement."""
    counts = {}
    for cell in cells:
        counts[cell["outcome"]] = counts.get(cell["outcome"], 0) + 1
    print(f"{len(cells)} cells:", file=sys.stderr)
    for outcome, count in sorted(counts.items(), key=lambda item: -item[1]):
        print(f"  {count:6} {outcome}", file=sys.stderr)
    defects = [cell for cell in cells if is_defect(cell)]
    for cell in defects:
        print(f"defect: {cell['cell']}: {cell['outcome']}: "
              f"{cell.get('error', '')}", file=sys.stderr)
    for line in disagreements:
        print(f"disagreement: {line}", file=sys.stderr)
    print(f"report: {report}", file=sys.stderr)
    return 1 if defects or disagreements else 0

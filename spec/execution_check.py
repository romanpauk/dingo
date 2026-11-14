"""Applies the plans of the semantics to C++ objects, and compares what the
steps copy and move of the leaf with the plans.

The semantics of the library (dingo/semantics/model.h) plan the steps from a
stored object to what satisfies a request; dingo/request/execution.h applies
them to C++ objects. This check asks the semantics the plan of every
conversion of the model: every stored form of every leaf, kept or made by its
scope, requested as every shape of every bound interface in every category.
It applies each plan to an object of the model's leaf types, which count
their copies and moves, and compares the counts with the plan. No container
takes part.
"""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).parents[1]))

from spec import semantics  # noqa: E402
from spec import semantics_check as check  # noqa: E402
from spec.generator import driver  # noqa: E402
from spec.generator.model import spell_shape, spell_stored  # noqa: E402
from spec.generator.registrations import (LEAF_TYPES,  # noqa: E402
                                          interface_of)
from spec.vocabulary import Category, InterfaceSet, Scope, StoredForm  # noqa: E402

STEPS = ("decay", "hand_over", "adopt", "element", "element_pointer",
         "element_or_empty", "element_or_empty_copy", "convert", "upcast",
         "copy", "wrap")
SATISFACTIONS = ("refer", "point", "take", "move", "copy")
# The identity of a standard layer, which a step makes a layer of.
IDENTITIES = {
    "shared_ptr": "std::shared_ptr<dingo::detail::layer_probe>",
    "unique_ptr": "std::unique_ptr<dingo::detail::layer_probe>",
    "optional": "std::optional<dingo::detail::layer_probe>",
}
CELLS = 100


def cases():
    """The stored objects and requests the check plans: of every scope that
    does not publish its instance early, and of every stored form but an
    array a scope makes for each request, which is not a value a function
    returns."""
    for name, context, start, goals in check.standard():
        if (context.limits != semantics.LIMITATIONS or
                context.scope.publishes_early):
            continue
        scope, form, leaf, element, interfaces, _ = name.split("/")
        if interfaces != InterfaceSet.DEFAULT.value:
            continue
        form = StoredForm(form)
        if (context.scope.makes and
                any(semantics.LAYERS[layer].decays_to
                    for layer in semantics.FORMS[form][:1])):
            continue
        yield (Scope(scope), form, [key for key in semantics.LEAVES
                                    if key.value == leaf][0],
               context, start, goals)


def step_type(kind, layer, leaf_type):
    """The C++ step of a kind of step to a layer of the standard universe."""
    universe = check.order(check.STANDARD)
    name = universe[layer] if layer < len(universe) else None
    kind = STEPS[kind]
    prefix = "dingo::detail::execution::"
    if kind == "wrap":
        if name == "variant":
            target = (f"dingo::detail::alternative_identity<std::variant<"
                      f"{leaf_type}, dingo::model::other>, {leaf_type}>")
        else:
            target = IDENTITIES[name]
        return f"{prefix}step<dingo::semantics::step::wrap, {target}>"
    if kind == "adopt":
        return f"{prefix}adopt_step<{IDENTITIES[name]}, ARRAY>"
    return f"{prefix}step<dingo::semantics::step::{kind}>"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--flags", default="-std=c++17 -O0 -Iinclude",
                        help="compiler flags, with include paths")
    parser.add_argument("--jobs", type=int, default=driver.default_jobs())
    parser.add_argument("--only", default="",
                        help="check only the cells whose name has this text")
    options = parser.parse_args()
    options.work.mkdir(parents=True, exist_ok=True)
    driver.set_jobs(options.jobs)
    flags = shlex.split(options.flags)
    semantics_program = options.work / "semantics"
    subprocess.run(driver.compiler_command(
        options.compiler, *shlex.split("-std=c++17 -O2"),
        *[flag for flag in flags if flag.startswith("-I")],
        str(check.SOURCE), "-o", str(semantics_program)), check=True)

    requests = []
    lines = []
    for scope, form, leaf, context, start, goals in cases():
        for layers, element, goal in goals:
            requests.append((scope, form, leaf, layers, element, goal))
            lines.append(check.encode(context, start, layers, element))
    header = check.header()
    run = subprocess.run([str(semantics_program), "plans"],
                         input="\n".join(header + lines) + "\n",
                         capture_output=True, text=True, check=True)
    plans = run.stdout.splitlines()
    assert len(plans) == len(requests), (len(plans), len(requests))

    cells = []
    for (scope, form, leaf, layers, element, goal), line in zip(requests,
                                                                 plans):
        for category, planned in zip(Category, line.split()):
            if planned == "-":
                continue
            name = f"{scope.value}/{form.value}/{leaf.value}/{goal}/{category.value}"
            if options.only not in name:
                continue
            cells.append(cell(name, scope, form, leaf, layers, element,
                              category, planned))
    parts = [cells[index:index + CELLS]
             for index in range(0, len(cells), CELLS)]
    differ = []
    failed = []
    for found in driver.concurrently([
            lambda index=index, part=part: run_part(
                options, flags, index, part)
            for index, part in enumerate(parts)]):
        differ += found[0]
        failed += found[1]
    print(f"{len(cells)} cells, {len(failed)} programs fail, "
          f"{len(differ)} cells differ")
    (options.work / "differ.txt").write_text("".join(
        f"{name}\t{want}\t{got}\n" for name, want, got in differ))
    for name, error in failed[:5]:
        print(f"  {name}: {error}")
    for name, want, got in differ[:20]:
        print(f"  {name}\n    plan    {want}\n    applied {got}")
    return 1 if differ or failed else 0


def cell(name, scope, form, leaf, layers, element, category, planned):
    """The code of a cell, and the copies and moves its plan says."""
    leaf_type = f"dingo::model::{LEAF_TYPES[leaf]}"
    stored = spell_stored(form).replace("L", leaf_type)
    if stored.endswith("&"):
        stored = stored[:-1].strip()
    interface = interface_of(semantics.Bound((), element), leaf).spelled
    how, cost, *steps = planned.split(":")
    shape = spell_request(layers, element, leaf_type, interface)
    request = f"{check.CATEGORY_TYPES[category]}<{shape}>"
    makes = "true" if semantics.SCOPES[scope].makes else "false"
    if stored.endswith("]"):
        source = f"external_object<{stored}>()"
        make = None
    elif stored.endswith("*") and semantics.SCOPES[scope].makes:
        make = f"fresh<{stored}>::make()"
    else:
        make = f"instance<{stored}>::make()"
    # The library classifies the types and plans the request as the code
    # compiles; the plan has to be found, and to satisfy the request as the
    # semantics program's does.
    plan = (f"dingo::detail::execution::request_plan<{stored}, {request}, "
            f"{leaf_type}, {makes}, false>")
    code = f"""  {{
    // {name}
    using plan = {plan};
    static_assert(plan::transient.found);
    static_assert(static_cast<int>(plan::transient.how) == {how});
    keeper kept;
    auto &stored = {"kept.keep<" + stored + ">([] { return " + make + "; })" if make else source};
    counting_on();
    {{
      auto reached = dingo::detail::execution::interpreter<{leaf_type}, plan::interface_type>::run_list(dingo::detail::execution::plan_steps_t<plan::transient, plan::identities>{{}}, dingo::detail::execution::carry<{makes}>(stored), kept);
      {request} result = dingo::detail::execution::satisfy<plan::transient.how, {request}, {leaf_type}, plan::interface_type>(reached, kept);
      (void)result;
    }}
    counted();
  }}"""
    return name, code, cost


def semantics_array_layer():
    return check.LANGUAGE.index("array")


def spell_request(layers, element, leaf_type, interface):
    """The shape of a request, around its element, as C++ spells it."""
    spelled = leaf_type if element == "leaf" else interface
    for name in reversed(layers):
        if name == "pointer":
            spelled = f"std::add_pointer_t<{spelled}>"
        elif name == "array":
            spelled = f"{spelled}[]"
        elif name == "array2":
            spelled = f"{spelled}[2]"
        elif name == "variant":
            spelled = f"std::variant<{spelled}, dingo::model::other>"
        else:
            spelled = {"shared_ptr": "std::shared_ptr<{}>",
                       "unique_ptr": "std::unique_ptr<{}>",
                       "optional": "std::optional<{}>"}[name].format(spelled)
    return spelled


PROGRAM = """// Generated by spec/execution_check.py. Do not edit.
#include "model/probe.h"

#include <dingo/request/execution.h>

#include <cstdio>
#include <functional>
#include <new>
#include <vector>

using namespace dingo::model;

namespace {
template <typename X> using value_of = X;
template <typename X> using const_lvalue_of = const X &;
template <typename X> using const_pointer_of = const X *;

// What the steps make for the request, constructed in place and kept until
// the cell ends.
class keeper {
public:
  template <typename T, typename Make> T &keep(Make &&make) {
    void *place = ::operator new(sizeof(T), std::align_val_t(alignof(T)));
    T *made = ::new (place) T(make());
    kept_.push_back([made] {
      made->~T();
      ::operator delete(made, std::align_val_t(alignof(T)));
    });
    return *made;
  }
  ~keeper() {
    for (auto each = kept_.rbegin(); each != kept_.rend(); ++each) {
      (*each)();
    }
  }

private:
  std::vector<std::function<void()>> kept_;
};

void counting_on() {
  leaf_interface::copies = 0;
  leaf_interface::moves = 0;
  leaf_interface::counting = true;
}

void counted() {
  leaf_interface::counting = false;
  std::printf("%ld,%ld\\n", leaf_interface::copies, leaf_interface::moves);
}
} // namespace

int main() {
CELLS
}
"""


def run_part(options, flags, index, part):
    """Compiles and runs a part of the cells: those whose counts differ from
    their plans, and the program if it fails."""
    source = options.work / f"execution_{index}.cpp"
    source.write_text(PROGRAM.replace(
        "CELLS", "\n".join(code for _, code, _ in part)))
    program = options.work / f"execution_{index}"
    test = str(Path(__file__).parents[1] / "test")
    # Every compile waits for one of the compilers the driver allows at once.
    with driver.COMPILES:
        result = subprocess.run(driver.compiler_command(
            options.compiler, *flags, "-I" + test, str(source), "-o",
            str(program)), capture_output=True, text=True)
    if result.returncode != 0:
        if driver.EXHAUSTED.search(result.stderr) and len(part) > 1:
            # Fewer cells take less memory: the halves are compiled apart.
            half = len(part) // 2
            found = [run_part(options, flags, f"{index}_{side}", cells)
                     for side, cells in enumerate((part[:half],
                                                   part[half:]))]
            return ([item for each in found for item in each[0]],
                    [item for each in found for item in each[1]])
        return [], [(f"execution_{index}", driver.first_error(result.stderr))]
    run = subprocess.run([str(program)], capture_output=True, text=True)
    counts = run.stdout.splitlines()
    if run.returncode != 0 or len(counts) != len(part):
        return [], [(f"execution_{index}",
                     f"exited {run.returncode} after {len(counts)} cells")]
    return [(name, cost, got) for (name, _, cost), got in zip(part, counts)
            if cost != got], []


if __name__ == "__main__":
    sys.exit(main())

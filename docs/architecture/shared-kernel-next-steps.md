# Shared-Kernel Next Steps

Status: working notes, deliberately untracked. `feature/shared-kernel` tip is
`c66fde5`.

This file records the genericity/unification work for `feature/shared-kernel`.
It stays untracked until the work is accepted or folded into the architecture
docs.

The M9.2 branch is treated as a semantic reference, not a merge target. The
current library stays incremental; the model gets more generic.

References:

- branch: `feature/m9.2-switch`, tip `90e44c5`
- switch milestone:
  `docs/architecture/spec-driven-design.md#m92-switch-and-removal` on that
  branch
- migration ledger: `docs/architecture/test-migration.md` on that branch
- route rules: `spec/content/rules.py` on that branch
- wrapper semantics: `spec/content/wrapper_semantics.py` on that branch
- contracts: `spec/content/contracts.py` on that branch

## Goal

Make wrapper support and route expectations derive from semantic properties and
rules instead of concrete wrapper names and per-form lists.

The library keeps answering "is this route supported?". The model answers:

- what a wrapper can do (`owning`, `sharing`, `nullable`, `borrowable`, ...),
- how a route composes from rules,
- why a shape is unavailable,
- which cases must be generated and checked.

## Non-goals

- Do not port the M9.2 spec-driven library or its 578-file switch.
- Do not generate the whole library from the spec.
- Do not replace `type_traits` with a new public extension point; extend it
  compatibly.
- Do not adopt M9.2 behavior changes wholesale; decide each one against our
  ownership/lifetime rules.
- Do not make the model the source of truth for whether the library compiles.
  The library remains the oracle; the model generates expectations and reasons.

## Diagnosis

| area | current shape | limitation | | --- | --- | --- | | Wrapper
classification | `type_traits` flags (`is_pointer_like`, `is_owning_handle`,
`is_array_handle`, `value_type`, `wrap`/`make`/`from_pointer`) | every new
wrapper or composition adds a branch in `request_lift.h`; semantics are implicit
| | Route model | `storage_form_traits` lists + `binding_*_resolutions` +
per-shape expectations | new optional/const/handle targets require editing form
lists, ledgers, interface counts, and static trait expectations | | Model tables
| `spec/model/semantics/routes/ledger.py` (`OPERATION_KINDS`, `RETENTION`),
`spec/gen/resolution/interface.py` (`INTERFACE_ROUTES` counts) |
hand-maintained; const-handle support required 19 ledger edits and five
interface-count edits | | Shape grammar | `spec/gen/resolution/type_table.py`
`Shape`/`Constructor` | no variants, no custom wrappers; wrapper properties are
implied by constructor names | | Lift/direct decision |
`container::resolve_entry`, the dependency origin overload, `static_container`
public and origin overloads | the same policy is encoded in four places | |
Contracts | `tools/cost/measure.py`, `test-cost`, codegen probes | not tied to
routes/features, so new routes can silently cost more |

## Principles borrowed from M9.2

| M9.2 idea | smaller version here | | --- | --- | | Wrapper semantics are
classes of handles with traits, not concrete types | property model in `spec`;
later optional semantic flags in `type_traits` | | A route is a derivation over
rules | Python `route_rules` model derives operation kind, retention, and
support reasons | | One engine, three binding times | factor the lift/direct
policy into one resolution-policy helper with static/runtime backends | |
Spec-generated expectations | generate table/behavior expectations and ledger
data from the rule model | | Contracts | object-size, instantiation-count,
codegen, and header probes for added routes | | Differential baseline | pinned
outcome JSON for table/behavior, with declared changes | | Wrapper extension by
semantics | `type_traits` semantic properties consumed by `request_lift` | |
Variants are first-class tree nodes | add variant shapes and alternative routes
to the grammar and behavior model |

## Step 1: wrapper semantics and route rules in the model

Risk: low. Python and generated tests only.

- [x] Add `spec/model/grammar/shapes.py`:
  - `WrapperSemantics(name, render, kind, properties)`.
  - Properties: `owning`, `sharing`, `sole_owner`, `nullable`, `borrowable`,
    `array`, `array_handle`, `variant`, `in_place`, `mapped`, `move`, `const`.
  - Built-ins: const, lvalue/rvalue reference, pointer, optional, unique/shared
    handle, unique/shared array handle, array reference/pointer, variant.
  - Applicability is declared per wrapper, so a user wrapper is one entry.
- [x] Add `spec/model/semantics/routes/rules.py`:
  - Rules: `access`, `borrow`, `move`, `copy_out`, `take`, `wrap`, `share`,
    `adopt`, `decay`, `select_alternative`, `nested_access`, `const_copy`,
    `target`.
  - The storage-form model (`storage policies` exposure) and the wrapper
    semantics are the rule input; a route is a derivation and the first blocking
    rule names the miss.
- [x] Replace `type_table.Constructor` composition with wrapper-chain
  composition. Existing generated C++ spellings are unchanged where the wrapper
  chain spells the same type.
- [x] Add variant shapes (`std::variant<int, long>`, `optional<variant>`,
  `shared_ptr<variant>`, alternative requests).
- [x] Add const as a wrapper property, not a special target list.
- [x] Derive the support reason from the failed rule, e.g.
  `take: a borrowing scope cannot become an owner`,
  `decay: the form does not store a bounded array`,
  `select_alternative: the form does not store a variant`.
- [x] Keep `SHAPES`/`build_rows` output compatible where possible so the C++
  table and behavior templates change minimally. The direct-support check now
  asks both the leaf interface and the stored-wrapper interface, because a
  binding publishes both route lists.

Definition of done:

- `SHAPES` is generated from wrapper semantics.
- `type_table` and `type_behavior` still pass against the library.
- The report explains misses by rule, not by structural category.
- No library changes.

## Step 2: generate the route ledger and interface expectations

Risk: low to medium. Model-only, but it touches assertions.

- [x] Derive `route_ledger.OPERATION_KINDS` and `RETENTION` from
  `storage policies` (scope/form) + route rules.
  - `route_derivation.py` derives the sequence from the semantic source shape
    (pointer, lvalue, rvalue, cyclical pointer/handle) and the stored wrapper.
    The later follow-up replaces its form scripts with the property rule engine.
    `route_ledger` still decides the outcome from the exposure and asks the
    recipe only for supported rows.
  - The hand tables are gone. A supported form with no rule raises during
    validation.
- [x] Derive `interface_routes.INTERFACE_ROUTES` counts from the same model.
  - `route_derivation.interface_route_counts` keys the counts by exposed form
    and whether the interface is a copyable value; the interface entries now
    name their form instead of carrying counts.
- [x] Keep the generated C++ route asserts as the oracle. A mismatch must name
  the form, the category, and the rule. The generated C++ is unchanged.
- [x] Remove the hand tables once the generated values match.
- [x] Add a pytest that checks every supported route has a derived kind and
  retention sequence.

Definition of done:

- Adding a form/wrapper target does not require editing `OPERATION_KINDS`,
  `RETENTION`, or interface counts by hand.
- The const-handle/optional-value change would have been a rule/property edit,
  not 19 ledger edits.

## Step 3: one resolution-policy helper for direct/lift

Risk: medium. Library change, but no public API change.

- [x] Introduce `include/dingo/static/resolution_policy.h`:
  - `resolution_policy<T, LookupKey, Bindings, Mode>` exposes `lift_enabled`,
    `static_binding_present`, `static_route_served`, `lift_before_direct`,
    `lift_type`, and `inner_type`.
  - It replaces the `direct_supported` / `inner_type` / `construct_lifted` split
    proposed here: the backend supplies the direct and lift bodies, and the
    policy names the branch.
  - `compile_time` lifts when the static route is not served; `runtime_fallback`
    lifts before direct only when a static binding exists but does not serve the
    wrapper, otherwise it tries direct and lifts on the missing-route exception.
- [x] Make `container::resolve_entry`, the dependency origin overload,
  `static_container` public, and `static_container` origin call the same policy.
- [x] Keep the existing distinct exception and direct precedence.
- [x] Keep runtime/static shape tests as the parity gate.

Definition of done:

- One place decides direct/lift; the four entry points only supply a backend
  policy.
- Existing `request_lift` tests and the 47 type-behavior tests still pass.

## Step 4: semantic properties in `type_traits`

Risk: medium. Public extension point, but additive.

- [x] Add semantic flags with defaults derived from the current flags:
  - `is_owning`, `is_sharing`, `is_nullable`, `is_borrowable`, `is_array`,
    `is_variant`.
  - Route operations `can_copy_out` and `can_take`.
- [x] Keep `is_pointer_like`, `is_owning_handle`, `is_value_borrowable`,
  `is_array_handle`, `value_type`, `wrap`, `make`, `from_pointer` unchanged.
- [x] Base `request_lift` on the semantic flags and the route operations
  (`copy_out`, `take`, `wrap`, `adopt`, `make`) instead of concrete-type
  branches.
- [x] Add a generated test that compares the C++ properties with the Python
  wrapper model for every built-in shape and `test_optional` / `test_shared` /
  `test_unique`.
- [x] Document the properties for user wrappers.

Definition of done:

- A user wrapper declares semantics once and gets lifting/route behavior from
  the properties.
- The Python model and C++ traits cannot drift silently.

## Step 5: variants and custom wrappers in generated coverage

Risk: low to medium. Test/model expansion.

- [x] Add `std::variant` shapes and alternative requests to the table and
  behavior catalog.
- [x] Add custom wrapper families (`test_optional`, `test_shared`,
  `test_unique`) to the generated table/behavior, not only to hand-written
  tests.
- [x] Add a custom variant-like wrapper if `alternative_type_traits` supports
  it.
- [x] Cover const inside wrappers from the rule model.
- [x] Extend the static shape suite automatically for the new rows.

Definition of done:

- Cross-wrapper and variant support is generated, not hand-enumerated.
- A new wrapper family is a model data entry plus tests.

## Step 6: contracts and a differential baseline

Risk: medium. Tooling and CI.

- [x] Record a baseline JSON for table/behavior outcomes:
  - form/shape, direct/lifted/top-level,
  - result or exception class,
  - construction/copy/move/destruction counts,
  - `sizeof` of containers/bindings.
  - `type_table` writes `build/test/type-table.json`; the behavior catalog
    contributes its expected request and count rows.
- [x] Fail on an undeclared difference; require a declared change entry to
  update the baseline.
  - `report/baseline.py check` compares the pinned file and prints the
    differences; `update --reason ...` is the only writer and appends the change
    to `test/baselines/model-changes.json`.
- [x] Add probes for:
  - object size versus number of bindings,
  - instantiation counts for `register_type` and `resolve` (time-trace),
  - codegen size/inlining for a representative `resolve`,
  - preprocessed header size.
  - The static-container size contract is independent of the binding count;
    `measure.py` records preprocessed bytes alongside text/data/bss and the
    optional time-trace count; the codegen probe remains the inlining gate.
- [x] Tie the probes to the route/feature being added, so a new route cannot
  silently increase cost.
  - A feature change must either match the baseline or use `update --reason`;
    the cost comparison records the declared size/header budget.

Definition of done:

- The optional-empty, const-handle, and cyclical-row-pointer changes would be
  visible as declared differences, not discovered ad hoc.
- Contract regressions are explicit.

## What not to bring over

- The 578-file spec-driven switch or deletion of current internals.
- A generator that owns the whole library.
- A new public extension point replacing `type_traits`.
- M9.2 behavior changes wholesale; review against our ownership/lifetime rules.
- The M9.2 fixed-capacity descriptor tables; they cost ~100 KB read-only per
  binding type before exact sizing, and still needed a read-only-data contract.

## M9.2 reference notes

What it did well:

- one engine over descriptors for static/runtime/mixed,
- wrapper semantics as traits, not concrete types,
- route derivation over rules,
- generated expectations and diagnostics,
- contracts and a differential baseline,
- a test-migration ledger for the switch.

What kept it unmerged:

- M9.1 in progress (remaining stored-variant case), M9.3 not started,
- first request still 90–105 ns versus the baseline's 55–70 ns,
- ~100 KB read-only data per binding type before M10.5 exact sizing,
- read-only-data contract still open,
- allocator used for bindings only; unused arena config macros,
- `dense_index::build` has no bound check,
- header-cost checking disabled,
- GCC 16 / Clang 22 contract baselines stale,
- MSVC not built,
- some migration-ledger claims not mutation-checked,
- long constructor chains exceed Clang 18 constexpr steps above 86 links,
- a big-bang 578-file switch.

## Progress log

- 2026-10-25: plan recorded. No code changes yet.
- 2026-10-25: Step 1 landed (no library changes).
  - The catalog is now 100 shapes (was 81), including 19 variant shapes.
  - Table: 3,200 rows, 290 supported (194 direct, 96 lifted), 15 variant rows
    supported; `shared`/`unique`/`external` + `alternative` cover
    `variant_value` and the leaf `value`.
  - Misses are grouped by rule: `take` 801, `decay` 527, `borrow` 484,
    `nested_access` 257, `select_alternative` 252, `move` 207, `wrap` 201,
    `target` 130, `copy_out` 33, `const_copy` 18.
  - The direct check now ORs the leaf and stored-wrapper interfaces. Asking only
    the stored-wrapper interface under-reported leaf and alternative routes (258
    supported before the fix).
  - `target` is now the honest "the rules derive a route but the library does
    not publish it" bucket (130 rows): double-wrapper requests, and rvalue cases
    where the hand exposure bits disagree with the published routes. Step 2's
    derived ledger should explain or remove them. Step 5 adds the remaining
    variant/custom-wrapper behavior cases.
  - Next: Step 2 derives `route_ledger.OPERATION_KINDS`/`RETENTION` and
    `interface_routes` counts from the same rule model.
- 2026-10-25: Step 2, operation kinds and retention landed.
  - `spec/model/semantics/routes/derivation.py` derives the kind/retention
    sequence from the form's stored wrapper and the scope's source shape. The
    two hand tables in `route_ledger.py` are removed; `route_ledger` keeps the
    outcome/access decisions.
  - Source shape is scope data now (`Scope.returns_pointer`): shared hands a
    non-wrapper value out as an address, external as an lvalue, unique as an
    rvalue, and cyclical pointer storage as a pointer-to-pointer.
  - All 3,200 rows and the generated route asserts are unchanged. The C++ route
    probe remains the oracle.
  - Still open in Step 2: derive `interface_routes.INTERFACE_ROUTES` counts.
- 2026-10-25: Step 2 complete.
  - `interface_routes` counts now come from
    `route_derivation.interface_route_counts`, keyed by exposed form and
    copyable interface value. The six interface cases keep their
    storage/interface data only; the generated C++ counts are unchanged.
  - The ledger and interface tables are fully derived; a supported route with no
    script fails validation.
  - Next: Step 3, one resolution-policy helper for direct/lift in the library.
- 2026-10-25: Step 3 landed.
  - `resolution_policy.h` owns the static route probe, liftability, and the
    direct/lift precedence. The four entry points branch on the policy and
    supply only their direct and lift bodies.
  - The bodies remain in separate `if constexpr` branches so a static-only
    direct route does not instantiate the inner request, and a runtime fallback
    keeps the original key for its retry.
  - No public API change. `request_lift` tests, the runtime/static shape suite,
    and the type-behavior tests pass.
  - Cost probe: 232,873 text / 573,136 object bytes versus 232,825 / 573,088
    before the step (+48 bytes, +0.02%); median compile 2.72 s versus 2.97 s.
  - Next: Step 4, semantic properties in `type_traits`.
- 2026-10-25: Step 4 landed.
  - `type_traits` now carries `is_owning`, `is_sharing`, `is_nullable`,
    `is_borrowable`, `is_array`, `is_variant`, `can_copy_out`, and `can_take`.
    Enabled specializations declare the semantic properties; the primary
    template supplies the disabled defaults.
  - `request_lift` uses the semantic flags directly (`can_take`, `is_array`,
    `is_variant`); its `wrap`/`adopt`/`make` selection is named as a route
    operation.
  - `families/property.py` generates a static property probe for every built-in
    wrapper and the custom `test_optional`/`test_unique`/`test_shared` families.
    It passes against the C++ flags.
  - Cost probe is unchanged from Step 3 (232,873 text / 573,136 object bytes);
    median compile 2.72 s.
  - Next: Step 5, variants and custom wrappers in generated coverage.
- 2026-10-25: Step 5 landed.
  - `wrapper_semantics.py` now emits one cross-wrapper shape pair per custom
    family: `test_optional`/`std::optional`, `test_unique`/`std::unique_ptr`,
    `test_shared`/`std::shared_ptr`, and `test_variant`/`std::variant`. The
    built-in cross-product is unchanged, so the custom families add nine shapes
    instead of multiplying the table.
  - The table grows from 3,200 to 3,584 rows (112 shapes); the new supported
    family rows are 14 `test_optional`, 2 `test_shared`, 2 `test_unique`, and
    the custom variant shapes are correctly reported as unsupported request
    shapes. `std::variant` keeps its 15 supported rows.
  - `type_table` and `type_behavior` include the custom wrapper header;
    `type_behavior` adds identity helpers for the custom families. The static
    suite follows the same `SHAPES` catalog automatically.
  - `wrapper_properties` covers `test_variant` as well; its
    `alternative_type_traits` specialization makes `is_variant_type_v` true.
  - Generated sizes grow from 5.4 MB to 6.0 MB for the table and 1.3 MB to 1.5
    MB for the runtime behavior suite; the library cost probe is unchanged.
  - Next: Step 6, contracts and a differential baseline.
- 2026-10-25: Step 6 landed.
  - `type_table` now writes `build/test/type-table.json`. The pinned
    `test/baselines/model.json` has 3,584 table rows and 95 actual behavior rows
    with construction/copy/move/destruction counts and result classes.
  - `dingo_model_baseline_test` compares after `dingo_model_test`; an undeclared
    difference fails and shows the update command. The declared change log
    starts empty at `test/baselines/model-changes.json`.
  - `measure.py` records preprocessed bytes and compares them with the same
    zero-default tolerance as object sizes and trace events.
  - `object_sizes.cpp` asserts the static-container size is independent of the
    binding count.
  - Next: no remaining shared-kernel step; keep the baseline and cost gate with
    each future route.

## Further unification

The six steps above are complete. The next work moves unification from the model
layer into the C++ probes and then the library.

- [x] One shared C++ probe header: `test/support/model_probe.h` owns route-list
  matching, access checks, kind/retention lookup, direct support, and lifted
  support. The table, behavior, route, and interface templates include it
  instead of re-declaring the helpers.
- [x] Derive `route_derivation.py` from a rule engine over wrapper properties
  and shape categories. The form scripts are gone; the engine selects the first
  matching rule by category, semantic kind, source shape, and wrapper
  properties. The generated C++ route asserts remain the oracle.
- [x] Derive behavior expectations from the selected route: request class,
  relation, and copy/move counts. The per-form `EXPECTATIONS` table is gone;
  `derive_expectation` uses the semantic form kind, wrapper properties, and
  scope. Construction counts are derived from scope/kind/element count. The
  external registration move stays explicit; extra multi-route cases are derived
  from the same route shape.
- [x] Derive custom wrapper partners from the semantic signature instead of
  `CUSTOM_WRAPPER_PARTNERS`. `wrapper_signature` uses kind plus shape properties
  and `partner_for` finds the unique built-in wrapper; a family with no partner
  still contributes its own shape.
- [x] Derive scope/form exposures from capability descriptors; keep explicit
  exceptions for raw arrays and cyclical array handles. `Form.kind` and
  `Form.sharing` feed `exposure`; the per-scope exposure tables are gone.
  Borrowing, consuming, and cyclical policies are explicit scope capabilities.
- [x] Add scope/source descriptors as the first source-materialization slice:
  - `scope_info_of<Scope>` derives consumes/stable/guard/retention/cycles from
    the generated scope policy.
  - `storage_yielded_type_t<Storage>` and `source_category_of<Storage>` derive
    pointer/lvalue/consume-value/consume-pointer from the storage shape.
  - The existing `with_*` materializers remain; a unified materializer is still
    blocked by template depth.
- [x] Add one `resolution_policy::resolve` executor shared by the runtime and
  static entry points. The direct and lift bodies are tagged callables so the
  unused arm is not instantiated. The non-liftable runtime path short-circuits
  before the executor to preserve the deep dependency-chain template depth.
- [x] Move the per-binding route list into a resolution-route descriptor.
  - `resolution_route<Storage, Resolution>` owns one selected route.
  - `resolution_candidates<Storage, Resolutions...>` owns the binding request
    list.
  - `resolve_candidate` dispatches a runtime index; the static resolver executes
    `resolution_route` directly.
- [x] Extend the baseline from expected behavior counts to actual runtime
  outcomes and exception classes. The behavior template writes
  `behavior-outcomes.jsonl`; the generated annotation negative tests record
  their exception class there too.
- [x] Represent a conversion as an ordered step list and execute it with one
  walker: `conversion_route` derives the steps from the nested conversion type,
  `conversion_walk` applies them, and `resolution::conversion_steps` exposes the
  result. Leaf conversions remain the primitives.

2026-10-25: the route-recipe rule engine landed. `route_derivation.py` now has
an ordered `RULES` table keyed by value/object/rvalue category, semantic form
kind, source shape, and wrapper properties; the per-form `_value_steps`,
`_object_steps`, and `_rvalue_steps` scripts are removed. `form_kind` derives
optional/handle/array-handle/variant/pointer/array/value categories from the
stored wrapper and form capabilities. Route kinds, retention, interface counts,
and generated C++ asserts are unchanged.

2026-10-25: behavior expectations are route-derived. `behavior_cases.py` no
longer has the per-form `EXPECTATIONS` table. `derive_expectation` selects the
primary request, relation, request class, leaf/array checks, and construction
count from the semantic form kind, stored wrapper properties, and scope.

- [x] Derive interface and annotation scenarios from behavior forms:
  - `INTERFACE_SHAPES` is derived from the forms; unsupported forms are in
    `INTERFACE_GAPS`.
  - Shared `ANNOTATION_SHAPES` are derived from the forms; unsupported forms are
    in `ANNOTATION_GAPS`. External instance/move facts remain explicit.
  - Request-form rules generate requests, relations, classes, and names.
    Generated behavior source and the outcome baseline are unchanged.

2026-10-25: custom partner derivation landed. `CUSTOM_WRAPPER_PARTNERS` is gone.
`wrapper_signature` compares the wrapper kind and shape properties
(owning/sharing/sole-owner/nullable/borrowable/array/variant), and `partner_for`
finds the unique built-in wrapper. Pipeline, property, and generated shape
catalogs are unchanged.

2026-10-25: scope/form exposures are capability-derived. The `Scope.exposures`
tables are gone. Each form now carries a semantic kind and sharing fact, and
`exposure` derives the bits from the scope policy: borrowing scopes publish
value/lvalue/pointer, consuming scopes publish rvalue plus the leaf lvalue or
pointer they can lend, and cyclical scopes publish the leaf and sharing handles.
Raw arrays and the cyclical shared-array handle remain explicit exceptions.
Generated scope policies, route outcomes, and the baseline are unchanged.

2026-10-25: the resolution-policy executor landed. `resolution_policy::resolve`
owns the direct/lift arm selection for `container::resolve_entry`, the
dependency origin overload, and both `static_container` resolve overloads.
Direct bodies are non-generic; lift bodies read the policy through a tag, so a
non-liftable request never names `inner_type`. The runtime non-liftable path
bypasses the executor to keep the 48-link dependency chain under the
template-depth limit. Generated behavior, model outcomes, and public API are
unchanged; the cost probe improves slightly (232,829 text / 573,112 object bytes
versus 232,873 / 573,136).

2026-10-25: the behavior baseline is actual runtime output. The generated
behavior tests write `build/test/behavior-outcomes.jsonl` with each executed
case's counts and result; the two annotation negative tests record
`exception:dingo::type_not_found_exception`. `report/baseline.py` now reads that
report instead of the Python expectation table, and the pinned baseline grows to
95 behavior rows. Generated behavior source, library behavior, and the table
outcomes are unchanged apart from the report side effect.

2026-10-25: per-binding resolution routes landed.
`include/dingo/binding/resolution_route.h` adds
`resolution_route<Storage, Resolution>` and
`resolution_candidates<Storage, Resolutions...>`. The runtime request-list fold
now dispatches through `resolve_candidate`, and the static resolver calls
`resolution_route::apply`; both reach the same conversion executor. The
generated outcomes and baseline are unchanged; text/object size is slightly
smaller and the header grows by ~3 KB.

2026-10-25: extra behavior routes are derived. `EXTRA_EXPECTATIONS` is gone;
`derive_extra_expectations` adds the unique plain rvalue case and the shared
alternative wrapper case from the semantic kind, scope, and primary request
class. Generated behavior source and the outcome baseline are unchanged.

2026-10-25: scope/source descriptors landed. `scope_info_of` exposes the
generated scope policy as one constexpr descriptor, and
`storage_yield_traits<Storage>` exposes `yielded_type` plus a
pointer/lvalue/consume-value/consume-pointer category. These are the M9.2-style
data inputs for a later single materializer; no runtime/static materializer was
changed in this slice.

2026-10-25: source-materialization cleanup. Attempting to drive the static
resolver branches from `source_category_of` still pushed the 48-link dependency
chain over the template-depth limit, so that attempt was reverted. The exact
duplicate `with_tracked_binding_source` was removed and its one caller now uses
`with_binding_source`. Cost improves to 232,546 text / 571,792 object bytes; the
header grows by the descriptor and resolution-route declarations.

2026-10-25: the smaller flat source-lifetime dispatch landed for the value path.
`with_source_lifetime` chooses context-lvalue/context/retained/local from
`source_category` and the scope/context capabilities, then calls the existing
lifetime backends. `consume_request` uses it; `resolve_request_address` keeps
the old branches so the deep dependency chain still compiles. Cost: 232,374 text
/ 574,376 object bytes, preprocessed bytes 2,560,586.

2026-10-25: source materialization is unified. `resolve_request_address` now
uses `with_source_lifetime` too. To make room, `resolve_singular` and
`resolve_static_selection` were inlined into their single callers, removing two
template layers per dependency link; the static and runtime dependency chains
pass. The old `with_*` functions remain as lifetime backends behind that
dispatch. Cost is effectively unchanged: 232,581 text / 571,856 object bytes,
preprocessed bytes 2,557,670.

2026-10-25: interface and annotation scenarios are derived. The flat
`INTERFACE_CASES` and `ANNOTATION_RECIPES` lists are now generated from storage
shape descriptors plus request-form rules; concrete storage/factory/leaf/
external-instance facts remain data. Generated behavior source and the outcome
baseline are unchanged.

2026-10-25: interface and annotation shapes are derived from the behavior form
catalog. `INTERFACE_SHAPES` comes from the shared/plain/unique handle forms and
a copyable handle variant; unsupported forms are explicit in `INTERFACE_GAPS`.
Shared annotation shapes come from plain/shared-handle/optional/pointer and the
interface leaf; unsupported forms are in `ANNOTATION_GAPS`; external
instance/move facts remain explicit. Generated behavior source and the baseline
are unchanged.

2026-10-25: the "plan" vocabulary is gone from the route concepts. The
direct/lift selector is `resolution_policy` in
`include/dingo/static/resolution_policy.h` with `resolution_mode::compile_time`
/ `runtime_fallback` and `resolution_policy::resolve`. One selected route is
`resolution_route` with `apply`; a binding's published set is
`resolution_candidates` with `resolve_candidate`. The source lifetime category
is `source_lifetime`, dispatched by `with_source_lifetime`. The Python rule
engine is `route_derivation.py` with `ResolutionCandidate` and
`DERIVATION_RULES`. Committed as `4414d48`. No behavior, generated source, or
baseline changed; `ctest` is 77/77, pytest is 216 passed, and the cost probe is
unchanged at 232,581 text / 571,856 object bytes (2,557,699 preprocessed bytes).

2026-10-25: conversion routes are one descriptor and one walker.
`conversion_route` flattens the nested `type_conversion` tree into steps in
execution order; `conversion_walk` runs them with a continuation and preserves
value/reference lifetime at each step. `resolution::conversion_steps` exposes
the descriptor, and generated route forms assert every published route has
steps. The model's `ResolutionStep` is renamed to `ResolutionCandidate` because
it describes one published route, not an internal step. Committed as `3b57a54`.
Generated source and the outcome baseline are unchanged; ctest is 77/77, pytest
is 216 passed, and the cost probe stays at 232,581 text / 571,856 object bytes
with 2,570,896 preprocessed bytes.

2026-10-25: the step-sequence gap is closed. `STEP_RULES` derives the internal
conversion step kinds for each published candidate from its category, candidate
kind, form kind, and source shape. Generated route forms now assert the exact
step count and every step kind, not just a non-empty list. Candidate-level
source retention remains on `ResolutionCandidate`; C++ does not model retention
per internal step, so the step descriptor carries kinds and order only.
Committed as `ec13d12`. Generated source and the outcome baseline are unchanged;
ctest is 77/77, pytest is 219 passed, and the cost probe is unchanged at 232,581
text / 571,856 object bytes.

2026-10-25: unavailable routes are now asserted, not just recorded. Generated
route forms emit zero-route assertions for every unavailable value/object/rvalue
category in a probed form. Forms that publish no route at all and cannot
instantiate a storage alias are listed explicitly in `ABSENCE_PROBE_GAPS`; the
route-form validator fails if an all-unavailable form is neither probed nor
listed. Committed as `7c78432`; ctest is 77/77 and pytest is 221 passed.

2026-10-25: candidate target shapes are asserted. `TARGET_SHAPES` and
`route_candidate_targets` derive whether each candidate resolves a value, an
lvalue/rvalue reference, a pointer, an array, an array pointer, or a wrapper;
the route ledger carries the sequence and generated route forms compare it with
a C++ `target_shape_of` probe. Exact C++ spellings, exact access types, and
cache types remain implementation territory. Committed as `ca6d3d0`; ctest is
77/77 and pytest is 224 passed.

2026-10-26: type-list element selection is ported from the M9.2 mechanics.
`type_list_at` now selects through C++26 pack indexing when available, else
`__type_pack_element`, else the portable inheritance selection, instead of
recursing one element at a time. `DINGO_VARIANT_PACK_AT_INDEXING` and
`DINGO_VARIANT_PACK_AT_BUILTIN` override the selected path; a forced-portable
translation unit compiles the fallback in CI. Committed as `da0b034`. Binary
size is unchanged; the cost probe reports +2 preprocessed bytes (0.0001%), and
the zero-tolerance header gate flags it.

2026-10-26: route candidates and probe accessors are unified. One
`RouteCandidate` now carries kind, retention, steps, and target shape; the
ledger stores one tuple and exposes kinds/retention/steps/targets as views. The
generated route forms iterate candidates directly. The recursive probe accessors
in `model_probe.h` are replaced by `route_at_t` over the non-recursive
`type_list_at`. Committed as `f0ca627`; ctest is 77/77 and pytest is 224 passed.

2026-10-26: conversion metadata is one traversal. `conversion_descriptor` now
carries both the execution steps and the retained cache types, replacing the
parallel `conversion_route` and `conversion_cache_types` walks.
`type_resolution` consumes the descriptor once. Committed as `8aa843a`;
preprocessed bytes drop by 864 and binary size is unchanged.

2026-10-26: composite conversion dispatch is one template. The seven
`type_conversion` specializations that only forwarded to `conversion_walk_t` are
gone; the generic `type_conversion` delegates every non-leaf conversion to the
walker. Leaf conversions keep their direct primitives. Preprocessed bytes drop
by 4,871 and compile median improves; binary size is unchanged.

2026-10-26: the binding core is in place. `binding_core` owns typed source
materialization, `resolution_route` application, conversion retention, and
address/value shaping. `static_binding_policy` supplies inline static storage
and direct source lifetime; `runtime_binding::runtime_binding_policy` supplies
the erased runtime storage and rollback wrapper. `static_binding_resolver` and
`runtime_binding::dispatch_request` now delegate to the core. The direct
`evaluate_static_binding` value path still uses the old materialization helper;
folding it in needs another template-depth check. Cost regressed for now: text
+2,403, object +22,792, preprocessed +2,630; defer to the next cycle. Committed
as `192a8b9`; ctest is 77/77 and pytest is 224 passed.

2026-10-26: the static value path joins the binding core.
`evaluate_static_binding` now uses `binding_core::resolve_value`; the old direct
materialization helper is gone. The unused runtime
`resolve_request_address_from_source` helper was removed, and the core methods
are left non-always-inline to keep text smaller. Cost still regressed: text
+1,972, object +20,584, preprocessed +2,320. The next cycle is cost recovery.
Committed as `144a22c`; ctest is 77/77 and pytest is 224 passed.

2026-10-26: runtime binding layers are thinner. Removed `runtime_binding_base`,
`binding_request`, `runtime_binding_state_traits`, the `resolution_container`
forwarder, and the separate runtime `static_binding_activation`; merged
`binding_cache_slot` into the conversion-cache base; inlined the runtime
`with_resolution_container` forwarder; and merged construction/retention into
`runtime_binding_policy`. Compared with the previous kernel slice, text drops by
451, object by 1,136, and preprocessed by 1,543. Relative to the pre-kernel
dispatch baseline, text is still +1,521 and preprocessed +777. Committed as
`d0d861c`; ctest is 77/77 and pytest is 224 passed.

2026-10-26: runtime construction and aliases are thinned further. The state has
one `construct_container(context*)` instead of two overloads, and unused
runtime-binding aliases are gone. Text/object are unchanged from `d0d861c`;
preprocessed bytes drop by another 146. Remaining text growth is in the static
core path, not the runtime adapter; the runtime cache/state merge is a larger
change because the state is shared across interfaces. Committed as `8c61ebd`;
ctest is 77/77 and pytest is 224 passed.

2026-10-26: branch-free execution core attempt. Removing the
`direct_source_lifetime`/`direct_value_source` hook makes `binding_core` call a
policy `with_source`/`with_value_source`, which adds one function-template layer
per dependency link. The 48-link chain then exceeds the 900 template-depth
limit. Inlining `make_binding_resolver` and duplicating the static resolve
adapter freed enough depth to compile, but duplicated the adapter logic and
regressed text/preprocessed size. The attempt was reverted. To remove the hook
without duplication, first free one layer in the source-lifetime ladder (or
inline one backend), then retry.

2026-10-26: `binding_core` is mode-agnostic. Removed the
`direct_source_lifetime`/`direct_value_source` branches; the core always calls
`Policy::with_source`/`with_value_source`. The static policy now owns the
source-lifetime ladder, and the runtime policy owns its rollback wrapper. Moving
the ladder into the static policy avoided the extra template layer that
previously exceeded the 900 depth limit. Text is unchanged from `8c61ebd`;
preprocessed bytes drop by 120. Committed as `dadb23e`; ctest is 77/77 and
pytest is 224 passed.

2026-10-26: selection status is shared. `selection_status_from_count` is the one
not-found/found/ambiguous rule; the static `static_binding` result and the
runtime selection accumulator both use it, and the runtime candidate visitor no
longer hand-counts matches. Static still returns a type and runtime returns a
pointer, so the next slice is the common candidate-set contract; the index and
type-list sources remain mode-specific. Text is unchanged; preprocessed grows by
1,166 from the new selection scaffolding. Committed as `e93d5c6`; ctest is 77/77
and pytest is 224 passed.

2026-10-26: runtime lookup storage split begins. `runtime_lookup_binding_view`,
`runtime_lookup_value`, and `runtime_bindings_state` moved from
`runtime/registry.h` to `runtime/selection.h`. This is the first slice of the
registry split; selection routes and registration validation still live in
`registry_store`. Text is unchanged; preprocessed grows by 210 from the new
header boundary. Committed as `779ce91`; ctest is 77/77 and pytest is 224
passed.

2026-10-26: handover added at `docs/architecture/shared-kernel-handover.md`
(commit `c66fde5`). It records the current core/conversion/lookup state, cost,
next work, and constraints. This plan file remains untracked.

## Container unification (M9.2 reference)

Status 2026-10-26. Reviewed `feature/m9.2-switch` tip `90e44c5`. This reworks
handover Next Work #4 ("container mode policies") against what that branch
actually did to unify the containers. No code changes yet.

### What M9.2 did

It did not merge the static and runtime container bodies. It split the code into
a source protocol, one selection engine, and thin containers.

**1. A source protocol with no decisions in it.** Every binding set answers the
same questions, whether the bindings are known when the type is named or arrive
at registration:

```text
count<Request, Key>(key_value)                     -> 0, 1, or 2
resolve_named<Request, Key>(resolver, key_value)   -> Request
select_named<Request, Key>(resolver, key_value, otherwise)
collect<Collection, Key>(resolver, out, key_value, append)
```

Static sources add compile-time-only `fixed_count<Request, Key>()`,
`serves_named<Request, Key>()`, and `holds_key_of<Binding>()` for cross-source
registration conflicts. Implementations: `backend/static_backend.h`,
`backend/runtime_backend.h`, and `backend/local.h` (`own_bindings`,
`local_view`). The backends only count, resolve, and collect; they make no
precedence, ambiguity, or fallback decision. The static backend resolves a
compile-time index; the runtime backend resolves a pointer candidate and
memoizes it.

**2. One selection engine.** `selection/selection.h` (`selection<Self>`) is the
single place that encodes precedence, ambiguity, parent fallback, collection
merging, auto-construction, and construct-vs-resolve. `Self` declares its
sources as capability flags plus accessors:

```text
has_static_source          + static_source()
has_runtime_source         + runtime_source()
parent_type (void if none) + parent()
has_local_static_source    + local_static_source()
has_local_runtime_source   + local_runtime_source()
container_resolver(), retained(), transactions()
```

The rule order lives in `selection/selection_rules.gen.h` as constexpr data:
singular = local -> container -> elements -> parent -> auto-construct;
collection = local -> container -> parent, merged; construct = local ->
container -> parent -> factory. The engine asks each source
`known_count<Request, Key, Source>()`; a step that cannot name anything when the
request is compiled is skipped with `if constexpr`, so the static path
instantiates no runtime code and emits the not-found/ambiguous diagnostic at
compile time only when no runtime source can still change the outcome.

**3. Thin containers.** `container.h` is `selection<Self>` +
`container_operations<Self>` + `container_member_types<Traits>` + state. All
four shapes are specializations of one template and differ only in declared
flags and state ownership:

| shape | static source | runtime source | local sources | moves | | --- | --- |
--- | --- | --- | | `container<Traits, Allocator, Parent>` | no | yes | no | yes
| | `container<bindings<...>, ...>` | yes | yes | no | iff no static bindings |
| `static_container<bindings<...>, ...>` | yes | no | no | no | | registration
object | host | host | yes | n/a |

`container_state<State, Allocator, bool Moves>` is the ownership policy: the
state is allocated apart and followed by its holder when the container moves, or
kept in place when it does not. `parent_link` points at the parent's *state*, so
a moved parent keeps its children valid. `runtime_container` became a one-line
alias of `container`; one `container_operations` owns `construct`, `invoke`,
`construct_collection`, and `register_type_collection`.

### What we have instead

The same decisions are currently written more than once:

| M9.2 concept | our tree today | problem | | --- | --- | --- | |
`selection<Self>` |
`container_with_static_bindings::{resolve, resolve_collection, resolve_missing, unresolved, binding_status}`
(`container.h`),
`static_container_impl::{resolve, construct, construct_request, resolve_request, resolve_missing, unresolved}`
(`static_container.h`), `resolution_policy::resolve`,
`binding_resolution_policy` | precedence/ambiguity/fallback encoded in four
places | | `static_backend` | `static_bindings::selection<>` + `static_registry`
| selection is a type-list computation, execution is in the registry; no shared
protocol | | `runtime_backend` | `runtime_registry` + `runtime/selection.h`
(split in progress) | the lookup half is becoming the natural runtime source;
registration stays in `registry_store` | | `local_view` / `own_bindings` |
`RuntimeConfig` flags `owns_runtime_data`, `merge_parent_collections`,
`runtime_only` + the same class instantiated recursively | local bindings are a
container configuration, not a source level | | `container_state` |
`runtime_container` heap wrapper + `impl_` + `container_implementation`
special-case; mixed container in place and non-movable | ownership is a second
container class and a pointer-mapping hack | | `container_operations` |
`resolve`/`construct`/`invoke`/collection entry points duplicated in
`container.h` (`container_with_static_bindings`, 1225 lines) and
`static_container.h` (`static_container_impl`, 676 lines) | two nearly parallel
container implementations instead of one engine + two configurations | |
`selection_rules.gen.h` | hand-coded order in the four places above | no single
readable rule order |

### Recommended target

```text
              container_collections<Self>      container_operations<Self>
                        \                 /
                         selection<Self>
                              |
          +-------------------+-------------------+
          |                   |                   |
   static source        runtime source       local view
 (static registry)   (lookup store + reg.)  (local statics + proxy)
```

- One `selection<Self>` owns the decisions and is the only place that knows the
  order of sources.
- `static_registry` and the runtime lookup store implement the same protocol
  names natively. Do not put an adapter object between the engine and a backend:
  it costs a template level per dependency link (see Risks).
- The container classes declare flags and provide accessors. `static_container`
  becomes the same engine specialization with `has_runtime_source = false`.
- Registration-local bindings become a `local_view` level with
  `has_local_static_source` / `has_local_runtime_source`, replacing the
  `RuntimeConfig` booleans.
- State ownership becomes a policy (`container_state<State, Allocator, Moves>`)
  and `runtime_container` becomes an alias. `container_implementation` and the
  `impl_` mapping can then be deleted.

### Slices

Each slice is one commit; behavior, generated source, and
`test/baselines/model.json` must stay unchanged.

1. **Prerequisite: finish the lookup split.** The runtime source is exactly the
   selection half of `registry_store`. Finish moving selection into
   `lookup_selection` first; do not move the same code twice.
2. **Name the source protocol on the existing sources.** *(Applied.)*
   `static_registry` exposes `resolve_named`, `collect`, `fixed_count`; the
   runtime store exposes `resolve_named`, `collect`, `count`. Slice 3 is the one
   decision function.
3. **One decision function.** Replace the hand-coded singular, collection, and
   construct branch trees in `container_with_static_bindings` with one
   always-inline function that asks the sources in one order. Keep the resolve
   bodies and the execution/registration in the registries. No CRTP base yet, so
   no per-link template level. Both runtime-only and mixed come for free
   (`has_static_source = sizeof...(Registrations) != 0`). The static-only path
   has only three links of headroom on GCC (see Risks), so this function must
   *replace* the existing static resolve entry, not wrap it.
4. **Share the public operations.** Move `resolve`, `construct`, `invoke`, and
   the collection entry points into `container_operations<Self>`, and make
   `static_container_impl` a configuration of the same engine. Encode today's
   behavior asymmetries as policy constants, not as separate code:
   `static_ambiguity = compile_error`, `allow_auto_construct = false`,
   `empty_collection = compile_error`. Do not change the behavior in this slice.
5. **State and move policy.** Introduce
   `container_state<State, Allocator, Moves>`, make `runtime_container` an alias
   of `container`, and keep the mixed/static state in place. Re-pin
   `test/registration/object_sizes.cpp` (the current bounds are `EXPECT_LE`, so
   a smaller runtime container is fine, but movability and address stability are
   contracts).
6. **Local view.** Replace `owns_runtime_data`, `merge_parent_collections`, and
   `runtime_only` with `has_local_static_source` / `has_local_runtime_source` on
   a view that implements the same protocol. Collection merging becomes a rule
   of the engine, not a boolean threaded through the container.

Slices 2-4 add no template level per dependency link; slice 5 removes one
container class; slice 6 removes the recursive configuration.

### 2026-10-28 status

The cleanup work on `feature/shared-kernel` is done: dead declarations, the
forwarding adapters, the construct dispatch, the storage slots, and conversion
path slices 1-2. Both sources carry the protocol names: `static_registry`
exposes `resolve_named`, `collect`, and `fixed_count`; the runtime store exposes
`resolve_named`, `collect`, and `count`. The static source's singular resolve no
longer branches on collections. `container_operations<Self, RuntimeSource>` now
owns both no-scope `resolve` entries, the dependency-path `resolve`,
`construct`, `invoke`, and the collection count/append/construction entries,
including the static minimal-context and construct rules.

What remains is the rest of the decision layer:

- `static_registry::resolve_named` resolves its compile-time selection;
- the runtime `resolve_named` executes a selection the engine made, with the
  cache probe keeping `select_binding` between select and resolve;
- collection `count`/`collect`/construct still live on each source with
  different shapes.

The next commit should finish the operations layer and replace the static
resolve entry with it, not wrap it:

1. Add `container_operations<Self, RuntimeSource>` and move `resolve` and
   `construct` into it; make both front-ends derive from it and remove the old
   public entries in the same commit. *(Both resolve entries moved; `construct`
   remains.)*
2. Move the static minimal-context fast path into the layer as a policy query.
   *(Applied with the static resolve body.)*
3. Fold the construct tree into the layer's construct variant.
4. Move `invoke` and the collection entry points last.

[Container Engine](container-engine.md) records the shape, constants, and gate.

Gate: `test/container/dependency_chain.cpp` (45/48 links), `object_sizes.cpp`,
and a cost probe before and after. The static-only path has three links of
headroom; one wrapper that is not the final engine entry costs one of them.

### What not to copy

- **Generated rule tables.** `selection_rules.gen.h` is constexpr read-only data
  driven by the spec generator. We keep one small hand-written policy struct
  with the same order; no `spec/`, no generator, no arrays.
- **Runtime loops over rules on the static path.** M9.2 relies on `known_count`
  \+ `if constexpr` to erase them. Keep that discipline; never instantiate a
  runtime source for a static-only container.
- **Thunks, descriptors, and the `dense_index` tables.** They are the reason
  M9.2 paid ~100 KB of read-only data per binding type before M10.5.
- **Registration-time duplicate rejection** (`add_unless` / `holds_key_of`). Our
  contract is that a static and a runtime binding of one key are ambiguous at
  request time; keep it and keep `holds_key_of` out of the protocol until we
  decide otherwise.
- **Behavior parity changes.** Auto-construction, static ambiguity as a compile
  error, and the empty-collection rule stay as they are. Any change is a
  separate declared baseline update.
- **Component renames and the big-bang switch.** Not part of this branch.

### Risks

- **Template depth is the hard gate.** `test/container/dependency_chain.cpp`
  runs 48 links on GCC and 100 on Clang for the mixed container; the 900 limit
  is about 18 levels per link, so one extra function-template level per link
  already overflows. Measured headroom (2026-10-26, GCC and Clang 22):

  - mixed `container<bindings<...>>`: 48 links on GCC, 100 on Clang;
  - static-only `static_container<bindings<...>>`: 45 links on GCC (46 fails),
    100 on Clang.

  The static-only path is the tighter side and now has its own canary in the
  same test. It must not gain a per-link layer; a shared decision function has
  to replace the existing static resolve entry, or free a layer first. The
  historical precedent is `dadb23e` (moving the source-lifetime ladder into the
  static policy) and the inlining of `resolve_singular` /
  `resolve_static_selection` that made room.

- **Zero-overhead static path.** No vtable, no erased request, no runtime
  members in a static-only container. The engine must branch on
  `Self::has_*_source` with `if constexpr`; unused sources must never be
  instantiated.

- **Behavior asymmetries are real.** `static_container` and
  `container<bindings<...>>` differ today in auto-construction, ambiguity
  reporting, and empty collections. The engine can express all three as data,
  but the first unification commit must preserve them. Changing them is a
  declared baseline change, not cleanup.

- **Cost gate.** Every slice compares `build/cost` (text/object/preprocessed),
  `sizeof` contracts, and the codegen lit probe. Expect a small header growth
  for the new boundary; explain it, do not hide it.

- **Model/C++ asymmetry is intentional.** The Python model remains the
  independent expectation oracle; do not derive the engine's order from it in
  the same commit.

- **Untracked plan.** This file stays untracked.

### Verification

The existing gate plus the two canaries:

```bash
cmake --build build -t format
cmake --build build -t generated-check
cmake --build build -j8
ctest --test-dir build -j8
./.venv/bin/python -m pytest -q spec/tests tools/cost
cmake --build build -t check
```

- `dependency_chain_test` on GCC and Clang after every slice that touches the
  resolve entry chain.
- `object_sizes_test` and the codegen lit probe after slices 4-6.
- Generated behavior source and `test/baselines/model.json` unchanged.

2026-10-26: runtime lookup selection is split. `runtime/selection.h` now owns
the lookup index types, `lookup_index_route`, the entry traits and per-interface
filtering, `select_binding` / `select_binding_at_interface` / `select_lookup` /
`make_selection`, `for_each_lookup`, collection candidate iteration,
`count_collection`, `binding_status`, and `source_select`. `registry_store`
extends it and keeps execution, registration validation and commit, rollback,
and registry data ownership. Behavior, generated source, and the outcome
baseline are unchanged; ctest is 77/77, pytest is 224 passed, and `check`
passes. Cost against `build/cost/split1.json`: text 234246 (+144), object 591448
(+144), preprocessed 2569711 (+2515). The growth is the header boundary and the
explicit protocol re-exports, not new instantiations. Committed as `dc78f48`.
Next: the container source protocol and one decision function, starting with the
runtime source.

2026-10-26: the static-only dependency chain is a canary. The depth budget was
measured and pinned in `test/container/dependency_chain.cpp`: the mixed
container resolves 48 links on GCC and 100 on Clang, while `static_container`
tops out at 45 links on GCC (46 fails) and 100 on Clang. The static-only test
uses the measured bound, so a shared decision function that adds one template
level per link now fails the build instead of a user's deep graph. The next
container slice must replace the static resolve entry rather than wrap it.
Committed as `d303c83`; ctest is 77/77, pytest is 224 passed, and `check`
passes. Cost is unchanged (test-only change).

2026-10-26: the `invoke` operation is shared. `detail::container_invoke` in
`factory/invoke.h` owns the callable dispatch and the type guard; each container
only makes the context and runs the transaction when it has one. Text and object
size are unchanged; preprocessed grows by 111 bytes from the helper. Committed
as `0dfde92`.

2026-10-26: the miss rule is shared. `binding/container_miss.h` owns
`resolve_missing_request` with two capability policies: a runtime/mixed policy
(auto-construction, default collections, diagnostics carrying the resolve
context) and a static policy (no auto-construction, unbound types reported at
compile time). Both `unresolved` entry points are thin forwarders, so the
"nothing answered" rule is written once. The helper must stay always-inline:
without it the static codegen probe grew by 1,311 text bytes; with it the probe
is 234248 text (+2) and 2572066 preprocessed (+2244). Committed as `3b0a0a1`.
Next: share the collection rule, or take on the singular source engine.

2026-10-26: both depth canaries were verified on Clang 22 through the sanitizer
build: all three `dependency_chain_test` cases pass at 100 links, and the full
unit suite is 360/360. The GCC canaries remain 48 mixed and 45 static-only.

2026-10-26: a runtime-only cost probe was added. `tools/cost/probes/runtime.cpp`
and the `cost-record-runtime` target record shared/unique/interface/collection/
external runtime resolution without any static bindings, writing
`build/cost/current-runtime.json`. First record: text 149550, object 383896, bss
30, preprocessed 2528539. This gives runtime layer changes their own signal
instead of measuring them through the mixed static probe. Committed as
`73201c3`; the handover records the target and the remaining time-trace gap as
`736ac8f`.

2026-10-26: the singular selection consumer landed.
`binding/container_selection.h` adds `container_selection<Self>`: the source
order for a singular request, the compile-time static status combined with the
runtime selection, the static-only branch, and the hand-off to each container's
miss rule. `Self` declares `has_static_source`, `has_runtime_source`,
`static_binding_uses_result`, the static status/selection aliases,
`may_auto_construct_v`, `parent_serves_v`, and non-template
`static_source()`/`runtime_source()` accessors. The inherited resolve replaces
the old entry, so the canaries are unchanged.

- `43585b7` wired the mixed/runtime container. The cache entry keeps a
  `resolve_selected` path so a miss is not selected twice; the parent-search
  test caught the double select and now passes.
- `d3f48c6` wired `static_container_impl` and deleted `resolve_request`. The
  static-only canary is still exactly 45 links on GCC (46 fails), so the
  replacement is depth-neutral.

Cost against the pre-engine record (`build/cost/miss2.json`): text 232102
(-2146), object 589224 (-2224), preprocessed 2575597 (+3531). The runtime-only
probe is 147523 text (-2027), 383488 object (-408), 2533139 preprocessed
(+4600). The duplicated resolve bodies are gone; the header and mode branches
account for the preprocessed growth.

Still mode-specific: collection resolution (the engine's collection branch
delegates to `resolve_collection_request`) and `construct`. Those are the next
slices. Next progress entry: after the collection consumer.

2026-10-26: the static-key collection rule moved into the engine. The engine's
collection branch now owns the source count, the explicit-empty lookup, the
parent fallback, and the static-only compile-time empty-collection report; the
static container's `resolve_collection_request` is gone and the mixed
container's static-key branch is gone. The runtime-only and runtime-key
collection paths remain in the mixed container because they select scalar
collection factories and differ source by source. Committed as `094ac84`.

Cost: static probe 232228 text (+126), 589752 object (+528), 2577366
preprocessed (+1769) against `build/cost/engine-final.json`. Runtime probe
147523 text and 383488 object (unchanged), 2535009 preprocessed (+1870). All
tests pass and the static-only canary is still 45 links.

Next: `construct` / auto-construction. It is the last mode-specific resolve path
and needs its own cycle: rvalue conversion, normalized requests, factories, and
the static compile-time diagnostics are all involved.

2026-10-26: the static-binding half of `construct` is shared.
`static/construction.h` adds `construct_from_static_source`: the exact binding,
the normalized binding wrapped into the request, the rvalue-conversion
diagnostic, and the factory fallback. The mixed container's `construct_static`
and the static container's `construct_request` now call it with their own
selection types, so the selection normalization is unchanged. The runtime/mixed
outer construct flow (runtime binding checks, transactions, normalized requests,
factories) is still in the container.

One C++ lesson: non-generic lambdas passed as callbacks are instantiated with
the enclosing function, so the static-only `resolve<user_type>()` path fired its
compile-time unbound assertion even in a discarded `if constexpr` branch. The
callbacks are generic lambdas taking a tag; their bodies are instantiated only
when the used branch calls them.

Committed as `86ecb23`. Cost: static probe 232128 text (-100), 589392 object
(-360), 2578000 preprocessed (+634) against `build/cost/collections.json`;
runtime probe unchanged in text/object, +1363 preprocessed. All tests pass and
the static-only canary is still 45 links.

Next: the runtime/mixed outer construct flow and the runtime-only/runtime-key
collection paths.

2026-10-26: the runtime and static module files now use role-parallel names.

- `dfa7bf4` renamed the files: `runtime/lookup_selection.h` -> `selection.h`,
  `runtime/runtime_binding.h` -> `binding.h`, `runtime/container_runtime.h` ->
  `storage.h`, `runtime/registration_api.h` -> `registration.h`, and
  `static/activation_set.h` -> `static/binding.h`. All includes, tests,
  examples, the benchmark, docs, and the CMake header list were updated.
- `1bc6516` moved the static container requirements into
  `static/registration.h`.
- `f7036f7` extracted the type-level selection from `static/registry.h` into
  `static/selection.h`, mirroring `runtime/selection.h`.
- `8335dc2` split the static source by role:
  - `static/bindings.h` — `static_bindings`, dependency rules, diagnostics;
  - `static/registry.h` — `static_registry`, which now owns its definition;
  - `static/binding.h` — activation sets, storage slots, resolver;
  - `static/construction.h` — the construct rule plus the factory helpers from
    the deleted `static/resolution.h`. `static/graph.h` now includes
    `bindings.h`, which breaks the old `activation_set -> graph -> registry`
    cycle and lets `registry.h` define `static_registry` instead of only
    declaring it.
- `b38454e` records the layout in the handover.

The folders now read:

```text
runtime/                      static/
  container_traits.h            container_traits.h
  context.h                     context.h
  registry.h                    registry.h
  selection.h                   selection.h
  binding.h                     binding.h
  storage.h                     bindings.h
  registration.h                registration.h
  transaction.h                 graph.h
  lookup_index.h                construction.h
                                resolution_policy.h
```

Gates: ctest 77/77, pytest 224 passed, `check` passes, static-only canary 45
links, text/object unchanged (preprocessed +481 for the new header boundary).
The remaining asymmetry is `static/storage.h`: `static_registry_state` inherits
`static_binding_storage`, so the storage cannot move out of `binding.h` without
changing that inheritance to composition.

2026-10-26: the container and module classes now follow the same role names as
the files.

- `9b03b73` renamed `detail::container_with_static_bindings` to
  `detail::runtime_container_impl`, pairing it with `static_container_impl`.
- `4347641` renamed the detail components: `lookup_store` -> `lookup_selection`,
  `runtime_registration_api` -> `runtime_registration`, `static_requirements` ->
  `static_registration_requirements`, and `container_api` ->
  `container_collections` (the file is `container_collections.h`).
- `ecbe0d2` renamed the public storage/selection classes:
  `dingo::container_runtime` -> `dingo::runtime_storage` (test file is
  `test/runtime/storage.cpp`), `detail::static_binding_storage` ->
  `detail::static_storage`, and the static selection primary template
  `binding_selection` -> `static_selection` (`binding_selection` and
  `binding_selection_t` remain as compatibility aliases).

Class map:

```text
container                 runtime_container / static_container
runtime_container_impl    static_container_impl
container_base            container_collections            container_selection
runtime_registry          static_registry
lookup_selection          static_selection
runtime_storage           static_storage
runtime_binding           static_binding_resolver
runtime_registration      static_registration_requirements
```

Gates: ctest 77/77, pytest 224 passed, `check` passes, static-only canary 45
links, text/object unchanged, preprocessed +466 bytes for the renames and the
compatibility aliases. `static_storage` still lives in `static/binding.h`; the
`static/storage.h` split remains the composition change noted above.

2026-10-26: `static/storage.h` is extracted. `static_storage` now holds only the
per-registration storage and conversion-cache slots. Local activation sets moved
to `local_scope_storage` in `static/binding.h`, and `static_registry_state` is
now the static registry state: it composes the storage and the local scopes, and
the registry owns the activation set. The containers use
`static_registry_state<Registrations...>` for the registry type;
`static_storage_state` remains the storage alias. This is the composition change
the earlier note called for, not a file move.

Cost: text unchanged (230500), object 584960 (-224), preprocessed 2578976
(+502). ctest 77/77, pytest 224 passed, `check` passes, static-only canary 45
links. Committed as `495273a`; the handover records it as `4df7286`.

The runtime/static file and class naming work is now complete. Next: the
runtime/mixed outer construct flow and the runtime-only/runtime-key collection
paths, then `registration_store`.

2026-10-26: the binding storage slot is a shared concept.
`binding_slot<Storage>` (was `binding_storage_holder`) is the base of both the
static `static_binding_slot` and the runtime `runtime_binding_state`, and
`binding_core` reads the storage through `Policy::slot()` instead of
`Policy::storage()` (both policies expose the slot). This is the storage-access
seam, not the full slot protocol.

Committed as `8863db0`. Cost: text unchanged, preprocessed +392. All tests and
the 45-link canary pass.

What remains different is the owner and the source lifetime: the static owner is
`static_storage` inside `static_registry_state`; the runtime owner is
`runtime_registry` + `runtime_storage` + `runtime_scope_state`. The static
lifetime ladder is still in `static_binding_policy`, the runtime one in
`runtime_binding_policy`/`runtime_binding`. Unifying those is the
`settled(context)` / lazy-slot half of the M9.2 slot model; it is the
depth-sensitive next step, not another rename.

2026-10-26: the static side now has the same three-level state model as the
runtime side.

- `basic_static_activation_set` -> `static_registry_state` (the registry state);
  `..._view` -> `static_registry_state_view`.
- `runtime_registry_data` -> `runtime_registry_state`; `runtime_data_holder` ->
  `runtime_state_holder`.
- `kernel_policy` -> `runtime_binding_policy`.
- `binding_storage_slot` -> `static_binding_slot`; `local_binding_scope_slot` ->
  `static_local_scope_slot`; `binding_activation` ->
  `static_binding_activation`.
- `static_binding_state<Registration>` now owns the slot, the conversion cache,
  and the local registry state (the three old per-registration bases). The
  `static_storage` and `local_scope_storage` aggregates are gone;
  `static_registry_state` aggregates `static_binding_state`s and forwards
  `get_slot/storage/conversion_cache/local_scope_for_model`.

The model is now:

```text
static_registry_state        runtime_registry_state
  static_binding_state         runtime_binding_state
    binding_slot                 binding_slot
    conversion cache             conversion cache
    static_local_scope_slot      runtime_scope_state
static_registry              runtime_registry
static_binding_resolver      runtime_binding
static_binding_policy        runtime_binding_policy
```

2026-10-28: the `static_registry_state_view` adapter was removed.
`static_registry` now calls its `static_registry_state` directly, so the static
side has the same shape as the runtime registry, which reads its state without a
view. The earlier rename note above records where the adapter came from; it no
longer exists.

`static_binding_resolver` keeps its name because `static_binding` is already the
type-level selection result; it is a transient resolver over one state, while
`runtime_binding` is the stored erased object. What remains different is the
storage policy (inline typed slots vs arena) and dispatch (typed resolver vs
erased thunk); the source-lifetime ladder is still per-mode.

Committed as `52ef646`; handover `4fd1b31`. Cost: text 230620 (+120), object
585792 (+832), preprocessed 2578940 (-428). ctest 77/77, pytest 224 passed,
`check` passes, static-only canary 45 links.

2026-10-26: `static_binding_activation` is gone. Its methods (`resolve_request`,
`resolve`, `retain_conversion`) moved onto `static_binding_policy`, and
`activation()` now returns `*this`, matching `runtime_binding_policy`. The
static activation class was a split-off view left from the source-lifetime move,
not a second state model.

Committed as `db7cbbb`. Cost: text 230813 (+193), preprocessed unchanged, all
tests and the 45-link canary pass.

Construct is now shared where there is a shared concept: the static-binding rule
is `construct_from_static_source`, used by both containers. The runtime outer
flow (runtime binding check, transactions, normalized requests, factories)
exists only on the runtime side and has no static twin. Folding it further means
moving runtime-specific code into the consumer, not removing a duplicate. The
next real internal unification is the slot lifetime protocol
(`with_source`/`settled` in `binding_slot`), which is depth-gated.

2026-10-26: the first registration slice landed. `runtime/registration_store.h`
owns `key_value_interface`, the registration key traits, key matching, and the
supplied/required key validation. `registry_store` derives from it; commit and
rollback are still in `registry_store` and will move in the next slice.

Committed as `9dfe57b`. Cost: text/object unchanged, preprocessed 2579362 (+916
from the header boundary). All tests and the 45-link canary pass.

2026-10-26: the registration split is complete. `runtime/registration_store.h`
now owns the runtime binding value owners, `insert_lookup`, `commit_lookup`,
`commit_static_lookup`, `commit_registration`, `commit_singular_base_lookup`,
`erase_lookup_binding`, `record_lookup_rollback`, the lookup value factories,
and `check_interface_requirements`, in addition to the key validation.
`registry_store` is execution-only: collection construction, binding resolution,
the lookup cache, and binding-value evaluation.

Committed as `add623a`. Cost: text 230893 (+80), object 587256 (+104),
preprocessed 2580964 (+1602). ctest 77/77, pytest 224 passed, `check` passes,
static-only canary 45 links.

# Cell Coverage Model

This directory is the first slice of the shared kernel's coverage model. It
owns the declarations that the library used to keep only in C++ and in the
hand-curated matrix tags.

## Layout

The model is layered by pipeline stage:

```text
grammar/   what types exist; cpp.py owns C++ spelling
domain/    the storage x request case product
semantics/ support, capabilities, route rules, cells, gaps
emit/      C++ generators, shared recipe/runner, templates
report/    coverage report and outcome baseline
tests/     the model test suite
legacy/    request/binding algebra still consumed by test/matrix
```

Imports flow downwards only: `grammar -> domain -> semantics -> emit -> report`.
`tests/architecture_test.py` fails on an upward import.

- `grammar/cpp.py` is the C++ type algebra. `CppType` is rendered once;
  storage forms, wrappers, annotated/interface types, and emitters build the
  structure instead of concatenating or parsing type strings.
- `grammar/catalog.py` is the single source for the built-in scopes and storage
  forms. A form is a semantic kind plus a `FormConstructor`; applying it to an
  element `CppType` produces the stored type. Storage spellings, factory names,
  const factories, and external storage are derived from that structure, not
  stored as formatting templates. The structural probes instantiate the same
  form with `int` and the behavior suites with their tracked leaf, so the two
  no longer keep parallel catalogs.
- `emit/families/storage.py` writes `classification/<scope>/<form>/storage.cpp` and enumerates every built-in storage scope by storage form
  cell. Each scope declares capabilities (consuming, cyclical, pointer source)
  and each form declares its semantic kind and array/handle facts; the exposure
  bits are derived from those capabilities rather than a per-form table. Raw
  arrays and the cyclical shared-array handle stay explicit unavailable
  exceptions. The model is compared against the scope policies and the
  `storage_form` enum, renders one compile-time assertion per cell, and rejects
  a catalog that is missing or stale. It also generates the committed
  `include/dingo/storage/scopes.gen.h` header, which owns the built-in
  `storage_scope_policy` specializations.
- `emit/selection.py` enumerates every selection policy and pair of source
  statuses. It compares the model against the C++ policy and status enums and
  renders the full rule table as compile-time assertions.
- `semantics/routes/ledger.py` enumerates every scope, form, and route category with one
  explicit outcome: supported, unavailable with a reason, or a tracked gap.
  Supported routes also carry their expected operation-kind and source-retention
  sequence. Raw dynamic arrays are declared unavailable: the element count is
  not part of the type, so no scope can size, construct, or destroy them.
  Unsized shared array handles under `shared_cyclical` are an explicit
  constraint for the same reason.
- `report/baseline.py` pins the actual matrix outcomes and the actual behavior
  runtime outcomes in `test/baselines/model.json`. The behavior report records
  each executed case's counts and result (`ok` or an exception class).
  `check` fails on any difference; `update --reason ...` rewrites the baseline
  and records the declared change in `test/baselines/model-changes.json`.
- `emit/families/property.py` writes `classification/global/wrapper/<wrapper>.cpp` and generates static assertions that compare the C++ wrapper
  properties (`is_owning`, `is_sharing`, `is_nullable`, `is_borrowable`,
  `is_array`, `is_variant`, `can_copy_out`, `can_take`) with the Python wrapper
  declarations for every built-in wrapper and the `test_optional`,
  `test_unique`, and `test_shared` families. A wrapper cannot silently drift
  from its declared semantics.
- `semantics/routes/derivation.py` derives the published `RouteCandidate` sequence for a
  supported route from an ordered rule list over the form's semantic kind, the
  source shape the scope hands out (pointer, lvalue, rvalue, cyclical row
  pointer, or cyclical handle), and the stored wrapper properties. Each
  candidate carries its operation kind, source-retention flag, conversion
  steps, target label, and the exact request `Shape` it resolves, so consumers
  render the request instead of re-deriving it from the form. A second rule
  layer derives each candidate's internal conversion step kinds in execution
  order, so dereference, borrow, and wrapper candidates expose their composed
  leaf. A target-shape rule derives whether the candidate resolves a value,
  reference, pointer, array, or wrapper. Rules do not name a storage form, so a
  custom wrapper with the same kind and properties follows the same rule as the
  built-in wrapper it mirrors. A supported cell with no matching rule fails
  validation instead of losing its kind coverage.
- `emit/families/route.py` instantiates the real storage type and resolution list for
  every supported storage cell, including bounded arrays and array handles. It
  checks value, object, and rvalue route availability against the ledger and
  asserts that an unavailable category publishes no route, requires every
  published route target to be reachable from both the static and
  runtime request matchers, checks that every route's required access is
  satisfied by the scope, checks the operation-kind and source-retention
  sequences derived by `semantics/routes/derivation.py`, checks the full C++ conversion
  step kind sequence and target shape for every published route, and asserts
  that representative routes do not unexpectedly cache conversions.
- `grammar/shapes.py` describes request shapes as a leaf wrapped in zero or
  more wrappers. A wrapper declares its semantics (`owning`, `sharing`,
  `nullable`, `borrowable`, `array`, `variant`, `in_place`), a constructor that
  builds a `CppType`, and which shapes it can wrap. The built-ins include const, references,
  pointers, optional, unique/shared handles, owning array handles, arrays, and
  variants; a user wrapper is another declaration with the same fields. The
  test-only families (`test_optional`, `test_unique`, `test_shared`, and
  `test_variant`) add one cross-wrapper pair each. Their built-in partner is
  derived by shape signature (kind plus owning/sharing/sole-owner/nullable/
  borrowable/array/variant properties), so a new family does not need a
  hand-written partner map.
- `semantics/routes/rules.py` derives a request route from the wrapper semantics and the
  scope/form exposure. The rules are `access`, `borrow`, `move`, `copy_out`,
  `take`, `wrap`, `share`, `adopt`, `decay`, `select_alternative`,
  `nested_access`, `const_copy`, and `target`. The library still answers
  whether a route is published; the rule model names the first rule that blocks
  a request, so the report says `take: a borrowing scope cannot become an owner` instead of a structural category.
- `emit/matrix.py` composes every shape from the wrapper semantics and applies
  it to every storable scope/form cell through the real resolution kernel. It
  writes a `type-matrix.md` report with the direct route, the lifted request,
  the top-level verdict, the route rule for every miss, and the published route
  targets per cell. Supported rows are answered by the library, not by a
  hand-written case list; the report is the exhaustive support table. The depth
  bound is an explicit model limitation. A route that only works as a dependency
  of a longer-lived construction is reported as `top-level = no`: a consuming
  scope can lend an lvalue reference to its dependent, but a borrow returned
  from the container itself would outlive the transaction that produced it.
  A shape that is not a direct target is reported as `lifted` when its value is
  resolvable: value wrappers copy or move their value, and owning handles only
  lift from a consumed value. The report groups misses by rule.
- `emit/traits.py` builds every supported storage cell for a copyable and a
  move-only leaf and asks the resolution kernel whether each shape is
  published. The move-only route set must be a subset of the copyable one and
  must lose exactly the routes that copy the leaf (value copies, const
  pointees promoted from a value, inline wrappers that contain the leaf).
  Borrows, moves, adopted pointers/arrays, and owning-handle copies stay.
- `emit/resolve.py` generates the storage-driven runtime probe. On disk the
  hierarchy is `resolution/runtime/<scope>/<storage>/<category>.cpp`; one file is one
  storage path and category (`resolve` today),
  one GTest suite is that path, and one test is one `(trait, binding, request)`.
  Each test registers the storage once and calls `resolve` through the
  `probe<Request>` helper. A resolve that throws is recorded and the test is
  skipped. Every case has a path
  `<phase>/<container>/<scope>/<storage>/<category>/<traits>/<binding>/<request>`; the GTest test
  name is the path tail and `RecordProperty("path", ...)` writes the full path
  into the test output. Its fixtures are named for the trait
  (`copyable`/`movable`); the behavior suites keep `tracked` because they count
  lifetime operations. The generated tests record the outcome
  instead of asserting; `report/resolves.py` turns `build/test/resolve-gaps.md`
  into covered calls, overclaims, declared gaps, and stale declarations. It is
  a report target (`resolve-check`), not part of `check`, while the model is
  being tightened.
- `emit/detection_constructor.py` generates the first classification slice under
  `classification/constructor/<shape>.cpp`: constructor arity and kind for
  zero-argument, copy-only, move-only, aggregate, dependency, annotated, and
  move-only array shapes.
- `emit/binding_matrix.py` emits compile-time `dingo::bindings<dingo::bind<...>>`
  instantiations under `registration/static_asserts/<scope>/<storage>/binding.cpp`,
  one file per storage, for both leaf traits. It
  checks the registration composition (scope, storage spelling, annotation,
  interface list, and lookup key); runtime families check the route.
- `emit/behavior.py` executes the top-level half of that matrix. Every
  supported shape is resolved twice through the same registration recipe as the
  behavior cells, stable scopes are checked for identity, fresh scopes for
  distinctness, owned raw pointers are released according to the transfer
  contract, and every block finishes with the lifetime balance intact. The
  runtime suite uses `container<>`; a second translation unit runs the same
  catalog through `static_container` for every statically bindable cell
  (`external` is runtime-only). Both are emitted once per leaf trait: the
  copyable and move-only fixtures share the shape catalog, and the
  copy-requiring shapes compile out for the move-only leaf. The shape catalog
  and the top-level verdict come
  from `emit/matrix.py`, so the suite follows the library instead of listing
  supported requests by hand. Unowned or non-transferred pointers are never
  released, and the two consuming lvalue-borrow shapes are emitted as explicit
  top-level gaps rather than silently skipped.
- `emit/families/interface.py` instantiates the real interface resolution kernel for
  storage exposed through `dingo::interfaces<...>`. It checks value, object, and
  rvalue availability and exact route counts, requires every published target to
  be reachable from both request matchers, and checks that the scope access
  satisfies every interface route. The counts come from
  `route_derivation.interface_route_counts`, keyed by the exposed semantic kind and
  whether the interface is a copyable value. Single- and multiple-interface
  bindings are covered.
- `emit/families/behavior.py` generates one behavior class per supported scope/form
  cell in the applicable runtime and static containers. Form registration and
  expectations are data: a form comes from `grammar/catalog.py`, and its
  scope/container product is derived from the route ledger. The tests check
  request identity, construction counts, copy and move counts, array element
  identity, leaf addresses, repeated resolution, and the lifetime balance after
  the container scope ends. The primary request, relation, request class, and
  construction count are derived from the semantic form kind, wrapper
  properties, and scope. Copy and move counts come from the derived request
  class (`borrow_value`, `consume_value`, `consume_rvalue`, `reference`,
  `handle`, `array`) instead of being hand-written per cell, so a count that
  does not match the request shape fails the build. The external instance and
  its registration move are derived by `emit/common/recipe.py` from the form's wrapper
  properties. A consumed raw
  pointer or row pointer transfers ownership to the caller, and its behavior
  case deletes the result before checking the balance. Multi-route cells add
  derived secondary cases: the unique plain cell also moves the rvalue route,
  and the shared alternative cell also copies the variant wrapper value.
  Supported
  cells without a resolvable behavior case are listed in `BEHAVIOR_GAPS` with a
  reason, so a cell cannot silently lose its test. Interface and annotation
  scenarios are derived from the behavior forms plus a request-form rule set,
  not a flat list of complete cases. Forms with no interface or annotation
  route are listed in `INTERFACE_GAPS` / `ANNOTATION_GAPS`. The interface cases
  exercise storage
  exposed through `dingo::interfaces<...>`: shared
  references, pointers, and shared handles, plus unique owning handles. These
  cases use the same request-class cost model and assert the same construction,
  copy, move, and lifetime balance checks. Multiple-interface bindings are
  covered too: each interface is resolved through the same implementation, and
  the secondary request check proves both interfaces alias one object. A
  copyable interface value case covers value slicing, including the tracked
  copy count. Annotation recipes cover shared annotated concrete values,
  annotated interface references, pointers, shared handles, and handle
  references, and external annotated value, reference, pointer, and
  shared-handle registrations. Negative tests refuse unannotated and wrong-tag
  requests and reject duplicate annotated registration.
- `grammar/expr.py` is the type grammar: leaf, `const`, references, pointers,
  arrays (including multidimensional extents), optional/unique/shared handles,
  array handles, variants, and annotations. Constructors reject invalid C++
  compositions; `Expr.cpp()` produces a structured `CppType`, and
  `grammar/cpp.py` renders it for probes.
- `grammar/bindings.py` is the binding side of a path case. A `Binding` is
  concrete, annotated, keyed, interface, annotated-interface, or collection; it
  decides which request qualifiers the registration exposes. The
  `annotated_interface` kind composes a tag and an interface list, so
  `annotated(interface(request, interface_type), tag)` maps to
  `annotated<...interface_type..., tag>` and registers
  `interfaces<annotated<interface_type, tag>>`.
- `grammar/enumerate.py` is the single bounded type enumeration. Given a leaf it produces
  the object universe, the request projection, and the storage projection.
  Composed objects (wrappers and variants) get the same access closure as plain
  paths. Constructor names come from `type_expr.CONSTRUCTORS`; `tests/grammar_test.py`
  fails when a registered constructor is not consumed by the request
  projection, storage forms, wrapper semantics, shape mapping, or bindings.
- `grammar/traits.py` declares the leaf copy/move traits. A path describes a
  shape; the leaf's special members decide whether a copying route exists.
  Every storage cell and request shape is classified once per trait, and
  `emit/traits.py` asserts the move-only answer is exactly the copyable answer
  minus the routes that copy the leaf. Every generated runtime family runs
  both leaves; the move-only variant selects the first non-copying route and
  expects zero copies.
- `grammar/storage.py` projects stored expressions from `grammar.storage_types()` and
  filters them by scope. A path is storable when its expression fits the scope;
  const is a normal grammar constructor, not a separate table. The result is a
  `StoragePath` with its scope, expression, form, and name.
- `domain/cases.py` pairs the request projection from `grammar.request_types()` with
  the storage paths. The bounds are qualifier depth, wrapper depth, array
  extents, and variant arity.
- `semantics/support.py` classifies a path as supported, unavailable, unsupported with a
  rule, or unmodeled. The classification uses the existing route rules and
  ledger; unmodeled paths are never silently dropped.
- `report/paths.py` writes the path coverage report (`build/paths.md` and JSON) with
  domain counts, outcome totals, per-family/per-storage/binding tables, rule
  histograms, binding applicability, a per-leaf-trait coverage table, and the
  comparison between supported paths and the cases the generated C++ suites
  exercise. Its path-extended shapes are
  merged into the type matrix. The full design is in
  [Type Path Coverage](../../docs/architecture/type-path-coverage.md).
- `report/resolves.py` compares the per-storage resolve outcomes with the
  generated manifest. An overclaim is a call the model expects that threw; a
  stale declaration is a declared exception that resolved.
- `report/coverage.py` compares the supported path set with the compile-time
  type, trait, and binding matrices and the runtime shape, behavior, annotated,
  keyed, interface, and const probes. Every coverage key carries the leaf
  trait. The difference is split into undeclared gaps and declared gaps with
  reasons.
- `emit/families/annotated.py`, `emit/families/keyed.py`,
  `emit/families/interface_runtime.py`, and `emit/families/const.py` emit
  runtime probes. Annotated covers every supported leaf/reference/pointer
  interface, including const storage. Keyed covers every supported direct-leaf
  request; wrapped conversions are outside the keyed capability rather than
  claimed as supported. Interface runtime covers direct value/reference/pointer
  and handle requests in stable scopes, including annotated-interface
  registrations. Const walks the same classified case set as the report and derives
  each registration from the storage form. The report shows the cases that
  remain outside each harness.
- `semantics/cells.py` owns the scope/form product and its supported subset. The route
  probes, type matrix, type behavior, and behavior families iterate the same
  cell list; "is this cell publishable" has one answer.
- `legacy/` is test/matrix-only and is not imported by the packaged model;
  `tests/architecture_test.py` enforces the boundary. `legacy/request.py` is
  the request algebra. A request is a `Shape` plus qualifiers;
  its `provides`, `requires`, and `dependency_forms` follow from the wrapper
  chain. The first slice derives the concrete resolved rows; interface, key,
  annotation, and collection qualifiers follow the same mechanism.
- `legacy/binding.py` is the binding/exposure algebra. An exposed type is a binding
  form (concrete, interface, custom, keyed, annotated) applied to a stored
  kind; the capability tags and registration recipe are derived from the form
  and its interface/key/tag qualifiers.
- `semantics/gaps.py` is the one exception registry: behavior, interface, annotation,
  and absence-probe exceptions live together with their reasons, so the full
  undeclared-coverage surface is visible in one place and a reason cannot be
  empty.
- `emit/common/recipe.py` renders the stored type, factory, external instance, runtime
  registration, and static binding source for one cell. The behavior,
  type-behavior, interface, and annotation families share it, so a form is
  spelled once.
- `emit/common/runner.py` owns the Jinja environment, output writing, and `--check`
  handling shared by every family. A family module provides `build_rows()` and
  a template name; it does not re-implement the CLI.
- `templates/` contains logic-free Jinja templates. The Python model computes
  rows; the templates only lay them out.
- `*_test.py` is the pytest suite for the models and their deterministic
  output.

The generated translation units are built by `test/CMakeLists.txt` into
`build/test/generated/model/`. They are development artifacts, not shipped
headers, so consumer builds do not need Python.

`cells-check` regenerates both outputs and fails on any difference. The same
check also validates the model against the C++ catalogs, so adding a built-in
scope, storage form, selection policy, or status without extending the model is
an error rather than an untested cell.

Add a form or scope by extending the model first:

1. Add the C++ declaration.
1. Add one entry to `grammar/catalog.py`: the scope policy or the form's semantic kind,
   wrapper, leaf-relative C++ spelling, headers, and capability facts. Exposure,
   registration, factories, and external instance facts are derived.
1. Run `cells-check`; the generated assertions are the list of cells that must
   be implemented.

When the scope policies change, regenerate the committed header:

```bash
uv run --locked python -m tools.model storage \
    --out build/model/storage_cells.cpp \
    --header include/dingo/storage/scopes.gen.h \
    --include include
```

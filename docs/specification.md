# Specification

[`spec/`](../spec) holds an executable specification of the library and the
checks that compare the library with it. The specification does not call the
library:

- [`vocabulary.py`](../spec/vocabulary.py) names the concepts of the library,
  one enum for each concept.
- [`semantics.py`](../spec/semantics.py) takes a registration and a request. It
  determines whether the library accepts the registration, which interfaces the
  registration binds, whether the request resolves, and how the request fails.

The C++ headers that the checks include are in [`test/model/`](../test/model).
[Development](development.md#model-checks) describes how to run the checks.

## Semantics

Terms:

- A _registration_ stores a type in a scope and binds interfaces.
- The _leaf_ `L` is the object type at the center of the stored type.
- The _stored form_ is the type around the leaf: `L`, `L &`, `L *`, `L[2]`,
  `std::unique_ptr<L>`, `std::unique_ptr<L[]>`, `std::shared_ptr<L>`,
  `std::shared_ptr<L[]>`, `std::shared_ptr<L[2]>`, `std::optional<L>`,
  `std::variant<L, other>`, or two of `std::shared_ptr`, `std::unique_ptr` and
  `std::optional`.
- A _request_ names a _shape_ of a bound interface `I` in a _category_. A shape
  is `I`, an array handle of `I`, or up to three of `std::shared_ptr`,
  `std::unique_ptr` and `std::optional` around `I`. A category is value,
  `const`, `&`, `const &`, `&&`, `*` or `const *`.

The semantics see a type as a stack of layers around an element:
`std::shared_ptr<L>` is the layer `shared_ptr` around `L`. Each layer, leaf and
scope has declared properties: what it owns, whether a copy of it shares what it
holds, and whether it can be copied, moved or made from a value. The rules use
only these properties, never a stored form.

A request resolves if both conditions are true:

1. A derivation reaches the request from what the scope stores. A derivation is
   a sequence of steps, such as `element` (take the element of a handle),
   `copy`, `transfer` (a `std::unique_ptr` to a `std::shared_ptr`), `upcast` (to
   the abstract interface), or `wrap` (move a value into a new layer).
2. Everything that the result refers to lives long enough. A result returned to
   the caller can refer only to objects that the container or the caller keeps.
   A dependency of a call through `invoke`, or of a kept class, can also refer
   to objects made for the request.

A new layer holds only what the request owns: a value made for the request, or a
copy that shares the instance, such as a copy of a shared handle. A copy that
shares nothing is not the instance, so no new layer holds it. For example, a
`shared` scope that keeps a `T` gives no `std::shared_ptr<T>` and no
`std::optional<T>`. A `shared` scope that keeps a `std::shared_ptr<T>` gives a
`std::optional<std::shared_ptr<T>>`, which shares the `T`. The binding keeps
such a layer with the instance, so the caller can refer to it.

The abstract interface of the leaf is a base of the leaf. A reference, a pointer
or a handle refers to it. The semantics never copy it and never put it by value
into a new optional or alternative type, because that would slice the leaf.

A request that does not resolve fails as follows:

- Both containers reject at compile time a request that its types rule out.
- The static container rejects all other such requests at compile time, except a
  request for the alternative that an alternative type does not hold. That
  request throws `type_not_convertible`.
- The runtime container throws `type_not_convertible` for all other such
  requests.

### Empty Layers

A pointer, a handle and an optional may hold nothing. An alternative type may
hold the other alternative. The element of such a layer may be absent, and so is
everything that is derived from it. A reference to an absent element, or a value
of it, fails when the layer is empty. Two steps from such a layer do not fail:

- `element_pointer` gives a pointer to the element, which is null when the layer
  is empty.
- `optional_element` gives the optional that the layer holds, which is empty
  when the layer is empty.

Empty layers do not change whether a request resolves, or its cost. The
semantics also define whether the cheapest derivation of a request is defined
when a layer is empty: it is if it reaches the request without going through an
absent element.

### Layer Sources

A binding has the type that it stores. A request has only the interface that it
names. So the steps of a derivation divide between them:

- The binding takes the steps that need the stored type or that only go into the
  stored object: `element`, `element_pointer`, `optional_element`, `upcast`, and
  the release of what a pointer that it made points to. A request of the
  abstract interface cannot name the leaf, so the binding also takes the steps
  from an object that holds the leaf that the upcast needs: it wraps the leaf
  into a layer that rebinds to the interface, such as a new handle, or it wraps
  the stored layers whole. What these steps reach are the _layer sources_ of the
  binding. A binding of the leaf has at most three layer sources of each layer
  of the stored type: the layer, a pointer into it, and the optional that it
  holds.
- The request takes every other step from the layer sources. Each of these steps
  adds a layer or converts one, so a request finds the layer sources that it
  needs from its own shape.

A request resolves from the layer sources exactly as it resolves from the stored
object, at the same cost, and is as defined when a layer is empty.
[`semantics_test.py`](../spec/semantics_test.py) checks this for every cell of
the conversion check.

Both containers derive a request in two halves, each of one type:

- The binding closes its layer sources under every step of a request but a wrap
  and a conversion to a more qualified element, in the layers it stores and the
  layers they convert to. A request that the closure satisfies resolves from it.
- Any other request extends a state of the closure whose type the request
  spells: the request below some of its outer layers, of its own qualifiers
  below the outer layer or of none, or the pointer that a handle of it adopts or
  observes, of the qualifiers of the request below the pointer or of none. The
  extension depends only on that type and on the form of the state: whether it
  is borrowed, made or a copy, whether it is an instance, which of its levels
  live as long as the container, whether its outer layer was made of what it
  holds, and whether it owns what its pointer points to. A borrowed object that
  is not an instance extends as a copy that is not one, and a copy of a pointer
  that owns what it points to extends as the pointer made. The extension of each
  type and form is derived of the request type alone, and the resolution takes
  the one of the fewest copies and moves of the leaf.

## Limitations

A _limitation_ is a step or a reference that the semantics allow and the library
does not do. The semantics name each limitation and apply it as a filter over
the steps and references of a derivation.

A _cell_ is one combination of the dimensions of a check, such as one request of
one registration. Its _outcome_ is what the library does in it. A cell that a
limitation keeps from resolving is `limited`. A cell that resolves although a
limitation covers it is a `stale_limitation`. The [Model Probe](#model-probe)
fails on a stale limitation.

The library has these limitations.

`POINTER_OUT_OF_MADE_HOLDER` is kept on purpose. The library gives no raw
pointer out of a holder that the registration makes, such as the `T *` of a
`std::unique_ptr<T[]>` that a unique scope makes. To give it, the library would
release the holder that the registration declares, and the caller would get an
owning raw pointer. The caller requests the holder instead. A pointer that a
call through `invoke` or a kept class takes points into a holder that the
library keeps while the pointer is in use.

`POINTER_INTO_CONVERTED_COPY`: the library gives no pointer into a copy that a
binding converts of what it stores. An example is the `std::shared_ptr<I> *`
into the `std::optional<std::shared_ptr<I>>` that a binding of `I` converts of a
kept `std::optional<std::shared_ptr<T>>`. The library points to the handle that
it converts of the element instead. That pointer is not null when the stored
optional is empty: the resolution fails.

`QUALIFIED_HANDLE_CONVERSION`: the library converts no pointer or handle of
qualifiers of its own to one of a more qualified element. An example is the
`std::shared_ptr<const L>` of a kept `const std::shared_ptr<L>`. Both containers
extend a request only from a layer source of no qualifiers of its own, so the
library copies the handle to a `std::shared_ptr<L>` first and converts the copy.
The copies and the moves of the leaf are the same; the conversion makes one
object more.

## Model Probe

[`spec/probe.py`](../spec/probe.py) builds a program for every combination of
container, scope, stored form, leaf, interfaces declared, selection, operation
and request. It compares each outcome with the specification.

- Leaf: copyable, move-only, immovable, or `factory` (only its factory makes
  it).
- Interfaces declared: none (the library binds the interfaces it derives from
  the stored type), the abstract interface `leaf_interface`, or both the leaf
  and `leaf_interface`.
- Selection: plain, by an annotation, or by a key.

A survey program first checks that each registration binds exactly the
interfaces that the specification gives. Then the probe makes these requests of
each bound interface `I`:

- The shapes `I`, `std::shared_ptr<I>`, `std::unique_ptr<I>`,
  `std::optional<I>`, `std::shared_ptr<I[]>`, `std::unique_ptr<I[]>`,
  `std::shared_ptr<I[2]>`, `std::shared_ptr<const I>` and
  `std::unique_ptr<const I>`, each in every category. A handle of a `const I` is
  a conversion of a handle of `I`, as a pointer to `I` converts.
- For a stored form of two wrappers, such as
  `std::shared_ptr<std::optional<L>>`, also its own shape. For a
  `std::unique_ptr` outside, also the shape with `std::shared_ptr` outside,
  because the unique handle converts to it.
- For a copyable or move-only leaf stored as `L` or in one wrapper, also every
  shape of two wrappers, such as `std::optional<std::shared_ptr<I>>` and
  `std::optional<std::shared_ptr<const I>>`.

The probe records a request that is not valid C++ as `invalid`, with the reason.
An example is an abstract type held by value.

An operation is an entry point and a position. The entry points are `resolve`,
`invoke` and `construct`. The positions are:

- `direct`: the request itself.
- `consumer`: a dependency of an unregistered class that the container
  constructs.
- `shared` and `cyclical`: a dependency of a class that the `shared` or
  `shared_cyclical` scope keeps, together with what the class receives.

The probe compiles the probes of several registrations in one program, with
[`test/model/probe.h`](../test/model/probe.h): up to 64 probes of up to 16
registrations of one container and one scope, each registration in a part of its
own. The container and the probe machinery are then compiled once for all of
them. Each probe resolves its request twice and records the outcome. A request
that the library rejects at compile time is `rejected`, with the message of its
`static_assert`. The compiler names the program lines of each failed request.
The driver records those requests and compiles the others again. Probes share
template instantiations, and the compiler reports a shared error only once. So
the driver compiles the probes of a failed operation or request in a program for
each operation. The driver precompiles the header once and turns off the error
limit of clang.

A resolved request is `violated` if one of these checks fails:

- The result reaches the registered leaf.
- A scope that keeps its instance gives the same leaf by reference each time.
- A `unique` scope gives a different leaf each time.
- A kept result does not refer to a leaf that is gone.
- No leaf leaks.

The probe counts the copies and the moves of leaves in each resolution, until
the class or the callable that the request is for receives the result.
`semantics.fewest()` gives the fewest copies and moves that the resolution
needs:

- A derivation that copies the leaf needs one copy, made where the request holds
  it.
- A derivation that only moves the leaf needs one move. A value made for the
  request moves once, into all its new layers.
- Making, referring to and sharing the instance need none.
- The runtime container gives a value from its context. If the derivation moved
  the leaf, a request that takes the value by a move needs one more move.
- `construct<T>()` of a `T` that it does not resolve resolves the leaf by value
  and moves it into the `T`: one more move.
- `resolve<T &&>()` and `construct<T &&>()` return the value moved out of what
  the request refers to.

A resolution with more copies or more moves than the fewest is `excess`.

Programs of up to 32 registrations also check every request that each
registration publishes. A published request that selects the binding of another
interface is `unreachable`. A published request that no probe makes is
`unprobed`. If such a program fails, the driver checks its registrations in
halves, down to the one that fails.

The copies and the moves of a resolution come from three layers: the storage,
the conversion and passing the result on. A check counts each layer alone, for
every registration that the library accepts:

- `probe.py --layer storage` requests the stored form itself, directly and
  selected plainly: as `&` through `resolve` from a scope that keeps it, and as
  `&&` through `invoke` from a scope that makes it. Only the storage copies or
  moves the leaf here, and the specification needs none.
- [`spec/execution_check.py`](../spec/execution_check.py) counts the copies and
  the moves of the derivation of every conversion of the model.
- `probe.py --layer resolution` requests the leaf of a plain registration,
  stored as `L` with no interfaces declared. No conversion copies or moves it.
  It covers every category, operation, scope, selection and container, and every
  leaf except `factory`, which copies and moves like a copyable leaf.

The probe fails if one of these is true:

- A request does not compile, other than by a `static_assert`
  (`does_not_compile`), or its probe crashes (`crashed`).
- A resolved request is `violated` or `excess`.
- A cell has another outcome than the specification defines (`unexpected`).
- A limited cell resolves (`stale_limitation`).
- A registration binds other interfaces than the specification gives.

A _cover_ probes a covering array of the registrations that the library accepts.
The array holds every combination of values of two (small) or three (medium) of
scope, stored form, leaf, interfaces declared and selection. Each registration
of the array is probed in both containers. The array has 82 or 268 of the 2880
registrations; the large cover probes all of them. A registration that the
library does not accept resolves nothing, so it covers nothing.

## Conversion Check

[`spec/conversion.py`](../spec/conversion.py) compiles, for the binding of each
registration, what it derives for every request of up to three wrappers, and of
each array shape. The result comes from compilation only
([`test/model/conversion.h`](../test/model/conversion.h)). No container is made
and nothing resolves, so the check covers every registration of the runtime
container that the library accepts, several in one program. The outcome is the
derivation of the static container, which runs it as the code compiles. The
runtime container selects the conversion of the same request as it runs, of the
tables of the binding and of the request, and the check compares that selection
with the derivation ([Runtime Selection](#runtime-selection)).

| Outcome           | Meaning                                                          |
| ----------------- | ---------------------------------------------------------------- |
| `published`       | The binding derives the request for as long as the container.    |
| `placed`          | The binding derives the request only for as long as the request. |
| `unpublished`     | The binding derives the request not at all.                      |
| `selects_another` | The request selects the binding of another interface.            |

The check compares each outcome with the specification, with the limitations of
the library:

- A `published` or `placed` request must resolve in some position.
- A request that resolves for the caller of a direct `resolve` must be
  `published`. A placed value ends with the call, so it cannot go back to the
  caller.
- A storage that cannot be made is not compared.
- The alternative type of a probe never holds its other alternative, so the
  check compares the other alternative as a copyable type of its own.

The check also compares the specification with C++ itself. For each cell, the
program records how C++ builds the request from what the binding holds, without
the library. The source is the stored object, or an object that it dereferences
to, down to the leaf. An array is not dereferenced. The request owns the stored
object of a unique scope. It also owns what a `std::unique_ptr`, a
`std::optional` or a raw pointer that it owns holds. C++ builds a request in one
of these ways:

- It binds a reference or a pointer to the source without a temporary. An rvalue
  reference binds only to a source that the request owns. A pointer can also be
  what an array decays to.
- It moves a source that the request owns into a value, by an implicit
  conversion.
- It copies a source that the request does not own into a value, by an implicit
  conversion.
- It copies such a source into a new `std::optional`.

A request that C++ builds must resolve in some position in the specification
without the limitations of the library. A limitation of the library is not a
defect here. The exception is a request that the specification rejects on
purpose, for every way that C++ builds it
([`spec/generator/conversions.py`](../spec/generator/conversions.py),
`REJECTIONS`):

- A new layer does not hold a copy of an instance, or a copy of a value that
  shares nothing.
- A cyclical scope publishes its instance before it constructs it. A request can
  refer to the stored object and to what the stored object holds in place. It
  can copy only a stored handle.
- An rvalue reference resolves to a value moved out of what the request owns. An
  immovable leaf, or an optional of one, cannot be moved.

A request that the specification rejects on purpose and still resolves is a
defect too.

### Runtime Selection

For each cell and each lifetime, stable and transient, the check compares the
conversion that the runtime container selects with the derivation:

- Whether there is one. This decides `published`, `placed` and `unpublished` for
  both containers alike.
- The copies and the moves of the leaf, and the objects that the conversion
  keeps and makes. Of an answer of the closure of the binding, they are those of
  the route to its state and of how the request takes it. Of an extension, they
  are those of the route to the layer source and of the extension.
- Whether what satisfies the request may be absent.
- How the request takes what the conversion reaches: it refers to it, points to
  it, takes it, moves it or copies it.
- Whether the address that the conversion gives lives as long as the container.
  The derivation does not record it; it lives as long as the container when the
  object that the derivation reaches does, or what that object points to, for a
  request that points to it.
- How the request takes the value: as the conversion satisfies it, or made of a
  copy, of its qualifiers, or of a run of wraps, of their number.

The selection runs `detail::find_conversion`, which the runtime container runs
too, without a container: of the layout of the binding, the key of its interface
and the runtime request (`detail::runtime_request_v`). A cell where any of these
differs is `unexpected`. A request whose binding or request type the runtime
container does not model is `unmodeled`; the derivation of such a request finds
nothing either, or the cell differs. A cell of which the program of the runtime
container records nothing, as it does not compile, is `unexpected` too. The
report counts the cells of each kind: `alike`, `unmodeled`, `apart` and
`unselected`. No cell of the grammar is `unmodeled`. A request that selects the
binding of another interface is not compared. The grammar spells no `volatile`
request, and `const` is part of the category of a request, so the answers of a
request of an object of its own qualifiers are not compared.

The tables of a request type take the compiler the most, and are the same for
every binding of its interface. So the selection is a program of its own for
each interface type, apart from the derivation. Its parts are compiled apart and
linked: the tables of up to 40 request types, the layouts of up to 25 bindings,
and a part that selects the conversion of each cell.

Another outcome is `unexpected`. The check fails on an `unexpected` cell and on
a registration that does not compile. The [Model Probe](#model-probe) checks
what resolving a request does, and
[`spec/execution_check.py`](../spec/execution_check.py) the copies and the moves
of each derivation.

## Emptiness Check

[`spec/emptiness.py`](../spec/emptiness.py) registers a stored object that holds
no leaf, in each container, each scope that is given or makes its instance, and
each stored form with a layer that may be empty. The outermost layer is empty,
or the layer that it holds is: a null handle, an empty optional, or an
alternative type that holds the other alternative. The check resolves through
`invoke` each request that the specification marks as defined when a layer is
empty ([`test/model/emptiness.h`](../test/model/emptiness.h)).

| Outcome            | Meaning                                                   |
| ------------------ | --------------------------------------------------------- |
| `defined`          | The request gives what holds no leaf.                     |
| `violated`         | The request gives what holds a leaf.                      |
| `unexpected`       | The request throws.                                       |
| `crashed`          | The request ends the program, as a null dereference does. |
| `does_not_compile` | The request does not compile.                             |

A request that is not defined when a layer is empty may fail in any way, so the
check does not make it. The check fails on any outcome other than `defined`. A
program that does not compile or that crashes is made again with half of its
registrations, and a registration with half of its requests, down to one
request.

## Constructor Detection Probe

[`spec/detection.py`](../spec/detection.py) probes constructor detection. A
parameter is a request of a copyable leaf, a move-only leaf or the abstract
interface, in each shape of at most one wrapper and each array shape. It is
plain, annotated or keyed, in every category. Each parameter is used in a class
of each kind: a constructor, an explicit constructor, an aggregate member, and
the two parameters of a constructor
([`test/model/detection.h`](../test/model/detection.h)).

For each class, the probe records what shape detection and signature detection
report, against the constructor that the generator wrote: `detected` or
`misdetected`. Then it makes the class in both containers, over a few
registrations of the leaf:

| Constructor                            | Made through                                                                       |
| -------------------------------------- | ---------------------------------------------------------------------------------- |
| The one that an explicit factory names | `construct`, a `shared` registration                                               |
| The one that shape detection finds     | `resolve`, `construct`, `invoke` of an unregistered class, a `shared` registration |
| The one that signature detection finds | `construct`, a `shared` registration                                               |

A class made with a detected constructor must have the outcome of the explicit
constructor. Through a registration, the probe compares it with the explicit
registration. Through `resolve` or `construct`, it compares it with explicit
`construct`. A class that `invoke` makes has no explicit counterpart: it can
refer to what the request makes, and a class returned to the caller cannot. The
probe checks it only for defects. Another outcome is a disagreement, which fails
the probe.

`LIMITATIONS` in [`spec/generator/detection.py`](../spec/generator/detection.py)
lists the known limitations of detection, with their reasons. Each lists which
detections report a class wrongly, and which detected constructions make it
differently from its explicit constructor:

- A misdetection that a limitation describes is `limited`.
- A construction that a limitation describes is not compared with the explicit
  constructor.
- A correct detection that a limitation describes as wrong is a
  `stale_limitation`, which is a defect.

A cover makes classes over a covering array of leaf, selection, kind and
registration of the leaf, with the same sizes as the Model Probe. The probe
records what detection reports for every class.

## Generator

The checks share [`spec/generator/`](../spec/generator):

- [`dimensions.py`](../spec/generator/dimensions.py): an enum for each dimension
  that only the checks have, next to the vocabulary. The value of a member is
  its name in a cell. `Outcome.is_defect` marks the outcomes that are defects.
- [`model.py`](../spec/generator/model.py): the interfaces that the library
  names, the requests of the grammar (a shape and a category of an interface),
  the rules for requests that are not valid C++, and a probe of a request.
- [`registrations.py`](../spec/generator/registrations.py),
  [`conversions.py`](../spec/generator/conversions.py) and
  [`detection.py`](../spec/generator/detection.py): the units of each check and
  their cells, without any C++, and their comparison with the specification.
- [`cover.py`](../spec/generator/cover.py): the covering arrays.
- `templates/`: the Jinja templates that spell the units in C++.
  [`macros.j2`](../spec/generator/templates/macros.j2) spells the dimensions,
  [`registration.j2`](../spec/generator/templates/registration.j2) a
  registration.
- [`driver.py`](../spec/generator/driver.py): compiles and runs the programs,
  and finds the probe of each compile error. A template marks the lines of each
  probe with `// probe N`. Each compiler can use at most 1 GiB; a program that
  does not fit is split.
- [`survey.py`](../spec/generator/survey.py) and
  [`report.py`](../spec/generator/report.py): the survey of the interfaces that
  a registration binds, and the baseline with its defects and disagreements.

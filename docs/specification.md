# Specification

[`spec/`](../spec) holds an executable specification of the library and the
checks that compare the library with it. The specification does not ask the
library anything:

- [`vocabulary.py`](../spec/vocabulary.py) names the concepts of the library,
  one enum for each concept.
- [`semantics.py`](../spec/semantics.py) takes a registration and a request. It
  decides whether the library accepts the registration, which interfaces the
  registration binds, whether the request resolves, and how the request fails.

The C++ headers that the checks include are in [`test/model/`](../test/model).
[Development](development.md#model-checks) tells how to run the checks.

## Semantics

Terms:

- A _registration_ stores a type in a scope and binds interfaces.
- The _leaf_ `L` is the object type at the center of the stored type.
- The _stored form_ is the type around the leaf: `L`, `L &`, `L *`, `L[2]`,
  `std::unique_ptr<L>`, `std::unique_ptr<L[]>`, `std::shared_ptr<L>`,
  `std::shared_ptr<L[]>`, `std::shared_ptr<L[2]>`, `std::optional<L>`,
  `std::variant<L, other>`, or two of `std::shared_ptr`, `std::unique_ptr` and
  `std::optional`.
- A _request_ asks for a _shape_ of a bound interface `I` in a _category_. A
  shape is `I`, an array handle of `I`, or up to three of `std::shared_ptr`,
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
   `copy`, `convert` (a `std::unique_ptr` to a `std::shared_ptr`), `upcast` (to
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
semantics also tell whether the cheapest derivation of a request is defined when
a layer is empty: it is if it reaches the request without going through an
absent element.

### Layer Sources

A binding knows the type that it stores. A request knows only the interface that
it asks for. So the steps of a derivation divide between them:

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

## Limitations

A _limitation_ is a step or a reference that the semantics allow and the library
does not do. The semantics name each limitation and apply it as a filter over
the steps and references of a derivation.

A _cell_ is one combination of the dimensions of a check, such as one request of
one registration. Its _outcome_ is what the library does in it. A cell that a
limitation keeps from resolving is `limited`. A cell that resolves although a
limitation says it does not is a `stale_limitation`. The
[Model Probe](#model-probe) fails on a stale limitation.

The library has two limitations.

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

## Model Probe

[`spec/probe.py`](../spec/probe.py) asks the library about every combination of
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
  `std::optional<I>`, `std::shared_ptr<I[]>`, `std::unique_ptr<I[]>` and
  `std::shared_ptr<I[2]>`, each in every category.
- For a stored form of two wrappers, such as
  `std::shared_ptr<std::optional<L>>`, also its own shape. For a
  `std::unique_ptr` outside, also the shape with `std::shared_ptr` outside,
  because the unique handle converts to it.
- For a copyable or move-only leaf stored as `L` or in one wrapper, also every
  shape of two wrappers, such as `std::optional<std::shared_ptr<I>>`.

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
registrations, each registration in a part of its own. The container and the
probe machinery are then compiled once for all of them. Each probe resolves its
request twice and records the outcome. A request that the library rejects at
compile time is `rejected`, with the message of its `static_assert`. The
compiler names the program lines of each failed request. The driver records
those requests and compiles the others again. Probes share template
instantiations, and the compiler reports a shared error only once. So the driver
compiles the probes of a failed operation or request in a program for each
operation. The driver precompiles the header once and turns off the error limit
of clang.

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
- The [Conversion Check](#conversion-check) counts the conversion of every
  published request.
- `probe.py --layer resolution` requests the leaf of a plain registration,
  stored as `L` with no interfaces declared. No conversion copies or moves it.
  It covers every category, operation, scope, selection and container, and every
  leaf except `factory`, which copies and moves like a copyable leaf.

The probe fails if one of these is true:

- A request does not compile, other than by a `static_assert`
  (`does_not_compile`), or its probe crashes (`crashed`).
- A resolved request is `violated` or `excess`.
- A published request is `unreachable` or `unprobed`.
- A cell has another outcome than the specification expects (`unexpected`).
- A limited cell resolves (`stale_limitation`).
- A registration binds other interfaces than the specification gives.

A _cover_ probes a covering array of the registrations that the library accepts.
The array holds every combination of values of two (small) or three (medium) of
scope, stored form, leaf, interfaces declared and selection. Each registration
of the array is probed in both containers. The array has 82 or 268 of the 2880
registrations; the large cover probes all of them. A registration that the
library does not accept resolves nothing, so it covers nothing.

## Conversion Check

[`spec/conversion.py`](../spec/conversion.py) asks the conversion layer of each
registration what it publishes for every request of up to three wrappers, and of
each array shape. The answer comes from compilation only
([`test/model/conversion.h`](../test/model/conversion.h)). No container is made
and nothing resolves, so the check covers every registration of the runtime
container that the library accepts, several in one program. The request lists
are the same in both containers, and selection does not change them.

| Outcome           | Meaning                                                                                                                                         |
| ----------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| `published`       | Both containers select a resolution, and its conversion is available.                                                                           |
| `unconvertible`   | Both select a resolution, and its conversion is not available.                                                                                  |
| `composed`        | The binding composes a wrapper of a request it resolves, or keeps it with the instance.                                                         |
| `derived`         | The request derives the request from what the binding publishes: refers to it, takes its address, copies or moves it, or converts a sole owner. |
| `adopted`         | The binding takes the value out of a pointer that it hands over.                                                                                |
| `placed`          | The binding resolves the request into the context of the call: a value placed there, or a reference into a pointer it adopts.                   |
| `unpublished`     | None of the above.                                                                                                                              |
| `selects_another` | The request selects the binding of another interface.                                                                                           |
| `diverged`        | The containers select or derive the request differently.                                                                                        |

The static container selects by its request lists, the runtime container by type
descriptors. For a request that a binding does not publish, the check follows
the chain of requests it derives from, in both containers. Each container
derives from what its own binding tells about itself.

The check compares each outcome with the specification, with the limitations of
the library:

- A `published`, `composed`, `derived`, `adopted` or `placed` request must
  resolve as the dependency of a direct `invoke`.
- A request that resolves for the caller of a direct `resolve` must be
  `published`, `composed` or `adopted`. A placed value ends with the call, so it
  cannot go back to the caller.
- A storage that cannot be made is not compared.
- The alternative type of a probe never holds its other alternative, so the
  check compares the other alternative as a copyable type of its own.
- What a binding reaches through `element_pointer` or `optional_element` must be
  `published` in one of the categories that take it. A request that derives it
  goes through an element that may be absent, and fails where the layer is
  empty.

Another outcome is `unexpected`. The check fails on an `unexpected` or
`diverged` cell, on a registration that does not compile, and on a published
request that is `unprobed` or `unreachable`. The [Model Probe](#model-probe)
checks what resolving a request does.

The check also applies the conversion of each `published` request alone to a new
object of its source. It makes the request from the result as a binding does,
and counts the copies and the moves of the leaf. `semantics.converted()` gives
the fewest copies and moves of the derivations that reach a match for the
request, and of the match. It does not include giving the result to the request.
More copies or more moves are `excess`, which fails the check.

## Emptiness Check

[`spec/emptiness.py`](../spec/emptiness.py) registers a stored object that holds
no leaf, in each container, each scope that is given or makes its instance, and
each stored form with a layer that may be empty. The outermost layer is empty,
or the layer that it holds is: a null handle, an empty optional, or an
alternative type that holds the other alternative. The check resolves through
`invoke` each request that the specification says is defined when a layer is
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
lists the known limitations of detection, with their reasons. Each says which
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

- `dimensions.py`: an enum for each dimension that only the checks have, next to
  the vocabulary. The value of a member is its name in a cell.
  `Outcome.is_defect` tells which outcomes are defects.
- `model.py`: the interfaces that the library names, the requests of the grammar
  (a shape and a category of an interface), the rules for requests that are not
  valid C++, and a probe of a request.
- `registrations.py`, `conversions.py` and `detection.py`: the units of each
  check and their cells, without any C++, and their comparison with the
  specification.
- `cover.py`: the covering arrays.
- `templates/`: the Jinja templates that spell the units in C++. `macros.j2`
  spells the dimensions, `registration.j2` a registration.
- `driver.py`: compiles and runs the programs, and finds the probe of each
  compile error. A template marks the lines of each probe with `// probe N`.
  Each compiler can use at most 1 GiB; a program that does not fit is split.
- `survey.py` and `report.py`: the survey of the interfaces that a registration
  binds, and the baseline with its defects and disagreements.

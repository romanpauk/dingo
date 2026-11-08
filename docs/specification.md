# Specification

[`spec/`](../spec) says what the library should do, without asking it, and holds
the probes that compare the library with it:
[`vocabulary.py`](../spec/vocabulary.py) names the concepts of the library, an
enum each, and [`semantics.py`](../spec/semantics.py) says, for a registration
and a request of it, whether the library accepts the registration, which
interfaces it binds, whether the request resolves, and how it fails when it does
not. The C++ headers the probes include are in [`test/model/`](../test/model).
How to run the probes is in [Development](development.md#model-checks).

## Semantics

A type is a stack of layers around an element, `std::shared_ptr<L>` the layer
`shared_ptr` around `L`, and each layer, leaf and scope is declared by its
properties: what it owns, whether a copy of it shares what it holds, whether it
can be copied, moved or made of a value. A request resolves when what it asks
for can be derived from what the scope stores, by steps such as taking the
element of a handle, copying, converting a handle, or moving a value into a new
layer, and when what it refers to lives long enough: what is returned to the
caller only refers to what the container or the caller keeps. A request that
does not resolve the static container rejects as it compiles, except a request
for the alternative an alternative type does not hold; the runtime container
throws `type_not_convertible`, except for what the types of a request rule out.

## Limitations

What the library does not do, although the semantics allow it, is a named
limitation: a filter over the steps and references of the semantics, kept apart
from them. A cell a limitation keeps from resolving is limited, and one that
resolves although a limitation says it does not is a stale limitation, which the
[Model Probe](#model-probe) fails on.

The library has two:

- `VALUE_OF_MADE_POINTEE`: a value of what a pointer a unique scope makes points
  to, such as a `T` or a `std::optional<T>` of a unique `T *`: the library takes
  the pointer over into a `std::unique_ptr<T>` or a `std::shared_ptr<T>`, or for
  a reference to `T` that a callable `invoke` calls or a kept class takes, but
  copies and moves nothing out of it.
- `POINTER_OUT_OF_MADE_HOLDER`, which it keeps on purpose: a raw pointer handed
  to the caller out of a holder the registration makes, such as the `T *` of a
  `std::unique_ptr<T[]>` a unique scope makes. Handing it over would release the
  holder the registration declares and give the caller an owning raw pointer
  instead. The caller requests the holder; a pointer a callable `invoke` calls
  or a kept class takes points into a holder kept for as long as it is used.

## Model Probe

[`spec/probe.py`](../spec/probe.py) asks the library what every combination of
container, scope, stored form, leaf, interfaces declared, selection, operation
and request does, and compares it with what the specification expects.

A registration declares no interfaces, so that the library binds those it
derives from the stored type; its abstract interface `leaf_interface`; or both
its leaf and that interface. Its requests select it plainly, by an annotation,
or by a key.

The specification says which interfaces each registration binds, and a survey
program first checks that the library binds exactly those types. The requests
are every shape of every bound interface `I`: `I`, `std::shared_ptr<I>`,
`std::unique_ptr<I>`, `std::optional<I>`, `std::shared_ptr<I[]>` and
`std::unique_ptr<I[]>`, each by value, `const`, as `&`, `const &`, `&&`, `*` and
`const *`. A registration of a stored form of two layers, such as
`std::shared_ptr<std::optional<L>>`, requests its own shape too, and a unique
handle of one the shape of the shared handle it converts to. A request that is
not valid C++, such as an abstract type held by value, is recorded as invalid
with the reason.

Each request is made through every entry point, `resolve`, `invoke` and
`construct`, in four positions: itself; as the dependency of the constructor of
an unregistered class, which the container constructs; and as the dependency of
a class registered in the `shared` or `shared_cyclical` scope, which keeps it
and what it receives.

Each registration is compiled into a program per position with
[`test/model/probe.h`](../test/model/probe.h), which makes each request,
resolving it twice, and records the outcome. A request the library rejects at
compile time is recorded with the message of its `static_assert`: the compiler
names the requests that fail by their lines in the program, and the others are
compiled again without them. Probes share instantiations, which the compiler
reports once, so the operations of a failed request are then compiled in a
program each. The header is precompiled once, and clang's error limit is lifted.

A resolved request is also checked: its result has to reach the registered leaf,
a scope that keeps its instance has to return the same one by reference, a
unique scope a new one, a kept class may not refer to a leaf that is gone, and
nothing may leak. Each program also checks every request its registration
publishes: one that selects another binding is unreachable, and one that no
probe makes is unprobed.

A cell a limitation keeps from resolving is recorded as limited, with the
limitations. The probe fails when a request does not compile without a
`static_assert`, crashes or fails these checks, when a published request is
unreachable or unprobed, when a cell is not what the specification expects
(unexpected), when a limited cell resolves (a stale limitation), or when a
registration binds other interfaces than the specification says.

A cover probes a covering array of the registrations the specification says the
library accepts: every combination of values of two (small) or three (medium) of
scope, stored form, leaf, interfaces declared and selection that such a
registration holds is probed in both containers, with 62 or 200 of the 2160
registrations (large). A registration the library does not accept resolves
nothing, so it covers nothing.

## Conversion Check

[`spec/conversion.py`](../spec/conversion.py) asks the conversion layer of every
registration the library accepts what it publishes for every request of the
grammar, as the code compiles: whether the request lists of the static container
select a resolution of the request, whether the descriptors the runtime
container dispatches on select the same one, and whether its conversion is
available ([`test/model/conversion.h`](../test/model/conversion.h)). No
container is made and nothing resolves, so every registration is checked, many
in a program, in seconds.

A published request has to be one the specification, with the limitations of the
library, resolves in some position, and a request it resolves for the caller has
to be published: what the library resolves into the context of a call, a value
placed there or a pointer it adopts, it does not publish. The check fails when
the two containers select differently, a registration does not compile, a
published request is unprobed or unreachable, or a cell is not what the
specification expects. Of a storage that cannot be made, and of the other
alternative of an alternative type, which a probe's never holds, it compares
nothing. What resolving a request does is the [Model Probe](#model-probe)'s.

## Constructor Detection Probe

[`spec/detection.py`](../spec/detection.py) probes constructor detection. A
parameter is every shape of every supported leaf (copyable, move-only, and an
abstract interface), plain, annotated and keyed, in every category. Each is the
parameter of a class of each kind: a constructor, an explicit constructor, an
aggregate member, and the two parameters of a constructor
([`test/model/detection.h`](../test/model/detection.h)).

For each class it records what shape and signature detection say, against the
constructor the generator wrote, and makes the class through `resolve`,
`construct` and `invoke` of an unregistered class and through a `shared`
registration of it, in both containers over a few registrations of the leaf. It
makes the class with the constructor an explicit factory names, the one shape
detection finds and the one signature detection finds. A detected constructor
has to make what the explicit one makes: through a registration as a
registration does, and through `resolve` or `construct` as `construct` does. A
class `invoke` makes has no explicit counterpart, as it may refer to what the
request makes, which a class returned to the caller may not; it is checked only
for defects.

The known limitations of detection are listed, with their reasons, in
`LIMITATIONS` of
[`spec/generator/detection.py`](../spec/generator/detection.py): each says which
detections report a class wrongly and which detected constructions make it
differently from its explicit constructor. A misdetection one of them describes
is recorded as `limited`, and a construction one of them describes is not
compared with the explicit constructor; a limitation whose misdetection no
longer occurs is a defect, `stale_limitation`.

The probe covers leaf, selection, kind and provider on the same cover sizes.

## Generator

The probe and the detection probe share [`spec/generator/`](../spec/generator):

- `dimensions.py`: an enum per dimension only the probes have, beside the
  vocabulary of the specification. The value of a member is its name in a cell.
- `model.py`: an interface the library names, a request of the grammar as a
  shape and a category of an interface, with the rules for requests that are not
  valid C++, and a probe of a request.
- `registrations.py` and `detection.py`: the units of each family and their
  cells, without any C++.
- `templates/`: the Jinja templates that spell the units in C++. `macros.j2`
  holds the spellings of the dimensions, `registration.j2` those of a
  registration of the probe.
- `driver.py`: compiles and runs the programs, and tells compile errors apart by
  probe. A template marks the lines of each probe with `// probe N`.
- `survey.py` and `report.py`: the survey that checks the interfaces a
  registration binds, and the baseline, its defects and its disagreements.

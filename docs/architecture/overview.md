# Overview

Dingo has four concerns:

- registration deduction
- lookup shape normalization
- storage and factory ownership
- conversion and wrapper adaptation

## Header Layers

The headers under `include/dingo/` form a hierarchy. A header can include
headers of lower layers and headers of its own directory. Directories in the
same layer do not include each other. The `layers-check` target enforces this
rule. The layer list is in [tools/layers.py](../../tools/layers.py).

From the bottom:

00. `base/`: configuration, `none`, `selected`, exceptions, `type_list`, type
    descriptors
01. `memory/`, `rtti/`, `type/`: allocators and object stores, rtti providers,
    type traits, keys, annotations
02. `conversion/`, `lookup/`: the conversion paths, the routes from a source to
    a target and their application, lookup indexes
03. `request/`: what a request is, how a request that a binding does not publish
    is derived or composed, collection requests
04. `factory/`: constructors, callables, constructor detection, factory traits
05. `context/`: resolution context base and recursion guard
06. `storage/`: storage tags, scope policies, storage exposure traits
07. `registration/`: registration options, requirements
08. `binding/`: the binding model, binding selection, the cell of a binding, and
    how a binding answers a request
09. `resolution/`: the resolution algorithm of both containers, and the requests
    both containers answer
10. `runtime/`, `static/`: the runtime and the static container, with their
    registries, bindings, contexts and traits, and the static graph
11. `dingo/*.h`: `runtime_container` (also named `container`),
    `static_container`

## Main Owners

### Public Containers

The public entry points are:

- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h):
  the runtime container, `runtime_container<>`, also named `container<>`
- [include/dingo/static_container.h](../../include/dingo/static_container.h):
  the static container, `static_container<bindings<...>>`
- [include/dingo/container.h](../../include/dingo/container.h): both

`container<>` holds runtime registrations. `static_container<bindings<...>>`
holds compile-time bindings. A container uses one registration mode.

The runtime registration path owns:

- the record of each registration, in one allocation: the header its bindings
  share (the request cache, the container it is made in and the registration
  container of its local bindings), its binding cell, and one binding for each
  exposed interface
- the edge of each binding (`runtime_binding_edge`): its constant node and the
  functions of its type
- the lookup index (`lookup_indexes`), which does lookup, collection
  enumeration, counting, and duplicate checks
- the parent lookup for nested containers
- the public registration and resolution entry points

The root container owns one runtime domain. This domain also serves the
registration containers that runtime registrations create for their local
bindings. A registration container has the type of the container the
registration is made in, and borrows the runtime data of its root. It has its
own lookup state, which it allocates when it registers its first local binding.
The root keeps these states in a dense scope table. Public parent and child
containers are independent owners.

The compile-time binding path owns:

- compile-time binding normalization
- static dependency graph validation
- compile-time binding selection and static resolution state

The container layer coordinates registration and resolution and does not
construct objects. It stores or selects bindings and asks factories or static
resolution helpers for values, references, or pointers.

### Type Registration

[include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
reduces a `register_type<...>()` declaration to a complete policy set:

- `scope_type`
- `storage_type`
- `factory_type`
- `interface_type`
- `conversions_type`

This deduction is the boundary between the public registration syntax and the
internal binding code.

Compile-time sources use `bindings<...>`. The static registry in
[include/dingo/static/registry.h](../../include/dingo/static/registry.h)
normalizes them.

### Factory And Storage

Both containers keep the state of a registration in a binding cell, defined in
[include/dingo/binding/activation.h](../../include/dingo/binding/activation.h).
The cell holds the storage object and its retention: what it keeps of the
instance for its interfaces. A static container holds one cell per registration
inline. A runtime registration holds its cell in its record
(`runtime_binding_record` in
[include/dingo/runtime/binding.h](../../include/dingo/runtime/binding.h)), after
the header that points to the registration container of its local bindings, when
it has one.

The storage classes under [include/dingo/storage/](../../include/dingo/storage)
own the lifetime policy:

- `unique`: constructs a new instance for each resolution
- `shared`: caches one instance
- `external`: refers to an existing object
- `shared_cyclical`: shared storage that supports cycles

### Resolution And Conversion Path

[include/dingo/binding/activation.h](../../include/dingo/binding/activation.h)
owns the resolution flow of both containers. A binding activation:

- asks the storage to `resolve(...)` its source
- applies the recursion guard that the `scope_traits` of the scope declare
- places the source in the resolution context when the request needs it
- keeps converted objects in the retention of a scope that keeps its instance
- rolls back what a failed runtime transaction left in the cell

The containers differ only in how they select the resolution of a request. A
static container selects it at compile time. A runtime binding matches the
requested type descriptor at run time.

[include/dingo/conversion/paths.h](../../include/dingo/conversion/paths.h)
defines the last conversion step, from the stored source to the requested
target. A conversion is a path of operations. Dingo takes the first available
path.

A request passes through four folders that both containers share:

- [include/dingo/resolution/](../../include/dingo/resolution): how a container
  runs a request. `entry_points.h` holds the requests a container answers, and
  `algorithm.h` the algorithm both containers run.
- [include/dingo/binding/](../../include/dingo/binding): what a binding is and
  how it answers a request. `model.h` holds what a registration declares,
  `selection.h` how a request selects a binding, `activation.h` the cell of a
  binding, and `resolution.h` how a binding resolves a request.
- [include/dingo/request/](../../include/dingo/request): what a request is.
  `decomposition.h` takes a request apart, and `derivation.h` and
  `composition.h` say how a request that a binding does not publish is derived
  or composed.
- [include/dingo/conversion/](../../include/dingo/conversion): the conversion of
  the stored source to the requested target. `paths.h` holds the conversion
  paths, `available.h` the routes a stored type publishes, and `apply.h` their
  application.

The code of each container is in:

- [include/dingo/runtime/](../../include/dingo/runtime)
- [include/dingo/static/](../../include/dingo/static)

## Resolution Sketch

Dingo runs this sequence:

1. `register_type<...>()` deduces the missing registration policies.
2. The runtime path creates one binding for each exposed interface. The static
   path normalizes `bindings<...>` into a validated static source.
3. `resolve<T>()` converts `T` into `request_type<T>` and a lookup key.
4. The container selects the matching runtime or static binding.
5. A runtime container passes an `instance_request` to the binding. The request
   holds the lookup type and the requested type descriptor. The binding matches
   the descriptor against the requests it publishes. A static container selects
   the resolution at compile time.
6. The binding activation gets or builds the stored instance through its
   storage.
7. The selected resolution operation converts the source into the requested `T`.

Registration child bindings and static local bindings take precedence over the
container they extend, for singular and collection requests.

These steps are parts of one pipeline, not independent extension systems.

## Reading The Code

To read the core path in source order, start here:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/runtime_container.h](../../include/dingo/runtime_container.h)
- [include/dingo/static_container.h](../../include/dingo/static_container.h)
- [include/dingo/registration/type_registration.h](../../include/dingo/registration/type_registration.h)
- [include/dingo/runtime/container.h](../../include/dingo/runtime/container.h)
- [include/dingo/static/registry.h](../../include/dingo/static/registry.h)
- [include/dingo/static/graph.h](../../include/dingo/static/graph.h)
- [include/dingo/runtime/binding.h](../../include/dingo/runtime/binding.h)
- [include/dingo/conversion/apply.h](../../include/dingo/conversion/apply.h)

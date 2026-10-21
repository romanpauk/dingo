# Registration And Resolution

This page follows the registration and resolution path through Dingo. Runtime
registration and compile-time registration reduce to the same binding type, and
the static and the runtime backend serve a request by the same engine and
selection rules.

## Runtime Registration

The public API starts at `container::register_type<...>()` in
[container.h](../../include/dingo/container.h).

`core::registration<Parts...>::binding_for<Args...>` in
[registration.h](../../include/dingo/registration.h) reduces the policies and
the arguments of the registration to a `core::binding_type`:

- scope, which a registration must name, and whose tag must have `scope_traits`
- storage, which a registration must name
- interfaces, none unless declared; declaring only the stored type is the same
  as declaring none
- factory: a `factory<...>` part, the constructor of the types of a
  `dependencies<...>` part, a callable passed as an argument, or the detected
  constructor; a registration declares its dependencies by a `dependencies<...>`
  part or by its factory, not both
- key: a `key_type<...>` part, or the runtime key of a `key_value` argument, but
  not both
- local static bindings of a `bindings<...>` part

A missing scope or storage, a scope without `scope_traits`, and a variant that
names an alternative twice are rejected when the registration is compiled, each
with its diagnostic.

The runtime backend allocates the binding by the allocator of the container.
Before it stores the binding, it rejects a key value outside the bound of the
dense array of its domain with `type_index_out_of_range_exception`, and a key
that a binding of a common interface already holds, unless the lookup
declarations declare the domain many, with
`lookup_already_registered_exception`. A rejected registration leaves the
container as it was. `register_type` returns the proxy that registers bindings
local to the registration.

## Compile-Time Registration

`container<bindings<...>>` accepts a compile-time binding source:

```c++
using app_bindings = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Config>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<Service>>
>;

dingo::container<app_bindings> container;
```

Each `bind<...>` entry is reduced by the same `core::registration` as a runtime
registration, and the bindings form a `core::static_backend` of
[core/backend/static_backend.h](../../include/dingo/core/backend/static_backend.h)
with the lookup declarations of the traits. When the container type is formed,
the static backend rejects a second binding of a key of an interface, unless the
lookup declarations declare the domain many, and an illegal cycle of the
dependencies the factories declare. A static container also rejects a declared
dependency that names no binding when that is decided when it is compiled.

There are two registration modes:

- runtime registration through `register_type<...>()`
- compile-time registration through `bindings<...>`

`container<bindings<...>>` can host compile-time bindings and runtime
registrations at the same time.

## Stored Type Versus Exposed Interface

Registration may expose a type through one or more interfaces. The stored type
is the type the registration names: a binding with one interface stores the same
shape as one with several, and reaches each interface by the upcast forms of the
routes. A binding without declared interfaces is named by the leaf of its stored
shape, and through a variant by the leaf of each alternative.

A registration is rejected when it is compiled when its scope does not store the
form or the nested layers of its stored type, when its scope constructs the
object and the stored type has no default factory and the registration declares
none, when it stores an array and declares interfaces, or when no interface it
declares can be served from its stored type.

## Resolution

The public `resolve<T>()` and `resolve<T>(key)` are members of `core::selection`
in [core/backend/selection.h](../../include/dingo/core/backend/selection.h). A
key is a `key_type` object, a `key_value`, or a value of the key type. At that
boundary Dingo describes the request with
`core::request_traits<Request, Key>::descriptor` of
[core/engine/request.h](../../include/dingo/core/engine/request.h): its kind,
value, lvalue reference, or rvalue reference; its shape, whose leaf is the
interface it names, annotated when the request is; and its key.

The sequence is:

1. The selection asks the sources of the container in the order of the selection
   rules below.
2. A source counts the bindings the request names: the bindings that serve its
   interface with a key of the same domain and value.
3. The first step that names a binding answers the request.
4. The source that holds the binding selects the term that serves the request,
   the route from its stored shape to the requested shape, and walks it.

A static source decides all of it when the request is compiled. A runtime source
keeps the memo of the request type: the candidates of its domain in the order of
registration, each with the term that serves the request, valid until the next
registration. A request by a key value searches the memo by the index of its
domain.

### Selection

The selection rules are data of the spec, interpreted by `core::selection`.

- A singular request asks the local bindings of the registration whose object is
  being constructed, then the static and runtime bindings of the container,
  then, for a collection type no binding stores, the bindings of its element
  type, then the parent, and last auto-construction of an unregistered type. The
  first step that names a binding answers: one binding is the result, two are
  ambiguous, and none at any step is not found.
- A collection request asks the local bindings, then the bindings of the
  container, then the parent, and takes the first that names any. The bindings
  of a level are in the order of their sources, runtime before static, and of
  registration within a source. No binding is an empty collection.
- A construct request asks the local bindings, the container, and the parent as
  a singular request does, and otherwise makes the object by its factory.

A step whose sources all know what they name when the request is compiled is
decided then. A failure decided then, such as an ambiguity among static
bindings, is a compile error, since a runtime registration only adds candidates.
Any other step is decided when the request is resolved.

### Transactions

A request that starts at a container, `resolve`, `construct`,
`construct_collection`, or `invoke`, is a transaction of the container. When it
fails, the bindings registered into the container during it, the objects its
slots constructed, and the objects it kept for requests are undone in reverse
order, whether or not a request that made it catches the failure. A request that
returns keeps its effects, which the request that made it undoes if it fails.

The transaction log of the container records each effect in the frame of the
outermost request. The effects on other containers are their own: a parent keeps
an object a failed request of its child constructed by the parent's bindings,
and a child keeps a binding registered into it while a request of its parent
failed.

The transactions cover container-managed state only. Side effects that user
constructors, factories, callbacks, or destructors make outside the container
cannot be undone.

## Factory Path

The slot of a binding, in
[core/backend/slot.h](../../include/dingo/core/backend/slot.h), keeps:

- the stored object, as the slot kind of its scope keeps it
- the factory of the binding, empty unless it holds a callable object
- the objects the cached rules of its routes derive, such as a sharing handle of
  an interface converted from the stored handle, so a request can refer to them

A runtime binding is an entry of the runtime backend with the terms of its
binding type and a thunk per term. A request calls the thunk of its term, which
walks the term to its boundary over the object of the slot, and completes it.
Once the slot of a stable binding, shared or external, is settled, the memo
keeps the address the term borrows, so a later resolution makes no call.

The static path uses the same binding type and routes, but it walks the selected
term directly.

## Registration Child Bindings

`register_type<...>()` can carry local `bindings<...>` that declare the static
part of the local bindings of the registration. The proxy it returns registers
local runtime bindings, which are created by the first such registration and
follow the lookup declarations of the container.

The object of the registration resolves its dependencies through a view, in
[core/backend/local.h](../../include/dingo/core/backend/local.h), whose local
sources are those bindings, whose container sources are the host's own bindings,
and whose parent is the host's parent:

- local bindings shadow the host's for singular requests
- local bindings shadow the host's for collection requests: a collection holds
  the local bindings of its key when there are any, and the host's otherwise
- a nested registration sees the local bindings of the registration that
  encloses it

## Parent Containers And Auto Construction

If no binding of the container names a request, the container delegates to its
parent.

If no level names a binding, a request of a value or a const lvalue reference of
an unregistered type that `is_auto_constructible` declares, a complete aggregate
by default, and whose constructor is detected, constructs it. That rule is in
[core/projection/auto_constructible.h](../../include/dingo/core/projection/auto_constructible.h).

The detection lives in
[core/primitives/constructor_detection.h](../../include/dingo/core/primitives/constructor_detection.h)
and
[core/primitives/constructor.h](../../include/dingo/core/primitives/constructor.h).
Constructor detection probes the type with arguments that convert to any
parameter type, from the largest arity down to zero, so the constructor with the
most parameters wins, and it ignores the copy constructor. It reports the kind
of the constructor, concrete, generic, or invalid, and its arity. A constructor
that accepts any argument, such as a constructor template, is generic and needs
an explicit factory. A type that declares its constructor with
`DINGO_CONSTRUCTOR` is constructed by it.

The factory calls the detected constructor with arguments that resolve exactly
the type each parameter asks for from the container. A signature mode of the
detection recovers the parameter types of the detected constructor; the
containers do not use it, so the dependencies of a detected constructor are
known only when the object is constructed.

Dingo does not try to model the full C++ overload-resolution space, which is why
ambiguous, policy-sensitive, or readability-critical cases should move to an
explicit factory.

## Good Source Companions

These files are useful while reading this flow:

- [include/dingo/container.h](../../include/dingo/container.h)
- [include/dingo/registration.h](../../include/dingo/registration.h)
- [include/dingo/registration_proxy.h](../../include/dingo/registration_proxy.h)
- [include/dingo/core/backend/selection.h](../../include/dingo/core/backend/selection.h)
- [include/dingo/core/backend/runtime_backend.h](../../include/dingo/core/backend/runtime_backend.h)
- [include/dingo/core/backend/static_backend.h](../../include/dingo/core/backend/static_backend.h)
- [include/dingo/core/backend/memo.h](../../include/dingo/core/backend/memo.h)
- [include/dingo/core/backend/slot.h](../../include/dingo/core/backend/slot.h)
- [include/dingo/core/primitives/constructor_detection.h](../../include/dingo/core/primitives/constructor_detection.h)

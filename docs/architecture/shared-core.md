# Shared Core

Dingo has two containers and one core. This page says what the core is, how a
request goes from `resolve<T>()` to a binding and back in each container, and
what happens as the code compiles, at the first request of a type, and at every
request.

## What Both Containers Share

The containers differ in two things only: how they select a binding, and how
they run a conversion. Everything else is one implementation.

| Part                       | Code                                                                                                                                                                          | Static container                            | Runtime container                                                                                                          |
| -------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| Request decomposition      | [`request/decomposition.h`](../../include/dingo/request/decomposition.h), [`request/classification.h`](../../include/dingo/request/classification.h)                          | as the code compiles                        | as the code compiles                                                                                                       |
| Entry points               | [`resolution/entry_points.h`](../../include/dingo/resolution/entry_points.h) (`container_requests`)                                                                           | shared                                      | shared                                                                                                                     |
| Resolution algorithm       | [`resolution/algorithm.h`](../../include/dingo/resolution/algorithm.h) (`request_resolution`)                                                                                 | shared, branches on a selection type        | shared, branches on a selection value                                                                                      |
| Selection rules            | [`binding/selection.h`](../../include/dingo/binding/selection.h)                                                                                                              | over its bindings, as a type                | over its lookups, as a value                                                                                               |
| Binding model              | [`binding/model.h`](../../include/dingo/binding/model.h), [`registration/type_registration.h`](../../include/dingo/registration/type_registration.h)                          | shared                                      | shared                                                                                                                     |
| Activation of a cell       | [`binding/activation.h`](../../include/dingo/binding/activation.h), [`binding/materialization.h`](../../include/dingo/binding/materialization.h)                              | shared                                      | shared, in a transaction                                                                                                   |
| Semantics                  | [`semantics/model.h`](../../include/dingo/semantics/model.h)                                                                                                                  | shared                                      | shared                                                                                                                     |
| Layer sources              | `binding_semantics` in [`request/binding_semantics.h`](../../include/dingo/request/binding_semantics.h)                                                                       | as the code compiles                        | as the code compiles                                                                                                       |
| Derivation of a request    | `semantics::satisfy`, `semantics::answer_derivation`                                                                                                                          | as the code compiles (`request_derivation`) | at the first request of its type (`make_runtime_conversions`)                                                              |
| Conversion of a derivation | `compile_conversion` in [`request/conversion.h`](../../include/dingo/request/conversion.h)                                                                                    | as the code compiles                        | at the first request of its type                                                                                           |
| Steps                      | the operations of [`request/operation.h`](../../include/dingo/request/operation.h)                                                                                            | inline (`execute_conversion`)               | through tables built of the same operations ([`runtime/operation_table.h`](../../include/dingo/runtime/operation_table.h)) |
| Store of what is made      | `static_store` in [`static/binding.h`](../../include/dingo/static/binding.h), `runtime_store` in [`runtime/operation_table.h`](../../include/dingo/runtime/operation_table.h) | typed                                       | by the layout of the type (`object_layout`)                                                                                |

The semantics are constant expressions of no C++ type: what a binding publishes,
which steps reach a request, and how many copies and moves of the leaf each
takes. Both containers use them. The static container evaluates them as the code
compiles. The runtime container evaluates the layer sources of a binding as the
code compiles too, and searches a request from them at its first request, when
it knows the binding.

## A Request In The Static Container

```text
resolve<T>()                        container_requests        resolution/entry_points.h
  resolve_entry<Request, R>         static_container_impl     static/container.h
    request_resolution::resolve     the algorithm             resolution/algorithm.h
      select_request<Request>       a selection type          static/container.h
      resolve_selected              the selected binding      static/container.h
        static_binding::resolve     the derivation of the request   static/binding.h
          activation.use_source     the source of the cell    binding/activation.h
          conversion_start          the stored object         binding/resolution.h
          typed_conversion::execute<R>    the steps, inline         request/conversion.h
          typed_conversion::finished      the value of R            request/conversion.h
```

1. `resolve<T>()` makes `request_type<T>` and a lookup key, and calls the entry
   of the container.
2. The algorithm selects the bindings of the request. The selection is a type:
   one binding, none or several is known as the code compiles. Several do not
   compile. None falls back to the parent, or to the container the request
   entered through, which can construct the type.
3. The selected binding takes the derivation of the request for the lifetime it
   needs: as long as the container when the request is passed to the caller, as
   long as the request otherwise (`static_execution::derived`). The derivation
   and its conversion are constant data. A request no derivation satisfies does
   not compile.
4. The activation of the binding cell gives the source of its storage: the
   instance it keeps, or one it makes for the request.
5. The conversion applies its steps to the stored object, each a function the
   compiler inlines. What a step makes is kept by the store of the request: in
   the context, or in the retention of the cell when it lives as long as the
   container.
6. The last instruction gives the request its value: it refers to what the
   conversion reached, moves or copies it, or makes the value in place of the
   one it returns.

## A Request In The Runtime Container

```text
resolve<T>()                        container_requests        resolution/entry_points.h
  resolve_entry<Request, R>         runtime_container_impl    runtime/container.h
    select_request<Request>         the data of the request   runtime/container.h
      select_levels                 of no request type        runtime/container.h
    lookup_cache                    a cached address          runtime/container.h
    resolve_transacted              a transaction frame       runtime/container.h
      request_resolution::resolve_level                       resolution/algorithm.h
        resolve_selected            the selected binding      runtime/container.h
          runtime_binding_engine::dispatch                    runtime/binding.h
            conversion_cache::find        the derivation of the request   runtime/request.h
              make_runtime_conversions  at its first request  runtime/conversion.h
            run_materialization     guard and rollback        binding/materialization.h
              runtime_binding_edge::run                       runtime/binding.h
                conversion_start    the stored object         binding/resolution.h
                the calls of the conversion, through erased tables  runtime/operation_table.h
                finish_request      a resolved_address        runtime/request.h
        returned_value<T>           the value of T            runtime/request.h
```

1. `resolve<T>()` makes `request_type<T>` and a lookup key, and calls the entry
   of the container.
2. The container makes the data of the request: the lookups and the keys of its
   exact and of its normalized interface. One function of no request type
   selects by them, in the container and in the registration containers it falls
   back to. A reference or a pointer request first reads the request cache of
   the selected binding.
3. A transaction opens. The algorithm resolves the selection: one binding is
   resolved, several throw `type_ambiguous_exception`, and none falls back to
   the parent or to the container the request entered through.
4. The engine of the container is one function for every binding. It finds the
   derivation of the request type for the binding in the conversion cache of the
   request type, and makes it at the first request: it searches the request from
   the layer sources of the binding, which the code compiled, and maps the
   conversion of the derivation to the operations of the tables of the binding
   and of the request. The conversion is kept for the lifetime a request needs.
5. The materialization guards the binding against recursion, and resets its cell
   if the transaction fails. The edge of the binding, a few functions of its
   type, gives the source of its storage.
6. The calls of the conversion apply its steps to the stored object. What a step
   makes is kept by the store of the request, which knows only the layout of its
   type. The engine returns the address of what the conversion reached, and how
   the request takes it (`resolved_address`). An address that lives as long as
   the container is published in the request cache.
7. `returned_value<T>` gives the request its value, of the address: a reference
   or a pointer, a move or a copy, or a value made in place of the one it
   returns.

## When Each Part Runs

| When                           | Static container                                                                     | Runtime container                                                                                         |
| ------------------------------ | ------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------- |
| As the code compiles           | the selection, the layer sources, the derivation and the conversion of every request | the layer sources of every binding and their tables, the tables of every request type                     |
| At the first request of a type | nothing                                                                              | the selection, the search of the request, its derivation and its conversion, kept in the conversion cache |
| At every request               | the activation of the cell and the inlined steps                                     | the selection, the request cache, the transaction, the activation and the calls of the conversion         |

A search holds at most `semantics::max_states` states. One the code compiles
that reaches more does not compile. A runtime search that reaches more fails the
request.

## Related Pages

- [Registration And Resolution](registration-and-resolution.md): registration,
  selection, transactions and the runtime engine.
- [Conversion Model](conversion-model.md): the steps, the conversions and the
  tables.
- [Layered Semantics](layered-semantics.md): the design that makes resolution
  correct by construction.

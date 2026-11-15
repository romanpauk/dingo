# Layered Semantics

This page describes how the resolution code is built so that it is correct by
construction, and which parts of the code belong to which layer.

## The Problem

A cell of the model is a container, a scope, a stored form, a leaf, the
interfaces declared, a selection, an operation, an interface and a request. The
product has millions of cells. The model checks compile a cover of it, so a
defect in a cell outside the cover is not found. Tests of the product cannot
show that the library is correct.

## The Layers

The library is built of four layers. Each layer has a small domain of its own,
which a test covers completely. The composition of the layers is correct when
each layer is.

1. **Classification.** A C++ type is mapped to abstract facts: the layers around
   its element (a shared handle, a unique handle, an optional, an alternative
   type, an array, a pointer), the element (the leaf or its abstract interface),
   the qualification and the reference of the request, and what the leaf can do
   (be copied, be moved, be made by a factory). This layer holds every rule of
   C++ the library depends on. Its domain is a few wrapper kinds by a few leaf
   kinds by the request shapes.
2. **Semantics.** Of these facts alone, with no C++ type, the layer decides
   everything: what a storage gives, what a binding publishes, how a request is
   derived of what is published, how long each object lives, and how many copies
   and moves the leaf takes. Its output is a plan: the steps from the stored
   object to the request. The domain is finite. It is compared with
   [spec/semantics.py](../../spec/semantics.py) for every element of the domain.
3. **Steps.** Each kind of step is one function template with a contract: what
   it requires of its source, what it gives, how long the result lives and what
   it copies or moves of the leaf. Each step is tested alone, over the wrapper
   kinds and the leaf kinds.
4. **Execution.** A plan is compiled into one program, which both containers run
   through the steps: the runtime container through tables of erased operations,
   and the static container inline.

If the classification is right, the semantics equal the specification, each step
keeps its contract and the program runs the plan as it is, every cell is right.
The tests are then the sum of the layers, not their product. A cover of the
product checks only the composition.

## The Specification Is Layer 2

[spec/semantics.py](../../spec/semantics.py) is already a model of layer 2. A
`Thing` is a stack of layers around an element, with a lifetime for each layer,
an origin, and what it is of the instance. `transitions` gives the steps from a
thing, and `reachable` gives every thing a stored object reaches, with the
copies and the moves of the leaf. A binding takes the steps that need the type
it stores (`binding_steps`); a request takes the others (`request_steps`).
`satisfactions` tells which things satisfy a request.

The library computes the same: one implementation of the steps, the reachable
things and the satisfactions, as constant expressions over these values. Its
results are compared with the specification for every starting object and every
request.

## A Layer Is Its Properties

The rules name no wrapper. A layer is what the rules ask of it: what it owns,
whether a copy of it holds the same element, whether it can be copied, moved,
made of a value or be empty, whether an empty one is a value, whether it holds
only its element, whether it takes over a raw pointer, and whether a value of it
converts to one of a more qualified element. Whether an owned value of one layer
converts to another is a fact of the two, given for the layers in play. So is
whether a value of a layer that holds an array of a known bound converts to
another that holds the same array as one of unknown bound.

The layers of C++ itself are fixed: a pointer to an object, an array of unknown
bound, a bounded array, and the pointer an array decays to. Every other layer is
a type a request or a stored object is made of, whose `type_traits` give its
properties. A search takes the universe of its layers: those of C++ itself, and
those of the types it is of. A user's wrapper needs no change of the rules.

Not every combination of properties is a layer a type can be.
`semantics.possible` tells which are: a layer that owns nothing, or shares what
it owns, holds an element that lives apart from it; a layer that owns its
element alone and shares it with a copy cannot be copied; a layer that owns
nothing is not made of a value and takes over no pointer; a layer that takes
over a pointer holds its element apart from it, and so does a layer the leaf is
rebound to the interface in, and one that converts to one of a more qualified
element or of an array of unknown bound; a layer made of a pointer it does not
own owns nothing and may be empty; and an empty value is a value only of a layer
that may be empty. Of the combinations that give their element and are no raw
pointer and no array, 1,520 are possible. The classification rejects the type
traits of any other.

The steps of the specification and the code that takes them. Each is in
[include/dingo/request/execution.h](../../include/dingo/request/execution.h):

| Step                                                                      | Who takes it | Code                                                   |
| ------------------------------------------------------------------------- | ------------ | ------------------------------------------------------ |
| `element`, `element_pointer`, `element_or_empty`, `element_or_empty_copy` | binding      | `step<element>` and the others of the same name        |
| `upcast`, `convert`, `unknown_bound`                                      | binding      | `step<upcast>`, `step<convert>`, `step<unknown_bound>` |
| `release`                                                                 | binding      | `release_step`                                         |
| `copy`, `decay`, `hand_over`                                              | binding      | `step<copy>`, `step<decay>`, `step<hand_over>`         |
| `wrap`                                                                    | request      | `step<wrap, Target>`; a run of wraps is `made_of`      |
| `adopt`                                                                   | request      | `adopt_step`                                           |
| `observe`                                                                 | request      | `observe_step`                                         |
| `qualify`                                                                 | request      | `qualify_step`                                         |

A step belongs to the side that knows the type it is applied to (see phase 7). A
request takes the others through the tables of the binding.

## The Headers

| Layer          | Headers                                                                                                                                                                                                          |
| -------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Classification | `type/type_traits.h`, `type/shape.h`, `type/annotated.h`, `request/decomposition.h`, `request/classification.h`, `factory/constructor_detection.hpp`, `base/type_descriptor.h`                                   |
| Semantics      | `semantics/model.h`, `binding/materialization.h`, `storage/scope_traits.h`, `binding/selection.h`                                                                                                                |
| Steps          | the steps of `request/execution.h`, the storages of `storage/`, `factory/`                                                                                                                                       |
| Execution      | the programs of `request/execution.h`, the tables of `runtime/execution.h`, `runtime/request.h`, `binding/activation.h`, `runtime/binding.h`, `static_execution` of `static/binding.h`, `resolution/algorithm.h` |
| Plumbing       | `lookup/`, `memory/`, the registries and the transactions of `runtime/` and `static/`, `context/`                                                                                                                |

The plumbing does not decide what a request resolves to. It is tested as any
other code.

## The Phases

1. **Semantics as values.** Done.
   [include/dingo/semantics/model.h](../../include/dingo/semantics/model.h)
   ports the steps, the search and the satisfactions of the specification to
   constant expressions over layers, things and requests. It includes no other
   part of the library. The port does not search every thing a stored object
   reaches: up to 2859 of them, 468 on average. It searches the things on the
   way to the request, which wraps a layer around a thing only as one of the
   layers of the request, or as a layer that converts to one of them: 145 states
   at most and 13 on average.

2. **Exhaustive comparison.** Done. `semantics-check` compares the port with the
   specification in two domains. For each request it compares the fewest copies
   and moves of what satisfies each of the 7 categories, for both lifetimes,
   with the specification's search of every thing.

   - The standard layers: every stored object the specification makes, with and
     without the known limitations, and every request of every shape of every
     bound interface. 673,440 requests, 9,428,160 answers.

   - Every possible layer a type could define: each of the 1,520, beside the
     layers of C++ itself, stored and requested in every stack of it, of a
     pointer and of an array, of every scope and leaf. A layer that converts to
     one of an array of unknown bound is stored only around an array of a known
     bound: anywhere else it is the same as its twin that does not convert.
     5,948,544 requests, 83,279,616 answers.

   - With `semantics-check-pairs`, every pair of possible layers, with each
     conversion between them that `semantics.possible_conversion` allows: an
     owned value of a layer that owns its element alone converts to a layer that
     owns an element apart from it. Each pair is stored and requested in every
     stack that holds both of them, of a pointer and of an array, of every scope
     and leaf: 1,669,960 universes.

   All answers are equal. A rule of the port changed on purpose makes 216
   requests differ. The pairs found a defect the single layers did not: the
   search directed by the request left out a layer the binding keeps with a
   copy, whose element lives as long as the binding. It now keeps that wrap
   around anything that may become the request. Three or more layers a type
   defines in one stack are not compared.

3. **Classification.** Done.
   [include/dingo/request/classification.h](../../include/dingo/request/classification.h)
   maps a C++ type to what the semantics know of it, of its `type_traits` and of
   what C++ says of it:

   - The properties of the layer of a wrapper: what its semantics say it owns,
     and whether the same wrapper of an element that can be copied, or of one
     that can only be moved, can be copied, moved, or made of a raw pointer. The
     type traits of a layer no type can be fail to compile.

   - The identity of a layer: the same wrapper of a probe element. Two wrappers
     of different elements are one layer.

   - The stack of a type: its pointers, arrays and wrappers around the leaf, its
     abstract interface or another type. A handle that holds an array has the
     array as a layer of its own, bounded when its type traits give the
     `extent`.

   - The universe of the layers of stacks, with the layer a sole owner hands its
     element over to among them.

   - The layer of an alternative type, which cannot rebind its element, is the
     type and the alternative that holds the element. It can be copied, or
     moved, when every other alternative can.

   - What the leaf can do: be copied, moved, or constructed of nothing.

   - What a scope does, of its `scope_traits` (`scope_semantics_v` in
     [include/dingo/storage/scope_traits.h](../../include/dingo/storage/scope_traits.h)):
     it makes a new instance when it consumes what it resolves, it is given its
     instance when it can hold a reference, and it publishes its instance early
     when it allows cycles.

   - The category of a request: its outer reference or pointer, and whether that
     is const. A pointer is always the category, as no request shape begins with
     one. A registration that binds a pointer type itself is told apart by the
     binding a request selects, in the execution. An rvalue reference to const
     is not modeled.

   `semantics-check` classifies every type the model spells and compares each
   with what the specification says of it: every stored form and every request
   shape of every bound interface of every leaf, as the layers it is made of;
   every valid request of the grammar, as its category and its shape; and the
   leaves and the scopes, as their traits. That is 4,224 types, all equal. It
   also checks that the standard wrappers are the standard layers of the
   specification. A test shows that a wrapper a user defines is classified as
   the standard one of the same semantics.

4. **Plans.** Done. A state of the search keeps the state and the step it
   reached its thing from first. The answer to a request is the state that
   satisfies it at the fewest copies and moves, and how: a reference or a
   pointer refers to it, a pointer request takes it as a pointer value, an
   rvalue reference takes what is made for the request, or a value is moved or
   copied of it. `plan_of` gives the steps from the stored object to it.
   `semantics-check` replays the plan of every answer through the steps of the
   semantics, with its copies and moves.

5. **Steps.** Done for plans known as the code compiles.
   [include/dingo/request/execution.h](../../include/dingo/request/execution.h)
   has one step for each kind of step of the semantics, by the type traits of
   its layers and by C++ itself, and the program that a plan is compiled into,
   which applies the steps of a plan one after another. A run of wraps makes its
   outermost layer at once, in place, and the last run is made where the request
   needs it, so that the leaf is moved once. What a step makes for the request
   is kept by a store. A step that takes over what a pointer points to releases
   the handle it was taken out of, by the `release` of its type traits.
   `execution_check.py` asks the semantics the plan of every conversion of the
   model, applies each to the leaf types of the model, which count their copies
   and moves, and compares the counts with the plan, without a container: 60,694
   cells, all equal. It found three defects, each fixed: a handle a pointer was
   taken out of released it too; a wrapper emplaced of its own element type
   moved the leaf twice; and the pointer to the first element of an array was
   upcast to an array of the interface, which the specification and the
   semantics then both allowed the request to take over.

6. **Request rules.** Done. `resolve` in
   [include/dingo/semantics/model.h](../../include/dingo/semantics/model.h)
   applies the rules of a request on top of the conversion, as `fewest` of the
   specification does: how long what a request refers to has to live, by its
   position and entry point; `construct<T>()` resolving what it cannot
   construct; what a key or an annotation can hold by value; the move of a value
   the runtime container hands over from its context; and `construct<T>()`
   making a wrapper of the leaf the container resolves by value.
   `semantics-check` compares it with `fewest` for every stored object, request,
   category, position, entry point, selection and container: 105,759,360
   answers, all equal.

7. **Execution.** Done. The containers apply plans. The conversion paths, the
   derivations, the request lists of the scopes and the resolutions a binding
   published before are deleted. A static binding plans a request as the code
   compiles (`static_execution` in
   [include/dingo/static/binding.h](../../include/dingo/static/binding.h)) and
   runs the program of the plan as typed functions. The runtime container plans
   a request of a binding at its first use, and keeps the plan for the type of
   the request. Both are built as follows.

   **The model.** Three additions are specified in
   [spec/semantics.py](../../spec/semantics.py) first, ported to
   [include/dingo/semantics/model.h](../../include/dingo/semantics/model.h), and
   compared in every domain of `semantics-check`.

   - Qualifiers. A thing has the const and volatile qualifiers of each of its
     levels: of the outermost as it is reached, of the others as the type above
     declares them. An element held in place takes those of its holder. What a
     copy or a move makes anew has none. A const or a volatile object is not
     moved from, and a volatile one is copied only by a leaf that can be. A
     reference may add qualifiers to what it refers to, and only to that; a
     pointer converts as C++ converts one, adding them below where every level
     above is a const raw pointer. Inner levels match exactly otherwise.

   - Observers. A layer that owns nothing and is made of a raw pointer to its
     element is made of a pointer that owns nothing either: the `observe` step.

   - Qualification. A raw pointer, and a handle whose type traits rebind to a
     more qualified element it converts to, converts to one of a more qualified
     element as C++ converts a pointer to the element: the `qualify` step. Its
     element may gain any qualifiers, and a level below the element only where
     every level above it is a raw pointer made const: a `std::shared_ptr<T>`
     gives a `std::shared_ptr<const T>`, and a `std::shared_ptr<T *>` a
     `std::shared_ptr<T * const>` but no `std::shared_ptr<const T *>`. The step
     makes a new object, copied of a pointer or a handle the request does not
     own and moved out of one it owns; one the request owns as a pointer it
     hands over is not converted. A reference never refers to a converted one as
     to the one converted. A layer the request makes of a value or of a pointer
     is made of the qualifiers it needs at once, so the step after it makes no
     other object. A converted layer is not taken apart, copied or converted
     again. The specification marks each level a conversion may qualify, instead
     of making a thing for each qualifier, and a request is satisfied of a
     marked level of the qualifiers it asks.

   - Arrays of unknown bound. A handle of an array of a known bound whose type
     traits name `unknown_bound_t` converts to that handle, which holds the same
     array as one of unknown bound: the `unknown_bound` step. A
     `std::shared_ptr<T[2]>` gives a `std::shared_ptr<T[]>`. The step makes a
     new object, copied of a handle the request does not own and moved out of
     one it owns. A handle converted to a more qualified element is not
     converted again.

   - Ownership of a pointee. A thing tells whether the request owns what its
     outer raw pointer points to: a pointer a scope makes does, a copy of it
     does too, and adopting it or releasing its element ends that. Such a
     pointer is handed over or taken over by a handle, never wrapped, and an
     observer is made only of a pointer that owns nothing. A pointer request
     takes a pointer value whose pointee the request does not own as one way to
     satisfy it besides referring to the object. It takes one whose pointee it
     owns, which hands that over, only where nothing can be referred to for as
     long as the request needs. A layer made of a pointer it does not own may be
     empty, as the pointer may be null.

   - Cost. The cost of a plan is lexicographic: the copies of the leaf, its
     moves, whether what satisfies the request may be absent when a layer is
     empty, the objects kept as long as the container, and the objects made for
     the request. The cheapest plan is the fewest objects by construction, not
     by the order of the search. A step that makes a new object counts one; one
     kept with the binding counts as kept. A search keeps a thing at a cost
     unless it has the thing at the same copies and moves of the leaf and at no
     more objects: objects only grow along a derivation, so the search ends, and
     no cheaper plan is dropped.

   - Who makes a layer. A request makes a new layer, by a wrap, an adoption or
     an observer, only as the layer of the request at the depth it will have: a
     layer the derivation made is not converted, so another leads nowhere. A
     request does not wrap the bare interface, which only the binding can name
     the leaf of. A binding wraps the leaf itself into a layer that rebinds, or
     the stored layers as they are stored; every other wrap of what holds the
     leaf is upcast to nothing a request takes. A layer the derivation wrapped a
     value in is not taken apart again: its element is what it was made of, and
     taking it would only drop what tells it apart, as a copy, a const or a
     limitation.

   - The leaf. Whether the leaf can be copied is what the library says of it: a
     collection of what cannot be copied cannot be either, whatever
     `std::is_copy_constructible` says.

   **Steps by the type that knows them.** A step needs the C++ type of what it
   is applied to. Its function belongs to the side that knows that type.

   - A binding knows the type of each of its layer sources and of everything a
     step makes of them. For each such type it has one table of erased
     operations: its size and alignment, destruction, copy, move, the address of
     its element, the pointer to it, the empty value of it, the decay of an
     array, the hand-over of an array, the conversion of a sole owner to its
     sharing handle, the release of what a pointer owns, and the upcast to the
     interface. Each operation names the table of the type it makes. The tables
     are built from the typed steps of the static container, so both containers
     apply one implementation of each step.

   - A request knows the types it spells. It has a table for each of its nested
     types, with the operations that make that type of the one below: a run of
     wraps, the adoption of a pointer it owns, and an observer of a pointer.
     Nothing else is the request's.

   - A plan is compiled once, at its first use, into the operations it calls. A
     step of a layer the request makes is found among the request's operations,
     by the depth of the layer in the request; every other step is the operation
     of the table of what it is applied to. Each table has, as the code
     compiles, every operation the semantics allow from its type: each binding
     checks its tables against the steps the semantics allow from each of its
     sources, and each request its tables against the layers it makes. A plan
     meets no missing operation.

   **Cost as the code compiles.** The functions a binding instantiates scale
   with the distinct C++ types of its sources, not with the steps between them.
   A binding of the abstract interface publishes only the sources a request
   takes something of, those of the interface, and the sources on the way to
   them. The sources and the tables are emitted as many as there are.

   **Undo follows nesting.** A runtime binding registers the rollback of its
   cell as soon as its source is materialized, before the plan runs. What the
   plan keeps with the binding is undone before the cell is reset, so the undo
   of what a retention keeps is unconditional.

   **Objects made of a value.** A storage, a factory and `construct<T>()` make
   an object of a value with `made_as`: the object constructed of the value, a
   run of wraps around it (`made_of`), or a handle of an array of it that its
   type traits emplace. A factory emplaces a wrapper of what it constructs in
   place, down the layers that declare `emplaces_element`, and makes it of the
   constructed value only where it cannot.

   **Values are made where they are returned.** A plan for a value ends in what
   makes the value: a copy, a move, or a run of wraps. The runtime container
   does not apply that last step. It returns what the plan reached, and the
   request makes its value of it in place of the value the call returns. The
   static container makes it in place of the value it returns.

   **Known limitations.** An empty holder of a pointer is not mapped to an empty
   holder of an observer of it: a request through the element of an empty holder
   fails. A shared handle that cannot be made of a value is not made of a sole
   owner a request makes and converts; one stored by the binding still converts.
   A handle family a request uses for the bare interface is one of
   `owning_handles`, which the binding wraps the leaf in. A pointer or a handle
   is converted to one more qualified only at its element: a level below it, as
   of a `std::shared_ptr<T *>` to a `std::shared_ptr<const T * const>`, would
   take a conversion for every qualifier of every level.

   **Measured.** A runtime type costs about 0.32 G instructions, 12.4 KB of code
   and 5.7 MB of compiler memory, against 12.5 KB and 5.1 MB of the published
   resolutions; a static type 0.30 G, 10.5 KB and 5.3 MB. The rest of the memory
   is the compile-time search of the layer sources of each binding, which the
   static container makes too.

8. **Tests.** The model checks become a cover of the composition, and the full
   model run a regression.

## The Cost

A search evaluated as the code compiles takes about 14 million instructions with
GCC and 37 million with clang. That is about 5% of what a type registered in a
static container costs, which makes few requests of it. A program that makes
thousands of requests, as the conversion check does, would double its time. A
search depends only on its values, and in a program most requests share them: a
variable template of the values, encoded as integers, makes each search once for
a translation unit. The runtime container searches as it resolves, with one
function of no type, which is compiled once.

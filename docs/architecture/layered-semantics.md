# Layered Semantics

This page is a design in progress. It describes how the resolution code is to be
built so that it is correct by construction, and which parts of the code today
belong to which layer. The sections marked "today" describe the code as it is.

## The Problem

A cell of the model is a container, a scope, a stored form, a leaf, the
interfaces declared, a selection, an operation, an interface and a request. The
product has millions of cells. The model checks compile a cover of it, so a
defect in a cell outside the cover is not found. Tests of the product cannot
show that the library is correct.

## The Layers

The library is to be built of four layers. Each layer has a small domain of its
own, which a test covers completely. The composition of the layers is correct
when each layer is.

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
4. **Execution.** One interpreter applies a plan through the steps. Both
   containers use it: the runtime container keeps the plan as the data of a
   node, and the static container applies the same steps inline.

If the classification is right, the semantics equal the specification, each step
keeps its contract and the interpreter applies the plan as it is, every cell is
right. The tests are then the sum of the layers, not their product. A cover of
the product checks only the composition.

## The Specification Is Layer 2

[spec/semantics.py](../../spec/semantics.py) is already a model of layer 2. A
`Thing` is a stack of layers around an element, with a lifetime for each layer,
an origin, and what it is of the instance. `transitions` gives the steps from a
thing, and `reachable` gives every thing a stored object reaches, with the
copies and the moves of the leaf. A binding takes the steps that need the type
it stores (`binding_steps`); a request takes the others (`request_steps`).
`satisfactions` tells which things satisfy a request.

The library is to compute the same: one implementation of the steps, the
reachable things and the satisfactions, as constant expressions over these
values. Its results are then compared with the specification for every starting
object and every request.

## A Layer Is Its Properties

The rules name no wrapper. A layer is what the rules ask of it: what it owns,
whether a copy of it holds the same element, whether it can be copied, moved,
made of a value or be empty, whether an empty one is a value, whether it holds
only its element, and whether it takes over a raw pointer. Whether an owned
value of one layer converts to another is a fact of the two, given for the
layers in play.

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
over a pointer holds its element apart from it; and an empty value is a value
only of a layer that may be empty. Of the combinations, 216 are possible. The
classification is to reject the type traits of any other.

The steps of the specification and the code that takes them today:

| Step                                             | Who takes it | Code today                                                                        |
| ------------------------------------------------ | ------------ | --------------------------------------------------------------------------------- |
| `element`, `element_pointer`, `element_or_empty` | binding      | the resolutions a storage publishes (`binding_resolutions`, `conversion/paths.h`) |
| `upcast`                                         | binding      | the rebinding of a handle, the base reference (`conversion/paths.h`)              |
| `release`                                        | binding      | `release_object` (`binding/resolution.h`)                                         |
| `copy`                                           | request      | `derivation::copy`, `copy_object`                                                 |
| `wrap`                                           | request      | `derivation::compose`, `derivation::keep`                                         |
| `convert`                                        | request      | `derivation::convert`                                                             |
| `adopt`, `hand_over`                             | request      | `derivation::adopt`, `derivation::take`                                           |
| `decay`                                          | request      | the pointer of an array (`conversion/paths.h`)                                    |
| satisfaction of a reference or a pointer         | request      | `derivation::refer`, `derivation::address`                                        |
| satisfaction of a value                          | request      | `derivation::place`, `derivation::move`, `derivation::copy`                       |

The library names its derivations by what the request does, and the
specification by the step. The first phase sets one vocabulary.

## The Headers Today

| Layer          | Headers                                                                                                                                                                                     |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Classification | `type/type_traits.h`, `type/shape.h`, `type/annotated.h`, `request/decomposition.h`, `factory/constructor_detection.hpp`, `base/type_descriptor.h`                                          |
| Semantics      | `request/derivation.h`, `binding/facts.h`, `binding/materialization.h`, `storage/scope_traits.h`, `binding/selection.h`, the decisions of `conversion/paths.h` and `conversion/available.h` |
| Steps          | the operations of `conversion/paths.h`, `conversion/apply.h`, `request/composition.h`, the derivations of `binding/resolution.h`, the storages of `storage/`, `factory/`                    |
| Execution      | `binding/activation.h`, the engine of `runtime/binding.h`, `static_plan` of `static/binding.h`, `resolution/algorithm.h`                                                                    |
| Plumbing       | `lookup/`, `memory/`, the registries and the transactions of `runtime/` and `static/`, `context/`                                                                                           |

The plumbing does not decide what a request resolves to. It is tested as any
other code.

Most decisions of layer 2 are values already: `request_facts`,
`derivation_facts`, `binding_facts` and `derive_request` take no C++ type. The
gap is the conversion paths. `conversion/paths.h` searches a path through the
C++ types of a request, so the path is not a value that can be compared with the
specification. It is also the largest part of the compile time that is left.

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
     bound interface. 629,520 requests, 8,813,280 answers.

   - Every possible layer a type could define: each of the 216, beside the
     layers of C++ itself, stored and requested in every stack of it, of a
     pointer and of an array, of every scope and leaf. 1,129,344 requests,
     15,810,816 answers.

   - With `semantics-check-pairs`, every pair of possible layers, with each
     conversion between them that `semantics.possible_conversion` allows: an
     owned value of a layer that owns its element alone converts to a layer that
     owns an element apart from it. Each pair is stored and requested in every
     stack that holds both of them, of a pointer and of an array, of every scope
     and leaf: 31,212 universes, 247,595,712 requests, 3,466,339,968 answers.

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
   its layers and by C++ itself, and an interpreter that applies the steps of a
   plan one after another. A run of wraps makes its outermost layer at once, in
   place, and the last run is made where the request needs it, so that the leaf
   is moved once. What a step makes for the request is kept by a keeper. A step
   that takes over what a pointer points to releases the handle it was taken out
   of, by the `release` of its type traits. `execution_check.py` asks the
   semantics the plan of every conversion of the model, applies each to the leaf
   types of the model, which count their copies and moves, and compares the
   counts with the plan, without a container: 60,694 cells, all equal. It found
   three defects, each fixed: a handle a pointer was taken out of released it
   too; a wrapper emplaced of its own element type moved the leaf twice; and the
   pointer to the first element of an array was upcast to an array of the
   interface, which the specification and the semantics then both allowed the
   request to take over.

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

7. **Execution.** The containers apply plans by the interpreter, in place of the
   conversion paths and the derivations. The steps split as the specification
   splits them. A binding takes the steps that need the type it stores: the
   element of a layer, a pointer to it, the optional it holds, and the upcast to
   the interface. What they reach are the sources the binding publishes, which
   it makes by functions of its type. A request takes every other step: each
   wraps, converts, copies, adopts, decays or hands over toward the layers of
   the request, so its C++ types are those of the suffixes of the request type,
   and the request has a function of its type for each step from each suffix.
   The static container plans each request as the code compiles. The runtime
   container plans a request of a binding as it resolves, by one function of no
   type, and keeps the plan in the binding.

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

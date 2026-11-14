# Layered Semantics

This page is a design in progress. It describes how the resolution code is to
be built so that it is correct by construction, and which parts of the code
today belong to which layer. The sections marked "today" describe the code as
it is.

## The Problem

A cell of the model is a container, a scope, a stored form, a leaf, the
interfaces declared, a selection, an operation, an interface and a request.
The product has millions of cells. The model checks compile a cover of it, so
a defect in a cell outside the cover is not found. Tests of the product cannot
show that the library is correct.

## The Layers

The library is to be built of four layers. Each layer has a small domain of its
own, which a test covers completely. The composition of the layers is correct
when each layer is.

1. **Classification.** A C++ type is mapped to abstract facts: the layers
   around its element (a shared handle, a unique handle, an optional, an
   alternative type, an array, a pointer), the element (the leaf or its
   abstract interface), the qualification and the reference of the request,
   and what the leaf can do (be copied, be moved, be made by a factory). This
   layer holds every rule of C++ the library depends on. Its domain is a few
   wrapper kinds by a few leaf kinds by the request shapes.
2. **Semantics.** Of these facts alone, with no C++ type, the layer decides
   everything: what a storage gives, what a binding publishes, how a request is
   derived of what is published, how long each object lives, and how many
   copies and moves the leaf takes. Its output is a plan: the steps from the
   stored object to the request. The domain is finite. It is compared with
   [spec/semantics.py](../../spec/semantics.py) for every element of the domain.
3. **Steps.** Each kind of step is one function template with a contract: what
   it requires of its source, what it gives, how long the result lives and
   what it copies or moves of the leaf. Each step is tested alone, over the
   wrapper kinds and the leaf kinds.
4. **Execution.** One interpreter applies a plan through the steps. Both
   containers use it: the runtime container keeps the plan as the data of a
   node, and the static container applies the same steps inline.

If the classification is right, the semantics equal the specification, each
step keeps its contract and the interpreter applies the plan as it is, every
cell is right. The tests are then the sum of the layers, not their product.
A cover of the product checks only the composition.

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
values. Its results are then compared with the specification for every
starting object and every request.

The steps of the specification and the code that takes them today:

| Step                                        | Who takes it | Code today                                                                     |
| ------------------------------------------- | ------------ | ------------------------------------------------------------------------------ |
| `element`, `element_pointer`, `element_or_empty` | binding      | the resolutions a storage publishes (`binding_resolutions`, `conversion/paths.h`) |
| `upcast`                                    | binding      | the rebinding of a handle, the base reference (`conversion/paths.h`)          |
| `release`                                   | binding      | `release_object` (`binding/resolution.h`)                                      |
| `copy`                                      | request      | `derivation::copy`, `copy_object`                                              |
| `wrap`                                      | request      | `derivation::compose`, `derivation::keep`                                      |
| `convert`                                   | request      | `derivation::convert`                                                          |
| `adopt`, `hand_over`                        | request      | `derivation::adopt`, `derivation::take`                                        |
| `decay`                                     | request      | the pointer of an array (`conversion/paths.h`)                                 |
| satisfaction of a reference or a pointer    | request      | `derivation::refer`, `derivation::address`                                     |
| satisfaction of a value                     | request      | `derivation::place`, `derivation::move`, `derivation::copy`                    |

The library names its derivations by what the request does, and the
specification by the step. The first phase sets one vocabulary.

## The Headers Today

| Layer          | Headers                                                                                                                                                                                     |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Classification | `type/type_traits.h`, `type/shape.h`, `type/annotated.h`, `request/decomposition.h`, `factory/constructor_detection.hpp`, `base/type_descriptor.h`                                       |
| Semantics      | `request/derivation.h`, `binding/facts.h`, `binding/materialization.h`, `storage/scope_traits.h`, `binding/selection.h`, the decisions of `conversion/paths.h` and `conversion/available.h` |
| Steps          | the operations of `conversion/paths.h`, `conversion/apply.h`, `request/composition.h`, the derivations of `binding/resolution.h`, the storages of `storage/`, `factory/`                 |
| Execution      | `binding/activation.h`, the engine of `runtime/binding.h`, `static_plan` of `static/binding.h`, `resolution/algorithm.h`                                                                   |
| Plumbing       | `lookup/`, `memory/`, the registries and the transactions of `runtime/` and `static/`, `context/`                                                                                         |

The plumbing does not decide what a request resolves to. It is tested as any
other code.

Most decisions of layer 2 are values already: `request_facts`,
`derivation_facts`, `binding_facts` and `derive_request` take no C++ type. The
gap is the conversion paths. `conversion/paths.h` searches a path through the
C++ types of a request, so the path is not a value that can be compared with
the specification. It is also the largest part of the compile time that is
left.

## The Phases

1. **Semantics as values.** Port the steps, the reachable things and the
   satisfactions of the specification to constant expressions over layer kinds,
   things and requests, in a header of their own that includes no other part of
   the library. Measure what computing the reachable things of a registration
   costs as the code compiles.
2. **Exhaustive comparison.** A test program asks the port every request of
   every starting object of the domain and prints the answers. A check compares
   them with the specification, answer by answer. The domain is finite, so the
   check is complete.
3. **Classification.** One module maps a C++ type to its things, with an
   exhaustive test over the wrapper kinds, the leaf kinds and the request
   shapes.
4. **Steps.** One function template for each kind of step, with its contract
   as static assertions, and a test for each.
5. **Execution.** A plan is the path to a satisfying thing in the reachable
   graph. The interpreter applies it in both containers, and replaces the
   conversion paths and the derivations.
6. **Tests.** The model checks become a cover of the composition, and the full
   model run a regression.

## The Risk

The reachable things of a stored object are computed once for a registration.
If that is too slow as the code compiles, layer 2 keeps the specification's
algorithm but computes it of less: of the stored layers only, with the request
shapes looked up, not searched. Phase 1 measures it before anything depends
on it.

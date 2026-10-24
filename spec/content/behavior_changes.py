"""Content of the behavior changes section.

Every intended difference between the target semantics of the spec and the
baseline library. A change lists the case paths of the checks that observe it,
see `BehaviorChange` in spec/schema.py. The route changes are observed by
test/routes/conform.py, the scope changes by test/spec/scopes_check.py against
the baseline. A change no table observes names the test of the next
implementation that asserts it, `test:<Suite>.<Name>`, or the diagnostic whose
generated test rejects it, `diagnostic:<id>`.
"""

from spec.schema import BehaviorChange

DECISION = "docs/architecture/spec-driven-design.md, Decision Log, 2026-10-19"
LOOKUP_DECISION = (
    "docs/architecture/spec-driven-design.md, Open Decisions, lookup and "
    "selection decisions of 2026-10-20"
)
COLLECTION_DECISION = (
    "docs/architecture/spec-driven-design.md, Decision Log, collections and "
    "mixed traits, 2026-10-20"
)
SWITCH_DECISION = (
    "docs/architecture/spec-driven-design.md, Decision Log, the coverage "
    "ledger of the switch, 2026-10-21"
)


def both(pattern):
    """The pattern in the static and the mixed container."""
    return (f"routes:static/{pattern}", f"routes:mixed/{pattern}")


behavior_changes: tuple[BehaviorChange, ...] = (
    BehaviorChange(
        id="borrowing_is_compositional",
        description=(
            "A borrowing scope reaches every object its stored shape holds, through "
            "any combination of layers, and copies every copyable one."
        ),
        cases=tuple(
            f"routes:*/*/{scope}/{stored}/*#served"
            for scope in ("external", "shared")
            for stored in (
                "*optional<*>",
                "*<*optional<*>>",
                "*unique*<*shared*<*>>",
            )
        ),
        baseline=(
            "Copies of optionals are refused, the leaf under a nested optional and "
            "the handle inside an optional are not reachable, and a sharing handle "
            "inside a sole-owner handle is not copied."
        ),
        now="Every reachable object is borrowed, and every copyable one is copied.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="borrowing_never_moves",
        description="A borrowing scope never serves an rvalue reference request.",
        cases=(
            "routes:mixed/*/shared/*&&#served",
            "routes:mixed/*/shared_cyclical/*&&#served",
        ),
        baseline=(
            "The mixed container serves an rvalue reference to a copy of a shared "
            "object, while the runtime and static containers refuse it."
        ),
        now="Every container refuses it.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="consuming_is_compositional",
        description=(
            "A consuming scope composes its rules: it takes the element out of an "
            "optional, and wraps or transfers what it took."
        ),
        cases=(
            "routes:*/*/unique/*optional<*>/*#served",
            "routes:*/*/unique/std::variant<*>/std::optional<*>*#served",
            "routes:*/*/unique/*leaf[*]/*leaf[*]&&#served",
        ),
        baseline=(
            "The element of a consumed optional cannot be taken, an alternative "
            "taken from a variant cannot be wrapped, and a consumed pointer is not "
            "served as an rvalue reference."
        ),
        now="Every composition of consuming rules is served.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="consumed_objects_are_never_borrowed",
        description=(
            "A consuming scope never serves an lvalue reference to the object it "
            "consumed."
        ),
        cases=(
            "routes:*/*/unique/*leaf/*leaf&#served",
            "scopes:unique/form plain",
            "matrix:dingo_matrix_test_runtime_unique_reference_regressions/*",
        ),
        baseline=(
            "The runtime and mixed containers return an lvalue reference to the "
            "constructed object, which is destroyed before the call returns."
        ),
        now="Every container refuses the request.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="consuming_moves_minimally",
        description=(
            "Serving a consumed object makes no more moves and destroys no more "
            "temporaries than the container that makes the fewest."
        ),
        cases=(
            "routes:*/*/unique/*#moves",
            "routes:*/*/unique/*#temporaries",
            "matrix:dingo_matrix_test_lifetime_counts/*_unique_*",
        ),
        baseline=(
            "The runtime container moves a consumed value one more time than the "
            "static container, and the mixed container one more time for rvalue "
            "references."
        ),
        now=(
            "Every container moves a consumed object only as far as the request "
            "needs, no more than the container that moves the fewest."
        ),
        decision=DECISION,
    ),
    BehaviorChange(
        id="eager_registration_check",
        description=(
            "A static or mixed container rejects a binding its scope cannot store "
            "when the container type is formed, as the runtime container rejects "
            "it at registration."
        ),
        cases=(
            *both("*/shared/std::*_ptr<*[]>/*#registration"),
            *both("*/unique/std::*_ptr<*[]>/*#registration"),
            *both("*/unique/*&/*#registration"),
            *both("impl[[]*]/unique/impl/*#registration"),
            *both("impl[[]*]/unique/std::optional<*>/*#registration"),
        ),
        baseline=(
            "The static and mixed containers accept the binding and fail only "
            "when a request reaches it."
        ),
        now="Every container rejects the binding when it is declared.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="shared_cyclical_stores_flat_shapes",
        description=(
            "shared_cyclical stores only shapes whose element is a leaf, because "
            "the element must exist before it is published."
        ),
        cases=("routes:*/*/shared_cyclical/std::shared_ptr<*<*>>/*#registration",),
        baseline=(
            "A nested sharing handle is accepted, and an lvalue reference request "
            "dereferences a null inner handle and aborts."
        ),
        now="The binding is rejected.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="shared_cyclical_stores_no_pointers",
        description=(
            "shared_cyclical does not store raw pointers, because a pointer cannot "
            "be published before the object it points to exists."
        ),
        cases=(
            "routes:*/*/shared_cyclical/*[*]/*#registration",
            "scopes:shared_cyclical/form pointer",
        ),
        baseline="The binding is accepted and publishes a null pointer.",
        now="The binding is rejected.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="shared_cyclical_exposes_no_arrays",
        description="The shared_cyclical descriptor exposes no array form.",
        cases=(
            "scopes:shared_cyclical/form bounded_array",
            "scopes:shared_cyclical/form shared_array_handle",
            "scopes:shared_cyclical/form unbounded_array",
        ),
        baseline=(
            "The scope policy exposes the array forms, but a binding of an array "
            "form does not compile."
        ),
        now="The forms are not exposed, so the binding is rejected by the descriptor.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="interface_bindings_compose_with_layers",
        description=(
            "A binding can declare interfaces of a leaf it holds in an optional or "
            "an alternative, which it reaches by borrowing."
        ),
        cases=tuple(
            f"routes:*/impl[[]*]/{scope}/{stored}/*#registration"
            for scope in ("external", "shared", "shared_cyclical")
            for stored in ("*optional<*", "std::variant<*")
        ),
        baseline="The binding does not compile.",
        now="The binding is accepted, and its interfaces are served by borrowing.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="interface_bindings_serve_an_interface",
        description=(
            "A binding whose declared interfaces cannot be served by any request is "
            "rejected."
        ),
        cases=("routes:*/impl[[]*]/unique/std::*<std::*<impl>>/*#registration",),
        baseline=(
            "A consumed nested handle with declared interfaces is accepted, and "
            "every request for it is not found."
        ),
        now="The binding is rejected.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="interface_handles_are_retained",
        description=(
            "A sharing handle of an interface converted from a borrowed sharing "
            "handle is kept by the binding, so it can be referred to in every "
            "container."
        ),
        cases=(
            "routes:mixed/impl[[]*]/shared/*#identity",
            "routes:mixed/impl[[]*]/shared/*#stable",
            "routes:static/impl[[]*]/shared/std::shared_ptr<*>/*std::shared_ptr<*iface>*#served",
        ),
        baseline=(
            "The mixed container returns a reference to a converted handle that is "
            "destroyed before the call returns, and the static container refuses "
            "the request."
        ),
        now="Every container returns a reference to the kept handle.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="interfaces_do_not_rebind_storage",
        description=(
            "A binding with one interface stores the same shape as one with "
            "several, and reaches the interface by conversion."
        ),
        cases=("routes:*/impl[[]iface]/*/std::unique_ptr<impl>/*std::unique_ptr<iface>*#served",),
        baseline=(
            "With exactly one interface, the stored shape is rebound to it, so a "
            "reference to a sole-owner handle of the interface is served, while with "
            "several interfaces it is not."
        ),
        now="It is not served with any number of interfaces.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="user_wrappers_are_layers",
        description=(
            "A user wrapper behaves as the wrapper semantics its traits declare, in every "
            "scope, as the standard wrappers do."
        ),
        cases=("routes:*/*/shared_cyclical/user_shared<*>/*#registration",),
        baseline=(
            "shared_cyclical stores std::shared_ptr but rejects a user sharing "
            "handle, because only the standard handles have storage cells."
        ),
        now="A user sharing handle is stored like the standard one.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="wrapper_extension_point",
        description=(
            "A wrapper is made a layer of the shape algebra by one public trait, "
            "dingo::wrapper_traits, which the standard wrappers specialize too."
        ),
        cases=("api:wrapper_traits",),
        baseline=(
            "dingo::type_traits passes internal types of the implementation to "
            "resolve_type and does not tell a sharing from a sole-owner handle, "
            "every scope needs a dingo::storage_traits specialization beside it, "
            "and dingo::is_interface_storage_rebindable_v decides whether a stored "
            "handle is rebound to an interface."
        ),
        now=(
            "dingo::wrapper_traits declares the wrapper semantics, the family, the "
            "element types, rebind, element, and make, and adopt and share for "
            "the handles that own their element. The storage traits and the "
            "interface rebinding trait are not part of the public API."
        ),
        decision=DECISION,
    ),
    BehaviorChange(
        id="wrapper_semantics_extension_point",
        description=(
            "A wrapper semantics is a declaration: a tag type whose public "
            "dingo::wrapper_semantics give its properties and forms, as the "
            "kinds of the library are declared, so a user adds a kind of a "
            "wrapper the library has none of."
        ),
        cases=("api:wrapper_semantics",),
        baseline=(
            "The kinds of handles are fixed by the implementation: a user "
            "wrapper behaves as a unique, shared, or optional handle at most."
        ),
        now=(
            "dingo::wrapper_semantics declares the layer_property bits and the "
            "forms of a kind, each a dingo::operation with the kind it leads "
            "to; the library's ::dingo::semantics::sole_owner, ::dingo::semantics::shared_owner, ::dingo::semantics::optional_value, "
            "::dingo::semantics::variant_value, ::dingo::semantics::fixed_array, ::dingo::semantics::dynamic_array, ::dingo::semantics::pointer, "
            "and ::dingo::semantics::reference are declared so too."
        ),
        decision=DECISION,
    ),
    BehaviorChange(
        id="static_external_instances",
        description=(
            "A static or mixed container accepts an external binding, whose "
            "instance it takes as a constructor argument."
        ),
        cases=both("*/external/*#registration"),
        baseline="Static storage must be default constructible, so it is rejected.",
        now=(
            "The binding is accepted, and the container takes the instance of "
            "every external binding as a constructor argument, in the order of "
            "the bindings."
        ),
        decision="docs/architecture/spec-driven-design.md, Decision Log, 2026-10-20",
    ),
    BehaviorChange(
        id="duplicates_rejected_in_both_domains",
        description=(
            "A second binding of a key of an interface is rejected in every "
            "container unless its lookup is declared many, and a domain declared "
            "many accepts any number."
        ),
        cases=(
            "selection:*#duplicate",
            "selection:static/many/*#registration",
            "selection:static_parent/many/*#registration",
            "matrix:dingo_matrix_test_static_parent_container/*",
        ),
        baseline=(
            "A static or mixed container accepts duplicates and fails only when "
            "one is resolved, and a static container rejects two bindings of one "
            "implementation and key even where many is declared."
        ),
        now="Duplicates are rejected when the container is formed or the binding is registered.",
        decision=LOOKUP_DECISION,
    ),
    BehaviorChange(
        id="out_of_range_keys_raise_the_index_exception",
        description=(
            "A registration whose key value is outside the bound of the dense "
            "array of its domain raises type_index_out_of_range_exception, the "
            "exception the public API names for it."
        ),
        cases=("selection:*/bounded/*#out_of_range",),
        baseline="It raises std::out_of_range.",
        now="It raises type_index_out_of_range_exception.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="empty_collections_are_valid",
        description="A collection request that names no binding yields an empty collection.",
        cases=(
            "selection:*#empty_collection",
            "matrix:dingo_matrix_test_runtime_exception_regressions/*",
        ),
        baseline=(
            "A runtime container returns an empty collection only where a many "
            "lookup is declared, a mixed container throws type_not_found_exception, "
            "and a static container fails to compile."
        ),
        now="Every container returns an empty collection.",
        decision=LOOKUP_DECISION,
    ),
    BehaviorChange(
        id="collections_of_a_domain_of_one",
        description=(
            "A collection request of a domain declared one yields the binding the "
            "domain holds."
        ),
        cases=tuple(
            f"selection:{domain}/{setting}/*/all:{key}[78]#collection"
            for domain in ("runtime", "parent")
            for setting in ("default", "bounded")
            for key in ("fixed", "runtime")
        )
        + ("matrix:dingo_matrix_test_lookup_route_regressions/*",),
        baseline="A collection of a value key declared one throws type_not_found_exception.",
        now="The collection holds the one binding, as a query over the bindings.",
        decision=COLLECTION_DECISION,
    ),
    BehaviorChange(
        id="collections_fall_back_to_the_parent",
        description=(
            "A collection request in a child container that names none of its "
            "bindings yields the collection of its parent."
        ),
        cases=(
            "selection:parent/*/all:*#collection",
            "selection:static_parent/*/all:*#collection",
        ),
        baseline=(
            "A child container never gathers the bindings of its parent into a "
            "collection; only a collection the parent registers whole is found."
        ),
        now="A child shadows its parent for collections as for singular requests.",
        decision=COLLECTION_DECISION,
    ),
    BehaviorChange(
        id="keys_in_every_container",
        description=(
            "Every key kind works in every container: a fixed key in a runtime "
            "registration, a key in a mixed container, and a runtime key value "
            "against static bindings."
        ),
        cases=(
            "selection:runtime/*#registration",
            "selection:parent/*#registration",
            "selection:mixed/default/*#registration",
            "selection:static/*/one:runtime[78]#selection",
            "selection:static/*/all:runtime[78]#collection",
            # The registrations and the requests the baseline rejects when
            # they are compiled.
            "selection:runtime/*#registration_time",
            "selection:parent/*#registration_time",
            "selection:mixed/default/*#registration_time",
            "selection:static/*/one:runtime[78]#selection_time",
            "selection:static_parent/*/one:runtime[78]#selection",
            "selection:static_parent/*/one:runtime[78]#selection_time",
        ),
        baseline=(
            "A fixed key in a runtime registration and a value key in a mixed "
            "container do not compile, and a static container rejects a request "
            "by a runtime key value."
        ),
        now=(
            "A fixed key is stored as its value, and a runtime key value is "
            "compared with the fixed keys of static bindings when it is resolved."
        ),
        decision=LOOKUP_DECISION,
    ),
    BehaviorChange(
        id="mixed_containers_declare_lookups",
        description="A mixed container takes container traits that declare its lookups.",
        cases=(
            "selection:mixed/many/*#registration",
            "selection:mixed/bounded/*#registration",
        ),
        baseline="A mixed container has fixed traits and cannot declare a lookup.",
        now="container<bindings<...>, Traits> declares lookups as the runtime container does.",
        decision=COLLECTION_DECISION,
    ),
    BehaviorChange(
        id="mixed_collections_with_unmatched_static_bindings",
        description=(
            "A collection request of a mixed container whose static bindings do "
            "not match it gathers its runtime bindings."
        ),
        cases=("selection:mixed/default/*:static+*:runtime/all:*#collection",),
        baseline="It fails to compile.",
        now="The collection holds the runtime bindings the request names.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="stored_objects_are_constructed_in_place",
        description=(
            "A binding constructs the object it stores in place, every layer "
            "around its leaf, so storing it moves nothing."
        ),
        cases=("matrix:dingo_matrix_test_lifetime_counts/*_shared_value_optional_*",),
        baseline=(
            "A binding that stores an optional of its leaf constructs the leaf "
            "and moves it into the optional."
        ),
        now="The leaf is constructed in the optional.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="static_bindings_refuse_when_compiled",
        description=(
            "A request that a static binding names and cannot serve is rejected "
            "when the request is compiled, in a static and in a mixed container."
        ),
        cases=(
            "matrix:dingo_matrix_test_conversion_delivery_regressions/*",
            "matrix:dingo_matrix_test_nested_wrappers/*leaf_access_move_only*",
            "matrix:dingo_matrix_test_nested_wrappers/*leaf_refusal*",
        ),
        baseline=(
            "A mixed container throws type_not_convertible_exception when the "
            "request is resolved, and a static container serves a move-only "
            "interface by value from a shared binding, which the runtime "
            "containers refuse."
        ),
        now="The request does not compile.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="keys_apply_to_every_interface",
        description=(
            "The key value of a registration is the key of every interface the "
            "binding declares, whether or not the lookups declare its domain."
        ),
        cases=("matrix:dingo_matrix_test_runtime_lookup_routing_regressions/*",),
        baseline=(
            "A key value applies only to the interfaces whose domain the lookups "
            "declare; the binding is found without a key by the others."
        ),
        now="The binding is found by its key for every interface.",
        decision=LOOKUP_DECISION,
    ),
    BehaviorChange(
        id="static_construct_requests_ask_the_parent",
        description=(
            "A construct request of a static child asks its parent when the "
            "child names no binding, as a resolve request does."
        ),
        cases=("selection:static_parent/*/make:none#selection",),
        baseline=(
            "A static child does not ask its parent for a construct request, "
            "which does not compile when only the parent names a binding."
        ),
        now="The child constructs the request from the binding of its parent.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="scope_extension_point",
        description=(
            "A scope is a tag type whose dingo::scope_traits hold its "
            "descriptor, and may name a slot class derived from "
            "dingo::scope_slot."
        ),
        cases=("api:scope_traits", "api:scope_descriptor", "api:scope_slot"),
        baseline=(
            "A scope is added by specializing dingo::storage_scope_policy and a "
            "storage class for its tag, which are internals of the "
            "implementation and not part of the public API."
        ),
        now=(
            "dingo::scope_traits holds the descriptor of a scope, the fields of "
            "the Scope schema, and may name a slot class; the built-in scopes "
            "are specializations of it."
        ),
        decision=DECISION,
    ),
    BehaviorChange(
        id="index_extension_point",
        description=(
            "A lookup backend names its own index of the key values of a "
            "domain, a member template index<Value, Candidate>, which takes "
            "dingo::index_item values; the backends of the library name theirs "
            "as a backend of the user does."
        ),
        cases=(
            "api:lookup_backend",
            "api:index_item",
            "diagnostic:lookup_backend_without_index",
        ),
        baseline=(
            "A lookup backend names a map as its storage, which the container "
            "constructs from its allocator and fills by try_emplace, and the "
            "index structures are fixed by the implementation."
        ),
        now=(
            "A backend names its index, and a backend that names none is "
            "rejected when it is declared; the index of a request is the one "
            "of the declaration of its domain, chosen when the request is "
            "compiled. A map is no longer a backend."
        ),
        decision=DECISION,
    ),
    BehaviorChange(
        id="leaves_are_copied_by_their_copy_constructor",
        description=(
            "A request of a value copies the leaf by its copy constructor; a "
            "type_conversion_traits of a leaf to its own type is not a route."
        ),
        cases=("test:next_object_routes.a_leaf_is_copied_by_its_copy_constructor",),
        baseline=(
            "A value request copies the leaf by the type_conversion_traits it "
            "declares to itself, when it declares one."
        ),
        now="The leaf is copied by its copy constructor.",
        decision=SWITCH_DECISION,
    ),
    BehaviorChange(
        id="optional_elements_are_not_converted",
        description=(
            "An optional of an interface is not made of a borrowed optional of "
            "an implementation, which would copy its element into one of the "
            "interface."
        ),
        cases=(
            "test:next_stored_shapes."
            "a_pointer_to_an_optional_serves_no_optional_of_its_interface",
        ),
        baseline=(
            "The request is served by an optional of the interface made from "
            "the element, or an empty one."
        ),
        now="The request throws type_not_convertible_exception.",
        decision=SWITCH_DECISION,
    ),
    BehaviorChange(
        id="empty_elements_are_refused",
        description=(
            "A request through the element of a nullable layer that holds it "
            "in place, whose object holds none, is a null address when the "
            "request is of an address, and is refused otherwise."
        ),
        cases=("test:next_stored_shapes.the_element_of_an_empty_optional_is_null",),
        baseline=(
            "A request of a reference or a value through an empty std::optional "
            "throws std::bad_optional_access."
        ),
        now=(
            "It throws type_not_convertible_exception, the diagnostic "
            "element_not_held; a request of an address is null, as before."
        ),
        decision=SWITCH_DECISION,
    ),
    BehaviorChange(
        id="dependencies_are_declared_once",
        description=(
            "A registration declares the dependencies of its binding by a "
            "dependencies<> or by its factory, not both."
        ),
        cases=("diagnostic:registration_dependencies_twice",),
        baseline=(
            "The dependencies<> of a registration with a factory are its "
            "dependencies, which the factory need not take."
        ),
        now="The registration does not compile.",
        decision=SWITCH_DECISION,
    ),
    BehaviorChange(
        id="mixed_containers_without_static_bindings_move",
        description=(
            "A mixed container without static bindings keeps its state apart "
            "from itself, as a runtime container does, and moves; its children "
            "follow it."
        ),
        cases=(
            "test:next_container_move_untyped."
            "a_mixed_container_without_static_bindings_moves",
        ),
        baseline="container<bindings<>> is neither movable nor copyable.",
        now="container<bindings<>> is movable and not copyable.",
        decision=SWITCH_DECISION,
    ),
    BehaviorChange(
        id="rvalue_references_to_copy_only_objects",
        description=(
            "A request of an rvalue reference to an object a binding constructs "
            "refers to the object the container keeps, constructed in place, "
            "so the object need not be movable."
        ),
        cases=(
            "test:next_runtime_registration."
            "refers_to_a_kept_copy_only_object_by_rvalue_reference",
        ),
        baseline="The request of an object that cannot be moved does not compile.",
        now="The request is served.",
        decision=SWITCH_DECISION,
    ),
    BehaviorChange(
        id="arrays_are_not_adopted_into_owning_handles",
        description=(
            "An array leaf is served as itself, its rows, and its elements; no "
            "route adopts it into an owning handle of an array of runtime "
            "size."
        ),
        cases=(
            "test:next_array_handles."
            "a_unique_array_is_not_adopted_into_an_owning_handle",
            "test:next_array_handles."
            "a_shared_array_is_not_adopted_into_an_owning_handle",
        ),
        baseline=(
            "A request of std::unique_ptr<T[]> of a binding of T[N] is served "
            "with a handle of a copy of the array."
        ),
        now="The request is not convertible.",
        decision=SWITCH_DECISION,
    ),
    BehaviorChange(
        id="scope_tags_need_no_header",
        description=(
            "The tags of the scopes of the library are complete wherever the "
            "container is, so a registration with one needs no header of the "
            "scope."
        ),
        cases=(
            "test:next_scope_headers."
            "the_container_declares_the_scopes_of_the_library",
        ),
        baseline=(
            "A registration with a scope whose header, such as "
            "dingo/storage/unique.h, is not included does not compile."
        ),
        now="The registration compiles.",
        decision=SWITCH_DECISION,
    ),
    BehaviorChange(
        id="local_bindings_shadow_collections",
        description=(
            "The local bindings of a request shadow the bindings of the "
            "container for a collection, as for one object."
        ),
        cases=(
            "test:next_local.shadows_the_collections_of_the_container",
            "matrix:dingo_matrix_test_local_bindings/*_local_collection_value_type_*",
        ),
        baseline=(
            "A collection without a key holds the container's and the local "
            "bindings; one by a key holds only the local bindings of the key."
        ),
        now=(
            "Every collection holds the local bindings of its key when there "
            "are any, and the container's otherwise."
        ),
        decision=SWITCH_DECISION,
    ),
)

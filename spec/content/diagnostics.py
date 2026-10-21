"""Content of the diagnostics section.

The messages of the runtime diagnostics are those of the current
implementation. A message names types as the C++ library spells them, and
`path` is the path of the types whose objects are being constructed,
outermost first, separated by " -> ".

A diagnostic of a request is reported when the request is compiled where the
selection decides it then, see selection_sources, and when it is resolved
otherwise. One that names the path of the objects being constructed is
reported only when it is resolved. The diagnostics of a declaration, which a
program spells wrong, are reported only when it is compiled.
"""

from spec.schema import BindingTime, Diagnostic

COMPILE = (BindingTime.COMPILE,)
BOTH = (BindingTime.COMPILE, BindingTime.RUNTIME)

diagnostics: tuple[Diagnostic, ...] = (
    Diagnostic(
        id="type_not_found",
        condition="No binding is named by the request.",
        exception="type_not_found_exception",
        message="type not found: {request}",
        placeholders=("request",),
        fallback_carrier="type_not_found_carrier",
        reported=BOTH,
    ),
    Diagnostic(
        id="type_not_found_required_by",
        condition=(
            "No binding is named by the request of a dependency of an object "
            "being constructed. A dependency the factory of a static binding "
            "declares is requested when the container is compiled: a container "
            "that decides then that no binding is named by it rejects it then, "
            "with the binding that declares it as the path."
        ),
        exception="type_not_found_exception",
        message="type not found: {request} (required by {path})",
        placeholders=("request", "path"),
        fallback_carrier="type_not_found_carrier",
        reported=BOTH,
    ),
    Diagnostic(
        id="type_not_found_keyed",
        condition="No binding is named by a request with a key, whose type is `key`.",
        exception="type_not_found_exception",
        message="type not found: {request} (index type: {key})",
        placeholders=("request", "key"),
        fallback_carrier="type_not_found_carrier",
        reported=BOTH,
    ),
    Diagnostic(
        id="type_not_found_keyed_required_by",
        condition=(
            "No binding is named by a request with a key of a dependency of an "
            "object being constructed."
        ),
        exception="type_not_found_exception",
        message="type not found: {request} (index type: {key}) (required by {path})",
        placeholders=("request", "key", "path"),
        fallback_carrier="type_not_found_carrier",
    ),
    Diagnostic(
        id="lookup_already_registered",
        condition=(
            "A binding is registered with a key that a binding of a common "
            "interface already holds, in a domain declared with cardinality one."
        ),
        exception="lookup_already_registered_exception",
        message="lookup already registered: interface {interface}, storage {stored}, key type {key}",
        placeholders=("interface", "stored", "key"),
        fallback_carrier="lookup_already_registered_carrier",
        reported=BOTH,
    ),
    Diagnostic(
        id="type_index_out_of_range",
        condition=(
            "A binding is registered with a key value outside the bound of the "
            "dense array that indexes its domain."
        ),
        exception="type_index_out_of_range_exception",
        message="type index out of range: key type {key}",
        placeholders=("key",),
        fallback_carrier="type_index_out_of_range_carrier",
        reported=BOTH,
    ),
    Diagnostic(
        id="type_ambiguous",
        condition="More than one binding is named by the request.",
        exception="type_ambiguous_exception",
        message="type resolution is ambiguous: {request}",
        placeholders=("request",),
        fallback_carrier="type_ambiguous_carrier",
        reported=BOTH,
    ),
    Diagnostic(
        id="type_ambiguous_required_by",
        condition=(
            "More than one binding is named by the request of a dependency of "
            "an object being constructed, whose type is `type`."
        ),
        exception="type_ambiguous_exception",
        message="type resolution is ambiguous: {request} (required by {type})",
        placeholders=("request", "type"),
        fallback_carrier="type_ambiguous_carrier",
    ),
    Diagnostic(
        id="type_not_convertible",
        condition="The binding the request names serves no term it accepts.",
        exception="type_not_convertible_exception",
        message="type is not convertible to {request} from {stored}",
        placeholders=("request", "stored"),
        fallback_carrier="type_not_convertible_carrier",
        reported=BOTH,
    ),
    Diagnostic(
        id="type_not_convertible_required_by",
        condition=(
            "The binding the request of a dependency of an object being "
            "constructed names serves no term it accepts."
        ),
        exception="type_not_convertible_exception",
        message="type is not convertible to {request} from {stored} (required by {type})",
        placeholders=("request", "stored", "type"),
        fallback_carrier="type_not_convertible_carrier",
    ),
    Diagnostic(
        id="type_recursion",
        condition=(
            "An object is requested while it is being constructed, by a scope "
            "that guards the recursion, or the dependencies static bindings "
            "declare form a cycle the graph rule does not allow, whose path "
            "ends with the binding it starts with."
        ),
        exception="type_recursion_exception",
        message="recursive dependency detected: {path}",
        placeholders=("path",),
        fallback_carrier="type_recursion_carrier",
        reported=BOTH,
    ),

    # Reported when the program is compiled
    Diagnostic(
        id="registration_without_scope",
        condition="A registration names no scope<>.",
        exception=None,
        message="a registration needs a scope<>",
        placeholders=(),
        fallback_carrier="registration_without_scope_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="registration_without_storage",
        condition="A registration names no storage<>.",
        exception=None,
        message="a registration needs a storage<>",
        placeholders=(),
        fallback_carrier="registration_without_storage_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="registration_key_twice",
        condition="A registration names a key_type<> and takes a key value too.",
        exception=None,
        message="the registration of {stored} takes its key from a key_type<> or a key_value, not both",
        placeholders=("stored",),
        fallback_carrier="registration_key_twice_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="registration_dependencies_twice",
        condition=(
            "A registration declares the dependencies of its binding by a "
            "dependencies<> and by a factory, which declares its own: a "
            "factory<> or a callable factory it takes."
        ),
        exception=None,
        message="the registration of {stored} declares its dependencies by a dependencies<> or by its factory, not both",
        placeholders=("stored",),
        fallback_carrier="registration_dependencies_twice_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="registration_key_values",
        condition=(
            "A registration takes two key values of one interface, or two key "
            "values that name no interface."
        ),
        exception=None,
        message=(
            "the registration of {stored} takes at most one key value of every "
            "interface, and one of each interface"
        ),
        placeholders=("stored",),
        fallback_carrier="registration_key_values_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="key_interface_undeclared",
        condition="A key value names an interface its binding does not declare.",
        exception=None,
        message="a key value names {interface}, which the binding of {stored} does not declare",
        placeholders=("interface", "stored"),
        fallback_carrier="key_interface_undeclared_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="key_type_values",
        condition="A key_type<> names more than one value.",
        exception=None,
        message="dingo::key_type of {key} accepts at most one value",
        placeholders=("key",),
        fallback_carrier="key_type_values_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="lookup_backend_without_index",
        condition=(
            "A lookup declaration names a backend that names no index of the "
            "key values of a domain."
        ),
        exception=None,
        message="the lookup backend {backend} names no index<Value, Candidate>",
        placeholders=("backend",),
        fallback_carrier="lookup_backend_without_index_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="dense_key_type",
        condition=(
            "An associative lookup indexed by a dense array takes a key that "
            "is not integral or an enumeration."
        ),
        exception=None,
        message=(
            "an associative lookup of {interface} indexed by dingo::array<N> takes "
            "an integral or enum key, not {key}"
        ),
        placeholders=("interface", "key"),
        fallback_carrier="dense_key_type_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="interface_limit",
        condition="A binding declares more interfaces than the limit.",
        exception=None,
        message="a binding of {stored} declares more than max_interfaces interfaces",
        placeholders=("stored",),
        fallback_carrier="interface_limit_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="key_alias_limit",
        condition="A binding has more keys of other key types than the limit.",
        exception=None,
        message="a binding of {stored} has more than max_key_aliases keys of other types",
        placeholders=("stored",),
        fallback_carrier="key_alias_limit_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="interface_not_derived",
        condition=(
            "A request is served by converting the stored leaf to an interface "
            "the binding declares, which the leaf does not derive from."
        ),
        exception=None,
        message="{stored} does not derive from the interface {interface}",
        placeholders=("stored", "interface"),
        fallback_carrier="interface_not_derived_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="scope_form_not_stored",
        condition="The scope of a binding does not store the form of its stored type.",
        exception=None,
        message="the scope of the binding does not store {stored}",
        placeholders=("stored",),
        fallback_carrier="scope_form_not_stored_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="scope_nested_layers",
        condition="The scope of a binding does not store nested layers, which its stored type has.",
        exception=None,
        message="the scope of the binding does not store the nested layers of {stored}",
        placeholders=("stored",),
        fallback_carrier="scope_nested_layers_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="no_default_factory",
        condition=(
            "A binding whose scope constructs its object declares no factory, "
            "and its stored type has no default factory."
        ),
        exception=None,
        message="{stored} has no default factory; the binding needs a factory<>",
        placeholders=("stored",),
        fallback_carrier="no_default_factory_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="interface_in_array",
        condition="A binding that stores an array declares interfaces.",
        exception=None,
        message="an element of the array {stored} cannot be exposed as an interface",
        placeholders=("stored",),
        fallback_carrier="interface_in_array_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="no_interface_served",
        condition="No interface a binding declares can be served from its stored type.",
        exception=None,
        message="no interface the binding of {stored} declares can be served",
        placeholders=("stored",),
        fallback_carrier="no_interface_served_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="cyclical_virtual_interface",
        condition=(
            "A shared_cyclical binding exposes its leaf as an interface the leaf "
            "inherits virtually, which reaching would read the object before it "
            "is constructed."
        ),
        exception=None,
        message=(
            "shared_cyclical cannot publish {stored} as {interface}, which it "
            "inherits virtually: the conversion reads the object, which is not "
            "constructed yet"
        ),
        placeholders=("stored", "interface"),
        fallback_carrier="cyclical_virtual_interface_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="constructor_not_detected",
        condition="No constructor of a type the detected factory makes is detected.",
        exception=None,
        message="no constructor of {type} is detected",
        placeholders=("type",),
        fallback_carrier="constructor_not_detected_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="constructor_generic",
        condition=(
            "A constructor of a type the detected factory makes accepts any "
            "argument, such as a constructor template."
        ),
        exception=None,
        message="a generic constructor of {type} is detected; use an explicit factory",
        placeholders=("type",),
        fallback_carrier="constructor_generic_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="static_instances",
        condition=(
            "A static container is not given the instance of every external "
            "binding."
        ),
        exception=None,
        message=(
            "a static container takes the instance of every external binding, "
            "in the order of the bindings"
        ),
        placeholders=(),
        fallback_carrier="static_instances_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="collection_arguments",
        condition="A collection registration takes more than one key or append function.",
        exception=None,
        message="a collection takes at most one key or append function",
        placeholders=(),
        fallback_carrier="collection_arguments_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="layer_kind_undeclared",
        condition="A type named as a layer kind, by a wrapper or as the kind a form leads to, declares no properties and forms.",
        exception=None,
        message="{kind} is not a layer kind: no layer_kind_traits declares its properties and forms",
        placeholders=("kind",),
        fallback_carrier="layer_kind_undeclared_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="kind_forms_malformed",
        condition="The forms of a layer kind are not a list of kind forms.",
        exception=None,
        message="the forms of the layer kind {kind} are not a type_list of kind_form",
        placeholders=("kind",),
        fallback_carrier="kind_forms_malformed_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="kind_form_of_leaf",
        condition="A layer kind declares a rule of a leaf, which belongs to no kind, as one of its forms.",
        exception=None,
        message="the layer kind {kind} declares a rule of a leaf as one of its forms",
        placeholders=("kind",),
        fallback_carrier="kind_form_of_leaf_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="kind_form_needs_target",
        condition="A layer kind declares a form that leads to another kind without naming the kind.",
        exception=None,
        message="the layer kind {kind} declares a form that leads to another kind without naming it",
        placeholders=("kind",),
        fallback_carrier="kind_form_needs_target_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="kind_form_takes_no_target",
        condition="A layer kind names a kind for a form that leads to no other kind.",
        exception=None,
        message="the layer kind {kind} names a kind for a form that leads to no other kind",
        placeholders=("kind",),
        fallback_carrier="kind_form_takes_no_target_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="kind_form_repeated",
        condition="A layer kind declares a form more than once.",
        exception=None,
        message="the layer kind {kind} declares a form more than once",
        placeholders=("kind",),
        fallback_carrier="kind_form_repeated_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="kind_property_unknown",
        condition="A layer kind declares a property bit that is no layer property.",
        exception=None,
        message="the layer kind {kind} declares a property bit that is no layer_property",
        placeholders=("kind",),
        fallback_carrier="kind_property_unknown_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="kind_form_needs_property",
        condition="A layer kind declares a form that needs a property the kind does not have.",
        exception=None,
        message="the layer kind {kind} declares a form that needs a property it does not have",
        placeholders=("kind",),
        fallback_carrier="kind_form_needs_property_carrier",
        reported=COMPILE,
    ),
    Diagnostic(
        id="alternative_not_held",
        condition=(
            "A request is served by a route through an alternative of a "
            "variadic layer, whose object holds another alternative."
        ),
        exception="type_not_convertible_exception",
        message="{type} holds another alternative than the one the request is routed through",
        placeholders=("type",),
        fallback_carrier="alternative_not_held_carrier",
        reported=(BindingTime.RUNTIME,),
    ),
    Diagnostic(
        id="element_not_held",
        condition=(
            "A request other than of an address is served by a route through "
            "the element of a nullable layer that holds it in place, whose "
            "object holds no element."
        ),
        exception="type_not_convertible_exception",
        message="{type} holds no element",
        placeholders=("type",),
        fallback_carrier="element_not_held_carrier",
        reported=(BindingTime.RUNTIME,),
    ),
)

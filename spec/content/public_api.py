"""Content of the public API entries section.

The public API is what the documentation, the README, and the examples tell
users to include, name, specialize, or define. Everything reachable only
through dingo::detail or an undocumented header is internal and free to change.
The entries describe what each member is for; the C++ spellings, headers, and
usages are in cpp_api_bindings.py.
"""

from spec.schema import ApiKind, PublicApiEntry

INCLUDE = ApiKind.INCLUDE_PATH
NAME = ApiKind.NAME
EXTENSION = ApiKind.EXTENSION_POINT
MACRO = ApiKind.CONFIGURATION_MACRO


def entry(id: str, kind: ApiKind, description: str) -> PublicApiEntry:
    return PublicApiEntry(id=id, kind=kind, description=description)


public_api: tuple[PublicApiEntry, ...] = (
    # Include paths
    entry(
        "include_container",
        INCLUDE,
        "The combined container together with the registration vocabulary, "
        "keys, lookups, annotations, container traits, and exceptions.",
    ),
    entry("include_runtime_container", INCLUDE, "The runtime-only container."),
    entry("include_static_container", INCLUDE, "The compile-time-only container."),
    entry("include_scope_shared", INCLUDE, "The shared scope tag."),
    entry("include_scope_unique", INCLUDE, "The unique scope tag."),
    entry("include_scope_external", INCLUDE, "The external scope tag."),
    entry("include_scope_shared_cyclical", INCLUDE, "The shared cyclical scope tag."),
    entry("include_factory_constructor", INCLUDE, "The constructor factory."),
    entry("include_factory_function", INCLUDE, "The function factory."),
    entry("include_factory_callable", INCLUDE, "The callable factory."),
    entry(
        "include_constructor_annotation",
        INCLUDE,
        "The annotation that selects the constructor of a user type, which "
        "has no dependencies of its own.",
    ),
    entry("include_annotated", INCLUDE, "The annotated request and interface."),
    entry("include_lookup", INCLUDE, "The lookup definitions of container traits."),
    entry(
        "include_wrapper_traits",
        INCLUDE,
        "The wrapper traits that describe the semantics of a handle type.",
    ),
    entry(
        "include_conversion_traits",
        INCLUDE,
        "The conversion traits that build a target handle from a source handle.",
    ),
    entry(
        "include_collection_traits",
        INCLUDE,
        "The collection traits that describe how a collection is filled.",
    ),
    entry(
        "include_auto_constructible",
        INCLUDE,
        "The opt-in for constructing an unregistered type on resolution.",
    ),
    entry(
        "include_rebind",
        INCLUDE,
        "The exact lookup marker used by storage traits.",
    ),
    entry("include_rtti_typeid", INCLUDE, "The RTTI provider based on the language type information."),
    entry("include_rtti_static", INCLUDE, "The RTTI provider based on template specializations."),
    # Containers
    entry(
        "container",
        NAME,
        "The combined container: runtime registrations only, or compile-time "
        "bindings with optional runtime registrations, with an optional "
        "container traits type and allocator.",
    ),
    entry("runtime_container", NAME, "The movable runtime-only container."),
    entry(
        "static_container",
        NAME,
        "The compile-time-only container, with an optional container traits type.",
    ),
    entry(
        "container_parent",
        NAME,
        "A container that falls back to a parent container for the bindings it "
        "does not have, constructed from a pointer to the parent.",
    ),
    # Container operations
    entry(
        "register_type",
        NAME,
        "Registers a type at runtime from a scope, a storage, interfaces, a "
        "factory, and keys, with an optional initial value or factory argument.",
    ),
    entry(
        "register_type_collection",
        NAME,
        "Registers a collection that is filled from the registrations of its "
        "element interface, with an optional insertion function.",
    ),
    entry(
        "registration_proxy",
        NAME,
        "The result of a registration, which registers further types into the "
        "child container that belongs to it.",
    ),
    entry(
        "resolve",
        NAME,
        "Resolves a request, given as a type, with an optional runtime key or "
        "typed key.",
    ),
    entry(
        "construct",
        NAME,
        "Constructs an unregistered object and injects its dependencies, with "
        "an optional factory.",
    ),
    entry(
        "invoke",
        NAME,
        "Calls a callable with the arguments resolved from the container, with "
        "an optional signature that selects an overload.",
    ),
    # Registration vocabulary
    entry("bindings", NAME, "The list of compile-time bindings of a container."),
    entry("bind", NAME, "A compile-time binding built from the registration policies."),
    entry("scope", NAME, "The registration policy that selects the lifetime."),
    entry("storage", NAME, "The registration policy that selects the stored form."),
    entry("factory", NAME, "The registration policy that selects how the instance is created."),
    entry(
        "interfaces",
        NAME,
        "The registration policy that lists the types a registration can be resolved as.",
    ),
    entry(
        "dependencies",
        NAME,
        "The registration policy that declares the dependencies of a "
        "compile-time binding to the static graph.",
    ),
    # Keys and requests
    entry("key_type", NAME, "A key given by a type, optionally with a fixed value."),
    entry("key_value", NAME, "A runtime key value given at registration."),
    entry("annotated", NAME, "A request or interface distinguished by a tag type."),
    entry("dependency", NAME, "A constructor argument with a fixed key."),
    # Scopes
    entry("scope_shared", NAME, "Creates one instance and reuses it."),
    entry("scope_unique", NAME, "Creates a new instance for each resolution."),
    entry("scope_external", NAME, "Refers to an instance that lives outside the container."),
    entry(
        "scope_shared_cyclical",
        NAME,
        "Creates one instance in two phases, which allows cyclic graphs.",
    ),
    # Factory kinds
    entry("factory_constructor", NAME, "Creates an instance with a detected or an explicit constructor."),
    entry("factory_function", NAME, "Creates an instance by calling a function."),
    entry("factory_callable", NAME, "Creates an instance by calling a callable object."),
    # Lookups
    entry("lookups", NAME, "The list of lookup definitions of container traits."),
    entry(
        "lookup_associative",
        NAME,
        "A lookup by interface and runtime key, with a cardinality and a "
        "storage backend.",
    ),
    entry("lookup_typed", NAME, "A lookup by interface and typed key, with a cardinality."),
    entry("lookup_collection", NAME, "A lookup of the registrations of an interface without a key."),
    entry("cardinality_one", NAME, "The cardinality of a lookup that holds one binding per key."),
    entry("cardinality_many", NAME, "The cardinality of a lookup that holds many bindings per key."),
    entry("backend_ordered", NAME, "The lookup backend ordered by key."),
    entry("backend_unordered", NAME, "The lookup backend hashed by key."),
    entry("backend_array", NAME, "The lookup backend indexed by a dense integral or enumeration key."),
    # Container traits and RTTI
    entry("dynamic_container_traits", NAME, "The default container traits of runtime containers."),
    entry("static_container_traits", NAME, "The default container traits of static containers."),
    entry("rtti", NAME, "The RTTI of a provider, which the container traits select."),
    entry("rtti_typeid_provider", NAME, "The provider of the language type information."),
    entry("rtti_static_provider", NAME, "The provider that needs no language type information."),
    # Exceptions
    entry("exception", NAME, "The base of every exception of the library."),
    entry("type_not_found_exception", NAME, "No binding matches a request."),
    entry("type_not_convertible_exception", NAME, "A binding cannot produce the requested form."),
    entry("type_ambiguous_exception", NAME, "More than one binding matches a singular request."),
    entry("type_recursion_exception", NAME, "A dependency cycle was found while constructing."),
    entry("lookup_already_registered_exception", NAME, "A second registration collides with a lookup."),
    entry("type_index_out_of_range_exception", NAME, "A key is outside the range of an array lookup."),
    entry("virtual_pointer_exception", NAME, "A pointer cannot be adjusted through a virtual base."),
    # Names used by the extension points
    entry("type_list", NAME, "A list of types, which wrapper traits use to name element types."),
    entry("exact_lookup", NAME, "A marker for a result form that must match the wrapper exactly."),
    entry("access_borrow", NAME, "The access of a conversion that reads its source without consuming it."),
    entry("access_consume", NAME, "The access of a conversion that consumes its source."),
    # Extension points
    entry(
        "wrapper_traits",
        EXTENSION,
        "Makes a wrapper type a layer of the shape algebra: declares its layer "
        "kind, its family, and its element types, and how to rebind it, reach "
        "its element, make it, adopt an element it owns, share the element of "
        "a sole-owner handle, and tell which alternative a wrapper of a "
        "variadic kind holds, which a route through another does not reach.",
    ),
    entry(
        "layer_kind_traits",
        EXTENSION,
        "Makes a tag type a layer kind: declares its properties and its "
        "forms, each a route form with the kind it leads to, as the library "
        "declares its own kinds, such as the unique and the shared kind. A "
        "wrapper of the kind behaves as its properties and forms say.",
    ),
    entry(
        "conversion_traits",
        EXTENSION,
        "Builds a target wrapper from a source wrapper and declares the access "
        "the conversion needs.",
    ),
    entry(
        "collection_traits",
        EXTENSION,
        "Describes how a collection type is sized and filled.",
    ),
    entry(
        "auto_construction",
        EXTENSION,
        "Opts a type into construction on resolution when it is not registered.",
    ),
    entry(
        "constructor_annotation",
        EXTENSION,
        "Annotates a constructor of a user type so that it is selected as the "
        "constructor of the type.",
    ),
    entry(
        "container_traits",
        EXTENSION,
        "A traits type that selects the RTTI, the allocator, and the lookup "
        "definitions of a container.",
    ),
    entry(
        "scope_traits",
        EXTENSION,
        "Makes a tag type a scope: holds its descriptor, which may copy the "
        "descriptor of a built-in scope, and may name a slot class of its own.",
    ),
    entry(
        "scope_descriptor",
        NAME,
        "The descriptor of a scope, the fields of the spec's Scope schema.",
    ),
    entry(
        "scope_slot",
        EXTENSION,
        "The base of a slot class a scope names, which keeps the factory of the "
        "binding and constructs its stored object.",
    ),
    entry(
        "index_item",
        NAME,
        "A candidate of a request with the key value it is found by, which an "
        "index of a lookup backend takes.",
    ),
    entry(
        "lookup_backend",
        EXTENSION,
        "A lookup backend of the user, which names its own index of the key "
        "values of a domain, visiting the candidates of a key in the order of "
        "registration.",
    ),
    entry("rtti_provider", EXTENSION, "A user-defined provider of type information."),
    entry(
        "allocator",
        EXTENSION,
        "A user-supplied allocator for the internal data of a container.",
    ),
    # Configuration macros
    entry(
        "config_constructor_detection_args",
        MACRO,
        "The largest number of constructor arguments that detection tries.",
    ),
    entry(
        "config_context_arena_buffer_size",
        MACRO,
        "The size of the inline buffer of a resolution context.",
    ),
    entry(
        "config_runtime_arena_block_size",
        MACRO,
        "The size of the blocks of a runtime container arena.",
    ),
    entry("config_always_inline", MACRO, "The attribute that forces inlining."),
    entry("config_noinline", MACRO, "The attribute that prevents inlining."),
    entry(
        "config_empty_bases",
        MACRO,
        "The attribute that enables the empty base optimization for all bases.",
    ),
)

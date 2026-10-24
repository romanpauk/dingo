"""Types of the Dingo specification.

The specification is the language-agnostic source of truth described in
docs/architecture/spec-driven-design.md. Every entity has a snake_case `id`
that is unique within its section. The semantic sections (everything outside
`CppProjection`) contain no C++ text: type trees are abstract constructors,
scopes are descriptors, and routes are rules over type tree patterns. C++
spellings live only in `CppProjection`.

All types are frozen dataclasses and every collection is a tuple, so a
`Spec` is immutable, hashable, and exports to deterministic JSON.
"""

from dataclasses import dataclass
from enum import Enum

__all__ = [
    "Access",
    "ApiKind",
    "BehaviorChange",
    "BindingTime",
    "BindingTimeConstraint",
    "Bound",
    "BoundKind",
    "Capability",
    "Cardinality",
    "Combine",
    "Contract",
    "ContractDimension",
    "CppApiBinding",
    "CppContractProbe",
    "CppDiagnosticExample",
    "CppSemanticsTag",
    "CppProjection",
    "CppScopeProjection",
    "Decoration",
    "DependencyVisibility",
    "Diagnostic",
    "Domain",
    "EdgeTime",
    "Entity",
    "Exposure",
    "ExposureRule",
    "FactoryKind",
    "FilterDimension",
    "GraphRule",
    "IndexStructure",
    "KeyKind",
    "SemanticsOperation",
    "PropertyClass",
    "WrapperSemantics",
    "Property",
    "LookupDeclaration",
    "LookupFilter",
    "PatternForm",
    "Predicate",
    "PublicApiEntry",
    "RecursionGuard",
    "RequestCategory",
    "Operation",
    "OperationTemplate",
    "Requirement",
    "Rule",
    "Scope",
    "SelectionMode",
    "SelectionOutcome",
    "SelectionRule",
    "SelectionSource",
    "SelectionSourceTime",
    "SelectionStep",
    "TypeTreePattern",
    "Slot",
    "SourceAccess",
    "SourceTime",
    "Spec",
    "Status",
    "StepCondition",
    "StorageForm",
    "TransactionEffect",
    "TransactionRule",
    "ValueCategory",
    "VariantKind",
    "VariantPoint",
    "Via",
]


@dataclass(frozen=True, slots=True, kw_only=True)
class Entity:
    """Base of every named entity of the spec."""

    id: str


# Conversion rules


class Property(Enum):
    """Properties of a wrapper semantics that the rules and the storage forms
    refer to."""

    OWNING = "owning"
    SHARING = "sharing"
    NULLABLE = "nullable"
    BORROWABLE = "borrowable"
    # Holds its element in its own storage, as an optional or an array does,
    # rather than by a handle.
    IN_PLACE = "in_place"
    # Holds elements whose number is known only at runtime.
    UNSIZED = "unsized"
    # A default factory makes a wrapper from the arguments of its element.
    CONSTRUCTIBLE = "constructible"
    # A wrapper of the semantics is copied as a handle, whatever its element;
    # one that holds its element in place is copied with its elements unless it
    # is immovable.
    COPYABLE = "copyable"
    # A wrapper of the semantics is not a value a request takes, only an object
    # it refers to, as an array of the language is.
    IMMOVABLE = "immovable"
    # The semantics is mapped from a class template through an extension point
    # and has a family, rather than being a type of the language: a wrapper the
    # rules create may be of any family of its semantics.
    MAPPED = "mapped"


@dataclass(frozen=True, slots=True, kw_only=True)
class WrapperSemantics(Entity):
    """An abstract type tree constructor such as Val, Ptr, Own, or Array.

    `parameters` is the number of type tree arguments. `has_extent` marks a
    constructor that also takes a constant size.
    """

    description: str
    parameters: int = 0
    variadic: bool = False
    has_extent: bool = False
    traits: tuple[Property, ...] = ()
    # The forms of the kind, from which its routes are made.
    forms: tuple["SemanticsOperation", ...] = ()


# Scopes


class Slot(Enum):
    """The lifetime slot primitive a scope is stored in."""

    NONE = "none"
    LAZY = "lazy"
    TWO_PHASE = "two_phase"
    EXTERNAL_REF = "external_ref"


class ValueCategory(Enum):
    """The category of a stored value or of a reachable type a route derives.

    A scope materializes its stored value as an LVALUE, an object requests
    borrow, or an RVALUE, an object requests consume. Routes also derive
    PRVALUE reachable types: fresh values, such as copies and addresses, that serve value
    requests but never rvalue reference requests.
    """

    LVALUE = "lvalue"
    RVALUE = "rvalue"
    PRVALUE = "prvalue"


class SourceAccess(Enum):
    """How a route reads the stored source: shared, or used up."""

    BORROW = "borrow"
    CONSUME = "consume"


@dataclass(frozen=True, slots=True, kw_only=True)
class PropertyClass:
    """A class of wrappers by the properties of their kinds: the traits a kind
    has and lacks, and whether it is variadic or has an extent, None for
    either."""

    has: tuple["Property", ...] = ()
    lacks: tuple["Property", ...] = ()
    variadic: bool | None = None
    extent: bool | None = None


@dataclass(frozen=True, slots=True, kw_only=True)
class StorageForm(Entity):
    """A class of stored type_trees that storage exposure distinguishes.

    `outer` classifies the outer wrapper of a stored type_tree and `inner`, when
    given, the wrapper of its element; the most specific form a type_tree is of
    applies. The plain form, without a class, is a type_tree no other form is.
    """

    description: str
    outer: PropertyClass | None = None
    inner: PropertyClass | None = None


class RequestCategory(Enum):
    """The request lists a storage publishes for a form."""

    VALUE = "value"
    LVALUE = "lvalue"
    RVALUE = "rvalue"
    POINTER = "pointer"


class RecursionGuard(Enum):
    """When a recursive materialization of a storage is detected."""

    NONE = "none"
    WHILE_UNRESOLVED = "while_unresolved"
    ALWAYS = "always"


class ExposureRule(Enum):
    """How a scope restricts the routes of its exposed categories.

    FORM applies every route of the exposed categories. HANDLE_COPIES applies
    them too, except that a copy is made only of a type tree whose outer wrapper is
    sharing: the object of such a scope may be published before it is
    complete, so it is never copied, while a sharing handle to it is.
    """

    FORM = "form"
    HANDLE_COPIES = "handle_copies"


@dataclass(frozen=True, slots=True, kw_only=True)
class Exposure:
    """The request categories a scope publishes for one storage form."""

    form: str  # storage form id
    categories: tuple[RequestCategory, ...]


@dataclass(frozen=True, slots=True, kw_only=True)
class Scope(Entity):
    """A storage descriptor: slot, materialized category, stability, access.

    `exposures` lists every storage form the scope can store; a form that is
    not listed cannot be registered in the scope. Without `nested_wrappers`, the
    type_tree under the outer wrapper of a stored form must be a leaf.
    """

    description: str
    slot: Slot
    materializes: ValueCategory
    stable: bool
    source_access: SourceAccess
    guard: RecursionGuard = RecursionGuard.NONE
    retains_unresolved_source: bool = False
    exposure_rule: ExposureRule = ExposureRule.FORM
    exposures: tuple[Exposure, ...] = ()
    nested_wrappers: bool = True
    extends: str | None = None  # scope id
    phases: tuple[str, ...] = ()
    needs: tuple[str, ...] = ()


# Routes


class Access(Enum):
    """The access a route requires of its source."""

    INHERIT = "inherit"
    BORROW = "borrow"
    CONSUME = "consume"


class PatternForm(Enum):
    """The form of a node of a type tree pattern.

    VARIABLE binds any type tree (`name` is the variable). SEMANTICS applies the
    wrapper semantics `name` to `args`. WRAPPER applies a user wrapper variable
    `name` to its single argument. ROUTE stands for the type tree derived by
    routing its single argument again.
    """

    VARIABLE = "variable"
    SEMANTICS = "semantics"
    WRAPPER = "wrapper"
    ROUTE = "route"


@dataclass(frozen=True, slots=True, kw_only=True)
class TypeTreePattern:
    form: PatternForm
    name: str | None = None
    args: tuple["TypeTreePattern", ...] = ()
    category: ValueCategory | None = None


class Predicate(Enum):
    STABLE = "stable"
    COPYABLE = "copyable"
    BORROWABLE = "borrowable"
    # An array of the language: a fixed number of elements, which is not a
    # value a request takes, so a consumed one hands out its address.
    ARRAY = "array"
    ADDRESSABLE = "addressable"
    LEAF = "leaf"


@dataclass(frozen=True, slots=True, kw_only=True)
class Requirement:
    """A predicate over a pattern variable, used by `requires` and `where`."""

    predicate: Predicate
    subject: str


class Via(Enum):
    """The operation a route performs when it is not a structural rewrite.

    UPCAST replaces the leaf `L` of the derived type tree with each interface the
    binding declares for it. CONVERT does so for each declared interface the
    leaf declares a conversion to, a user conversion that makes the interface
    object from the leaf.
    """

    CONVERT = "convert"
    UPCAST = "upcast"


class Operation(Enum):
    """The forms of the routes of a wrapper, each executed by one operation of
    the traits of a wrapper of the kind. The forms, the rules of a leaf, and the
    value categories are the algebra of type_trees; a wrapper semantics, of the library or
    of the user, is a declaration of the forms it has."""

    # A borrowed wrapper reaches its element (element).
    BORROW = "borrow"
    # A consumed wrapper gives up its element (element).
    TAKE = "take"
    # A consumed element is wrapped into a wrapper of the kind (make).
    WRAP = "wrap"
    # A consumed wrapper of the target kind, a pointer, is adopted by a wrapper of
    # the kind (adopt).
    ADOPT = "adopt"
    # A consumed wrapper hands its element to a wrapper of the target kind, the
    # sharing wrapper of its family (share).
    SHARE = "share"
    # The address of a borrowed object is a wrapper of the kind, a pointer.
    ADDRESS = "address"
    # A borrowed wrapper decays to a wrapper of the target kind, a pointer to its
    # first element.
    DECAY = "decay"
    # A consumed wrapper hands out a wrapper of the target kind, a pointer to its
    # first element, whose ownership passes to the caller.
    RELEASE = "release"
    # A consumed array is handed out as a wrapper of the kind, a pointer, whose
    # ownership passes to the caller.
    RELEASE_WHOLE = "release_whole"
    # A copied wrapper converts to the wrapper of a declared interface (rebind).
    UPCAST_COPIED = "upcast_copied"
    # A consumed wrapper converts to the wrapper of a declared interface (rebind).
    UPCAST_CONSUMED = "upcast_consumed"
    # A borrowed wrapper converts to the wrapper of a declared interface, which
    # the binding keeps (rebind).
    RETAIN = "retain"


@dataclass(frozen=True, slots=True, kw_only=True)
class OperationTemplate(Entity):
    """The route of a form, whose patterns name the kind that has the form
    `K` and the kind it leads to `T`. `kind_traits` are the traits every kind
    of the form has, which the form relies on."""

    form: Operation
    from_type_tree: "TypeTreePattern"
    to_type_tree: "TypeTreePattern"
    access: "Access"
    exposure: "RequestCategory | None" = None
    requires: tuple["Requirement", ...] = ()
    via: "Via | None" = None
    cache: "TypeTreePattern | None" = None
    kind_traits: tuple["Property", ...] = ()


@dataclass(frozen=True, slots=True, kw_only=True)
class SemanticsOperation(Entity):
    """A form a wrapper semantics has: the route `id` of it, and the kind it leads
    to, for a form whose template names `T`."""

    form: Operation
    description: str
    target: str | None = None  # wrapper semantics id


@dataclass(frozen=True, slots=True, kw_only=True)
class Rule(Entity):
    """A route rule: a derivation from one type_tree pattern to another.

    The `category` of `from_type_tree` is the category of the reachable_type the rule
    applies to, and the `category` of `to_type_tree` that of the reachable_type it derives.
    `exposure` is the request category a scope must expose for the stored form
    for the rule to apply; a rule without one always applies.
    """

    description: str
    from_type_tree: TypeTreePattern
    to_type_tree: TypeTreePattern
    access: Access
    exposure: RequestCategory | None = None
    requires: tuple[Requirement, ...] = ()
    where: tuple[Requirement, ...] = ()
    via: Via | None = None
    cache: TypeTreePattern | None = None
    # The form a route of a wrapper semantics is of; None for a rule of a leaf.
    form: Operation | None = None


# Requests and keys


class BindingTime(Enum):
    COMPILE = "compile"
    RUNTIME = "runtime"


@dataclass(frozen=True, slots=True, kw_only=True)
class KeyKind(Entity):
    """A kind of lookup key. `value_time` is None when there is no value."""

    description: str
    value_time: BindingTime | None = None


@dataclass(frozen=True, slots=True, kw_only=True)
class Decoration(Entity):
    """A request decoration: keyed, annotated, selected, collection, ..."""

    description: str


# Lookups


class FilterDimension(Enum):
    INTERFACE = "interface"
    KEY_KIND = "key_kind"
    KEY_VALUE = "key_value"


@dataclass(frozen=True, slots=True, kw_only=True)
class LookupFilter(Entity):
    """The lookup semantics: a filter over the binding table."""

    description: str
    dimensions: tuple[FilterDimension, ...]


@dataclass(frozen=True, slots=True, kw_only=True)
class LookupDeclaration(Entity):
    """What a container declares about the bindings of an interface and a key
    domain: the key kinds the declaration covers and the cardinality it allows.

    A domain declared with cardinality one holds at most one binding, so a
    second registration is rejected; a domain declared many holds any number.
    A domain without a declaration has `default_cardinality` of the declaration
    that is the default (`default` is True).
    """

    description: str
    key_kinds: tuple[str, ...]  # key kind ids
    cardinalities: tuple["Cardinality", ...]
    default_cardinality: "Cardinality"
    default: bool = False


@dataclass(frozen=True, slots=True, kw_only=True)
class IndexStructure(Entity):
    """An acceleration that must return what its `filter` returns.

    It is chosen by the key kinds it serves and the lookup declarations that
    select it.
    """

    description: str
    filter: str  # lookup filter id
    key_kinds: tuple[str, ...]  # key kind ids
    declarations: tuple[str, ...] = ()  # lookup declaration ids


# Bindings and factories


class DependencyVisibility(Enum):
    DECLARED = "declared"
    DETECTED = "detected"


@dataclass(frozen=True, slots=True, kw_only=True)
class FactoryKind(Entity):
    description: str
    dependencies: DependencyVisibility


# Selection rules


class SelectionMode(Enum):
    SINGULAR = "singular"
    COLLECTION = "collection"
    # A request to construct an object, which a binding of its type answers
    # before the object is made by its factory.
    CONSTRUCT = "construct"


class SelectionSource(Enum):
    LOCAL_STATIC = "local_static"
    LOCAL_RUNTIME = "local_runtime"
    CONTAINER_STATIC = "container_static"
    CONTAINER_RUNTIME = "container_runtime"
    PARENT = "parent"
    AUTO_CONSTRUCT = "auto_construct"
    # The factory of a construct request: the detected constructor of its
    # type, or the factory the request names.
    FACTORY = "factory"
    # The bindings of the element type of a collection request at the level,
    # its local and container bindings, collected.
    ELEMENTS = "elements"


class Combine(Enum):
    FIRST_NON_EMPTY = "first_non_empty"
    CONCAT = "concat"


class StepCondition(Enum):
    # The request is of a type the container may auto-construct.
    CONSTRUCTIBLE = "constructible"
    # The request is of a type its factory can make.
    HAS_FACTORY = "has_factory"
    # The request is of a collection type, by value.
    COLLECTION = "collection"


class Cardinality(Enum):
    EMPTY = "empty"
    ONE = "one"
    MANY = "many"


class Status(Enum):
    OK = "ok"
    AMBIGUOUS = "ambiguous"
    NOT_FOUND = "not_found"
    AS_DECLARED = "as_declared"


@dataclass(frozen=True, slots=True, kw_only=True)
class SelectionStep:
    """The union of `sources`, taken only when `when` holds."""

    sources: tuple[SelectionSource, ...]
    when: StepCondition | None = None


@dataclass(frozen=True, slots=True, kw_only=True)
class SelectionOutcome:
    count: Cardinality
    status: Status


@dataclass(frozen=True, slots=True, kw_only=True)
class SelectionRule(Entity):
    description: str
    mode: SelectionMode
    combine: Combine
    steps: tuple[SelectionStep, ...]
    outcomes: tuple[SelectionOutcome, ...]


class SourceTime(Enum):
    """When the bindings a selection source names are known."""

    # Known when the request is compiled.
    COMPILE = "compile"
    # Registered at runtime: open while the level takes runtime registrations.
    RUNTIME = "runtime"
    # The bindings of the level, static and runtime: open while the level
    # takes runtime registrations.
    LEVEL = "level"
    # The selection of the parent level: known when it is decided when the
    # request is compiled.
    PARENT = "parent"


@dataclass(frozen=True, slots=True, kw_only=True)
class SelectionSourceTime(Entity):
    """When the bindings of `source` are known. A source that is open may name
    more bindings after the request is compiled."""

    description: str
    source: SelectionSource
    time: SourceTime


# Graph rules


class EdgeTime(Enum):
    """When the edges a check needs become known."""

    CONTAINER_FORMATION = "container_formation"
    RESOLUTION = "resolution"


class TransactionEffect(Enum):
    """An effect on a container that a failed transaction of it undoes."""

    # A binding registered into the container.
    REGISTRATION = "registration"
    # An object a binding of the container constructed and keeps.
    CONSTRUCTION = "construction"
    # An object the container keeps for a request, such as an auto-constructed
    # object a reference refers to.
    RETENTION = "retention"


@dataclass(frozen=True, slots=True, kw_only=True)
class TransactionRule(Entity):
    """A request that starts at a container is a transaction of it.

    A request of a container that is in progress, together with the requests
    it makes of the container, commits to the request that made it when it
    returns, and when it fails undoes the `undoes` effects it and they had on
    the container, in the reverse order they happened, whether or not the
    request that made it catches the failure. The effects on other
    containers, a parent or a child, belong to their own transactions.
    `operations` are the public operations that start one.
    """

    description: str
    operations: tuple[str, ...]
    undoes: tuple["TransactionEffect", ...]


@dataclass(frozen=True, slots=True, kw_only=True)
class GraphRule(Entity):
    """A cycle is legal if and only if the scope of every node has a slot in
    `legal_slots`: a property of the descriptor, so a user scope with such a
    slot is legal too."""

    description: str
    legal_slots: tuple[Slot, ...]
    checked_at: tuple[EdgeTime, ...]
    diagnostic: str  # diagnostic id


# Diagnostics


@dataclass(frozen=True, slots=True, kw_only=True)
class Diagnostic(Entity):
    """A diagnostic. `message` uses `{name}` placeholders listed in `placeholders`.

    `reported` lists when it is reported. RUNTIME throws `exception` when the
    condition arises as a program runs. COMPILE is a compile error, where the
    condition is decided when the program is compiled, see
    selection_sources: the placeholders then name the types the diagnostic is
    about, its subjects in placeholder order, and a tier without formatted
    compile errors shows the subjects as the arguments of `fallback_carrier`.
    A diagnostic reported only when compiled has no exception.
    """

    condition: str
    exception: str | None
    message: str
    placeholders: tuple[str, ...]
    fallback_carrier: str
    reported: tuple[BindingTime, ...] = (BindingTime.RUNTIME,)


# Binding-time constraints


class Domain(Enum):
    STATIC = "static"
    RUNTIME = "runtime"
    MIXED = "mixed"


@dataclass(frozen=True, slots=True, kw_only=True)
class BindingTimeConstraint(Entity):
    """A feature that is missing from `domain`, with its justification."""

    domain: Domain
    feature: str
    justification: str


# Variant points


class VariantKind(Enum):
    BINDING_TIME_DISPATCH = "binding_time_dispatch"
    LANGUAGE_TIER = "language_tier"
    COMPILER_VARIANT = "compiler_variant"
    INDEX_STRUCTURE = "index_structure"


@dataclass(frozen=True, slots=True, kw_only=True)
class VariantPoint(Entity):
    """A place where separate code is allowed.

    A language tier names its `capability` (a cpp capability id) and an index
    structure variant names its `index_structure`.
    """

    kind: VariantKind
    justification: str
    capability: str | None = None
    index_structure: str | None = None


# Contracts


class ContractDimension(Enum):
    COMPILE_COST = "compile_cost"
    OBJECT_SIZE = "object_size"
    RUNTIME_COST = "runtime_cost"
    CODE_SIZE = "code_size"
    INLINING = "inlining"
    HEADER_COST = "header_cost"


class BoundKind(Enum):
    """AT_MOST and EXACTLY take a `value`, LINEAR takes a `variable`.

    ALWAYS, NEVER, and COLD are the inlining bounds. CONSTANT means
    independent of the inputs. RECORDED means at most a limit that is recorded
    per probe and toolchain outside the spec, for bounds such as code size
    that differ per compiler.

    LINEAR means at most linear in its `variable`. A LINEAR bound may also
    carry a `value`, the most `unit` per unit of the variable, so that
    `LINEAR(value=1, unit="instantiations", variable="bindings")` reads at
    most one instantiation per binding.
    """

    AT_MOST = "at_most"
    EXACTLY = "exactly"
    CONSTANT = "constant"
    LINEAR = "linear"
    RECORDED = "recorded"
    ALWAYS = "always"
    NEVER = "never"
    COLD = "cold"


@dataclass(frozen=True, slots=True, kw_only=True)
class Bound:
    kind: BoundKind
    value: int | None = None
    unit: str | None = None
    variable: str | None = None


@dataclass(frozen=True, slots=True, kw_only=True)
class Contract(Entity):
    """The bound on one dimension of one operation or component.

    A compile cost contract names the internal `construct` whose
    instantiations it counts, such as a kernel; without one it counts all
    instantiations of the operation. Constructs and operations are abstract
    names, their C++ spellings are not part of the contract.
    """

    description: str
    operation: str
    dimension: ContractDimension
    bound: Bound
    domain: Domain | None = None
    construct: str | None = None


# Behavior changes


@dataclass(frozen=True, slots=True, kw_only=True)
class BehaviorChange(Entity):
    """An intended difference from the baseline library.

    `cases` are fnmatch patterns over case paths prefixed with the check that
    observes them: `routes:<domain>/<leaf>/<scope>/<stored>/<request>#<field>`
    for the route conformance check, `scopes:<scope>/<property>` for the scope
    check, `matrix:<executable>/<Suite>.<Name>` for the differential tests of
    the generated matrix, `api:<public API entry id>` for the public API
    check, `messages:<case id>` for the exception messages of the route
    cases, `selection:<case id>#<field>` for the selection conformance
    check, `test:<Suite>.<Name>` for a test of the next implementation that
    asserts a behavior no table observes, and `diagnostic:<id>` for the
    generated test of a diagnostic of such a behavior.
    """

    description: str
    cases: tuple[str, ...]
    baseline: str
    now: str
    decision: str


# Public API


class ApiKind(Enum):
    INCLUDE_PATH = "include_path"
    NAME = "name"
    EXTENSION_POINT = "extension_point"
    CONFIGURATION_MACRO = "configuration_macro"


@dataclass(frozen=True, slots=True, kw_only=True)
class PublicApiEntry(Entity):
    """A member of the public API. Its spelling is in the C++ projection."""

    kind: ApiKind
    description: str


# C++ projection


@dataclass(frozen=True, slots=True, kw_only=True)
class Capability:
    id: str  # snake_case
    description: str
    feature_macro: str | None = None  # C++ feature-test macro, e.g. "__cpp_pack_indexing"
    minimum: int | None = None  # required macro value as an integer, e.g. 202311
    builtin: str | None = None  # compiler builtin detected with __has_builtin
    header: str | None = None  # standard header that defines a library feature macro


@dataclass(frozen=True, slots=True, kw_only=True)
class CppScopeProjection(Entity):
    """The C++ tag of a scope and the header that declares it."""

    scope: str  # scope id
    spelling: str
    header: str


@dataclass(frozen=True, slots=True, kw_only=True)
class CppSemanticsTag(Entity):
    """The C++ tag of a wrapper semantics of the library, whose wrapper_semantics
    declare it as the semantics of a user's wrappers are declared."""

    semantics: str  # wrapper semantics id
    spelling: str


@dataclass(frozen=True, slots=True, kw_only=True)
class CppApiBinding(Entity):
    """The C++ spelling (path, name, or macro) of a public API entry.

    `header` is the public include path that makes a name or extension point
    available. `declarations` is namespace-scope code and `usage` the
    statements of a function body that use the name; the generated public API
    check compiles them.
    """

    api: str  # public API entry id
    spelling: str
    header: str | None = None
    declarations: str | None = None
    usage: str | None = None


@dataclass(frozen=True, slots=True, kw_only=True)
class CppContractProbe(Entity):
    """What observes a contract on the C++ implementation.

    `metrics` are fnmatch patterns over the flattened metric paths of
    test/contracts/baseline.json (`object_size/flat_resolved.static.32`,
    `codegen/probes/<probe>/dingo_calls`), and `excluding` removes paths from
    them. A contract that no current probe isolates sets `not_observable` to the
    reason instead.
    """

    contract: str  # contract id
    metrics: tuple[str, ...] = ()
    excluding: tuple[str, ...] = ()
    not_observable: str | None = None


@dataclass(frozen=True, slots=True, kw_only=True)
class CppDiagnosticExample(Entity):
    """A C++ program that a diagnostic reported when compiled rejects.

    `program` is the source after the `includes`; `subjects` spell the types
    the diagnostic names, in placeholder order, as its message prints them.
    The generator makes a lit test of the program for each language tier.
    """

    diagnostic: str  # diagnostic id
    includes: tuple[str, ...]
    program: str
    subjects: tuple[str, ...] = ()


@dataclass(frozen=True, slots=True, kw_only=True)
class CppProjection:
    capabilities: tuple[Capability, ...] = ()
    scope_projections: tuple[CppScopeProjection, ...] = ()
    semantics_tags: tuple[CppSemanticsTag, ...] = ()
    api_bindings: tuple[CppApiBinding, ...] = ()
    contract_probes: tuple[CppContractProbe, ...] = ()
    diagnostic_examples: tuple[CppDiagnosticExample, ...] = ()


# The spec


@dataclass(frozen=True, slots=True, kw_only=True)
class Spec:
    wrapper_semantics: tuple[WrapperSemantics, ...] = ()
    storage_forms: tuple[StorageForm, ...] = ()
    scopes: tuple[Scope, ...] = ()
    operations: tuple[OperationTemplate, ...] = ()
    rules: tuple[Rule, ...] = ()
    key_kinds: tuple[KeyKind, ...] = ()
    decorations: tuple[Decoration, ...] = ()
    lookup_filters: tuple[LookupFilter, ...] = ()
    lookup_declarations: tuple[LookupDeclaration, ...] = ()
    index_structures: tuple[IndexStructure, ...] = ()
    factory_kinds: tuple[FactoryKind, ...] = ()
    selection_rules: tuple[SelectionRule, ...] = ()
    selection_sources: tuple[SelectionSourceTime, ...] = ()
    transaction_rules: tuple[TransactionRule, ...] = ()
    graph_rules: tuple[GraphRule, ...] = ()
    diagnostics: tuple[Diagnostic, ...] = ()
    binding_time_constraints: tuple[BindingTimeConstraint, ...] = ()
    variant_points: tuple[VariantPoint, ...] = ()
    contracts: tuple[Contract, ...] = ()
    behavior_changes: tuple[BehaviorChange, ...] = ()
    public_api: tuple[PublicApiEntry, ...] = ()
    cpp: CppProjection = CppProjection()

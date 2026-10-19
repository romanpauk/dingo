"""Types of the Dingo specification.

The specification is the language-agnostic source of truth described in
docs/architecture/spec-driven-design.md. Every entity has a snake_case `id`
that is unique within its section. The semantic sections (everything outside
`CppProjection`) contain no C++ text: shapes are abstract constructors,
scopes are descriptors, and routes are rules over shape patterns. C++
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
    "CppProjection",
    "CppShapeProjection",
    "Decoration",
    "DependencyVisibility",
    "Diagnostic",
    "Domain",
    "EdgeTime",
    "Entity",
    "FactoryKind",
    "FilterDimension",
    "GraphRule",
    "IndexStructure",
    "KeyKind",
    "LayerKind",
    "LayerTrait",
    "LookupFilter",
    "PatternForm",
    "Predicate",
    "PublicApiEntry",
    "Requirement",
    "Route",
    "Scope",
    "SelectionMode",
    "SelectionOutcome",
    "SelectionRule",
    "SelectionSource",
    "SelectionStep",
    "ShapePattern",
    "Slot",
    "SourceAccess",
    "Spec",
    "Status",
    "StepCondition",
    "ValueCategory",
    "VariantKind",
    "VariantPoint",
    "Via",
]


@dataclass(frozen=True, slots=True, kw_only=True)
class Entity:
    """Base of every named entity of the spec."""

    id: str


# Shape algebra


class LayerTrait(Enum):
    """Properties of a layer kind that route rules and wrappers refer to."""

    OWNING = "owning"
    SHARING = "sharing"
    NULLABLE = "nullable"
    BORROWABLE = "borrowable"


@dataclass(frozen=True, slots=True, kw_only=True)
class LayerKind(Entity):
    """An abstract shape constructor such as Val, Ptr, Own, or Array.

    `parameters` is the number of shape arguments. `has_extent` marks a
    constructor that also takes a constant size.
    """

    description: str
    parameters: int = 0
    variadic: bool = False
    has_extent: bool = False
    traits: tuple[LayerTrait, ...] = ()


# Scopes


class Slot(Enum):
    """The lifetime slot primitive a scope is stored in."""

    NONE = "none"
    LAZY = "lazy"
    TWO_PHASE = "two_phase"
    EXTERNAL_REF = "external_ref"


class ValueCategory(Enum):
    """The category a stored value materializes as."""

    LVALUE = "lvalue"
    RVALUE = "rvalue"


class SourceAccess(Enum):
    """How a route reads the stored source: shared, or used up."""

    BORROW = "borrow"
    CONSUME = "consume"


@dataclass(frozen=True, slots=True, kw_only=True)
class Scope(Entity):
    """A storage descriptor: slot, materialized category, stability, access."""

    description: str
    slot: Slot
    materializes: ValueCategory
    stable: bool
    source_access: SourceAccess
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
    """The form of a node of a shape pattern.

    VARIABLE binds any shape (`name` is the variable). LAYER applies the layer
    kind `name` to `args`. WRAPPER applies a user wrapper variable `name` to
    its single argument. ROUTE stands for the shape derived by routing its
    single argument again.
    """

    VARIABLE = "variable"
    LAYER = "layer"
    WRAPPER = "wrapper"
    ROUTE = "route"


@dataclass(frozen=True, slots=True, kw_only=True)
class ShapePattern:
    form: PatternForm
    name: str | None = None
    args: tuple["ShapePattern", ...] = ()
    category: ValueCategory | None = None


class Predicate(Enum):
    STABLE = "stable"
    COPYABLE = "copyable"
    BORROWABLE = "borrowable"


@dataclass(frozen=True, slots=True, kw_only=True)
class Requirement:
    """A predicate over a pattern variable, used by `requires` and `where`."""

    predicate: Predicate
    subject: str


class Via(Enum):
    """The operation a route performs when it is not a structural rewrite."""

    CONVERT = "convert"


@dataclass(frozen=True, slots=True, kw_only=True)
class Route(Entity):
    """A route rule: a derivation from one shape pattern to another."""

    description: str
    from_shape: ShapePattern
    to_shape: ShapePattern
    access: Access
    requires: tuple[Requirement, ...] = ()
    where: tuple[Requirement, ...] = ()
    via: Via | None = None
    cache: ShapePattern | None = None


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
class IndexStructure(Entity):
    """An acceleration that must return what its `filter` returns.

    It is chosen by the key kinds it serves and the lookup declarations that
    select it.
    """

    description: str
    filter: str  # lookup filter id
    key_kinds: tuple[str, ...]  # key kind ids
    declarations: tuple[str, ...] = ()


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


class SelectionSource(Enum):
    LOCAL_STATIC = "local_static"
    LOCAL_RUNTIME = "local_runtime"
    CONTAINER_STATIC = "container_static"
    CONTAINER_RUNTIME = "container_runtime"
    PARENT = "parent"
    AUTO_CONSTRUCT = "auto_construct"


class Combine(Enum):
    FIRST_NON_EMPTY = "first_non_empty"
    CONCAT = "concat"


class StepCondition(Enum):
    CONSTRUCTIBLE = "constructible"
    MERGE_PARENT = "merge_parent"


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


# Graph rules


class EdgeTime(Enum):
    """When the edges a check needs become known."""

    CONTAINER_FORMATION = "container_formation"
    RESOLUTION = "resolution"


@dataclass(frozen=True, slots=True, kw_only=True)
class GraphRule(Entity):
    """A cycle is legal if and only if every node has a scope in `legal_scopes`."""

    description: str
    legal_scopes: tuple[str, ...]  # scope ids
    checked_at: tuple[EdgeTime, ...]
    diagnostic: str  # diagnostic id


# Diagnostics


@dataclass(frozen=True, slots=True, kw_only=True)
class Diagnostic(Entity):
    """A diagnostic. `message` uses `{name}` placeholders listed in `placeholders`."""

    condition: str
    exception: str
    message: str
    placeholders: tuple[str, ...]
    fallback_carrier: str


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
    independent of the inputs.
    """

    AT_MOST = "at_most"
    EXACTLY = "exactly"
    CONSTANT = "constant"
    LINEAR = "linear"
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
    """The bound on one dimension of one operation or component."""

    operation: str
    dimension: ContractDimension
    bound: Bound
    domain: Domain | None = None


# Behavior changes


@dataclass(frozen=True, slots=True, kw_only=True)
class BehaviorChange(Entity):
    """An intended difference from the baseline library.

    `cases` are fnmatch patterns over the differential case paths.
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
class CppShapeProjection(Entity):
    """The C++ spelling of a layer kind and the headers it needs."""

    layer: str  # layer kind id
    spelling: str
    headers: tuple[str, ...] = ()


@dataclass(frozen=True, slots=True, kw_only=True)
class CppApiBinding(Entity):
    """The C++ spelling (path, name, or macro) of a public API entry."""

    api: str  # public API entry id
    spelling: str


@dataclass(frozen=True, slots=True, kw_only=True)
class CppProjection:
    capabilities: tuple[Capability, ...] = ()
    shape_projections: tuple[CppShapeProjection, ...] = ()
    api_bindings: tuple[CppApiBinding, ...] = ()


# The spec


@dataclass(frozen=True, slots=True, kw_only=True)
class Spec:
    layer_kinds: tuple[LayerKind, ...] = ()
    scopes: tuple[Scope, ...] = ()
    routes: tuple[Route, ...] = ()
    key_kinds: tuple[KeyKind, ...] = ()
    decorations: tuple[Decoration, ...] = ()
    lookup_filters: tuple[LookupFilter, ...] = ()
    index_structures: tuple[IndexStructure, ...] = ()
    factory_kinds: tuple[FactoryKind, ...] = ()
    selection_rules: tuple[SelectionRule, ...] = ()
    graph_rules: tuple[GraphRule, ...] = ()
    diagnostics: tuple[Diagnostic, ...] = ()
    binding_time_constraints: tuple[BindingTimeConstraint, ...] = ()
    variant_points: tuple[VariantPoint, ...] = ()
    contracts: tuple[Contract, ...] = ()
    behavior_changes: tuple[BehaviorChange, ...] = ()
    public_api: tuple[PublicApiEntry, ...] = ()
    cpp: CppProjection = CppProjection()

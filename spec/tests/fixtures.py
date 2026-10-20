"""A small, fully populated valid spec for tests.

The shipped content leaves most sections empty, so the tests build their own
spec with one or more entities of every section and mutate it.
"""

from spec.schema import (
    Access,
    ApiKind,
    BehaviorChange,
    BindingTime,
    BindingTimeConstraint,
    Bound,
    BoundKind,
    Capability,
    Cardinality,
    Combine,
    Contract,
    ContractDimension,
    CppApiBinding,
    CppProjection,
    CppScopeProjection,
    Decoration,
    DependencyVisibility,
    Diagnostic,
    Domain,
    EdgeTime,
    Exposure,
    FactoryKind,
    FilterDimension,
    GraphRule,
    IndexStructure,
    KeyKind,
    LookupDeclaration,
    LayerKind,
    LayerTrait,
    LookupFilter,
    PatternForm,
    Predicate,
    PublicApiEntry,
    RequestCategory,
    Requirement,
    Route,
    Scope,
    SelectionMode,
    SelectionOutcome,
    SelectionRule,
    SelectionSource,
    SelectionStep,
    ShapePattern,
    Slot,
    SourceAccess,
    Spec,
    StorageForm,
    Status,
    StepCondition,
    ValueCategory,
    VariantKind,
    VariantPoint,
    Via,
)


def variable(name):
    return ShapePattern(form=PatternForm.VARIABLE, name=name)


def layer(name, *args):
    return ShapePattern(form=PatternForm.LAYER, name=name, args=args)


def sample_spec() -> Spec:
    return Spec(
        layer_kinds=(
            LayerKind(id="val", description="A value."),
            LayerKind(id="ptr", description="A pointer.", parameters=1),
            LayerKind(
                id="own",
                description="An owning wrapper.",
                parameters=1,
                traits=(LayerTrait.OWNING, LayerTrait.NULLABLE),
            ),
            LayerKind(id="alt", description="An alternative.", parameters=1, variadic=True),
            LayerKind(id="array", description="A sized array.", parameters=1, has_extent=True),
        ),
        storage_forms=(
            StorageForm(id="plain", description="The leaf.", pattern=variable("T")),
            StorageForm(
                id="unique_handle",
                description="An owning handle.",
                pattern=layer("own", variable("T")),
            ),
        ),
        scopes=(
            Scope(
                id="shared",
                description="Created once.",
                slot=Slot.LAZY,
                materializes=ValueCategory.LVALUE,
                stable=True,
                source_access=SourceAccess.BORROW,
                exposures=(
                    Exposure(
                        form="plain",
                        categories=(RequestCategory.VALUE, RequestCategory.LVALUE),
                    ),
                    Exposure(form="unique_handle", categories=(RequestCategory.LVALUE,)),
                ),
            ),
            Scope(
                id="shared_cyclical",
                description="Created in two phases.",
                slot=Slot.TWO_PHASE,
                materializes=ValueCategory.LVALUE,
                stable=True,
                source_access=SourceAccess.BORROW,
                extends="shared",
                exposures=(Exposure(form="plain", categories=(RequestCategory.LVALUE,)),),
                phases=("publish", "complete"),
                needs=("instance",),
            ),
        ),
        routes=(
            Route(
                id="address_of",
                description="Takes the address of a stable lvalue.",
                from_shape=ShapePattern(
                    form=PatternForm.VARIABLE, name="X", category=ValueCategory.LVALUE
                ),
                to_shape=layer("ptr", variable("X")),
                access=Access.BORROW,
                requires=(Requirement(predicate=Predicate.STABLE, subject="X"),),
            ),
            Route(
                id="deref",
                description="Dereferences a pointer.",
                from_shape=layer("ptr", variable("X")),
                to_shape=ShapePattern(
                    form=PatternForm.ROUTE, args=(variable("X"),)
                ),
                access=Access.INHERIT,
            ),
            Route(
                id="wrapper_borrow",
                description="Borrows from a wrapper.",
                from_shape=ShapePattern(
                    form=PatternForm.WRAPPER, name="W", args=(variable("X"),)
                ),
                to_shape=ShapePattern(form=PatternForm.ROUTE, args=(variable("X"),)),
                access=Access.INHERIT,
                where=(Requirement(predicate=Predicate.BORROWABLE, subject="W"),),
            ),
            Route(
                id="retain",
                description="Converts and caches.",
                from_shape=variable("S"),
                to_shape=variable("T"),
                access=Access.BORROW,
                via=Via.CONVERT,
                cache=variable("T"),
            ),
        ),
        key_kinds=(
            KeyKind(id="none", description="No key."),
            KeyKind(id="typed", description="A type key.", value_time=BindingTime.COMPILE),
            KeyKind(id="runtime", description="A runtime key.", value_time=BindingTime.RUNTIME),
        ),
        decorations=(Decoration(id="keyed", description="Carries a key."),),
        lookup_filters=(
            LookupFilter(
                id="binding_filter",
                description="Filters on interface, key kind, and key value.",
                dimensions=(
                    FilterDimension.INTERFACE,
                    FilterDimension.KEY_KIND,
                    FilterDimension.KEY_VALUE,
                ),
            ),
        ),
        lookup_declarations=(
            LookupDeclaration(
                id="ordered",
                description="Bindings by key value.",
                key_kinds=("typed", "runtime"),
                cardinalities=(Cardinality.ONE, Cardinality.MANY),
                default_cardinality=Cardinality.ONE,
                default=True,
            ),
        ),
        index_structures=(
            IndexStructure(
                id="ordered",
                description="An ordered index.",
                filter="binding_filter",
                key_kinds=("typed", "runtime"),
                declarations=("ordered",),
            ),
        ),
        factory_kinds=(
            FactoryKind(
                id="constructor",
                description="Constructs the type.",
                dependencies=DependencyVisibility.DETECTED,
            ),
        ),
        selection_rules=(
            SelectionRule(
                id="singular",
                description="First non-empty source.",
                mode=SelectionMode.SINGULAR,
                combine=Combine.FIRST_NON_EMPTY,
                steps=(
                    SelectionStep(
                        sources=(
                            SelectionSource.LOCAL_STATIC,
                            SelectionSource.LOCAL_RUNTIME,
                        )
                    ),
                    SelectionStep(sources=(SelectionSource.PARENT,)),
                    SelectionStep(
                        sources=(SelectionSource.AUTO_CONSTRUCT,),
                        when=StepCondition.CONSTRUCTIBLE,
                    ),
                ),
                outcomes=(
                    SelectionOutcome(count=Cardinality.ONE, status=Status.OK),
                    SelectionOutcome(count=Cardinality.MANY, status=Status.AMBIGUOUS),
                    SelectionOutcome(count=Cardinality.EMPTY, status=Status.NOT_FOUND),
                ),
            ),
        ),
        graph_rules=(
            GraphRule(
                id="cycle",
                description="Only cyclical scopes may form a cycle.",
                legal_scopes=("shared_cyclical",),
                checked_at=(EdgeTime.CONTAINER_FORMATION, EdgeTime.RESOLUTION),
                diagnostic="ambiguous_binding",
            ),
        ),
        diagnostics=(
            Diagnostic(
                id="ambiguous_binding",
                condition="A singular request matches more than one binding.",
                exception="type_ambiguous_exception",
                message="request {request} matches {count} bindings: {candidates}",
                placeholders=("request", "count", "candidates"),
                fallback_carrier="ambiguous_binding_carrier",
            ),
        ),
        binding_time_constraints=(
            BindingTimeConstraint(
                id="external_static",
                domain=Domain.STATIC,
                feature="external scope",
                justification="Needs a value that is only known at runtime.",
            ),
        ),
        variant_points=(
            VariantPoint(
                id="pack_at",
                kind=VariantKind.LANGUAGE_TIER,
                justification="Pack indexing is cheaper than the builtin.",
                capability="pack_indexing",
            ),
            VariantPoint(
                id="ordered_index",
                kind=VariantKind.INDEX_STRUCTURE,
                justification="Ordered keys.",
                index_structure="ordered",
            ),
            VariantPoint(
                id="dispatch",
                kind=VariantKind.BINDING_TIME_DISPATCH,
                justification="Static index or runtime thunk.",
            ),
        ),
        contracts=(
            Contract(
                id="resolve_static_allocations",
                description="A static resolution makes no allocation.",
                operation="resolve",
                dimension=ContractDimension.RUNTIME_COST,
                bound=Bound(kind=BoundKind.AT_MOST, value=0, unit="allocations"),
                domain=Domain.STATIC,
            ),
            Contract(
                id="resolve_static_inline",
                description="A static resolution is always inlined.",
                operation="resolve",
                dimension=ContractDimension.INLINING,
                bound=Bound(kind=BoundKind.ALWAYS),
                domain=Domain.STATIC,
            ),
            Contract(
                id="register_compile",
                description="Registering types grows linearly in the bindings.",
                operation="register_type",
                dimension=ContractDimension.COMPILE_COST,
                bound=Bound(kind=BoundKind.LINEAR, variable="bindings"),
            ),
        ),
        behavior_changes=(
            BehaviorChange(
                id="mixed_ambiguity",
                description="Static ambiguity in a mixed container.",
                cases=("mixed/*",),
                baseline="throws",
                now="compile error",
                decision="Decision Log",
            ),
        ),
        public_api=(
            PublicApiEntry(id="container_header", kind=ApiKind.INCLUDE_PATH, description="The container."),
            PublicApiEntry(id="shared_ptr_shape", kind=ApiKind.NAME, description="A name."),
        ),
        cpp=CppProjection(
            capabilities=(
                Capability(
                    id="pack_indexing",
                    description="Pack indexing.",
                    feature_macro="__cpp_pack_indexing",
                    minimum=202311,
                ),
            ),
            scope_projections=(
                CppScopeProjection(
                    id="shared_tag",
                    scope="shared",
                    spelling="dingo::shared",
                    header="dingo/storage/shared.h",
                ),
                CppScopeProjection(
                    id="shared_cyclical_tag",
                    scope="shared_cyclical",
                    spelling="dingo::shared_cyclical",
                    header="dingo/storage/shared_cyclical.h",
                ),
            ),
            api_bindings=(
                CppApiBinding(
                    id="container_header_path",
                    api="container_header",
                    spelling="dingo/container.h",
                ),
                CppApiBinding(
                    id="shared_ptr_shape_name",
                    api="shared_ptr_shape",
                    spelling="std::shared_ptr",
                    header="dingo/container.h",
                    usage="{name}<int> pointer;",
                ),
            ),
        ),
    )

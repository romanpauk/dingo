"""Axis definitions for matrix test generation."""

from .containers import CONTAINERS
from .dependency_forms import (
    DEPENDENCY_CARRIERS,
    DEPENDENCY_DECORATIONS,
    DEPENDENCY_FORMS,
    DEPENDENCY_PROVISIONINGS,
    DEPENDENCY_SHAPES,
)
from .exposed_types import EXPOSED_TYPES
from .feature_cases import FEATURE_CASES
from .features import FEATURES
from .modes import REGISTRATION_MODES
from .resolved_types import RESOLVED_TYPES
from .scopes import SCOPES
from .stored_types import STORED_TYPES

__all__ = (
    "CONTAINERS",
    "DEPENDENCY_CARRIERS",
    "DEPENDENCY_DECORATIONS",
    "DEPENDENCY_FORMS",
    "DEPENDENCY_PROVISIONINGS",
    "DEPENDENCY_SHAPES",
    "EXPOSED_TYPES",
    "FEATURES",
    "FEATURE_CASES",
    "REGISTRATION_MODES",
    "RESOLVED_TYPES",
    "SCOPES",
    "STORED_TYPES",
)

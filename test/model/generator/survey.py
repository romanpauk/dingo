"""Asks the library which interfaces each registration binds."""

import sys

from . import driver
from .model import interface


def survey(compiler, flags, work, registrations):
    """The interfaces each registration binds, by its survey key; a
    registration the library rejects binds none."""
    keyed = {}
    for registration in registrations:
        keyed.setdefault(registration.survey_key, registration)
    source = driver.render("survey.cpp.j2",
                           registrations=list(keyed.values()))
    records, stderr, crash = driver.build_and_run(compiler, flags, work,
                                                  "survey", source)
    if stderr is not None or crash is not None:
        sys.exit(f"the survey failed: {stderr or crash}")
    bound = {}
    for record in records:
        bound.setdefault(record["registration"], []).append(
            interface(record["interface"], record["abstract"],
                      record["array"]))
    return {key: tuple(found) for key, found in bound.items()}

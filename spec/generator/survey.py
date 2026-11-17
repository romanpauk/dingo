"""Checks whether each registration binds, in the library, the interfaces the
specification defines."""

import sys

from . import driver


def survey(compiler, flags, work, registrations):
    """The registrations the library accepts that bind other interfaces than
    the specification defines."""
    keyed = {}
    for registration in registrations:
        keyed.setdefault(registration.survey_key, registration)
    source = driver.render(
        "survey.cpp.j2", registrations=list(keyed.values()),
        headers=driver.program_headers(
            {registration.scope for registration in keyed.values()}))
    records, stderr, crash = driver.run_program(compiler, flags, work,
                                                  "survey", source)
    if stderr is not None or crash is not None:
        sys.exit(f"the survey failed: {stderr or crash}")
    return [keyed[record["registration"]] for record in records
            if not record["binds"]]

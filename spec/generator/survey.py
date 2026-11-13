"""Asks the library whether each registration binds the interfaces the
specification expects."""

import sys

from . import driver


def survey(compiler, flags, work, registrations):
    """The registrations the library accepts that bind other interfaces than
    the specification expects."""
    keyed = {}
    for registration in registrations:
        keyed.setdefault(registration.survey_key, registration)
    source = driver.render("survey.cpp.j2",
                           registrations=list(keyed.values()))
    records, stderr, crash = driver.build_and_run(compiler, flags, work,
                                                  "survey", source)
    if stderr is not None or crash is not None:
        sys.exit(f"the survey failed: {stderr or crash}")
    return [keyed[record["registration"]] for record in records
            if not record["binds"]]

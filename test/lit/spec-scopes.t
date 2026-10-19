# Checks that the scope descriptors of the spec match the storage scope policies
# of the current implementation. See test/spec/scopes_check.py.
# REQUIRES: linux, can-run-built-executables
# RUN: %python %dingo_project_root/test/spec/scopes_check.py --cxx-command '%dingo_cxx' --work-dir %t

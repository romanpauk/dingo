# Compiles the public API recorded in the spec against the next
# implementation, which provides the target API: the entries a behavior change
# declares as well. See docs/development.md.
# REQUIRES: linux
# RUN: %python %dingo_project_root/test/spec/public_api_check.py --implementation next --cxx-command '%dingo_next_cxx' --work-dir %t
# RUN: %python -c "print('dingo lit')" | %filecheck %s
# CHECK: dingo lit

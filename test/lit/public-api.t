# Compiles the public API recorded in the spec: every include path alone, and
# every public name, extension point, and configuration macro. See
# docs/development.md.
# REQUIRES: linux
# RUN: %python %dingo_project_root/test/spec/public_api_check.py --cxx-command '%dingo_cxx' --work-dir %t
# RUN: %python -c "print('dingo lit')" | %filecheck %s
# CHECK: dingo lit

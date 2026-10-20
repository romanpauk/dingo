# Checks the contract baseline of the next implementation recorded in
# test/contracts/baseline-next.json for the toolchain under test. See
# docs/development.md#contract-baseline.
# REQUIRES: linux, can-run-built-executables
# RUN: %python %dingo_project_root/test/contracts/probe.py check --implementation next --cxx-command '%dingo_cxx' --work-dir %t

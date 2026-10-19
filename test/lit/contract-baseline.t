# Checks the contract baseline recorded in test/contracts/baseline.json for the
# toolchain under test. See docs/development.md#contract-baseline.
# REQUIRES: linux, can-run-built-executables
# RUN: %python %dingo_project_root/test/contracts/probe.py check --cxx-command '%dingo_cxx' --work-dir %t

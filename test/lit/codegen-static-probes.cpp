// REQUIRES: linux
// RUN: %python %dingo_project_root/test/matrix/generate_probes.py --source %t.cpp --expectations %t.expectations.py
// RUN: %dingo_cxx -O2 -c %t.cpp -o %t.o
// RUN: nm -S --size-sort %t.o | c++filt > %t.nm
// RUN: %python %dingo_lit_root/check_codegen_probe_sizes.py %t.nm %t.expectations.py
// RUN: objdump -d --no-show-raw-insn %t.o | c++filt > %t.objdump
// RUN: %python %dingo_lit_root/check_codegen_probe_instructions.py %t.objdump %t.expectations.py

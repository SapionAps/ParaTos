# POSIX host backend

Linux and macOS use the same sources in this directory. `main.c` owns the
command-line entry point and POSIX signal recovery. `executable.c` discovers
the native emulator path using `/proc/self/exe` on Linux and
`_NSGetExecutablePath()` on macOS, for recursive execution of 68k programs.
`memory.c` reserves and protects the guest address space through `mmap` and
`mprotect`.

The remaining files implement native GEMDOS adapters: console/terminal I/O,
time, filesystem access and path translation, processes, signals, system
queries and errno conversion. They translate between native structures and
the guest ABI defined in `gemdos/gemdos.h`. Guest arguments and 8.3 names
are decoded in `gemdos/` before native execution or filesystem traversal.

The shared PRG loader receives the environment explicitly from native startup.
Guest memory and endian helpers use no OS headers. Backend-specific includes
and `_GNU_SOURCE` are local to the POSIX executable; native pTOS builds select
`host/ptos` without compiling this directory.

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^posix_memory$' --output-on-failure
ctest --test-dir build -R '^posix_launch$' --output-on-failure
```

`posix_memory` checks guest byte order, memory access and basepage state.
`posix_launch` requires Python 3 and generates ordinary 68000 PRGs with
relocations, without a cross compiler. It checks arguments, environment,
status propagation and nested Pexec through a renamed emulator with a
deliberately unrelated native `argv[0]`. Linux additionally checks the existing
SIGSEGV recovery and guest diagnostics. All test fixtures are temporary.

The Linux/macOS CI matrix runs these checks; pTOS ARM/x32 boot tests and the
existing compiled POSIX syscall regressions remain in the same workflow.

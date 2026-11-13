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

## Pexec and guest threads

| Mode | POSIX behavior |
| --- | --- |
| 0 | Start a native program or another ParaTos process and wait for its status. |
| 3 | Load and relocate a TOS PRG into this process's guest memory; return its basepage. Native executables are rejected. |
| 4 | Run the basepage in argument 2 on a host thread and wait; retain its image and environment for the caller. |
| 5 | Create a basepage/TPA and copy the command tail and environment; the caller supplies an entry point before using mode 4/6/104. Argument 1 is ignored. |
| 6 | Run a loaded basepage on a host thread and wait; transfer its image/environment ownership to the child and release them at termination. |
| 100 | Start as for mode 0 and return a child ID immediately, without waiting. |
| 104 | Run as for mode 4 and return a guest thread ID immediately. The caller retains the image/environment. |
| 200 | Replace the host process. |

Modes 3 and 5 allocate a separate environment block and the largest available
TPA, as on TOS. A caller must shrink its own TPA before loading children. A
program that shrinks its TPA must also move its stack into the retained area.
The child gets its own basepage, default DTA, command tail, copied environment
and parent-basepage link. A zero environment argument inherits the caller's
environment. Mode 6 executes the PRG already loaded into the supplied basepage;
it does not start another emulator process.

Musashi's CPU registers, cycle counters, tracing/address-error state and
callbacks are thread local, as is `current_process`. Opcode tables are shared
and initialized with `pthread_once`. Guest execution is serialized in bounded
CPU slices to protect shared guest RAM and allocator/runtime lists. Blocking
reads, writes, polling, child-process waits and thread joins release the
execution mutex so another guest thread can run.

`Pterm` and `Pterm0` end only the calling guest thread. The host waits for
remaining guest threads before returning the initial guest's status. Owned
allocations remain alive while descendant threads need them. Standard streams
are duplicated per execution thread, so a child's stdio cleanup cannot close
its parent's streams. Other descriptors and the working directory are shared
by threads. Modes 0/100 remain separate host processes. `Pfork`/`Pvfork` are
rejected while guest threads exist; duplicating a live threaded runtime through
fork without exec is unsupported.

The host catches SIGPIPE so a guest write to a pipe with no readers returns
`TOS_EPIPE` instead of terminating all guests. This also protects host diagnostic
writes. The disposition is caught rather than ignored: successful native exec
automatically restores the default disposition, while a failed replacement
leaves the runtime protected. This is host isolation, not emulation of guest
SIGPIPE handlers (`Psigaction` remains unimplemented).

`Pwait`, `Pwait3` and `Pwaitpid` can reap mode 100 processes and mode 104 guest
threads. IDs share a positive 16-bit namespace; nonblocking waits return zero
while a child is running. Guest-thread rusage currently reports zero.

The root guest's ID stays reserved for the entire runtime lifetime, including
while nested threads allocate their own identities. When a guest terminates,
its unclaimed native children transfer to a central reaper. Completed native
statuses are collected before their registry entries and IDs are released,
even if every surviving guest is blocked on I/O. Pfork remains unavailable
while that reaper is active; its finished thread is joined before permitting
fork again. At host shutdown, still-running native children may outlive the
emulator and are adopted by the OS rather than delaying mode-100 termination.

The generated `posix_pexec` fixtures cover split execution, shared-memory
effects, CPU/TLS and DTA/environment isolation, ownership and repeated cleanup,
nonblocking and wait-any operation, pipe I/O, malformed PRGs and descendant
lifetimes. Run all suites or just these fixtures:

```sh
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^posix_pexec$' --output-on-failure
```

Native child records carry the creating execution's identity. Synchronous
children are reserved for their own Pexec caller; guest waits poll only eligible
native PIDs and never use a host-wide `wait4(-1)`. Registry capacity is reserved
before fork. A close-on-exec pipe reports native launch errors separately from
normal exit status 127. Pfork propagates its reserved guest identity to the child.
Automatically launched ParaTos processes receive their reserved identity through
internal launcher arguments, consumed before constructing the guest command tail.
Blocking FIFO opens release the execution mutex so a peer guest can open the
other end.

Basepage allocations are explicitly tagged. Retired basepages cannot be reused
until descendants finish. Terminal mode sessions have a separate mutex, and
blocking I/O uses host staging buffers so guest RAM is accessed only while the
execution mutex is held. Environment scans are bounded before argument parsing;
fault diagnostics have their own recovery point.

Executable probes inspect regular files only, with nonblocking opens and a
descriptor type check so FIFO/device paths cannot rendezvous under the execution
mutex. Pause/Psigpause release that mutex while waiting for signals. Host trap,
warning and debug output is formatted into host-owned buffers before writing
without the execution mutex. Fault reports are likewise captured in memory while
CPU/disassembler state is stable, then emitted unlocked; a guest can therefore
drain a full diagnostic-output pipe while another guest reports a fault.

The initial guest's active basepage is also protected from concurrent restart.
Its stream mappings are closed before waiting for surviving guest threads, so
redirected pipe readers can observe EOF. Pending host diagnostics are flushed
first. PRG loading validates declared symbol bytes for absolute as well as
relocatable images; negative F_DUPFD minima remain invalid and cannot allocate
unexpected descriptors.

Linux additionally runs `posix_pid_registry` (allocation-failure injection and
child ownership/reservation) and `posix_io_staging` (buffer isolation while I/O
releases the execution mutex):

```sh
ctest --test-dir build -R '^posix_(pid_registry|io_staging)$' --output-on-failure
```

The Linux/macOS CI matrix runs these checks; pTOS ARM/x32 boot tests and the
existing compiled POSIX syscall regressions remain in the same workflow.

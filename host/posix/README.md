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

`bios.c` implements BIOS `Bconin(2)` using standard input. Terminal reads
wait for one byte without echo, preserve queued input, and restore the terminal
settings afterward. Redirected input is also supported; EOF is exposed as
Ctrl-D (ASCII 4), since BIOS has no EOF result. Returned characters are unsigned
bytes with zero scan-code and modifier bits; Atari special-key scan codes are
not synthesized. Other Bconin devices return `TOS_ENOSYS`.

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

## Pexec and guest processes

| Mode | POSIX behavior |
| --- | --- |
| 0 | Fork and exec a native program or another ParaTOS process, then wait for its status. |
| 3 | Load and relocate a TOS PRG into guest RAM and return its basepage. |
| 4 | Fork a child to run the staged basepage in shared guest RAM and wait. The caller retains the image and environment. |
| 5 | Create a basepage and copy the command tail and environment. The caller supplies the entry point. |
| 6 | Run as for mode 4 and transfer image and environment ownership to the child. |
| 100 | Fork and exec as for mode 0, returning a child ID for Pwait. |
| 104 | Fork as for mode 4, returning a child ID for Pwait. |
| 106 | Fork as for mode 6, returning a child ID for Pwait. |
| 200 | Replace the host process with execve. Return ENOSYS while staged guests still need this process to manage their lifetimes. |

Modes 3 and 5 allocate a separate environment block and the largest available
TPA. The caller must shrink its own TPA and move its stack into the retained
area before loading children. A zero environment argument inherits the caller's
environment.

Guest RAM starts private. The first staged launch copies the accessible RAM
windows and allocator state into a shared anonymous mapping. Guest addresses
and guard gaps are preserved. The child inherits that mapping at fork, with a
fresh emulated CPU context and its own host descriptor table and current
directory. Further staged launches within the same family reuse the mapping.

A process lock serializes guest CPU slices, allocation and child publication.
Blocking host I/O and waits release it. On private RAM, Pfork uses the host's
copy-on-write fork path. On shared RAM, the Pfork child takes a private snapshot
while the parent holds the lock, then acknowledges it before the parent reports
success. Pvfork uses the same behavior. Fork/exec launches and successful
replacement discard the old mapping. A fresh ParaTOS instance starts private.

The host catches SIGPIPE so a guest write to a pipe with no readers returns
`TOS_EPIPE` instead of terminating all guests. This also protects host diagnostic
writes. The disposition is caught rather than ignored: successful native exec
automatically restores the default disposition, while a failed replacement
leaves the runtime protected. This is host isolation, not emulation of guest
SIGPIPE handlers (`Psigaction` remains unimplemented).

Pwait, Pwait3 and Pwaitpid collect staged and native children. Positive guest
PIDs share a namespace within a staged family. A nonblocking wait returns zero
while an eligible child is running. Staged waits currently report zero rusage.

Pterm ends the calling guest and closes its descriptors. The initial guest
waits for surviving staged guests before returning its status. Owned
allocations remain alive while descendants can still reach the parent's
image. An exited staged guest can remain as the host parent of its native
children until it has reaped them. Its guest exit status is already available
to its caller.

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
`--paratos-child PID PPID`, followed by the shared PRG launch options. The child
restores the original 128-byte command tail and any explicit environment in its
basepage. For inherited variables, it builds `ARGV` from the arguments after `--`.
Large explicit environments use repeated Base64 environment options, sized to fit
individual host arguments. The host's aggregate `execve()` limit still applies.
The option layout is documented in [the native backend README](../ptos/README.txt).
Blocking FIFO opens release the family lock so a peer guest can open the
other end.

Basepage allocations are explicitly tagged. Retired basepages cannot be reused
until descendants finish. Terminal mode sessions have a separate lock, and
blocking I/O uses host staging buffers so guest RAM is accessed only while the
family lock is held. Environment scans are bounded before argument parsing;
fault diagnostics have their own recovery point.

Executable probes inspect regular files only, with nonblocking opens and a
descriptor type check so FIFO/device paths cannot rendezvous under the execution
mutex. Pause/Psigpause release that mutex while waiting for signals. Host trap,
warning and debug output is formatted into host-owned buffers before writing
without the family lock. Fault reports are likewise captured in memory while
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
releases the family lock):

```sh
ctest --test-dir build -R '^posix_(pid_registry|io_staging)$' --output-on-failure
```

The Linux/macOS CI matrix runs these checks; pTOS ARM/x32 boot tests and the
existing compiled POSIX syscall regressions remain in the same workflow.

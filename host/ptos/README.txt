pTOS backend (issue #7)
======================

Status
------
This is a native pTOS CLI backend for ARM and x32, not the POSIX executable
renamed to .prg. AES remains a follow-up. An optional x32 userspace VDI adapter
supports the operations documented in ../../vdi/README.md. The POSIX backend is unchanged.
Architecture-specific syscall implementations live in host/ptos/arm/ and
host/ptos/x32/; loading, guest memory and process emulation are shared.

Requires pTOS #946's high-image/low-RAM support. Direct 68k execution and
the real-kernel tests also require pTOS #944's binfmt/shebang support,
merged as fc3148179494f1b507ed38ee4c328ad52624fecf:
https://github.com/kelihlodversson/pTOS/pull/949
The native backend is boot-tested on both ARM and x32.

For direct execution, place the native emulator at BIN/PARATOS.PRG on the
boot drive and add this section to PTOS.INI in that drive's root:

  [binfmt.tos68k]
  magic=601A
  interpreter=/BIN\PARATOS.PRG

The kernel reads PTOS.INI at boot. Both path separators are accepted;
the root-relative interpreter path uses the boot drive even when a caller
changes its working directory. Native ELF programs retain native dispatch.

In particular, linking high is insufficient: the native basepage,
environment and startup mappings must also leave guest RAM available.
There is deliberately no fallback to a low native image or relocated guest
RAM. main() refuses low relocation, and guest mapping uses FIXED_NOREPLACE.

Build
-----
Requires CMake >= 3.13, the pTOS fork of libcmini, and GCC/binutils for the
selected target: x32-capable native GCC or arm-none-eabi-gcc for ARM.
Set LIBCMINI to an absolute path. The startup and archive must match the
target architecture.
The libcmini build changes its ignored .config; do not build different
libcmini architectures concurrently in one checkout. Library outputs stay
in separate directories below build/.

  LIBCMINI=/absolute/path/to/libcmini
  mkdir -p build/ptos/libcmini
  make -C "$LIBCMINI" BUILDDIR="$PWD/build/ptos/libcmini" \
      CROSSPREFIX= ptos_x86_64_defconfig
  make -C "$LIBCMINI" BUILDDIR="$PWD/build/ptos/libcmini" \
      CROSSPREFIX= ARCH_CFLAGS="-march=x86-64 -fno-pie -fno-stack-protector -fcf-protection=none -fno-unwind-tables -fno-asynchronous-unwind-tables" \
      -j4 dirs libs startups
  cmake -S . -B build/ptos/native \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/ptos-x32.cmake" \
      -DPTOS_LIBCMINI_ROOT="$LIBCMINI" \
      -DPTOS_LIBCMINI_ARCHIVE="$PWD/build/ptos/libcmini/libcmini.a" \
      -DPTOS_CRT0="$PWD/build/ptos/libcmini/objs/crt0.o" \
      -DCMAKE_BUILD_TYPE=Release
  cmake --build build/ptos/native -j4
  readelf -h -l build/ptos/native/host/ptos/paratos.prg

This builds an ELFCLASS32 / EM_X86_64 executable using the x32 ABI.
The explicit x86-64 instruction baseline is required even when the build
host compiler defaults to x86-64-v3: pTOS preserves x87/SSE, not AVX state.
The library must also disable host stack-protector/TLS instrumentation.
Use fresh build directories when changing compiler or ABI flags.

For ARM, use a separate libcmini build and the ARM toolchain file:

  mkdir -p build/ptos/arm-libcmini
  make -C "$LIBCMINI" BUILDDIR="$PWD/build/ptos/arm-libcmini" \
      ptos_arm_defconfig
  make -C "$LIBCMINI" BUILDDIR="$PWD/build/ptos/arm-libcmini" \
      -j4 dirs libs startups
  cmake -S . -B build/ptos/arm-native \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/ptos-arm.cmake" \
      -DPTOS_LIBCMINI_ROOT="$LIBCMINI" \
      -DPTOS_LIBCMINI_ARCHIVE="$PWD/build/ptos/arm-libcmini/libcmini.a" \
      -DPTOS_CRT0="$PWD/build/ptos/arm-libcmini/objs/crt0.o" \
      -DCMAKE_BUILD_TYPE=Release
  cmake --build build/ptos/arm-native -j4
  arm-none-eabi-readelf -h -l build/ptos/arm-native/host/ptos/paratos.prg

The ARM result is ELFCLASS32 / EM_ARM, EABI5, VFP hard-float, compiled for
ARMv6 in ARM mode (Pi 1 and newer ARM targets). It uses word relocations
rather than MOVW/MOVT relocation pairs. The GEMDOS bridge passes the function
in r0 and up to six arguments in r1-r6 through SVC #1.

Both executables have PT_LOAD segments starting at 0x41000000, above 1 GiB.
Relocations are retained for the native pTOS ELF loader; this does not
authorize relocating the native program into guest RAM.

Invoke it on pTOS as:

  paratos.prg program.prg [arguments...]

Boot-configured binfmt/shebang dispatch is a separate feature:
https://github.com/kelihlodversson/pTOS/issues/944

Guest memory and privileges
---------------------------
  00000000..0000ffff  private emulated low-memory shadow, not host address 0
  00010000..00ffffff  private identity-mapped guest RAM, host RW, not executable
  41000000 upward    native emulator image (native heap/stack follow host ABI)

The first 64 KiB of mapped guest RAM (00010000..0001ffff) is reserved for
the emulated supervisor stack. The guest heap starts at 00020000, also
reported by membot. This lets Supexec callbacks pass their stack buffers
to native file/console calls while preserving the low-memory shadow rules.

No TT-RAM or Atari hardware/MMIO mapping is supplied. Unavailable guest
ranges fault rather than falling through into native image/heap/stack
memory. A fatal guest fault ends the emulation run with status 255.

Live scalar low-memory fields use native Ssystem() on every read, including
byte/word reads within a wider field and reads crossing field boundaries.
Native Ssystem read-mode support is checked during initialization. The known
32-bit counters preserve every bit pattern, including values that coincide
with negative error codes at wraparound; narrow read errors remain errors.
Guest memory limits, vectors and the cookie jar are guest-local. Native
kernel pointers and native CPU cookies are not exposed. Exact-width writes
to live fields use Ssystem and honor its access checks; partial live writes
are refused. The guest S_GET*VAL/S_SET*VAL calls use the same memory layer.
S_OSFEATURES describes the flat guest, without advertising native MMU/ELF
services. Sysconf physical-page totals describe guest RAM; guest free-page
accounting is unavailable (ENOSYS), rather than reporting host free pages.
S_GETCOOKIE supports tag lookup, jar capacity and one-based tag enumeration.

Super(), Supexec() and the Setexc() cookie/vector convention operate only
on guest CPU/memory state. Supexec uses a guest return gate to restore the
calling mode and stack. No native supervisor call executes guest code.

Implemented initial call surface
--------------------------------
Console GEMDOS/BIOS operations; ordinary file open/create/read/write/seek,
close/delete/rename, attributes and date/time (with endian conversion);
directory get/set/create/delete; guest Malloc/Mfree/Mshrink and ST-RAM
Mxalloc; guest basepages, PRG loading/relocation, Sysconf and selected Ssystem calls.
BIOS device calls use native BIOS traps and bypass GEMDOS stream redirection.
CLI filesystem forwarding also covers Fsfirst/Fsnext, Dfree, Dgetcwd,
Dpathconf, Dlock, Dchroot, links/readlink, chmod/chown, Fxattr and Ssync.
Additional process calls forward effective/audit IDs, audit-ID setting,
priority queries/updates, nice/umask and Prusage.

Native output structures are marshalled into the 68k ABI: the 48-byte
native DTA becomes a 44-byte guest DTA, the 56-byte native XATTR becomes
52 bytes, and disk information/resource usage use big-endian 32-bit values.
Search buffers belong to a guest basepage and DTA address, so switching DTAs
or running an internal overlay preserves the parent's search. Each search
temporarily selects its native DTA and restores the original native pointer.
Native search state remains private; guest reserved DTA bytes are not native
pointers. Each native PROCESS has its own eight search slots. In-place
legacy overlays share that PROCESS's slots. A ninth live search in one
PROCESS returns ENSMEM before starting, so pressure cannot silently evict
a live cursor. Restarting an
existing search reuses its slot; EOF or guest termination releases it.
Unfinished native searches are exhausted through Fsnext before their buffers are
freed at guest
termination (including retained PE_GO images) and on emulator cleanup. If native
DTA restoration fails, the buffer stays alive with a retired owner until pointer
restoration and cleanup can be retried. Non-EOF cleanup errors also retain the
context for retry. Later search calls retry retired contexts before enforcing
the cursor limit, so a temporary failure cannot permanently consume the slots.
Failed or exhausted searches release their inactive bookkeeping immediately when
restoration succeeds; pending restoration retires the record for later cleanup.
Restarting a DTA first exhausts its previous search, so a path
error cannot leave a pluggable filesystem directory cookie behind. Cleanup may
therefore scan the remaining matching entries before releasing a search.
Copying reserved DTA bytes does not clone a native search: pTOS itself keys
its protected search state by DTA address.

Forwarding does not require a kernel implementation of the syscall. Native
errors, including ENOSYS, return unchanged in guest D0. Signed non-positive
Dgetcwd/Freadlink lengths use a native scratch buffer and still reach the kernel,
so native ERANGE/ENOSYS is preserved. Converted output is
committed only on success. Unknown structure/command ABIs still need explicit
adapters; directory streams, stat64, signals and networking remain follow-ups.
Full MiNT libc startup is not yet supported.

Pexec(0/100/200) delegates pathname-based execution to native Pexec.
For a 68k target, the kernel's binfmt registration selects another ParaTOS
instance. A guest cannot replace the shared native process while it has
outstanding cloned guests, or from within a cloned guest.

Guest Pexec(3) loads a 68k PRG into guest RAM and Pexec(5/7) creates a
guest basepage. Modes 4/6 start it and return the child's exit code.
Modes 104/106 start it and return a guest PID for Pwait, Pwait3 or Pwaitpid.
Modes 6/106 transfer ownership of the basepage and environment. All four
start modes share guest RAM, as in the POSIX backend. Native load-only
images from guest code remain unsupported.

The four start modes need the pTOS scheduling epic and its descriptor-only
Pclone flag (pTOS #1040, PR #1045). ParaTOS requests CLONE_VM,
CLONE_COPY_ALL_HANDLES and CLONE_COPY_CWD. Pclone writes the guest's native
PROCESS ID to child_pid. Pgetpid, Pgetppid and Pwaitpid use the kernel's IDs
and relationships, including Pexec(100) children. The synchronous GO modes
return the full Pterm word. Ordinary native Pexec(0) supplies the managed
caller, including for the first application started from boot (pTOS #1039).
Kernels without Pclone retain the legacy in-place modes 4/6.

Each cloned PROCESS inherits descriptor numbers and shares the underlying
open-file positions. Later close, duplicate and rebind operations affect
that PROCESS. pTOS owns their cleanup at native exit. ParaTOS serializes
Musashi and the shared guest allocator across CPU slices and releases the
CPU lock while blocking in Pwaitpid. Shared guest allocations and the native
startup stack remain valid through orphan and parent-first exit. The initial
emulator process drains its surviving guests before releasing shared RAM.

PE_LOAD/PE_BASEPAGE capture the child's standard streams and directories at
creation time. The newly cloned PROCESS installs that context once. Freeing
an unstarted image, abandoning it at parent termination, or failing a load
releases its saved references. Staged search cursors are not inherited.
Arguments use the basepage command tail and the ARGV extension for empty or
long arguments.

With Pclone, PROCESS-local files and native scheduling enabled, CTest adds
`ptos_kernel_pclone`. Run it against a kernel with #1040 to exercise the four
start modes, native PIDs and waits, inherited files and independent search
continuation through a real 68k guest.

Validation
----------
The real-kernel tests build native Paratos and a native test launcher, plus
ordinary 68k PRGs compiled against libcmini. They boot pTOS under QEMU,
using a real FAT disk image and boot-drive PTOS.INI, and run 48 top-level
launches per architecture:

- Direct 68k execution and split Pexec 3->4/6 through binfmt, repeated with image/basepage/
  environment cleanup and high native load addresses checked.
- Dfree, interleaved searches with distinct DTAs, nested-overlay search isolation
  under native cursor-table pressure,
  classic DTA field offsets and byte order, and native/guest status comparison
  for Fxattr, Dpathconf and Ssync, including host ENOSYS.
- x32 VDI array/MFDB conversion, drawing/inquiry, raster byte order and repeated
  retained/transferred overlays with workstation cleanup. ARM forwards SVC #2
  and compares kernel errors with an independent native VDI probe.
- Quoted/empty arguments, environment, console and file I/O, status 37.
- Guest Super/Supexec, live Ssystem-backed low memory, guest CPU cookies,
  denied writes and unsupported calls. All native execution stays in userspace.
- Repeated 68k overlays, including a child Supexec inside a parent's
  active supervisor callback; retained/transferred image ownership,
  file cleanup, inherited handles, stream and directory restoration.
- BIOS output remains on the console while GEMDOS stdout is redirected.
- Native Pexec with inherited handles, working directory and environment;
  mode 200's native ENOSYS without an extra child launch.
- Guest-to-guest Pexec(0) through the kernel and registered handler, with
  short command tails overriding inherited ARGV while preserving environment.
- Shebang scripts with a 68k interpreter (shebang followed by binfmt), an
  optional argument containing spaces/quotes, empty arguments, CRLF and
  propagated status 42, launched from native and guest callers.
- Explicit PARATOS.PRG CLI launches continue to work alongside direct execution.
- Invalid guest access to the native image and a partial live low-memory
  write terminate emulation with status 255; later launches still succeed.

Install the native/CMake/Python tools, kconfiglib, a m68k-atari-mintelf
toolchain, mtools, dosfstools and QEMU. ARM also needs arm-none-eabi-gcc;
x32 needs an x32-capable GCC and OVMF. Use a pTOS checkout with its submodules.
From the Paratos checkout:

  sh tests/ptos/build_kernel_test.sh arm /absolute/path/to/pTOS
  sh tests/ptos/build_kernel_test.sh x32 /absolute/path/to/pTOS

These commands select and build the kernel configuration, build guest and
native libcmini archives in separate output directories, and run CTest.
They change the kernel/libcmini ignored configurations: run architectures
sequentially and finish testing one before selecting the next kernel.
OVMF_DIR and QEMU_DATA_DIR override firmware locations when needed.

They also build pTOS's actual m68k test-hd.img with a CLI selection of its
unmodified regression sources: cookie_values, fseek_rewind,
open_unlink, process_calls and sysconf. Full crt0.o produces a TOS PRG instead of a native m68k ELF. The
additional ptos_m68k_disk test boots a copy of that image: RUNTESTS.TOS stays
in place, preserved byte for byte, and a native wrapper replaces EMUCON.TOS.
It launches RUNTESTS.TOS directly through native Pexec and the boot-configured
binfmt handler. The other disk files remain
in place. CTest requires both a successful upstream test summary and exit
status 0 from the guest; failures are not filtered into a passing result.
GUI/VDI, hardware, native-MMU and unimplemented MiNT ABI suites are outside
this initial CLI selection. To try another m68k PRG test disk, configure
PTOS_M68K_TEST_DISK with its path and run:

  ctest --test-dir build/ptos/arm-native -R '^ptos_m68k_disk$' --output-on-failure

The input image is never modified. serial.log, disk.log, kernel.json and the
adapted disk.img remain in host/ptos/m68k-disk-test for inspection.

To use an already built kernel instead, configure the native Paratos build
with PTOS_KERNEL_DIR and PTOS_GUEST_LIBCMINI_DIR (a m68k libcmini directory
containing libcmini.a and objs/crt0.o). Then build and run just the boot test:

  cmake --build build/ptos/arm-native -j4
  ctest --test-dir build/ptos/arm-native -R '^ptos_kernel$' --output-on-failure

The prepared kernel needs Mmap, the ELF loader, binfmt and an external CLI. ARM uses
virt-arm-aspace; x32 uses pc-x86_64 with Mmap enabled. The build script
disables the x32 kernel's own boot self-test to keep this test independent
of its fixtures. An already built default x32 kernel additionally needs
`make CROSS_COMPILE= x32test x32-vdi-import-test` and its test files.
Logs, the tested kernel revision and the disk image remain in the native
build's host/ptos/kernel-test directory. For separate kernel artifact
directories, preserve the image, obj/autoconf.h, tools/mkhdisk.sh and an
optional SOURCE_REVISION file, so changing the source checkout's active
configuration does not invalidate an earlier architecture's test setup.

GitHub Actions runs Linux/macOS POSIX builds and ARM/x32 legacy real-kernel
tests against the pinned merged kernel. The pTOS job also runs the ARM
bridge test. Use an MMU build of pTOS #1040 for `ptos_kernel_pclone`.

On Linux, the regular CMake build adds a separate Musashi instance using
the pTOS memory callbacks and an injected native-call backend. This maps
real private RAM at the guest virtual addresses but never issues pTOS
syscalls on Linux. It is not an end-to-end pTOS boot test.

  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  cmake --build build -j4
  ctest --test-dir build --output-on-failure
  ctest --test-dir build -R '^ptos_contract$' --output-on-failure

The contract test executes real 68k instructions for Supexec and overlays,
checks low-memory byte order/live updates, identity pointers, host launch
delegation, guest/native address separation, ownership and malformed PRG
cleanup. It also checks 100 nested overlay file-lifetime cycles against real
host descriptors. Filesystem contract cases check unaligned guest outputs,
DTA/XATTR/disk/usage byte order, DTA and guest ownership, bounded native cursor
slots, retained-image termination, restoration retries, bounded allocations across
256 completed/failed searches on distinct DTAs, signed sizes, exact errors,
and unchanged converted output on error. Fixtures are generated at runtime; no cross compiler is required.
Unstarted-image tests also verify descriptor counts through 100 load/free
cycles, snapshot/load failures, failed-start retry and allocator cleanup.
Assertions stay enabled in Release builds too. This test currently uses
Linux MAP_FIXED_NOREPLACE and is not enabled on macOS.

ARM native syscall transport is additionally tested under QEMU. With
qemu-system-arm available during configuration and BUILD_TESTING enabled
(the default), the ARM build registers:

  ctest --test-dir build/ptos/arm-native --output-on-failure
  ctest --test-dir build/ptos/arm-native -R '^ptos_arm_abi$' --output-on-failure

This links the actual ARM syscall bridge and runs it in USR mode on QEMU's
virt/Cortex-A7. A minimal SVC monitor checks argument registers (including
Mmap's fifth/sixth arguments), negative returns and register clobber handling.
It does not simulate the pTOS loader, memory services or a complete kernel
boot; the separate ptos_kernel test covers the real kernel boundary.

Optional compiled 68k CLI integration (m68k-atari-mintelf toolchain required):

  make -C "$LIBCMINI" BUILDDIR="$PWD/build/ptos/guest-libcmini" \
      mintelf_defconfig
  make -C "$LIBCMINI" BUILDDIR="$PWD/build/ptos/guest-libcmini" \
      ONLY_68K=y -j4 dirs libs startups
  m68k-atari-mintelf-gcc -std=c11 -m68000 -Os -nostdlib \
      -I"$LIBCMINI/include" build/ptos/guest-libcmini/objs/crt0.o \
      tests/ptos/cli.c build/ptos/guest-libcmini/libcmini.a -lgcc \
      -o build/ptos/cli.tos
  (cd build/tests/ptos && ./ptos_contract ../../ptos/cli.tos)

This verifies spaced/empty arguments, environment, file I/O, console output
and status 37 through the pTOS backend. The kernel boundary is injected.
Run the existing POSIX regression suite separately:

  make -C test_programs -j4
  test_programs/run_syscalls.sh --no-build

pTOS backend (issue #7, initial draft)
====================================

Status
------
This is a native pTOS/x32 CLI backend, not the POSIX executable renamed to
.prg. ARM and GEM/AES/VDI remain follow-ups. The POSIX backend is unchanged.

The executable builds, but the current pTOS kernel cannot run it with the
required layout. The loader and memory prerequisites are tracked in:
https://github.com/kelihlodversson/pTOS/issues/946

In particular, linking high is insufficient: the native basepage,
environment and startup mappings must also leave guest RAM available.
There is deliberately no fallback to a low native image or relocated guest
RAM. main() refuses low relocation, and guest mapping uses FIXED_NOREPLACE.

Build
-----
Requires CMake >= 3.13, a native x32-capable GCC/binutils, and the pTOS fork
of libcmini (including its x32 crt0). Set LIBCMINI to an absolute path.
The libcmini build changes its ignored .config; do not build different
libcmini architectures concurrently in one checkout. Library outputs stay
in separate directories below build/.

  LIBCMINI=/absolute/path/to/libcmini
  mkdir -p build/ptos/libcmini
  make -C "$LIBCMINI" BUILDDIR="$PWD/build/ptos/libcmini" \
      CROSSPREFIX= ptos_x86_64_defconfig
  make -C "$LIBCMINI" BUILDDIR="$PWD/build/ptos/libcmini" \
      CROSSPREFIX= -j4 dirs libs startups
  cmake -S . -B build/ptos/native \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/ptos-x32.cmake" \
      -DPTOS_LIBCMINI_ROOT="$LIBCMINI" \
      -DPTOS_LIBCMINI_ARCHIVE="$PWD/build/ptos/libcmini/libcmini.a" \
      -DPTOS_CRT0="$PWD/build/ptos/libcmini/objs/crt0.o" \
      -DCMAKE_BUILD_TYPE=Release
  cmake --build build/ptos/native -j4
  readelf -h -l build/ptos/native/host/ptos/paratos.prg

The result is an ELFCLASS32 / EM_X86_64 executable using the x32 ABI. Its
PT_LOAD segments start at 0x41000000, above 1 GiB. Relocations are retained
for the native pTOS ELF loader; this does not authorize relocating it into
guest RAM.

After kernel prerequisites are available:

  paratos.prg program.prg [arguments...]

Boot-configured binfmt/shebang dispatch is a separate feature:
https://github.com/kelihlodversson/pTOS/issues/944

Guest memory and privileges
---------------------------
  00000000..0000ffff  private emulated low-memory shadow, not host address 0
  00010000..00ffffff  private identity-mapped guest RAM, host RW, not executable
  41000000 upward    native emulator image (native heap/stack follow host ABI)

No TT-RAM or Atari hardware/MMIO mapping is supplied. Unavailable guest
ranges fault rather than falling through into native image/heap/stack
memory. A fatal guest fault ends the emulation run with status 255.

Live scalar low-memory fields use native Ssystem() on every read, including
byte/word reads within a wider field and reads crossing field boundaries.
Guest memory limits, vectors and the cookie jar are guest-local. Native
kernel pointers and native CPU cookies are not exposed. Exact-width writes
to live fields use Ssystem and honor its access checks; partial live writes
are refused. The guest S_GET*VAL/S_SET*VAL calls use the same memory layer.

Super(), Supexec() and the Setexc() cookie/vector convention operate only
on guest CPU/memory state. Supexec uses a guest return gate to restore the
calling mode and stack. No native supervisor call executes guest code.

Implemented initial call surface
--------------------------------
Console GEMDOS/BIOS operations; ordinary file open/create/read/write/seek,
close/delete/rename, attributes and date/time (with endian conversion);
directory get/set/create/delete; guest Malloc/Mfree/Mshrink and ST-RAM
Mxalloc; guest basepages, PRG loading/relocation and selected Ssystem calls.
Calls with unimplemented structure ABIs return ENOSYS instead of passing
big-endian guest structures blindly to native code. This includes directory
search/extended stat interfaces and much of MiNT; full MiNT libc startup
is not yet supported. Dfree, signals and networking are not implemented.

Pexec(0) and Pexec(200) delegate to native Pexec even for a 68k target.
This depends on binfmt registration to select another Paratos instance;
the pTOS backend does not use /proc/self/exe or a POSIX fork/exec path.
The current pTOS kernel returns ENOSYS for mode 200. The backend preserves
that result; it never substitutes a child launch for process replacement.

Guest Pexec(3/4/6) loads/starts 68k overlays internally and distinguishes
caller-retained images from images whose ownership is transferred. Guest
Pexec(5/7) creates an emulated basepage. Native load-only images from guest
code and additional modes/flags remain follow-ups; native and guest
basepages must not be confused. Native executable dispatch outside modes
0/200 is not claimed by this draft.

Validation
----------
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
cleanup. Fixtures are generated at runtime; no cross compiler is required.
Assertions stay enabled in Release builds too. This test currently uses
Linux MAP_FIXED_NOREPLACE and is not enabled on macOS.

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

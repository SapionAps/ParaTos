# ParaTos

Userspace Tos (and MiNT) emulator for Linux and macOS, with a native pTOS backend.

This is an emulator that instead of emulating ST hardware, it translates system calls into native OS calls.

This should allow running clean MiNT and TOS command line tools that don't do
direct HW access or attempt to patch the OS trap vectors.

ParaTos uses the Mushashi m68000 emulator in order to emulate the CPU and is
originally inspired by Vincent Riviere's 68kemu, which allows running applications compiled for m68020 CPU on TOS machines featuring a different physical CPU.

# Building and running

ParaTos uses cmake to build the project. The following should work:

	mkdir build
	cd build
	cmake ..
	make

You should now be able to run the generated paratos executable on simple Tos executables:

	./paratos hello.tos

Currently, only a handful of trap #1 functions have been implemented, so you should get a lot of unimplemented function call errors.

# Source layout and testing

Linux and macOS share [host/posix](host/posix/README.md). It owns native
startup, executable discovery, memory mapping and protection, filesystem/path
translation, processes, signals, errno mapping and syscall adapters.

`gemdos/` retains guest ABI structures, trap decoding, PRG loading and
relocation, arguments, basepages, guest allocation, cookies and 8.3 names.
`memory.c`, `memory.i`, `byteorder.h` and `runtime.c` provide guest memory
access, endian conversion and CPU setup/diagnostics. These use standard C
and the CPU interface without POSIX headers. `mushashi/` contains the CPU.

The default build selects the POSIX backend and still produces `build/paratos`.
`PARATOS_PTOS=ON` selects `host/ptos` through the ARM or x32 toolchain file.

Run all CTest checks, or one POSIX integration check:

```sh
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^posix_launch$' --output-on-failure
```

The POSIX tests cover guest byte order and basepage state. With Python 3
available at configuration time, generated 68000 PRGs also test startup,
arguments, environment, relocation and nested execution on Linux and macOS.
The pTOS contract test runs on Linux. The compiled POSIX syscall suite needs
the m68k-atari-mintelf toolchain:

```sh
make -C test_programs -j4
test_programs/run_syscalls.sh --no-build
```

# pTOS port

The native pTOS CLI backend runs on ARM and x32. Build commands,
real-kernel integration tests and current limitations are documented in
[host/ptos/README.txt](host/ptos/README.txt). It is separate from the POSIX
backend described above; GEM/AES and VDI are not part of its first milestone.

# License

ParaTos is distributed under the GNU General Public License, except for Mushashi, which is licensed under
a MIT-like license.

The function definitions for the emulated system calls contain documentation comments extracted from
http://toshyp.atari.org/en/index.html, which is also licensed under the GNU GPL.

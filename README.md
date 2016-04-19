# ParaTos

Userspace Tos (and MiNT) emulator for Linux.

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

You shoyld now be able run the generated paratos executable on simple Tos executables:

	./paratos hello.tos

Currently, only a handful of trap #1 functions have been implemented, so you should get a lot of unimplemented function call errors.

# License

ParaTos is distributed under the GNU General Public License, except for Mushashi, which is licensed under
a MIT-like license.

The function definitions for the emulated system calls contain documentation comments extracted from
http://toshyp.atari.org/en/index.html, which is also licensed under the GNU GPL.

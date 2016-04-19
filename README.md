# ParaTos

Userspace Tos (and MiNT) emulator for Linux.

This is an emulator that instead of emulating ST hardware, it translates system calls into native OS calls.

This should allow running clean MiNT and TOS command line tools that don't do
direct HW access or attempt to patch the OS trap vectors.

ParaTos uses the Mushashi m68000 emulator in order to emulate the CPU and is
originally inspired by Vincent Riviere's 68kemu, which allows running applications compiled for m68020 CPU on TOS machines featuring a different physical CPU.


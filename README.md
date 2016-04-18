# ParaTos

Userspace Tos (and MiNT) emulator for Linux.

This is an emulator that instead of emulating ST hardware, it translates system calls into native OS calls.

This should allow running clean MiNT and TOS command line tools that don't do
direct HW access or attempt to patch the OS trap vectors.


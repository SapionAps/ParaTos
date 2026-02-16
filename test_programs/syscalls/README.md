# GEMDOS Syscall Proxy Tests

These programs are small, focused test binaries for syscalls currently implemented in ParaTos.

They use standard C/POSIX calls that map to GEMDOS traps under MiNT libc.
Each file is intentionally tiny and targets one syscall behavior.

Build from repository root:

```bash
make -C test_programs
```

Binaries are emitted next to each source as `.tos` files.

To run all syscall test binaries through ParaTos in a stable order:

```bash
./test_programs/run_syscalls.sh
```

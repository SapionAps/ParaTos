# TOS Test Programs

This directory contains small test programs intended to be cross-compiled to `.tos` binaries.

## Requirements

- A MiNT/Atari cross toolchain in `PATH` (default prefix: `m68k-atari-mintelf-`)
- `make`

## Build

```bash
make
```

Override toolchain prefix if needed:

```bash
make CROSS=m68k-atari-mintelf-
```

## Produced binaries

- `hello.tos`
- `echo_args.tos`
- `exit_code.tos`
- `syscalls/*.tos` (one small proxy program per implemented syscall path)

## Clean

```bash
make clean
```

## Run syscall tests under ParaTos

```bash
./test_programs/run_syscalls.sh
```

Use `--no-build` to skip rebuilding first.

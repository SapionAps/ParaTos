# AGENTS.md

Guidance for coding agents working in `ParaTos`.
This repository is a C11 project built with CMake.

## 1) Project Snapshot

- Binary: `paratos` (userspace Atari TOS/MiNT emulator)
- Root build script: `CMakeLists.txt`
- Main executable sources: `paratos.c`, `memory.c`, `tos_errors.c`
- Internal library: `gemdos/` (GEMDOS syscall emulation)
- Bundled dependency: `mushashi/` (68k CPU emulator)

## 2) Build Commands

Use out-of-tree builds in `build/`.

```bash
cmake -S . -B build
cmake --build build -j
```

Common variants:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target clean
```

Run emulator:

```bash
./build/paratos <program.tos> [args...]
```

## 3) Test Commands (Including Single-Test Workflow)

There is currently no committed test suite (`add_test(...)` is not present).
Still, use CTest conventions whenever tests are added.

```bash
# run all tests
ctest --test-dir build --output-on-failure

# run one test by name/regex
ctest --test-dir build -R "<test-name-or-regex>" -V

# run one exact test name
ctest --test-dir build -R "^<exact-test-name>$" -V

# list discovered tests
ctest --test-dir build -N
```

Expected current behavior: CTest reports no tests found.

## 4) Lint / Static Analysis

No dedicated lint target or repo-wide formatter configuration is committed.
Preferred approach: warning-clean builds plus optional local static analysis.

```bash
cmake -S . -B build \
  -DCMAKE_C_FLAGS="-std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion"
cmake --build build -j
```

Optional single-file syntax check (if Clang is available):

```bash
clang -std=c11 -fsyntax-only -I. -I./gemdos -I./mushashi <file.c>
```

If `clang-tidy` is available, run it only on files you touched.

## 5) Code Style and Conventions

Follow local style in each file and avoid unrelated reformatting.

### 5.1 Includes and File Layout

- System headers first, then a blank line, then project headers.
- Keep include ordering stable and minimally edited.
- Use `#pragma once` in headers (current project convention).

### 5.2 Formatting

- Predominant indentation is tabs in project-owned C files.
- Opening braces are usually on new lines for functions and blocks.
- Avoid large whitespace-only diffs.

### 5.3 Types

- Prefer fixed-width integer types (`int16_t`, `uint32_t`, etc.).
- Use project typedefs where relevant (`emuptr32_t`, `emureg_t`).
- Preserve packed struct layouts where already required.
- Keep signedness/width accurate when writing emulated memory.

### 5.4 Naming

- GEMDOS syscall entrypoints use canonical names (`Fopen`, `Pwaitpid`).
- Internal helpers commonly use `snake_case`.
- Macros use `UPPER_SNAKE_CASE`.

### 5.5 Error Handling

- Map host failures via `MapErrno()` when returning GEMDOS/TOS errors.
- Return TOS/MiNT error codes (`TOS_E*`) for GEMDOS-facing APIs.
- Use early returns for invalid pointers/paths.
- Preserve cleanup on all return paths.
- Unimplemented paths normally use `NOT_IMPLEMENTED(...)` + `TOS_ENOSYS`.

### 5.6 Memory and Ownership

- `read_path()` / `tos_path_to_unix()` allocate; caller must `free()`.
- Free partial allocations on error paths.
- Do not leak `DIR*`, file descriptors, or temporary path buffers.

### 5.7 Emulated Memory Access

- Prefer `m68k_read_memory_*` / `m68k_write_memory_*` APIs.
- Use `m68k_read_field` / `m68k_write_field` for struct members.
- Avoid raw pointer casting into emulated RAM unless pattern already exists.
- Be explicit about endian conversions (host vs emulated big-endian).

### 5.8 Logging and Diagnostics

- Use `TRACEF(...)` for debug logging (`DEBUG`-guarded).
- Use `fprintf(stderr, ...)` or `perror(...)` for hard runtime errors.
- Keep noisy diagnostics out of hot paths unless intentionally debugging.

### 5.9 Platform Notes

- Linux is primary runtime; keep existing Apple conditionals intact.
- Preserve `_GNU_SOURCE` assumptions from current CMake setup.

## 6) Build Graph Conventions

- Root target links static libraries `mushashi` and `gemdos`.
- `mushashi` decoder sources are generated via `m68kmake` during build.
- Do not hand-edit generated decoder outputs if they appear.

## 7) Cursor / Copilot Rules

Checked repository locations:

- `.cursorrules`
- `.cursor/rules/`
- `.github/copilot-instructions.md`

Result: no Cursor or Copilot instruction files were found.

## 8) Agent Checklist Before Finishing

- Build should succeed with your changes.
- Do not reformat unrelated files.
- Ensure path-allocation helpers are freed correctly in new code.
- Ensure host errors map to TOS error returns where appropriate.
- Use emulated memory helper APIs for reads/writes.
- If tests are added, include all-tests and single-test `ctest` commands.

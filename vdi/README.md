# VDI marshalling

`vdi.c` translates the classic 68k VDI parameter block into the native pTOS
userspace client ABI. It uses Musashi memory accessors and a host range/call
adapter; it contains no privileged display access or AES parameter decoding.

The guest parameter block contains five big-endian 32-bit pointers. Its control
array is twelve 16-bit words: raster pointers start at byte 14. The native
control block has a padding word and starts its pointer union at byte 16.
Guest PBs, control arrays and MFDBs need only two-byte alignment. Native
temporary structures have the native pointer alignment.

Supported operations match the existing pTOS userspace client:

- Virtual workstation open/close, polyline, text and rectangular bars.
- Character height, line type/width/colour, text colour, fill attributes,
  writing mode, text-attribute queries and user-defined line patterns.
- Extended workstation inquiry, pixel queries, filled rectangles and clipping.
- MFDB raster copy, transformation and monochrome transparent copy, subject
  to the native client's existing format/operation restrictions.

Counts and output sizes come from each operation, not from the input values of
`contrl[2]` and `contrl[4]`. Polylines and text are bounded to 256 points/words.
All input and writable output ranges are checked before the native call.
Outputs are staged and committed only after success and valid returned counts.
Native errors, including ENOSYS, reach guest D0 unchanged. Invalid guest pointers
return EIMBA; malformed counts return EINVAL. Unsupported operations return ENOSYS.

MFDB fields are converted separately from their bitmap data. Each non-screen
bitmap is limited to 1 MiB. Planar words, packed RGB565 pixels and packed 32-bit
pixels are converted into native byte order in separate source/destination
snapshots, preserving overlapping guest buffers. Only the destination bitmap
and `fd_stand` for a successful transformation are written back. Descriptor
addresses and all other guest pointer fields retain their guest values.
Screen forms (`fd_addr == 0`) have no userspace bitmap. The native client owns
display access and currently supports screen-to-screen opcode 109; other screen
raster combinations retain its native unsupported result.

Font chains and VDI vector exchanges are unsupported. A 68k function pointer
must never be installed as a native callback, and a returned native callback
must never become guest executable code. These operations need a separate
emulated callback/chaining contract.

## Native pTOS integration

`host/ptos/vdi.c` tracks workstation handles by guest basepage, rejects use by
other internal guests, and closes a terminating guest's workstations even when
Pexec(4) retains its image. Failed closes retain a retired entry and are retried
before subsequent calls or emulator cleanup. The native kernel remains
responsible for final reclamation if the emulator process exits or faults.

For x32, build pTOS with `CONF_WITH_X32_VDI=y`, run `make x32-vdi`, and configure
ParaTos with `-DPTOS_VDI_LIBRARY=/path/to/pTOS/obj/x32/libptos-vdi.a`. This links
the checked userspace client; no general VDIPB is passed to a kernel syscall.
Without the optional archive, workstation opening returns ENOSYS. The current
ARM backend also returns ENOSYS because pTOS has no ring-3 typed VDI entry there.
The shared marshaller is compiled on both architectures.

The POSIX executable retains its preliminary GEM trap handling. This marshaller
is ready for a future POSIX graphics adapter; none is supplied by this change.
AES bindings, the planned 68k AES image and AESPB dispatch are unchanged.

## Validation

The Linux contract test injects native calls and checks aligned native versus
two-byte aligned guest structures, word/long byte order, bounded arrays,
native errors, unchanged outputs on failure, malformed output counts, rejected
guest callback/font pointers, bitmap spans and internal guest ownership/cleanup.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^ptos_contract$' --output-on-failure
```

The real-kernel test compiles ordinary 68k TRAP #2 callers. x32 exercises the
native client, text/line drawing, inquiry, one-pixel planar copies, multi-plane
MFDB transformation, invalid pointers and 64 unclosed internal overlays per
fixture invocation. ARM verifies ENOSYS without changing the guest's control
block. Each architecture runs 48 top-level launches plus the existing m68k disk.

```sh
sh tests/ptos/build_kernel_test.sh x32 /path/to/pTOS
sh tests/ptos/build_kernel_test.sh arm /path/to/pTOS
```

Run architectures sequentially: these scripts change ignored kernel/libcmini
configurations. The x32 script enables the VDI service and links its user client.

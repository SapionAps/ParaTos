#!/bin/sh
# Build both sides and run the real-kernel tests. Requires installed tools;
# changes the ignored kernel/libcmini configurations, sequentially.
set -eu
if [ "$#" -ne 2 ]; then
    echo "usage: $0 arm|x32 /path/to/pTOS" >&2
    exit 2
fi
arch=$1
kernel=$(cd "$2" && pwd)
top=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
build=$top/build/ptos/kernel-$arch
libcmini=$kernel/lib/libcmini
case $arch in
    arm)
        config=virt-arm-aspace
        prefix=arm-none-eabi-
        native_flags="-march=armv6 -marm"
        toolchain=ptos-arm.cmake
        ;;
    x32)
        config=pc-x86_64
        prefix=
        native_flags="-march=x86-64 -fcf-protection=none"
        toolchain=ptos-x32.cmake
        ;;
    *) echo "unknown architecture: $arch" >&2; exit 2 ;;
esac
native_flags="$native_flags -fno-pie -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables"

# Build pTOS's own m68k regression disk, with its existing CLI suites and
# full TOS startup. The native minicrt0 used by other pTOS tests emits ELF.
make -C "$libcmini" BUILDDIR="$build/guest-libcmini" mintelf_defconfig
make -C "$libcmini" BUILDDIR="$build/guest-libcmini" ONLY_68K=y -j4 dirs libs startups
make -C "$kernel" atari512_defconfig
make -C "$kernel" CROSS_COMPILE=m68k-atari-mintelf- \
    TEST_SUITES="cookie_values fseek_rewind open_unlink process_calls sysconf" \
    TEST_STARTUP="$build/guest-libcmini/objs/crt0.o" -j4 test-hd
cp "$kernel/test-hd.img" "$build/m68k-test-hd.img"

make -C "$kernel" "${config}_defconfig"
if [ "$arch" = x32 ]; then
    (cd "$kernel"
     CONFIG_= python3 - <<'PY'
import kconfiglib
config = kconfiglib.Kconfig()
config.load_config()
config.syms["CONF_WITH_MMAP"].set_value(2)
config.syms["CONF_WITH_X86_64_MEMTEST"].set_value(0)
config.write_config()
PY
    )
fi
make -C "$kernel" CROSS_COMPILE="$prefix" -j4

case $arch in
    arm) libc_config=ptos_arm_defconfig ;;
    x32) libc_config=ptos_x86_64_defconfig ;;
esac
make -C "$libcmini" BUILDDIR="$build/libcmini" CROSSPREFIX="$prefix" "$libc_config"
if [ -d "$build/libcmini/objs" ]; then
    make -C "$libcmini" BUILDDIR="$build/libcmini" CROSSPREFIX="$prefix" clean
fi
make -C "$libcmini" BUILDDIR="$build/libcmini" CROSSPREFIX="$prefix" \
    ARCH_CFLAGS="$native_flags" -j4 dirs libs startups
cmake -S "$top" -B "$build/native" \
    -DCMAKE_TOOLCHAIN_FILE="$top/cmake/$toolchain" -DCMAKE_BUILD_TYPE=Release \
    -DPTOS_LIBCMINI_ROOT="$libcmini" -DPTOS_LIBCMINI_ARCHIVE="$build/libcmini/libcmini.a" \
    -DPTOS_CRT0="$build/libcmini/objs/crt0.o" -DPTOS_KERNEL_DIR="$kernel" \
    -DPTOS_GUEST_LIBCMINI_DIR="$build/guest-libcmini" \
    -DPTOS_M68K_TEST_DISK="$build/m68k-test-hd.img" \
    -DPTOS_OVMF_DIR="${OVMF_DIR:-/usr/share/OVMF}" \
    -DPTOS_QEMU_DATA_DIR="${QEMU_DATA_DIR:-}"
cmake --build "$build/native" -j4
ctest --test-dir "$build/native" --output-on-failure

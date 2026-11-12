#!/usr/bin/env python3
"""Boot a prepared pTOS kernel and exercise native Paratos with real 68k PRGs.

The kernel checkout is read only during this test. Logs, the FAT disk and EFI
variables are kept in --output; no injected syscall implementation is used.
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time

BEGIN = "pTOS Paratos kernel integration: BEGIN"
PASS = "pTOS Paratos kernel integration: PASS"
FAIL = "pTOS Paratos kernel integration: FAIL"


def verify_image(path, arch):
    data = path.read_bytes()
    if data[:6] != b"\x7fELF\x01\x01":
        raise ValueError("Paratos must be a little-endian ELF32 native image")
    header = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    _, machine, _, entry, phoff, _, flags, _, phsize, phnum, *_ = header
    if machine != {"arm": 40, "x32": 62}[arch] or phsize != 32:
        raise ValueError("Paratos ELF architecture/program headers do not match the test")
    if arch == "arm" and not flags & 0x400:
        raise ValueError("Paratos ARM image must use the VFP hard-float ABI")
    segments = [struct.unpack_from("<IIIIIIII", data, phoff + i * phsize)
                for i in range(phnum)]
    loads = [p for p in segments if p[0] == 1]
    if not loads or min(p[2] for p in loads) != 0x41000000 or entry < 0x41000000:
        raise ValueError("Paratos must retain its high native load address")
    if any(p[0] == 3 for p in segments):
        raise ValueError("This test expects the static native Paratos executable")


def run(args):
    kernel = args.kernel_dir.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    begin = "pTOS 68k test disk: BEGIN" if args.test_disk else BEGIN
    passed = "pTOS 68k test disk: PASS" if args.test_disk else PASS
    failed = "pTOS 68k test disk: FAIL" if args.test_disk else FAIL
    config = (kernel / "obj/autoconf.h").read_text()

    def enabled(name):
        return re.search(r"^#define\s+" + name + r"\s+1\s*$", config, re.M) is not None

    for name in ("CONF_WITH_MMAP", "CONF_WITH_ELF_LOADER", "CONF_WITH_BINFMT", "CONF_EXTERNAL_CLI"):
        if not enabled(name):
            raise ValueError("The prepared kernel must enable " + name)
    machine = "MACHINE_VIRT_ARM" if args.arch == "arm" else "MACHINE_PC_X86_64"
    if not enabled(machine) or (args.arch == "arm" and not enabled("CONF_WITH_PROCESS_ASPACES")):
        raise ValueError("The prepared kernel does not match the selected architecture/address spaces")
    verify_image(args.paratos, args.arch)
    for name in ("cli", "semantics", "overlay", "fault", "interpreter", "filesystem", "fs_overlay", "vdi", "vdi_overlay"):
        if (args.fixtures / (name + ".tos")).read_bytes()[:2] != b"\x60\x1a":
            raise ValueError(name + " must be a compiled 68k TOS PRG")
    revision = "unknown"
    if (kernel / "SOURCE_REVISION").is_file():
        revision = (kernel / "SOURCE_REVISION").read_text().strip()
    elif (kernel / ".git").exists():
        try:
            revision = subprocess.check_output(["git", "-C", str(kernel), "rev-parse", "HEAD"],
                                               text=True, stderr=subprocess.DEVNULL).strip()
        except (OSError, subprocess.CalledProcessError):
            pass
    (output / "kernel.json").write_text(json.dumps({"source": str(kernel), "revision": revision,
                                                  "architecture": args.arch,
                                                  "guest_disk": str(args.test_disk) if args.test_disk else None},
                                                 indent=2) + "\n")
    log = output / "serial.log"
    log.write_text("")
    with tempfile.TemporaryDirectory(prefix="staging-", dir=output) as temporary:
        stage = Path(temporary)
        (stage / "BIN").mkdir()
        shutil.copyfile(args.paratos, stage / "BIN/PARATOS.PRG")
        shutil.copyfile(args.launcher, stage / "EMUCON.TOS")
        # Root-relative interpreter paths resolve on the boot drive. Exercise
        # both accepted separators and keep the emulator out of the disk root.
        (stage / "PTOS.INI").write_bytes(
            b"[binfmt.tos68k]\r\nmagic=601A\r\ninterpreter=/BIN\\PARATOS.PRG\r\n")
        original = None
        if args.test_disk:
            shutil.copyfile(args.test_disk, output / "disk.img")
            original = subprocess.check_output(["mtype", "-i", str(output / "disk.img") + "@@1048576",
                                                "::RUNTESTS.TOS"])
            if original[:2] != b"\x60\x1a":
                raise ValueError("The disk's RUNTESTS.TOS must be a 68k TOS PRG, not a native ELF")
        else:
            shutil.copyfile(args.native_child, stage / "NATIVE.TOS")
            shutil.copyfile(args.filesystem_native, stage / "FSNATIVE.TOS")
            for source, target in (("cli", "CLI.TOS"), ("semantics", "SEMANTIC.TOS"),
                                   ("overlay", "OVERLAY.TOS"), ("fault", "FAULT.TOS"),
                                   ("interpreter", "BIN/INTERP.TOS"),
                                   ("filesystem", "FSCOMP.TOS"), ("fs_overlay", "FSOVL.TOS"),
                                   ("vdi", "VDI.TOS"), ("vdi_overlay", "VDIOVL.TOS")):
                shutil.copyfile(args.fixtures / (source + ".tos"), stage / target)
            # The script's 68k interpreter itself needs binfmt redirection.
            # The optional shebang argument is one argument, including quotes.
            (stage / "SCRIPT.SH").write_bytes(
                b"#!/BIN\\INTERP.TOS option with 'quotes'\r\nscript fixture\r\n")
        qemu = [args.qemu]
        if args.qemu_data:
            qemu += ["-L", args.qemu_data]
        if args.arch == "arm":
            qemu += ["-M", "virt,highmem=off", "-cpu", "cortex-a7", "-m", "128",
                     "-global", "virtio-mmio.force-legacy=false",
                     "-kernel", str(kernel / "virt-arm.elf"),
                     "-drive", "if=none,id=disk,format=raw,file=" + str(output / "disk.img"),
                     "-device", "virtio-blk-device,drive=disk"]
        else:
            boot = stage / "EFI/BOOT"
            boot.mkdir(parents=True)
            shutil.copyfile(kernel / "pc-x86_64.efi", boot / "BOOTX64.EFI")
            shutil.copyfile(args.ovmf / "OVMF_VARS_4M.fd", output / "vars.fd")
            # The optional kernel boot self-test has its own native fixtures.
            # The recommended test configuration disables it; an already built
            # default kernel is usable too after `make x32test`.
            if enabled("CONF_WITH_X86_64_MEMTEST"):
                fixtures = {"x32hello.elf": "X32HELLO.TOS", "x32reloc.elf": "X32RELOC.TOS",
                            "x32noreloc.elf": "X32NOREL.TOS", "x32big.elf": "X32BIG.TOS",
                            "obj/x32/x32probe.elf": "X32CB.TOS"}
                for source, target in fixtures.items():
                    shutil.copyfile(kernel / source, stage / target)
                shutil.copyfile(stage / "X32CB.TOS", stage / "X32VDI.TOS")
                (stage / "X32TRUNC.TOS").write_bytes((stage / "X32HELLO.TOS").read_bytes()[:2000])
                for source in (kernel / "obj/x32").glob("VDI*.TOS"):
                    shutil.copyfile(source, stage / source.name)
            qemu += ["-machine", "pc", "-cpu", "qemu64,+pdpe1gb", "-m", "256",
                     "-drive", "if=pflash,format=raw,readonly=on,file=" + str(args.ovmf / "OVMF_CODE_4M.fd"),
                     "-drive", "if=pflash,format=raw,file=" + str(output / "vars.fd"),
                     "-drive", "format=raw,file=" + str(output / "disk.img")]
        with (output / "disk.log").open("w") as stream:
            if args.test_disk:
                command = ["mcopy", "-o", "-s", "-i", str(output / "disk.img") + "@@1048576",
                           *map(str, stage.iterdir()), "::"]
            else:
                command = ["sh", str(kernel / "tools/mkhdisk.sh"), str(output / "disk.img"),
                           "33554432", *map(str, stage.iterdir())]
            subprocess.run(command,
                           stdout=stream, stderr=subprocess.STDOUT, check=True)
        if original is not None:
            preserved = subprocess.check_output(["mtype", "-i", str(output / "disk.img") + "@@1048576",
                                                 "::RUNTESTS.TOS"])
            if preserved != original:
                raise RuntimeError("The original 68k test runner was not preserved byte for byte")
        qemu += ["-nic", "none", "-display", "none", "-monitor", "none", "-serial", "file:" + str(log)]
        with (output / "qemu.log").open("w") as stream:
            process = subprocess.Popen(qemu, stdout=stream, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + args.timeout
                while time.monotonic() < deadline:
                    text = log.read_text(errors="replace")
                    if begin in text:
                        body = text.split(begin, 1)[1]
                        if failed in body or "process fault:" in body:
                            raise RuntimeError("native launch, guest semantics or containment failed")
                        if passed in body:
                            if args.arch == "x32" and text.count("EFI entry reached") != 1:
                                raise RuntimeError("EFI reset/re-entry detected")
                            if args.test_disk and ("All tests passed." not in body or
                                                  not re.search(r"Summary: [1-9][0-9]* passed, 0 failed", body)):
                                raise RuntimeError("The 68k runner did not report a successful test summary")
                            print(begin + body[:body.index(passed) + len(passed)])
                            return
                    if process.poll() is not None:
                        raise RuntimeError("QEMU stopped before the integration result")
                    time.sleep(0.1)
                raise RuntimeError("timed out waiting for the kernel integration result")
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", required=True, choices=("arm", "x32"))
    for name in ("kernel-dir", "paratos", "launcher", "native-child", "filesystem-native", "fixtures", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--qemu-data", default="")
    parser.add_argument("--ovmf", type=Path, default=Path("/usr/share/OVMF"))
    parser.add_argument("--timeout", type=int, default=90)
    parser.add_argument("--test-disk", type=Path)
    args = parser.parse_args()
    try:
        run(args)
        return 0
    except (OSError, ValueError, RuntimeError, struct.error, subprocess.SubprocessError) as error:
        print("pTOS kernel integration:", error, file=sys.stderr)
        for name in ("serial.log", "qemu.log", "disk.log"):
            path = args.output / name
            if path.exists():
                print("--- " + str(path) + " ---", file=sys.stderr)
                print(path.read_text(errors="replace"), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

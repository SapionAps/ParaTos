#!/usr/bin/env python3
"""Exercise POSIX startup and nested execution with generated 68000 PRGs.

The fixtures use ordinary TOS traps and relocation records. No cross compiler
is required, so Linux and macOS run the same guest code in CTest.
"""
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest


PARATOS = Path(sys.argv.pop(1)).resolve()


def prg(code, relocations=()):
    header = struct.pack(">H6IH", 0x601a, len(code), 0, 0, 0, 0, 0, 0)
    records = bytearray(struct.pack(">I", relocations[0] if relocations else 0))
    for previous, current in zip(relocations, relocations[1:]):
        delta = current - previous
        while delta > 254:
            records.append(1)
            delta -= 254
        records.append(delta)
    if relocations:
        records.append(0)
    return header + code + records


def terminate(status=37):
    return bytes.fromhex("3f3c") + struct.pack(">H", status) + bytes.fromhex("3f3c004c4e41")


def print_tail():
    # A0 = basepage, A1 = command tail, D1 = unsigned tail length.
    code = bytes.fromhex("206f000443e8008072001219")
    # Fwrite(1, D1, A1); Pterm(37).
    return prg(code + bytes.fromhex("2f092f013f3c00013f3c00404e41") + terminate())


def print_environment(length):
    # A1 = basepage->p_env; Fwrite(1, length, A1); Pterm(37).
    code = bytes.fromhex("206f00042268002c2f092f3c") + struct.pack(">I", length)
    return prg(code + bytes.fromhex("3f3c00013f3c00404e41") + terminate())


def parent():
    # Pexec(0, child, tail, env), then Pterm(D0). PEA operands are relocated
    # from text-relative offsets by the ordinary PRG loader.
    code = bytearray(bytes.fromhex("487900000000487900000000487900000000"
                                   "3f3c00003f3c004b4e413f003f3c004c4e41"))
    env = len(code)
    code += b"\0\0"
    tail = len(code)
    argument = b"nested-child"
    code += bytes([len(argument)]) + argument + bytes(127 - len(argument))
    child = len(code)
    code += b"CHILD.TOS\0"
    for offset, value in ((2, env), (8, tail), (14, child)):
        struct.pack_into(">I", code, offset, value)
    return prg(code, (2, 8, 14))


class LaunchTests(unittest.TestCase):
    def test_seek_errors_use_tos_codes(self):
        for handle, expected in ((1, -6), (32767, -37)):
            with self.subTest(handle=handle):
                # Fseek(0, handle, SEEK_CUR); Pterm(D0).
                code = (bytes.fromhex("3f3c00013f3c")
                        + struct.pack(">H", handle)
                        + bytes.fromhex("2f3c000000003f3c00424e41"
                                        "3f003f3c004c4e41"))
                (self.directory / "SEEK.TOS").write_bytes(prg(code))
                result = self.run_guest("SEEK.TOS")
                self.assertEqual(result.returncode, expected & 255, result.stderr)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="paratos guest ")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def run_guest(self, name, *arguments, **kwargs):
        return subprocess.run([str(PARATOS), name, *arguments], cwd=self.directory,
                              capture_output=True, timeout=10, **kwargs)

    def test_usage_and_load_error(self):
        result = subprocess.run([str(PARATOS)], capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"<program.tos> [arguments...]", result.stderr)
        result = self.run_guest("MISSING.TOS")
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"error: cannot load MISSING.TOS.", result.stderr)

    def test_arguments(self):
        (self.directory / "TAIL.TOS").write_bytes(print_tail())
        result = self.run_guest("TAIL.TOS", "alpha", "beta")
        self.assertEqual(result.returncode, 37, result.stderr)
        self.assertEqual(result.stdout, b" alpha beta")

    def test_environment(self):
        value = b"PARATOS_HOST_TEST=environment-value"
        (self.directory / "ENV.TOS").write_bytes(print_environment(len(value)))
        result = self.run_guest("ENV.TOS", env={"PARATOS_HOST_TEST": "environment-value"})
        self.assertEqual(result.returncode, 37, result.stderr)
        self.assertEqual(result.stdout, value)

    def test_nested_execution_discovers_renamed_emulator(self):
        executable = self.directory / "renamed emulator"
        shutil.copy2(PARATOS, executable)
        (self.directory / "CHILD.TOS").write_bytes(print_tail())
        (self.directory / "PARENT.TOS").write_bytes(parent())
        # argv[0] deliberately does not identify the emulator. Native
        # executable discovery must supply its path for nested 68k Pexec.
        result = subprocess.run(["unused argv0", "PARENT.TOS"], executable=str(executable),
                                cwd=self.directory, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 37, result.stderr)
        self.assertEqual(result.stdout, b" nested-child")

    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux SIGSEGV contract")
    def test_memory_fault_diagnostics(self):
        (self.directory / "FAULT.TOS").write_bytes(prg(bytes.fromhex("203902000000")))
        result = self.run_guest("FAULT.TOS")
        self.assertEqual(result.returncode, 255)
        self.assertIn(b"Caught SEGV while executing instruction", result.stdout)


if __name__ == "__main__":
    unittest.main()

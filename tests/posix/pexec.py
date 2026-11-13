#!/usr/bin/env python3
"""Exercise split PRG execution, shared memory and asynchronous lifetimes."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import os
import termios
import time
import signal

PARATOS = Path(sys.argv.pop(1)).resolve()


class Program:
    def __init__(self):
        self.code = bytearray()
        self.labels = {}
        self.fixups = []

    def emit(self, code):
        self.code += bytes.fromhex(code)

    def address(self, label):
        self.fixups.append((len(self.code), label))
        self.code += bytes(4)

    def label(self, name, data=b""):
        if len(self.code) & 1:
            self.emit("00")
        self.labels[name] = len(self.code)
        self.code += data

    def push(self, label):
        self.emit("4879")
        self.address(label)

    def pexec(self, mode, basepage=False, env=None):
        if env:
            self.push(env)
        else:
            self.emit("42a7")  # inherited environment
        if basepage:
            self.emit("2f0c42a7")  # arg2=A4, ignored arg1=NULL
        else:
            self.push("tail")
            self.push("child")
        self.emit("3f3c" + struct.pack(">H", mode).hex() + "3f3c004b4e414fef0010")

    def report(self, tag=None):
        if tag is not None:
            self.emit("2f002f3c" + struct.pack(">I", tag).hex()
                      + "204f2f082f3c000000083f3c00013f3c00404e414fef0014")
            return
        # Fwrite(stdout, 4, &saved_D0), preserving A4/A5.
        self.emit("2f00204f2f082f3c000000043f3c00013f3c00404e414fef0010")

    def shrink(self):
        self.emit("2a6f00042f3c000100002f0d42673f3c004a4e414fef000c")
        self.emit("200d06800000fffc2e40")  # move stack into the retained TPA

    def finish(self, status=0):
        self.emit("3f3c" + struct.pack(">H", status).hex() + "3f3c004c4e41")

    def wait(self, flags=0, any_child=False):
        # Pwaitpid(D6, 0, NULL).
        self.emit("42a73f3c" + struct.pack(">H", flags).hex())
        self.emit("3f3cffff" if any_child else "3f06")
        self.emit("3f3c013a4e414fef000a")

    def free(self, environment=False):
        self.emit("2f2c002c" if environment else "2f0c")
        self.emit("3f3c00494e414fef0006")
        self.report()

    def image(self):
        relocations = []
        for offset, label in self.fixups:
            struct.pack_into(">I", self.code, offset, self.labels[label])
            relocations.append(offset)
        records = bytearray(struct.pack(">I", relocations[0] if relocations else 0))
        for previous, current in zip(relocations, relocations[1:]):
            distance = current - previous
            while distance > 254:
                records.append(1)
                distance -= 254
            records.append(distance)
        if relocations:
            records.append(0)
        return struct.pack(">H6IH", 0x601a, len(self.code), 0, 0, 0, 0, 0, 0) + self.code + records


def child(wait_gate=False, read_input=False, output=False):
    p = Program()
    if wait_gate:
        p.emit("4ab90000100467f8")  # wait for parent to open shared gate
    if read_input:
        # Fread(stdin, 1, scratch).
        p.emit("4879000010082f3c000000013f3c00003f3c003f4e414fef000c")
    p.emit("23fc1234567800001000")
    if output:
        p.emit("203c76543210")
        p.report()
    p.finish(37)
    return p.image()


def parent(mode, asynchronous=False, created=False, parent_exits=False):
    p = Program()
    p.shrink()
    p.emit("2e3c55aa33cc")  # D7 sentinel must survive child CPU initialization
    p.pexec(5 if created else 3)
    p.emit("2840")  # A4=basepage
    if created:
        p.emit("297c")
        p.address("entry")
        p.emit("0008")  # basepage->p_tbase = embedded entry point
    p.pexec(mode, basepage=True)
    if asynchronous:
        p.emit("2c00")  # D6=thread ID
        p.emit("23fc0000000100001004")
        if parent_exits:
            p.finish(23)
        else:
            p.wait()
            # Keep just the child's exit status (the ID is platform independent).
            p.emit("0280000000ff")
            p.report()
    else:
        p.report()
    p.emit("203900001000")
    p.report()
    p.emit("2007")
    p.report()
    if mode == 4:
        # Caller-owned image may be run again, then explicitly freed.
        p.pexec(4, basepage=True)
        p.report()
        p.free(environment=True)
        p.free()
    elif mode == 6:
        p.free()  # mode 6 already released its image
    p.finish(23)
    p.label("child", b"CHILD.TOS\0")
    p.label("tail", bytes(128))
    if created:
        p.label("entry")
        p.emit("23fc1234567800001000")
        p.finish(37)
    return p.image()


class PexecTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="paratos pexec ")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def run_pair(self, parent_image, child_image):
        (self.directory / "PARENT.TOS").write_bytes(parent_image)
        (self.directory / "CHILD.TOS").write_bytes(child_image)
        return subprocess.run([str(PARATOS), "PARENT.TOS"], cwd=self.directory,
                              capture_output=True, timeout=5)

    def assert_words(self, result, expected):
        self.assertEqual(result.returncode, 23, result.stderr)
        self.assertEqual(result.stderr, b"")
        self.assertEqual(result.stdout, b"".join(struct.pack(">I", x & 0xffffffff) for x in expected))

    def test_load_go_retains_image(self):
        self.assert_words(self.run_pair(parent(4), child()),
                          [37, 0x12345678, 0x55aa33cc, 37, 0, 0])

    def test_guest_broken_pipe_does_not_terminate_other_guests(self):
        for failed_replace in (False, True):
            with self.subTest(failed_replace=failed_replace):
                c = Program()
                if failed_replace:
                    c.pexec(200)
                    c.report()
                c.emit("4879000011082f3c000000013f39000011023f3c00404e414fef000c")
                c.report()
                # Console output must also report a TOS pipe error. Preserve
                # a reporting stream while redirecting only the child's stdout.
                c.emit("3f3c00013f3c00454e41588f2800")
                c.emit("3f39000011023f3c00013f3c00464e415c8f")
                c.emit("3f3c00413f3c00024e41588f2a00")
                c.emit("3f043f3c00013f3c00464e415c8f2005")
                c.report()
                c.emit("3f043f3c003e4e41588f")
                c.finish(37)
                c.label("child", b"MISSING.TOS\0")
                c.label("tail", bytes(128))
                p = Program()
                p.shrink()
                p.emit("2e3c55aa33cc4879000011003f3c01004e415c8f")
                p.emit("3f39000011003f3c003e4e41588f")  # close the only reader
                p.pexec(3)
                p.emit("2840")
                p.pexec(6, basepage=True)
                p.report()
                p.emit("2007")
                p.report()
                p.finish(23)
                p.label("child", b"CHILD.TOS\0")
                p.label("tail", bytes(128))
                expected = ([-33] if failed_replace else []) + [-81, -81, 37, 0x55aa33cc]
                self.assert_words(self.run_pair(p.image(), c.image()), expected)

    def test_broken_host_diagnostic_pipe_does_not_terminate_runtime(self):
        p = Program()
        p.emit("4879000011082f3c000000013f3c00003f3c003f4e414fef000c")
        p.emit("4ef902000000")  # fault diagnostics will write to closed stdout
        (self.directory / "FAULT.TOS").write_bytes(p.image())
        proc = subprocess.Popen([str(PARATOS), "FAULT.TOS"], cwd=self.directory,
                                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        try:
            proc.stdout.close()
            proc.stdout = None
            _, stderr = proc.communicate(b"x", timeout=3)
            self.assertEqual(proc.returncode, 255, stderr)
        finally:
            if proc.poll() is None:
                proc.kill()
            proc.communicate(timeout=3)

    def test_native_exec_keeps_default_sigpipe_behavior(self):
        for mode in (0, 100, 200):
            with self.subTest(mode=mode):
                native = self.directory / "NATIVE.SH"
                native.write_text("#!/bin/sh\nkill -s PIPE $$\nexit 99\n")
                native.chmod(0o755)
                p = Program()
                p.shrink()
                p.pexec(mode)
                if mode == 100:
                    p.emit("2c00")
                    p.wait()
                    p.emit("02800000ffff")
                p.report()
                p.finish(23)
                p.label("child", b"NATIVE.SH\0")
                p.label("tail", bytes(128))
                (self.directory / "PARENT.TOS").write_bytes(p.image())
                r = subprocess.run([str(PARATOS), "PARENT.TOS"], cwd=self.directory,
                                   capture_output=True, timeout=5)
                if mode == 200:
                    self.assertEqual(r.returncode, -signal.SIGPIPE, r.stderr)
                else:
                    self.assert_words(r, [signal.SIGPIPE << 8 if mode == 100 else -1])

    def test_worker_cannot_restart_the_active_initial_guest(self):
        c = Program()
        c.emit("206f000428680024")  # A4 = initial guest's basepage
        for mode in (4, 6, 104):
            c.pexec(mode, basepage=True)
            c.report()
        c.finish(37)
        self.assert_words(self.run_pair(parent(6), c.image()),
                          [-36, -36, -36, 37, 0, 0x55aa33cc, -40])

    def test_root_exit_closes_redirected_writer_before_waiting_for_child(self):
        c = Program()
        # Give the child the read end as stdin, then close its inherited writer.
        c.emit("3f390000110042673f3c00464e415c8f")
        c.emit("3f3c00013f3c003e4e41588f")
        c.emit("4879000011082f3c000000013f3c00003f3c003f4e414fef000c")
        # Report EOF on stderr: stdout was deliberately closed above.
        c.emit("2f00204f2f082f3c000000043f3c00023f3c00404e414fef0010")
        c.finish(37)
        p = Program()
        p.shrink()
        p.emit("4879000011003f3c01004e415c8f")  # Fpipe
        p.emit("3f39000011023f3c00013f3c00464e415c8f")  # Fforce(stdout, writer)
        p.emit("3f39000011023f3c003e4e41588f")  # close original writer
        p.pexec(3)
        p.emit("2840")
        p.pexec(104, basepage=True)
        p.finish(23)
        p.label("child", b"CHILD.TOS\0")
        p.label("tail", bytes(128))
        r = self.run_pair(p.image(), c.image())
        self.assertEqual(r.returncode, 23, r.stderr)
        self.assertEqual(r.stdout, b"")
        self.assertEqual(r.stderr, struct.pack(">I", 0))

    def test_negative_fcntl_dupfd_minimum_is_rejected_without_allocating(self):
        p = Program()
        p.shrink()
        p.emit("3f3c00013f3c00454e41588f2c003f063f3c003e4e41588f")
        p.emit("42672f3cffffffff3f3c00013f3c01044e414fef000a")
        p.report()
        p.emit("3f3c00013f3c00454e41588f9086")
        p.report()
        p.finish(23)
        self.assert_words(self.run_pair(p.image(), child()), [-25, 0])

    def test_absolute_prg_accepts_complete_declared_symbols(self):
        c = Program()
        c.finish(37)
        image = (struct.pack(">H6IH", 0x601a, len(c.code), 0, 0, 16, 0, 0, 1)
                 + c.code + bytes(16))
        self.assert_words(self.run_pair(parent(6), image),
                          [37, 0, 0x55aa33cc, -40])

    def test_shrink_does_not_treat_application_self_pointer_as_basepage(self):
        p = Program()
        p.shrink()
        p.emit("2f3c000004003f3c00484e415c8f2840288c297c112233440004")
        p.emit("2f3c000002002f0c42673f3c004a4e414fef000c202c0004")
        p.report()
        p.finish(23)
        self.assert_words(self.run_pair(p.image(), child()), [0x11223344])

    def test_fault_in_diagnostics_does_not_hang_other_guests(self):
        c = Program()
        c.emit("4ef902000000")  # inaccessible guest PC, also unsafe to disassemble
        r = self.run_pair(parent(6), c.image())
        self.assertEqual(r.returncode, 23, r.stderr)
        self.assertLess(len(r.stdout), 32768)
        self.assertIn(struct.pack(">4I", 255, 0, 0x55aa33cc, 0xffffffd8), r.stdout)

    def test_unterminated_explicit_and_inherited_environments(self):
        for inherited in (False, True):
            with self.subTest(inherited=inherited):
                p = Program()
                p.shrink()
                p.emit("33fc414100fffffe")
                if inherited:
                    p.emit("2b7c00fffffe002c42a7")
                else:
                    p.emit("2f3c00fffffe")
                p.push("tail")
                p.push("child")
                p.emit("3f3c00643f3c004b4e414fef0010")
                p.report()
                p.finish(23)
                p.label("child", b"/bin/true\0")
                p.label("tail", bytes(128))
                self.assert_words(self.run_pair(p.image(), child()), [-25])

    def test_failed_exec_reports_tos_error_without_child_id(self):
        for mode in (0, 100):
            for missing in (False, True):
                with self.subTest(mode=mode, missing=missing):
                    p = Program()
                    p.shrink()
                    p.pexec(mode)
                    p.report()
                    p.wait(any_child=True)
                    p.report()
                    p.finish(23)
                    p.label("child", b"MISSING.TOS\0" if missing else b"CHILD.TOS\0")
                    p.label("tail", bytes(128))
                    self.assert_words(self.run_pair(p.image(), b"not executable"),
                                      [-33 if missing else -36, -21])

    def test_successful_native_exit_127_is_not_launch_failure(self):
        p = Program()
        p.shrink()
        p.pexec(0)
        p.report()
        p.finish(23)
        p.label("child", b"CHILD.TOS\0")
        p.label("tail", bytes(128))
        native = self.directory / "CHILD.TOS"
        native.write_bytes(b"#!/bin/sh\nexit 127\n")
        native.chmod(0o755)
        (self.directory / "PARENT.TOS").write_bytes(p.image())
        r = subprocess.run([str(PARATOS), "PARENT.TOS"], cwd=self.directory,
                           capture_output=True, timeout=5)
        self.assert_words(r, [127])

    def test_pfork_propagates_reserved_identity_to_child(self):
        p = Program()
        p.shrink()
        p.emit("3f3c010b4e41548f")
        p.report()
        p.emit("3f3c011b4e41548f4a8066064ef9")
        p.address("fork_child")
        p.emit("2c00")
        p.report(1)
        p.wait()
        p.emit("0280000000ff")
        p.report(4)
        p.finish(23)
        p.label("fork_child")
        for tag, opcode in ((2, 267), (3, 268)):
            p.emit("3f3c" + struct.pack(">H", opcode).hex() + "4e41548f")
            p.report(tag)
        p.finish(37)
        r = self.run_pair(p.image(), child())
        self.assertEqual(r.returncode, 23, r.stderr)
        owner = struct.unpack(">I", r.stdout[:4])[0]
        records = dict(struct.unpack(">II", r.stdout[i:i + 8])
                       for i in range(4, len(r.stdout), 8))
        self.assertEqual(set(records), {1, 2, 3, 4})
        self.assertGreater(records[1], 0)
        self.assertEqual(records[1], records[2])
        self.assertEqual(records[3], owner)
        self.assertEqual(records[4], 37)

    def test_exec_child_receives_reserved_identity(self):
        c = Program()
        for tag, opcode in ((2, 267), (3, 268)):
            c.emit("3f3c" + struct.pack(">H", opcode).hex() + "4e41548f")
            c.report(tag)
        c.finish(37)
        p = Program()
        p.shrink()
        p.emit("3f3c010b4e41548f")
        p.report()
        p.pexec(100)
        p.emit("2c00")
        p.report(1)
        p.wait()
        p.emit("0280000000ff")
        p.report(4)
        p.finish(23)
        p.label("child", b"CHILD.TOS\0")
        p.label("tail", bytes(128))
        r = self.run_pair(p.image(), c.image())
        self.assertEqual(r.returncode, 23, r.stderr)
        owner = struct.unpack(">I", r.stdout[:4])[0]
        records = dict(struct.unpack(">II", r.stdout[i:i + 8])
                       for i in range(4, len(r.stdout), 8))
        self.assertEqual(set(records), {1, 2, 3, 4})
        self.assertGreater(records[1], 0)
        self.assertEqual(records[1], records[2])
        self.assertEqual(records[3], owner)
        self.assertEqual(records[4], 37)

    def test_exec_root_identity_stays_reserved_through_nested_threads(self):
        for depth in range(4):
            p = Program()
            p.shrink()
            p.emit("3f3c010b4e41548f")
            p.report()
            if depth < 3:
                p.pexec(3)
                p.emit("2840")
                p.pexec(4, basepage=True)
            p.finish(23 if depth == 0 else 37)
            p.label("child", ("LEVEL%d.TOS" % (depth + 1)).encode() + b"\0")
            p.label("tail", bytes(128))
            (self.directory / ("LEVEL%d.TOS" % depth)).write_bytes(p.image())
        r = subprocess.run([str(PARATOS), "--paratos-child", "1", "9", "LEVEL0.TOS"],
                           cwd=self.directory, capture_output=True, timeout=5)
        self.assertEqual(r.returncode, 23, r.stderr)
        ids = struct.unpack(">4I", r.stdout)
        self.assertEqual(ids[0], 1)
        self.assertEqual(len(set(ids)), 4)
        self.assertTrue(all(0 < pid <= 32767 for pid in ids))

    def test_orphan_native_child_is_reaped_while_root_blocks_on_input(self):
        native = self.directory / "ORPHAN.SH"
        native.write_text("#!/bin/sh\necho $$ > PIDFILE\n"
                          "while [ ! -e GO ]; do sleep 0.01; done\nexit 0\n")
        native.chmod(0o755)
        c = Program()
        c.pexec(100)
        c.finish(37)
        c.label("child", b"ORPHAN.SH\0")
        c.label("tail", bytes(128))
        p = Program()
        p.shrink()
        p.pexec(3)
        p.emit("2840")
        p.pexec(6, basepage=True)
        p.report()
        p.emit("4879000011002f3c000000013f3c00003f3c003f4e414fef000c")
        p.finish(23)
        p.label("child", b"CHILD.TOS\0")
        p.label("tail", bytes(128))
        (self.directory / "PARENT.TOS").write_bytes(p.image())
        (self.directory / "CHILD.TOS").write_bytes(c.image())
        proc = subprocess.Popen([str(PARATOS), "PARENT.TOS"], cwd=self.directory,
                                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        try:
            import select
            self.assertTrue(select.select([proc.stdout], [], [], 3)[0])
            self.assertEqual(proc.stdout.read(4), struct.pack(">I", 37))
            deadline = time.monotonic() + 3
            pidfile = self.directory / "PIDFILE"
            while not pidfile.exists() or not pidfile.read_text().strip():
                self.assertLess(time.monotonic(), deadline)
                time.sleep(0.01)
            native_pid = int(pidfile.read_text())
            (self.directory / "GO").touch()
            while True:
                try:
                    os.kill(native_pid, 0)
                except ProcessLookupError:
                    break
                self.assertLess(time.monotonic(), deadline, "orphan native child was not reaped")
                time.sleep(0.01)
            self.assertIsNone(proc.poll(), "root must remain blocked while orphan is collected")
            stdout, stderr = proc.communicate(b"x", timeout=3)
            self.assertEqual(proc.returncode, 23, stderr)
            self.assertEqual(stdout, b"")
        finally:
            (self.directory / "GO").touch()
            if proc.poll() is None:
                proc.kill()
            proc.communicate(timeout=3)

    def test_blocking_fifo_open_allows_peer_guest_to_open_other_end(self):
        os.mkfifo(self.directory / "CHANNEL")
        c = Program()
        c.emit("3f3c0001")
        c.push("fifo")
        c.emit("3f3c003d4e41508f2800")
        c.push("data")
        c.emit("2f3c000000013f043f3c00404e414fef000c")
        c.finish(37)
        c.label("fifo", b"CHANNEL\0")
        c.label("data", b"Z")
        p = Program()
        p.shrink()
        p.pexec(3)
        p.emit("2840")
        p.pexec(104, basepage=True)
        p.emit("2c004267")
        p.push("fifo")
        p.emit("3f3c003d4e41508f2800")
        p.emit("4879000011052f3c000000013f043f3c003f4e414fef000c")
        p.report()
        p.wait()
        p.emit("0280000000ff")
        p.report()
        p.emit("7000103900001105")
        p.report()
        p.finish(23)
        p.label("fifo", b"CHANNEL\0")
        p.label("child", b"CHILD.TOS\0")
        p.label("tail", bytes(128))
        self.assert_words(self.run_pair(p.image(), c.image()), [1, 37, ord("Z")])

    def test_load_go_free(self):
        self.assert_words(self.run_pair(parent(6), child()),
                          [37, 0x12345678, 0x55aa33cc, -40])

    def test_create_go(self):
        self.assert_words(self.run_pair(parent(4, created=True), child()),
                          [37, 0x12345678, 0x55aa33cc, 37, 0, 0])

    def test_go_without_wait(self):
        self.assert_words(self.run_pair(parent(104, asynchronous=True), child(wait_gate=True)),
                          [37, 0x12345678, 0x55aa33cc])

    def test_parent_pterm_does_not_kill_thread(self):
        result = self.run_pair(parent(104, asynchronous=True, parent_exits=True),
                               child(wait_gate=True, output=True))
        self.assert_words(result, [0x76543210])

    def test_pterm0_only_ends_child(self):
        p = Program()
        p.emit("3f3c00004e41")
        self.assert_words(self.run_pair(parent(6), p.image()),
                          [0, 0, 0x55aa33cc, -40])

    def test_child_closing_standard_streams_does_not_close_parent_streams(self):
        p = Program()
        for fd in (0, 1, 2):
            p.emit("3f3c" + struct.pack(">H", fd).hex() + "3f3c003e4e41588f")
        p.finish(37)
        self.assert_words(self.run_pair(parent(6), p.image()),
                          [37, 0, 0x55aa33cc, -40])

    def test_overlapping_console_reads_restore_terminal(self):
        c = Program()
        c.emit("3f3c00074e41548f")
        c.report()
        c.finish(37)
        p = Program()
        p.shrink()
        for register in ("2c00", "2a00"):
            p.pexec(3)
            p.emit("28402f3c000100002f0c42673f3c004a4e414fef000c")
            p.pexec(104, basepage=True)
            p.emit(register)
        p.wait()
        p.emit("0280000000ff")
        p.report()
        p.emit("2c05")
        p.wait()
        p.emit("0280000000ff")
        p.report()
        p.finish(23)
        p.label("child", b"CHILD.TOS\0")
        p.label("tail", bytes(128))
        (self.directory / "PARENT.TOS").write_bytes(p.image())
        (self.directory / "CHILD.TOS").write_bytes(c.image())
        master, slave = os.openpty()
        self.addCleanup(os.close, master)
        self.addCleanup(os.close, slave)
        saved = termios.tcgetattr(slave)
        proc = subprocess.Popen([str(PARATOS), "PARENT.TOS"], cwd=self.directory,
                                stdin=slave, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 3
            while termios.tcgetattr(slave)[3] & termios.ICANON:
                self.assertIsNone(proc.poll())
                self.assertLess(time.monotonic(), deadline)
                time.sleep(0.01)
            time.sleep(0.05)  # let the second guest attempt its terminal session
            os.write(master, b"x")
            time.sleep(0.05)
            os.write(master, b"y")
            stdout, stderr = proc.communicate(timeout=5)
            self.assertEqual(proc.returncode, 23, stderr)
            self.assertCountEqual(struct.unpack(">4I", stdout), [ord("x"), ord("y"), 37, 37])
            restored = termios.tcgetattr(slave)
            # Darwin marks input for reprocessing when canonical mode is
            # restored. PENDIN is kernel-maintained queue state, not a raw-mode
            # setting; all other flags and control characters must match.
            pending = getattr(termios, "PENDIN", 0)
            restored[3] &= ~pending
            saved[3] &= ~pending
            self.assertEqual(restored, saved)
        finally:
            if proc.poll() is None:
                proc.kill()
            proc.communicate()

    def test_nonblocking_wait_and_wait_any(self):
        p = Program()
        p.shrink()
        p.pexec(3)
        p.emit("2840")
        p.pexec(104, basepage=True)
        p.emit("2c00")
        p.wait(flags=1)
        p.report()
        p.emit("23fc0000000100001004")
        p.wait(any_child=True)
        p.emit("0280000000ff")
        p.report()
        p.finish(23)
        p.label("child", b"CHILD.TOS\0")
        p.label("tail", bytes(128))
        self.assert_words(self.run_pair(p.image(), child(wait_gate=True)), [0, 37])

    def test_bad_images_do_not_consume_guest_memory(self):
        header = lambda text: struct.pack(">H6IH", 0x601a, text, 0, 0, 0, 0, 0, 0)
        for image in (b"\x7fELF", header(8) + b"short", header(0x7fffffff),
                      struct.pack(">H6IH", 0x601a, 8, 0, 0, 16, 0, 0, 1) + bytes(8),
                      struct.pack(">H6IH", 0x601a, 8, 0, 0, 16, 0, 0, 1) + bytes(23),
                      header(8) + bytes(8) + struct.pack(">I", 10)):
            with self.subTest(image=image[:32]):
                p = Program()
                p.shrink()
                p.emit("2f3cffffffff3f3c00484e415c8f")
                p.report()
                p.pexec(3)
                p.report()
                p.emit("2f3cffffffff3f3c00484e415c8f")
                p.report()
                p.finish(23)
                p.label("child", b"CHILD.TOS\0")
                p.label("tail", bytes(128))
                r = self.run_pair(p.image(), image)
                self.assertEqual(r.returncode, 23, r.stderr)
                before, error, after = struct.unpack(">IiI", r.stdout)
                self.assertEqual(error, -66)
                self.assertEqual(before, after)

    def test_child_basepage_environment_and_dta(self):
        c = Program()
        c.emit("206f00042028002423c000001020")  # parent basepage
        c.emit("2028002c2040201023c000001018")  # copied environment bytes
        c.emit("206f00042028008023c00000101c")  # copied command tail
        c.emit("3f3c002f4e41548f23c000001010")  # child Fgetdta
        c.finish(37)
        p = Program()
        p.shrink()
        p.pexec(3, env="env")
        p.emit("2840")
        p.report()
        p.pexec(4, basepage=True)
        p.report()
        for address in (0x1010, 0x1018, 0x101c, 0x1020):
            p.emit("2039" + struct.pack(">I", address).hex())
            p.report()
        p.emit("200d")  # parent's basepage from A5
        p.report()
        p.finish(23)
        p.label("child", b"CHILD.TOS\0")
        p.label("tail", b"\x04 xyz" + bytes(123))
        p.label("env", b"TEST=child\0\0")
        r = self.run_pair(p.image(), c.image())
        self.assertEqual(r.returncode, 23, r.stderr)
        bp, status, dta, env, tail, caller, parent_bp = struct.unpack(">7I", r.stdout)
        self.assertEqual((status, dta, env, tail, caller),
                         (37, bp + 128, 0x54455354, 0x04207879, parent_bp))

    def test_repeated_go_free_reclaims_environment_and_image(self):
        p = Program()
        p.shrink()
        p.emit("2f3cffffffff3f3c00484e415c8f")
        p.report()
        for _ in range(20):
            p.pexec(3)
            p.emit("2840")
            p.pexec(6, basepage=True)
        p.emit("2f3cffffffff3f3c00484e415c8f")
        p.report()
        p.finish(23)
        p.label("child", b"CHILD.TOS\0")
        p.label("tail", bytes(128))
        r = self.run_pair(p.image(), child())
        self.assertEqual(r.returncode, 23, r.stderr)
        before, after = struct.unpack(">II", r.stdout)
        self.assertEqual(before, after)

    def test_blocking_pipe_read_allows_child_thread_to_write(self):
        c = Program()
        c.emit("4879000011042f3c000000013f39000011023f3c00404e414fef000c")
        c.finish(37)
        p = Program()
        p.shrink()
        p.emit("4879000011003f3c01004e415c8f13fc005a00001104")
        p.pexec(3)
        p.emit("2840")
        p.pexec(104, basepage=True)
        p.emit("2c00")
        p.emit("4879000011052f3c000000013f39000011003f3c003f4e414fef000c")
        p.report()
        p.wait()
        p.emit("0280000000ff")
        p.report()
        p.emit("7000103900001105")
        p.report()
        p.finish(23)
        p.label("child", b"CHILD.TOS\0")
        p.label("tail", bytes(128))
        self.assert_words(self.run_pair(p.image(), c.image()), [1, 37, ord("Z")])

    def test_nested_async_child_keeps_terminated_parent_image_alive(self):
        c = Program()
        c.shrink()
        c.pexec(5)
        c.emit("2840")
        c.emit("297c")
        c.address("entry")
        c.emit("0008")
        # Leave room for the root to allocate after this worker terminates.
        c.emit("2f3c000100002f0c42673f3c004a4e414fef000c")
        c.pexec(104, basepage=True)
        c.finish(37)
        c.label("entry")
        c.emit("4ab90000100467f8203c76543210")
        c.report()
        c.finish(0)
        c.label("child", b"ignored\0")
        c.label("tail", bytes(128))
        p = Program()
        p.shrink()
        p.pexec(3)
        p.emit("2840")
        p.pexec(6, basepage=True)
        # If the worker image was prematurely freed, this allocation reuses
        # and overwrites code still needed by its asynchronous child.
        p.pexec(4, basepage=True)  # retired basepage must remain unavailable
        p.report()
        p.emit("2f3c000100003f3c00484e415c8f2040")
        p.emit("323c3fff429851c9fffc")  # clear 64 KiB
        p.emit("23fc0000000100001004")
        p.finish(23)
        p.label("child", b"CHILD.TOS\0")
        p.label("tail", bytes(128))
        self.assert_words(self.run_pair(p.image(), c.image()), [-36, 0x76543210])

    def test_load_without_wait_native_process(self):
        p = Program()
        p.shrink()
        p.pexec(100)
        p.emit("2c00")
        p.report()  # Python must see this before unblocking the child.
        p.wait()
        p.emit("0280000000ff")
        p.report()
        p.finish(23)
        p.label("child", b"CHILD.TOS\0")
        p.label("tail", bytes(128))
        (self.directory / "PARENT.TOS").write_bytes(p.image())
        (self.directory / "CHILD.TOS").write_bytes(child(read_input=True))
        proc = subprocess.Popen([str(PARATOS), "PARENT.TOS"], cwd=self.directory,
                                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        try:
            import select
            self.assertTrue(select.select([proc.stdout], [], [], 3)[0], "mode 100 waited")
            pid = struct.unpack(">I", proc.stdout.read(4))[0]
            self.assertGreater(pid, 0)
            stdout, stderr = proc.communicate(b"x", timeout=5)
            self.assertEqual(proc.returncode, 23, stderr)
            self.assertEqual(stdout, struct.pack(">I", 37))
        finally:
            if proc.poll() is None:
                proc.kill()
            proc.communicate()


if __name__ == "__main__":
    unittest.main()

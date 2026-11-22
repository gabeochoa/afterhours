#!/usr/bin/env python3
"""Tests for e2e_parallel_runner, against a tiny fixture binary.

The fixture is a Python script the runner spawns per .e2e script; it
reads markers out of the script body, so a test script declares its own
behaviour: `fixture_sleep 0.5`, `fixture_exit 3`, `fixture_print x`.
Run: python3 -m unittest test_e2e_parallel_runner (in tools/).
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
RUNNER = TOOLS / "e2e_parallel_runner.py"
sys.path.insert(0, str(TOOLS))

import e2e_parallel_runner as runner_mod  # noqa: E402

FIXTURE = r"""
import os, socket, sys, time
mode = sys.argv[1] if len(sys.argv) > 1 else ""
if mode == "--serve":
    port = int(sys.argv[2])
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", port))
    srv.listen(1)
    print("READY", flush=True)
    try:
        conn, _ = srv.accept()
        conn.sendall(b"hello")
        conn.close()
        time.sleep(5)
    except Exception:
        pass
    sys.exit(0)
script = sys.argv[1]
text = open(script).read()
marker = os.environ.get("E2E_TEST_MARKER")
def stamp(tag):
    if marker:
        with open(marker, "a") as f:
            f.write(f"{tag} {time.time():.3f}\n")
stamp("start")
for line in text.splitlines():
    parts = line.split()
    if not parts:
        continue
    if parts[0] == "fixture_sleep":
        time.sleep(float(parts[1]))
    elif parts[0] == "fixture_print":
        print(" ".join(parts[1:]), flush=True)
    elif parts[0] == "fixture_connect":
        s = socket.create_connection(("127.0.0.1", int(os.environ["E2E_PORT"])))
        data = s.recv(16)
        print("connected:" + data.decode(), flush=True)
        s.close()
    elif parts[0] == "fixture_exit":
        stamp("end")
        sys.exit(int(parts[1]))
stamp("end")
"""


class HeaderTests(unittest.TestCase):
    def test_parse_headers(self) -> None:
        h = runner_mod.parse_headers(
            "# timeout: 12\n# flags: --a \"two words\"\n# batch: g\n"
            "# spawn: srv ./srv --x\n# wait_for_log: srv:UP\ncmd\n")
        self.assertEqual(h.timeout, 12)
        self.assertEqual(h.flags, ["--a", "two words"])
        self.assertEqual(h.batch, "g")
        self.assertEqual(h.spawns[0][0], "srv")
        self.assertEqual(h.wait_for_logs, [("srv", "UP")])
        self.assertEqual(h.errors, [])

    def test_bad_timeout_is_an_error(self) -> None:
        h = runner_mod.parse_headers("# timeout: soon\ncmd\n")
        self.assertTrue(h.errors)


class RunnerTests(unittest.TestCase):
    def setUp(self) -> None:
        self._td = tempfile.TemporaryDirectory()
        self.dir = Path(self._td.name)
        self.fixture = self.dir / "fixture.py"
        self.fixture.write_text(FIXTURE, encoding="utf-8")
        self.tmp_root = self.dir / "tmp"
        self.tmp_root.mkdir()

    def tearDown(self) -> None:
        self._td.cleanup()

    def script(self, name: str, body: str) -> Path:
        path = self.dir / f"{name}.e2e"
        path.write_text(body, encoding="utf-8")
        return path

    def run_runner(self, scripts, *extra, env_extra=None):
        env = dict(os.environ)
        env.pop("E2E_TEST_MARKER", None)
        if env_extra:
            env.update(env_extra)
        cmd = [sys.executable, str(RUNNER), "--jobs", "4",
               "--tmp-root", str(self.tmp_root),
               "--command",
               f"{sys.executable} {self.fixture} {{script}}",
               *[str(s) for s in scripts], *extra]
        started = time.monotonic()
        proc = subprocess.run(cmd, capture_output=True, text=True, env=env)
        return proc, time.monotonic() - started

    def test_parallel_pass_and_summary(self) -> None:
        scripts = [self.script(f"s{i}", "fixture_sleep 0.6\nfixture_print ok\n")
                   for i in range(4)]
        proc, elapsed = self.run_runner(scripts)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn("run=4 passed=4", proc.stdout)
        # Serial would take ~2.4s; parallel must come in well under.
        self.assertLess(elapsed, 2.0)

    def test_failure_tail_and_exit_code(self) -> None:
        bad = self.script("bad", "fixture_print first\nfixture_print last\n"
                                 "fixture_exit 3\n")
        good = self.script("good", "fixture_print fine\n")
        proc, _ = self.run_runner([good, bad])
        self.assertEqual(proc.returncode, 1)
        self.assertIn("FAIL    bad", proc.stdout)
        self.assertIn("last", proc.stdout)
        self.assertIn("run=2 passed=1 failed=1", proc.stdout)

    def test_header_timeout_enforced(self) -> None:
        slow = self.script("slow", "# timeout: 0.5\nfixture_sleep 5\n")
        proc, elapsed = self.run_runner([slow])
        self.assertEqual(proc.returncode, 1)
        self.assertIn("TIMEOUT slow", proc.stdout)
        self.assertLess(elapsed, 3.0)

    def test_requires_skips_not_fails(self) -> None:
        needy = self.script(
            "needy", "# requires: cmd:definitely-not-a-real-cmd-xyz\n"
                     "fixture_print hi\n")
        proc, _ = self.run_runner([needy])
        self.assertEqual(proc.returncode, 0, proc.stdout)
        self.assertIn("SKIP    needy", proc.stdout)

    def test_batch_group_serializes(self) -> None:
        marker = self.dir / "marks.txt"
        body = "# batch: shared\nfixture_sleep 0.4\n"
        scripts = [self.script("b1", body), self.script("b2", body)]
        proc, elapsed = self.run_runner(
            scripts, env_extra={"E2E_TEST_MARKER": str(marker)})
        self.assertEqual(proc.returncode, 0, proc.stdout)
        # Same batch: the two 0.4s sleeps cannot overlap.
        self.assertGreaterEqual(elapsed, 0.75)

    def test_spawn_wait_for_log_and_free_port(self) -> None:
        client = self.script(
            "client",
            f"# spawn: server {sys.executable} {self.fixture} --serve {{port}}\n"
            "# wait_for_log: server:READY\n"
            "fixture_connect\n")
        proc, _ = self.run_runner([client])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn("PASS    client", proc.stdout)

    def test_results_json(self) -> None:
        good = self.script("good", "fixture_print fine\n")
        out = self.dir / "results.json"
        proc, _ = self.run_runner([good], "--results-json", str(out))
        self.assertEqual(proc.returncode, 0)
        data = json.loads(out.read_text(encoding="utf-8"))
        self.assertEqual(data["run"], 1)
        self.assertTrue(data["success"])
        self.assertEqual(data["scripts"][0]["name"], "good")


if __name__ == "__main__":
    unittest.main()

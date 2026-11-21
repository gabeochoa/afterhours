#!/usr/bin/env python3
"""Shipped multi-process parallel runner for afterhours E2E scripts.

Every networked consumer used to hand-roll this file: spawn N game
processes, allocate free ports, barrier on log lines, harvest failure
tails (Pumpkin Beachball item M / Pharmasea G-M4; Cartographer's
script-header ask). This runner is that file, once.

    python3 tools/e2e_parallel_runner.py --binary ./game --jobs 4 \
        --script-dir e2e_scripts
    python3 tools/e2e_parallel_runner.py --binary ./game a.e2e b.e2e
    python3 tools/e2e_parallel_runner.py \
        --command 'python3 fixture.py {script} {port} {tmp}' test.e2e

Without --command the target is invoked the way the E2E harness
expects: `<binary> --test-script <script> <flags...>`.

Per-script headers (leading `#` comment block; see
src/plugins/e2e_testing/script_headers.h for the canonical spec):

    # timeout: 30            wall seconds (default: --timeout)
    # flags: --headless      extra target argv, shell-split
    # requires: gpu, file:x, env:VAR, cmd:ffmpeg
                             unmet -> SKIP (not fail); bare words are
                             capabilities granted with --provides
    # batch: name            same batch = sequential, in file order;
                             different batches run in parallel
    # spawn: server <cmd>    helper started before the target
    # wait_for_log: server:READY
                             target starts once the helper prints it

Each job gets a free TCP port ($E2E_PORT, {port}) and a private temp
dir ($E2E_TMPDIR / $TMPDIR, {tmp}); spawned helpers share both, so a
server can bind the port and the client can connect to it. Output is
captured per process; failures print a unified tail (--tail lines).
Exit code is 0 only when nothing failed or timed out (skips pass).

Deferred on purpose: screenshot baseline diffing
(expect_screenshot / --update-baselines), networked-multiplayer
orchestration beyond spawn/port/log-wait, and CI integration.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import re
import shlex
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path


# ---------------------------------------------------------------- headers

@dataclass
class Headers:
    timeout: float | None = None
    flags: list[str] = field(default_factory=list)
    requires: list[str] = field(default_factory=list)
    batch: str = ""
    spawns: list[tuple[str, str]] = field(default_factory=list)  # (label, raw)
    wait_for_logs: list[tuple[str, str]] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)


def parse_headers(text: str) -> Headers:
    """Mirror of testing::parse_script_headers_text (script_headers.h)."""
    headers = Headers()
    for line_number, raw in enumerate(text.splitlines(), 1):
        stripped = raw.strip()
        if not stripped:
            continue
        if not stripped.startswith("#"):
            break
        body = stripped[1:].strip()
        if ":" not in body:
            continue
        key, _, value = body.partition(":")
        key, value = key.strip(), value.strip()
        where = f"line {line_number}: # {key}:"
        try:
            if key == "timeout":
                seconds = value[:-1] if value.lower().endswith("s") else value
                timeout = float(seconds)
                if timeout <= 0:
                    raise ValueError
                headers.timeout = timeout
            elif key == "flags":
                headers.flags += shlex.split(value)
            elif key == "requires":
                headers.requires += [t for t in re.split(r"[,\s]+", value) if t]
            elif key == "batch":
                headers.batch = value
            elif key == "spawn":
                words = shlex.split(value)
                if len(words) < 2:
                    headers.errors.append(
                        f"{where} expected '<label> <command...>'")
                else:
                    headers.spawns.append((words[0], value))
            elif key == "wait_for_log":
                label, needle = "", value
                head, sep, rest = value.partition(":")
                if sep and head and " " not in head and "\t" not in head:
                    label, needle = head, rest.strip()
                if not needle:
                    headers.errors.append(f"{where} expected a log needle")
                else:
                    headers.wait_for_logs.append((label, needle))
            # Unknown headers are ignored by design.
        except ValueError:
            if key == "timeout":
                headers.errors.append(
                    f"{where} expected seconds > 0, got '{value}'")
            else:
                headers.errors.append(f"{where} unparsable value '{value}'")
        except Exception as exc:  # shlex raises ValueError on bad quotes
            headers.errors.append(f"{where} {exc}")
    return headers


# ---------------------------------------------------------------- jobs

@dataclass
class Job:
    index: int
    script: Path
    headers: Headers

    @property
    def name(self) -> str:
        return self.script.stem

    @property
    def batch(self) -> str:
        return self.headers.batch or self.name


@dataclass
class Result:
    job: Job
    status: str  # PASS FAIL TIMEOUT SKIP ERROR
    duration: float = 0.0
    exit_code: int | None = None
    reason: str = ""
    tail: list[str] = field(default_factory=list)
    tmpdir: str = ""


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def unmet_requires(headers: Headers, provides: set[str]) -> list[str]:
    missing = []
    for req in headers.requires:
        kind, sep, arg = req.partition(":")
        if sep and kind == "file":
            ok = Path(arg).exists()
        elif sep and kind == "env":
            ok = bool(os.environ.get(arg))
        elif sep and kind == "cmd":
            ok = shutil.which(arg) is not None
        elif sep and kind == "os":
            ok = sys.platform.startswith(arg)
        else:
            ok = req in provides
        if not ok:
            missing.append(req)
    return missing


def expand(template: str, values: dict[str, str]) -> str:
    # str.format would choke on braces in game args; only our known
    # placeholders expand, everything else passes through untouched.
    out = template
    for key, value in values.items():
        out = out.replace("{" + key + "}", value)
    return out


def read_tail(path: Path, lines: int) -> list[str]:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    return text.splitlines()[-lines:] if lines > 0 else text.splitlines()


def stop_process(proc: subprocess.Popen) -> None:
    if proc.poll() is not None:
        return
    try:
        os.killpg(proc.pid, signal.SIGTERM)
    except (ProcessLookupError, PermissionError):
        proc.terminate()
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            proc.kill()
        proc.wait(timeout=3)


class Runner:
    def __init__(self, args: argparse.Namespace):
        self.args = args
        self.provides = {p for p in args.provides.split(",") if p}

    def build_values(self, job: Job, port: int, tmpdir: Path) -> dict[str, str]:
        return {
            "binary": self.args.binary or "",
            "script": str(job.script),
            "port": str(port),
            "tmp": str(tmpdir),
            "index": str(job.index),
            "stem": job.name,
        }

    def main_argv(self, job: Job, values: dict[str, str]) -> list[str]:
        if self.args.command:
            return shlex.split(expand(self.args.command, values))
        argv = [self.args.binary, "--test-script", str(job.script)]
        argv += [expand(a, values) for a in self.args.extra_arg]
        argv += job.headers.flags
        return argv

    def run_job(self, job: Job) -> Result:
        started = time.monotonic()
        result = Result(job=job, status="PASS")
        headers = job.headers
        if headers.errors:
            result.status = "ERROR"
            result.reason = "; ".join(headers.errors)
            result.duration = time.monotonic() - started
            return result
        missing = unmet_requires(headers, self.provides)
        if missing:
            result.status = "SKIP"
            result.reason = "missing requires: " + ", ".join(missing)
            result.duration = time.monotonic() - started
            return result

        timeout = headers.timeout or self.args.timeout
        tmp_root = Path(self.args.tmp_root) if self.args.tmp_root else \
            Path(tempfile.gettempdir())
        tmpdir = tmp_root / f"e2e-{job.index:03d}-{job.name}"
        tmpdir.mkdir(parents=True, exist_ok=True)
        result.tmpdir = str(tmpdir)
        port = free_port()
        values = self.build_values(job, port, tmpdir)

        env = dict(os.environ)
        env.update({
            "E2E_PORT": str(port),
            "E2E_TMPDIR": str(tmpdir),
            "TMPDIR": str(tmpdir),
            "E2E_SCRIPT": str(job.script),
            "E2E_INDEX": str(job.index),
        })

        spawns: list[tuple[str, subprocess.Popen, Path]] = []
        main_log = tmpdir / "main.log"
        try:
            for label, raw in headers.spawns:
                # Drop the label word, expand placeholders, re-split.
                command_text = raw.strip()
                command_text = command_text[len(label):].strip() \
                    if command_text.startswith(label) else command_text
                argv = shlex.split(expand(command_text, values))
                log_path = tmpdir / f"spawn-{label}.log"
                handle = log_path.open("w", encoding="utf-8")
                proc = subprocess.Popen(
                    argv, stdout=handle, stderr=subprocess.STDOUT,
                    env=env, start_new_session=True)
                spawns.append((label, proc, log_path))

            for label, needle in headers.wait_for_logs:
                target = next(
                    ((p, lp) for lab, p, lp in spawns
                     if not label or lab == label),
                    None)
                if target is None:
                    result.status = "FAIL"
                    result.reason = \
                        f"wait_for_log names unknown spawn '{label}'"
                    break
                proc, log_path = target
                deadline = started + timeout
                while needle not in log_path.read_text(
                        encoding="utf-8", errors="replace"):
                    if proc.poll() is not None:
                        result.status = "FAIL"
                        result.reason = (
                            f"spawn '{label or proc.pid}' exited "
                            f"{proc.returncode} before printing '{needle}'")
                        break
                    if time.monotonic() > deadline:
                        result.status = "FAIL"
                        result.reason = \
                            f"timed out waiting for '{needle}' in spawn log"
                        break
                    time.sleep(0.02)
                if result.status == "FAIL":
                    break

            if result.status != "FAIL":
                with main_log.open("w", encoding="utf-8") as handle:
                    proc = subprocess.Popen(
                        self.main_argv(job, values), stdout=handle,
                        stderr=subprocess.STDOUT, env=env,
                        start_new_session=True)
                    try:
                        result.exit_code = proc.wait(timeout=timeout)
                        if result.exit_code != 0:
                            result.status = "FAIL"
                            result.reason = f"exit {result.exit_code}"
                    except subprocess.TimeoutExpired:
                        stop_process(proc)
                        result.status = "TIMEOUT"
                        result.reason = f"exceeded {timeout:g}s"
        except (OSError, ValueError) as exc:
            result.status = "ERROR"
            result.reason = str(exc)
        finally:
            for _, proc, _ in spawns:
                stop_process(proc)

        result.duration = time.monotonic() - started
        if result.status in ("FAIL", "TIMEOUT", "ERROR"):
            tails: list[str] = []
            for label, _, log_path in spawns:
                tails += [f"--- spawn {label} ({log_path.name}) ---"]
                tails += read_tail(log_path, self.args.tail)
            if spawns:
                tails.append("--- main ---")
            tails += read_tail(main_log, self.args.tail)
            result.tail = tails
        elif not self.args.keep_tmp:
            shutil.rmtree(tmpdir, ignore_errors=True)
            result.tmpdir = ""
        return result

    def run(self, jobs: list[Job]) -> list[Result]:
        batches: dict[str, list[Job]] = {}
        for job in jobs:
            batches.setdefault(job.batch, []).append(job)

        def run_batch(batch_jobs: list[Job]) -> list[Result]:
            return [self.run_job(job) for job in batch_jobs]

        results: list[Result] = []
        with concurrent.futures.ThreadPoolExecutor(
                max_workers=max(1, self.args.jobs)) as pool:
            futures = [pool.submit(run_batch, group)
                       for group in batches.values()]
            for future in futures:
                results += future.result()
        results.sort(key=lambda r: r.job.index)
        return results


# ---------------------------------------------------------------- report

def print_summary(results: list[Result]) -> None:
    counts = {"PASS": 0, "FAIL": 0, "TIMEOUT": 0, "SKIP": 0, "ERROR": 0}
    for result in results:
        counts[result.status] = counts.get(result.status, 0) + 1
        line = f"{result.status:<7} {result.job.name} ({result.duration:.2f}s)"
        if result.reason:
            line += f"  {result.reason}"
        print(line)
        if result.tail:
            for tail_line in result.tail:
                print(f"    {tail_line}")
            if result.tmpdir:
                print(f"    (kept: {result.tmpdir})")
    failed = counts.get("FAIL", 0) + counts.get("TIMEOUT", 0) + \
        counts.get("ERROR", 0)
    print("---")
    print(f"run={len(results)} passed={counts.get('PASS', 0)} "
          f"failed={counts.get('FAIL', 0)} timeout={counts.get('TIMEOUT', 0)} "
          f"skipped={counts.get('SKIP', 0)} errors={counts.get('ERROR', 0)}")
    print("RESULT:", "FAIL" if failed else "PASS")


def write_results_json(results: list[Result], path: str) -> None:
    failed = any(r.status in ("FAIL", "TIMEOUT", "ERROR") for r in results)
    payload = {
        "scripts": [{
            "name": r.job.name,
            "path": str(r.job.script),
            "passed": r.status in ("PASS", "SKIP"),
            "status": r.status.lower(),
            "expected_to_pass": True,
            "errors": 0 if r.status == "PASS" else 1,
            "timed_out": r.status == "TIMEOUT",
            "exit_code": r.exit_code,
            "duration_seconds": round(r.duration, 3),
            "reason": r.reason,
        } for r in results],
        "run": len(results),
        "passed": sum(1 for r in results if r.status == "PASS"),
        "failed": sum(1 for r in results
                      if r.status in ("FAIL", "TIMEOUT", "ERROR")),
        "skipped": sum(1 for r in results if r.status == "SKIP"),
        "success": not failed,
        "complete": True,
    }
    Path(path).write_text(json.dumps(payload, indent=2) + "\n",
                          encoding="utf-8")


def collect_scripts(args: argparse.Namespace) -> list[Path]:
    scripts = [Path(p) for p in args.scripts]
    if args.script_dir:
        scripts += sorted(Path(args.script_dir).glob("*.e2e"))
    return scripts


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="e2e_parallel_runner",
        description="Run .e2e scripts in parallel, one process each.")
    parser.add_argument("scripts", nargs="*", help=".e2e script files")
    parser.add_argument("--script-dir", help="directory of .e2e scripts")
    parser.add_argument("--binary", help="target binary; run as "
                        "'<binary> --test-script <script> <flags...>'")
    parser.add_argument("--command", help="command template using "
                        "{binary} {script} {port} {tmp} {index} {stem}")
    parser.add_argument("--extra-arg", action="append", default=[],
                        help="extra argv for the target (repeatable)")
    parser.add_argument("--jobs", type=int, default=4,
                        help="max scripts running at once (default 4)")
    parser.add_argument("--timeout", type=float, default=60.0,
                        help="default per-script seconds (default 60)")
    parser.add_argument("--tail", type=int, default=40,
                        help="log lines shown per failed process")
    parser.add_argument("--provides", default="",
                        help="comma-separated capabilities for # requires:")
    parser.add_argument("--tmp-root", help="parent dir for per-job temp dirs")
    parser.add_argument("--keep-tmp", action="store_true",
                        help="keep per-job temp dirs even on success")
    parser.add_argument("--results-json", help="write a results JSON file")
    args = parser.parse_args(argv)

    if not args.binary and not args.command:
        parser.error("give --binary or --command")
    scripts = collect_scripts(args)
    if not scripts:
        parser.error("no scripts: pass files or --script-dir")

    jobs = []
    for index, script in enumerate(scripts):
        try:
            text = script.read_text(encoding="utf-8", errors="replace")
            headers = parse_headers(text)
        except OSError as exc:
            headers = Headers(errors=[str(exc)])
        jobs.append(Job(index=index, script=script, headers=headers))

    results = Runner(args).run(jobs)
    print_summary(results)
    if args.results_json:
        write_results_json(results, args.results_json)
    return 1 if any(r.status in ("FAIL", "TIMEOUT", "ERROR")
                    for r in results) else 0


if __name__ == "__main__":
    sys.exit(main())

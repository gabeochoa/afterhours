# Parallel multi-process E2E runner

`tools/e2e_parallel_runner.py` runs `.e2e` scripts in parallel, one
process per script, so a suite (or a client + server pair) no longer
needs a hand-rolled spawn script per game. It is a Python tool because
the repo already ships Python tools users run
(`tools/sample_to_collapsed.py`); process groups, free-port allocation
and log tailing are stdlib work there, where a C++ version would need
a fork/exec/wait layer per platform. The header format it reads is
canonical in C++ (`src/plugins/e2e_testing/script_headers.h`,
std-only), so an in-process scheduler can parse the same scripts.

## Run it

```sh
python3 tools/e2e_parallel_runner.py --binary ./game --jobs 4 \
    --script-dir e2e_scripts --results-json results.json
python3 tools/e2e_parallel_runner.py --binary ./game a.e2e b.e2e
```

Without `--command` the target runs as the E2E harness expects:
`<binary> --test-script <script> <flags...>`, where `<flags...>` is
`--extra-arg`s followed by the script's `# flags:`. Any other target
shape takes a template:

```sh
python3 tools/e2e_parallel_runner.py \
    --command 'python3 fixture.py {script} {port} {tmp}' test.e2e
```

Placeholders: `{binary}` `{script}` `{port}` `{tmp}` `{index}` `{stem}`.
Useful flags: `--jobs N` (default 4), `--timeout S` (default 60),
`--tail N` (default 40), `--provides a,b`, `--tmp-root DIR`,
`--keep-tmp`, `--results-json PATH` (same shape as the runner's
`--results-json`: `scripts[]`, `run/passed/failed`, `success`).

Exit code is 0 only when nothing failed or timed out; skipped scripts
do not fail the run.

## Per-script headers

Headers are `#` comment lines before the first command in the script.
The single-process `E2ERunner` ignores them, so headed scripts still
run the old way.

| Header | Meaning |
| --- | --- |
| `# timeout: 30` | Wall-clock seconds for this script (overrides `--timeout`). |
| `# flags: --headless --seed=1` | Extra target argv, shell-split (quotes work). |
| `# requires: gpu, file:x, env:VAR, cmd:ffmpeg, os:darwin` | Unmet requirements **skip**, never fail. `file:`/`env:`/`cmd:`/`os:` check the machine; a bare word is a capability granted with `--provides`. |
| `# batch: name` | Scripts sharing a batch run sequentially, in file order. Different batches run in parallel up to `--jobs`. No header = its own batch. |
| `# spawn: server <cmd...>` | Helper process started before the target, killed after it. Repeatable. Placeholders expand. |
| `# wait_for_log: server:READY` | The target starts only once that spawned process prints the needle. A bare needle (no `label:`) means the first spawned process. |

Each job gets a free TCP port and a private temp dir, shared with its
spawned helpers so a server can bind and a client can connect:

- env: `E2E_PORT`, `E2E_TMPDIR` (also `TMPDIR`), `E2E_SCRIPT`, `E2E_INDEX`
- placeholders: `{port}`, `{tmp}`

A failed job's temp dir is kept (its path prints under the tail);
passing jobs clean up unless `--keep-tmp`.

## Sample run

```
PASS    menu (0.18s)
FAIL    combat (0.16s)  exit 3
    first
    boom
    (kept: /tmp/.../e2e-001-combat)
PASS    online (0.35s)
SKIP    needs_gpu (0.01s)  missing requires: cmd:no-such-cmd-xyz
---
run=4 passed=2 failed=1 timeout=0 skipped=1 errors=0
RESULT: FAIL
```

## Deferred

Screenshot baseline diffing (`expect_screenshot` / `--update-baselines`,
a separate gap), multiplayer orchestration beyond spawn / free ports /
log-wait, and CI integration.

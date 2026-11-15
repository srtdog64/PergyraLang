# Gates

Owner: Claude root lane. Checked 2026-11-15. Local results are from WSL
trees (`/root/cc-phi` side tree with the working changes, `/root/cc-nb` for
native trials); a local PASS is not CI evidence.

| Gate | Last observed result |
|---|---|
| Required CI at the published SHA | Last GREEN at `20325cad5` (run 43653186068). Later pushes: `f00fad7b8` failed on the native oracle (`local` reserved) and the cleanup gate, both fixed in `afbd9ab7f`; `e7bb64e06` failed on the word inventory and the ownership gate's public LLVM lane, both fixed in `24fe553f4`; the commit carrying this file is pending |
| Weekly full pipeline (`platform_full`, dispatch) | Run 43669288274 on `e1a9cba9e`: codegen and harness-evidence parity, Windows toolchain and formal proofs PASS; macOS/Windows transpile (fixed in `e7bb64e06`), semantic parity (fixed in `226613c3c`), Windows driver parity (composition root) and Linux driver/full (runner shutdown, time limit) open |
| SoT registry gate (`scripts/sot_registry_gate.py`) | PASS: 95 authorities, 213 derived carriers; CLOSED=70 BRIDGE=23 ACTIVE=2 (`selfhost.statement_result_type`, `selfhost.expression_place_kind` and `semantic.function_param_flow_summary` closed 2026-11-15) |
| Driver fixed point (gen2 == gen3), oracle/seed/join | PASS on CI at `a7d7655e1`, with the bootstrap policy corpus in the join job |
| Codegen bootstrap | PASS on CI at `a7d7655e1` |
| Bootstrap installed-driver jobs, CLI mode | PASS on CI at `a7d7655e1` |
| build-linux core shard | PASS on CI at `a7d7655e1`, including the builtin-effect and language-keyword registry checks and the native unsafe-negative refusal smoke; the Markdown contract step runs only on markdown-only changes |
| Contracts shard (identity probe in the shared probe build) | PASS on CI at `a7d7655e1` (job 16 min of the 30 min limit) |
| Backend compare (20 shards, 989 cases), sanitizers, native oracle | PASS on CI at `a7d7655e1` |
| macOS, Windows, formal proofs, tsan | PASS on CI at `a7d7655e1` |
| Memory sweeps (ASan; not CI gates) | Lifetime fixtures (868) on four legs: only endless recursion left. Every corpus (2597 programs, `/root/fx_wide.sh`): native C/LLVM only endless recursion after `226613c3c`; self-host C 40 and LLVM 10 open ([evidence](../evidence/2026-11-15_four_leg_wide_memory_sweep.md)) |
| LSP hover registry (`make self-host-lsp` first; not in a CI shard) | PASS locally on WSL at `751e5dd05` (language=27, builtin=7) |
| Entrypoint exit status on four legs (`entrypoint_exit_status_leg_owner.sh`, core shard) | 2026-11-15: PASS locally on WSL (`/root/mx-exit`, branch `claude/jovial-austin-8f780a`); FAIL on `a7d7655e1` binaries (native C exited 0 for `return 3`); not yet on CI |

Job times at `a7d7655e1`: seed part 22 min (limit 60), native oracle
13 min, build-linux 18 min, join 11 min, codegen bootstrap 19 min,
contracts 16 min.

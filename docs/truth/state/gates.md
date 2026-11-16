# Gates

Owner: Claude root lane. Checked 2026-11-17. Local results are from WSL
trees (`/root/mg-main` builds the integration head); a local PASS is not CI
evidence.

| Gate | Last observed result |
|---|---|
| Required CI at the published SHA | GREEN at `a1cb26a93` (run 43935444211, all 33 jobs); the commit carrying this file is pending |
| Weekly full pipeline (`platform_full`, dispatch) | Run 43914895554 on `3ad7444db`: Linux codegen seed and toolchain, Windows toolchain, formal proofs, and the parser, semantic, codegen, codegen-seed and harness-evidence parity shards PASS; macOS C-only, both driver shards, `platform-full-linux` (15 of 99 steps) and `platform-full-windows` (2 of 48) open ([blocker](blocker.md)) |
| SoT registry gate (`scripts/sot_registry_gate.py`) | PASS: 95 authorities, 214 derived carriers; CLOSED=70 BRIDGE=23 ACTIVE=2 |
| Driver fixed point (gen2 == gen3), oracle/seed/join | PASS on CI at `a1cb26a93` |
| Codegen bootstrap, installed-driver jobs | PASS on CI at `a1cb26a93` |
| Call hand-off rule (`call-handoff-rule-test-smoke`) and forged-row reader (`self-host-call-handoff-row-reader-test-smoke`) | PASS on CI at `a1cb26a93` (core and self-host shards) |
| Direct MIR general emitter coverage (`direct_mir_general_emitter_coverage_owner.sh`, core shard) | PASS on CI at `a1cb26a93` |
| Line caps (`semantic-tu-size`, `production-header-size`, `backend-inc-size`, `test-inc-size`, `semantic-core-shape`; weekly only) | FAIL on WSL at `f01cb5758`: rule header bodies, 2 headers over 600, semantic TUs over 599 |
| Memory sweeps (ASan; not CI gates) | Four legs, LLVM IR at -O0, at `a2b78e746`: lifetime fixtures (868) and tests/cases: only endless recursion, plus one unconfirmed native LLVM report ([evidence](../evidence/2026-11-16_llvm_legs_at_o0.md)) |
| LSP hover registry (`make self-host-lsp` first; not in a CI shard) | PASS locally on WSL at `751e5dd05` (language=27, builtin=7) |

Job times at `a1cb26a93`: self-host bootstrap 31 min, contracts 25 min
(limit 30), native oracle 17 min, join 13 min, codegen bootstrap 11 min.

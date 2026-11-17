# Gates

Owner: GPT root integration. Checked 2026-11-18. Local results are from WSL
trees (`/root/mg-main` builds the integration head); a local PASS is not CI
evidence.

| Gate | Last observed result |
|---|---|
| Required CI | Published/last wholly green revisions and the exact open job belong to [main](main.md). The candidate renderer repair passes locally. |
| String temporary seed bridge | Published CI PASS above; actual fixed point/driver, ASan/LSan, role C/LLVM and OOM controls PASS. Memory gates PASS; full DRV-2 OOM. Optional stale caps/qualified-kind/snapshot reds retained; [scope and hashes](../evidence/2026-11-18_selfhost_string_temporary_seed_bridge.md). |
| Declaration-ID joins | Published bootstrap, installed-driver, fixed-point join and contract jobs PASS above; [local hashes/limits](../evidence/2026-11-18_selfhost_ownership_declaration_identity.md). |
| Nested return transfer (local candidate) | Actual installed driver, preserved ownership gates, nested C/LLVM and invented-end refusals, ASan/LSan and four memory faults PASS; [scope/hashes](../evidence/2026-11-18_selfhost_nested_return_transfer.md). Full MIR passes the old transition and stops at ArrayReverse<String>. |
| A2 phi reuse / memory | PASS on runner at `7ea3c781f`: both backends/negatives; semantic MIR consumer 796,444 KiB <=800 MiB; [publication evidence](../evidence/2026-11-17_selfhost_memory_a2_ci_publication_fix.md) |
| A1 local integration | PASS on the [A1 source generation](../evidence/2026-11-17_selfhost_memory_a1_flat_projection_and_oom.md): codegen gen2 == gen3, fresh installed driver, complete MIR-lower C parity/RSS, C/LLVM lifetime negatives and nine OOM families. Published CI is above. Native LLVM release writer failed its optimizer assertion; LLVM dev wire/golden execution passed. |
| Published String allocation repair | `9f5573f18`: codegen bootstrap, 22 normal/OOM/unchecked controls and the phi consumer passed on CI. Five direct-MIR C/LLVM families and the bounded memory gate passed locally ([evidence](../evidence/2026-11-17_selfhost_string_allocation_boundary.md)); full DRV-2 still fails with typed OOM. |
| Role receiver dependency | Published CI PASS at `9049e547e`; local fixed point, installed role C/LLVM tests, 46 MIR refusals and String/intent controls passed. Full frozen MIR reaches collection transition 1054; [local limits](../evidence/2026-11-17_selfhost_memory_role_receiver_migration.md). |
| Weekly full pipeline (`platform_full`, dispatch) | Run 43914895554 on `3ad7444db`: Linux codegen seed and toolchain, Windows toolchain, formal proofs, and the parser, semantic, codegen, codegen-seed and harness-evidence parity shards PASS; macOS C-only, both driver shards, `platform-full-linux` (15 of 99 steps) and `platform-full-windows` (2 of 48) open ([blocker](blocker.md)) |
| SoT registry gate (`scripts/sot_registry_gate.py`) | PASS: 95 authorities, 214 derived carriers; CLOSED=70 BRIDGE=23 ACTIVE=2 |
| Driver fixed point (gen2 == gen3), oracle/seed/join | PASS on CI at `9f5573f18`, run 44073079061 |
| Codegen bootstrap, installed-driver jobs | PASS on CI at `9f5573f18`, run 44073079061 |
| Call hand-off rule (`call-handoff-rule-test-smoke`) and forged-row reader (`self-host-call-handoff-row-reader-test-smoke`) | PASS on CI at `a1cb26a93` (core and self-host shards) |
| Direct MIR general emitter coverage (`direct_mir_general_emitter_coverage_owner.sh`, core shard) | PASS on CI at `a1cb26a93` |
| Line caps (`semantic-tu-size`, `production-header-size`, `backend-inc-size`, `test-inc-size`, `semantic-core-shape`; weekly only) | FAIL on WSL at `f01cb5758`: rule header bodies, 2 headers over 600, semantic TUs over 599 |
| Memory sweeps (ASan; not CI gates) | Four legs, LLVM IR at -O0, at `a2b78e746`: lifetime fixtures (868) and tests/cases: only endless recursion, plus one unconfirmed native LLVM report ([evidence](../evidence/2026-11-16_llvm_legs_at_o0.md)) |
| LSP hover registry (`make self-host-lsp` first; not in a CI shard) | PASS locally on WSL at `751e5dd05` (language=27, builtin=7) |

Job times at `a1cb26a93`: self-host bootstrap 31 min, contracts 25 min
(limit 30), native oracle 17 min, join 13 min, codegen bootstrap 11 min.

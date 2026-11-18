# Gates

Owner: Claude lane (GPT root lane's work since 2026-11-19). Checked
2026-11-19. Local results are from WSL trees (`/root/ts2` builds the
candidate); a local PASS is not CI evidence.

| Gate | Last observed result |
|---|---|
| Required CI | Last green: [published main](main.md) `2c199fb28`, [run 44358794007](https://github.com/srtdog64/PergyraLang/actions/runs/44358794007), all 33 jobs, including self-host bootstrap (32 min), codegen bootstrap, installed-driver contracts, `formal-proofs-rocq9`, sanitizer and fixed-point join. |
| Collection program route deletion (published `34ae17546`, CI green) | Rewritten `one_mir_array_param_projection.sh` (general emitter, C/LLVM under ASan, 4 forged MIR refused) PASS on CI as `build-linux` push step 22, and locally with SoT edge, component contract, general-emitter coverage/builtin rows, Array<Int> return and Array<String> cleanup gates; leaks recorded; [run record](../evidence/2026-11-19_collection_program_route_deleted.md). |
| String temporary seed bridge | Published CI PASS above; actual fixed point/driver, ASan/LSan, role C/LLVM and OOM controls PASS. Memory gates PASS; full DRV-2 OOM. Optional stale caps/qualified-kind/snapshot reds retained; [scope and hashes](../evidence/2026-11-18_selfhost_string_temporary_seed_bridge.md). |
| Declaration-ID joins | Published bootstrap, installed-driver, fixed-point join and contract jobs PASS above; [local hashes/limits](../evidence/2026-11-18_selfhost_ownership_declaration_identity.md). |
| Nested return transfer (published) | Actual installed driver, preserved ownership gates, nested C/LLVM and invented-end refusals, ASan/LSan and four memory faults PASS; [scope/hashes](../evidence/2026-11-18_selfhost_nested_return_transfer.md). Its former full-MIR stop is superseded by [blocker](blocker.md). |
| Typed array-transform v5 (published) | Actual installed driver; 10 C/LLVM direct executions, forged ABI and retaining String alias refusals PASS. Isolated ASan/LSan checks and relevant mutants PASS; retained Int/Bool record cleanup remains unproved. Required 33-job CI is green at [main](main.md). Full one-process DRV-2 source-to-C remains OOM. [Run record](../evidence/2026-11-18_selfhost_array_transform_lifetime.md). |
| Bare Array<String> return and fresh copying push source v8 (published) | Actual Pergyra-built driver, C/LLVM `-O0` ASan/LSan, omitted-free leak mutant, forged-origin refusal, previous output byte equality, existing cleanup gate and component contract PASS. Exact published CI above passed. Fixed full MIR still stops at transition ordinal 1438 without output; scratch [census](../../audits/selfhost_full_mir_independent_refusal_census_2026-11-18.md) reached all route instances but is not admission evidence. [Local run record](../evidence/2026-11-18_selfhost_bare_array_return_and_push_operand_lifetime.md). |
| Inout member restoration v11 (published `a9fbfd298`, CI green) | Actual Pergyra-built driver, focused C/LLVM ASan and three-field ASan/LSan, hand-off refusal, forged MIR integrity refusal, existing cleanup gate, component contract and keyword registry PASS. Fixed full MIR returns to the prior first refusal ordinal 1438; no C output. Nonempty record LSan still leaks 32 bytes per backend. [Run record](../evidence/2026-11-18_inout_member_restore_transition.md). |
| A2 phi reuse / memory | PASS on runner at `7ea3c781f`: both backends/negatives; semantic MIR consumer 796,444 KiB <=800 MiB; [publication evidence](../evidence/2026-11-17_selfhost_memory_a2_ci_publication_fix.md) |
| A1 local integration | PASS on the [A1 source generation](../evidence/2026-11-17_selfhost_memory_a1_flat_projection_and_oom.md): codegen gen2 == gen3, fresh installed driver, complete MIR-lower C parity/RSS, C/LLVM lifetime negatives and nine OOM families. Published CI is above. Native LLVM release writer failed its optimizer assertion; LLVM dev wire/golden execution passed. |
| Published String allocation repair | `9f5573f18`: codegen bootstrap, 22 normal/OOM/unchecked controls and the phi consumer passed on CI. Five direct-MIR C/LLVM families and the bounded memory gate passed locally ([evidence](../evidence/2026-11-17_selfhost_string_allocation_boundary.md)); full DRV-2 still fails with typed OOM. |
| Role receiver dependency | Published CI PASS at `9049e547e`; local fixed point, installed role C/LLVM tests, 46 MIR refusals and String/intent controls passed. Full frozen MIR reaches collection transition 1054; [local limits](../evidence/2026-11-17_selfhost_memory_role_receiver_migration.md). |
| Weekly full pipeline (`platform_full`, dispatch) | Run 43914895554 on `3ad7444db`: Linux codegen seed and toolchain, Windows toolchain, formal proofs, and the parser, semantic, codegen, codegen-seed and harness-evidence parity shards PASS; macOS C-only, both driver shards, `platform-full-linux` (15 of 99 steps) and `platform-full-windows` (2 of 48) open ([blocker](blocker.md)) |
| SoT registry gate (`scripts/sot_registry_gate.py`) | PASS at `2c199fb28` (CI and local): 94 authorities, 210 derived carriers; CLOSED=70 BRIDGE=22 ACTIVE=2 |
| Driver fixed point (gen2 == gen3), oracle/seed/join | PASS on CI at `2c199fb28`, run 44358794007 |
| Codegen bootstrap, installed-driver jobs | PASS on CI at `2c199fb28`, run 44358794007 |
| Call hand-off rule (`call-handoff-rule-test-smoke`) and forged-row reader (`self-host-call-handoff-row-reader-test-smoke`) | PASS on CI at `a1cb26a93` (core and self-host shards) |
| Direct MIR general emitter coverage (`direct_mir_general_emitter_coverage_owner.sh`, core shard) | PASS on CI at `a1cb26a93` |
| Line caps (`semantic-tu-size`, `production-header-size`, `backend-inc-size`, `test-inc-size`, `semantic-core-shape`; weekly only) | FAIL on WSL at `f01cb5758`: rule header bodies, 2 headers over 600, semantic TUs over 599 |
| Memory sweeps (ASan; not CI gates) | Four legs, LLVM IR at -O0, at `a2b78e746`: lifetime fixtures (868) and tests/cases: only endless recursion, plus one unconfirmed native LLVM report ([evidence](../evidence/2026-11-16_llvm_legs_at_o0.md)) |
| LSP hover registry (`make self-host-lsp` first; not in a CI shard) | PASS locally on WSL at `751e5dd05` (language=27, builtin=7) |

Job times at `a1cb26a93`: self-host bootstrap 31 min, contracts 25 min
(limit 30), native oracle 17 min, join 13 min, codegen bootstrap 11 min.

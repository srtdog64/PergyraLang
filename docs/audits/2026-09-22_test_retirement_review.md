# Test retirement review (2026-09-22)

Status: AUDIT RECORD, not a gate or an owner. Snapshot:
`main@6e1eba06`; the removals landed in `401f53fb`.

Three read-only lanes classified the tracked tests against HEAD: assertions
(48,500 parsed checks), unreached gates (573 scripts run on a HEAD build), and
duplicates and orphans (3,183 fixture and data candidates). Their tables are
under `.tmp/test-retirement-2026-09-21/`.

## The main result: most dead-looking tests are not obsolete

Of the 573 tracked tests that no automatic runner executes:

| Verdict | Count | Meaning |
| --- | ---: | --- |
| WIRE | 208 | passes at HEAD; only its runner is missing |
| REPOINT+WIRE | 109 | subject alive; a pin went stale |
| REPAIR+WIRE | 57 | red because it catches a real defect |
| KEEP-MANUAL | 31 | a declared manual probe or census |
| RETIRE | 5 | subject gone or covered by a reached gate |

Deleting unreached tests would mostly hide defects. The 57 red ones include a
missing bounds check on public LLVM `Array<String>` reads, TextBuilder
single-owner violations accepted on the default route, and a struct
constructor call with too few arguments filling the rest with zero on the
native pipeline. The next step for this population is wiring and repair, not
removal; `dead_gates_classification.tsv` lists every row with its first
failing line.

## Removed in 401f53fb

Removed only where nothing reads the item and no check is lost, each verified
by hand against every reference in the repository, including docs:

- ten committed build outputs (seven `tests/capability` ELF binaries, three
  `tests/gyri_*` ELF binaries) and two `.ll` files;
- 18 fixtures nothing consumes, including one pinned only by a `require_file`
  line (the pin went with it);
- three `machine_layer_*_parity.sh` wrappers with no assertion of their own
  that printed SKIP with exit 0, and their manual-inventory rows. **Restored
  in the next source commit:** `tests/self_hosted_scaffold_smoke.sh` requires
  a parity script for every tool under `src/self_hosted/tools/`, and these are
  the ones for `machine_layer_air_validator`, `machine_layer_mir_projection_probe`
  and `machine_layer_rir_validator`. The verification below did not run that
  gate, and the CI for this commit ran only the markdown-only jobs, so the
  break first showed in a local run of the self-host push shard;
- four uncalled shell functions;
- in `semantic_core_shape_smoke.sh`, a deleted file dropped from a fifteen-file
  `grep -R` whose exit 2 had disabled the whole check since June, one check on
  that deleted file, and one check re-pointed to its renamed successor.

Verified on a `git archive` of HEAD with only this change applied: the
component contract end to end, semantic core shape, gate reachability,
build source inventory and documentation quality. That set missed
`self-host-preparation-contract-test-smoke`, which runs the scaffold gate.

**Not removed, although unused:** `src/self_hosted/lexer/expected/clean.txt`.
It is the only file in a directory the component contract requires; removing
it made that gate red.

## Not removed: needs a decision or an edit elsewhere

| Item | Why it stayed |
| --- | --- |
| 15 `backend_compare` case pairs with identical `main.pgy` | deleting one of each also means editing the `compare_backends.sh` case array and case-count pins; two pairs are named by `test_harness_backend_compare_paths_owner.pgy` |
| 3 parser fixture pairs with identical source and AST | `ParserFixtureManifestCount` 189 would become 186 in a compiled owner |
| `.ps1` twins (`perf_c_baseline_smoke`, `grammar_*`, `capability/run_manifest`) | the Makefile selects two of them on Windows, and `perf_contract_smoke.sh:147-148` pins the perf one |
| a script run twice in one job (`Makefile:3054` vs `:2561`), the `self-host-lex-minimal` alias | the Makefile is part of another session's uncommitted work |
| the two medium-confidence RETIRE candidates `mir_resource_graph_owner_smoke.sh`, `driver_rung2_generic_default_contract_parity.sh` | medium confidence: the successor's coverage was not shown assertion by assertion |
| orphan fixtures a README describes as checks (`tests/self_hosted/fixtures/*_fact.pgy`, four `concept_semantics/authority_effect/generic_identity_*`) | wire them or fix the README; either is a decision |
| `tests/alpha_full_keyword_test.pgy` | named by `docs/47` |
| the no-python fallbacks (`run_literal_*`, 940 assertions, already rotted in `cfg_body_dataflow_smoke.sh`) | they exist for hosts without Python; dropping them is a support decision |
| 40 `examples/` files no checker reads, and `examples/calendar/` superseded by `calendar_working` | a documentation decision |

## Checks that pass without testing anything (repair, not removal)

- `rg`-based negative checks: GitHub runners have no `rg`, so they cannot fail
  there (`domain_runtime_zone_identity_direct_mir_owner.sh:125` logs
  `rg: command not found` in platform-full run 35526838442). Eight sites.
- Skips that exit 0 in every CI environment: `backend_compare_bc_on_smoke.sh`
  (`scripts/build_runtime_bc.sh` is mode 100644, so the build is "Permission
  denied" and the gate skips), the `dogfood_webgl` wasm leg (no emscripten),
  the `driver_execution_action_optional_within` stage 2 (its variable is never
  set), the fuzz generator's LLVM leg (any LLVM compile failure reads as a
  skip), and three env-gated branches set nowhere.
- 37 assertions already red at HEAD, all in scripts push CI does not run;
  `perf_contract_smoke.sh:2387` also fails in the weekly platform run.

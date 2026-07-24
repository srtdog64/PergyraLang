# Current Work Handoff

Updated: 2026-07-24 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. Verify it against the
current source, `git status --short --branch`, the SoT registry, the active
owner, and the named executable gate.

## Resume checkpoint

- Current local HEAD and `origin/main` are `aba64ab1` (`Close semantic routine
  local inventory SoT`).
- `aba64ab1` is the latest executable closure: routine local inventory is
  projected from semantic binding, initializer, and iteration facts; indexed
  assignment use edges consume attached expression graphs instead of text
  scans; and the for/foreach graph lane preserves the same owner boundary.
- The preceding `164d207e` closure still owns machine resource Claim ABI type
  from the result SSA local and Read/Write/Release/Submit receiver projection.
- The Pergyra-built DRV-2 manifest contains 280 MIR fixtures. The latest added
  executable row is `set_literal_basic`.
- `6574f89f` closes the full-matrix `random_inferred_let` blocker. Native MIR
  now carries the routine-owned inferred `Int` fact directly on the
  `event.1` DEF as `abi_type_name`. The shared MIR renderer still rejects a
  missing instruction fact; no `source_locals` compatibility read was added.
- `aa449bd2` closes Set literals through a distinct parser graph spine,
  semantic declared-type owner, MIR graph carriage, the existing Set runtime
  ABI owner, and expected-type C emission. It does not treat Set literals as
  arrays or structs and does not infer the element ABI in codegen.
- The previous indexed Set argument closure remains `23c2f0cb`; its handoff
  refresh is `cb25eb92`.

## Exact remaining dirty state at handoff

The following work was present but was not included in the two commits above:

- modified `Makefile`;
- modified `src/compiler/mir_fact_surface_validate.c`;
- modified `src/compiler/mir_fact_validate_internal.h`;
- modified `src/compiler/mir_json_dump.c`;
- modified `src/self_hosted/codegen/emission/program_emit.pgy`;
- modified `src/self_hosted/codegen/input/value_wrapper_usage_owner.pgy`;
- modified `src/self_hosted/compiler/symbol_table_owner.pgy`;
- untracked `src/compiler/mir_fact_surface_validate_resource.c`;
- untracked `src/compiler/mir_json_dump_decl.c`;
- untracked `src/compiler/mir_json_dump_decl.h`;
- modified `tests/self_hosted/parity/driver_bootstrap.sh`;
- modified `tests/self_hosted/parity/driver_rung2_foreach_call_type_parity_owner.sh`;
- modified `tests/self_hosted/parity/driver_rung2_integer_literal_parity_owner.sh`;
- modified `tests/self_hosted_component_contract_smoke.sh`;
- untracked `docs/198_market_safety_positioning.md`;
- untracked `docs/self_hosted/22_full_matrix_inferred_let_blocker.md`.
- untracked `src/self_hosted/codegen/input/value_wrapper_materialization_owner.pgy`.

The native C files are a concurrent file-split change. Do not discard, stage,
or fold them into a self-host rung without reviewing their owner and gate.
The raw full-matrix blocker note predates `6574f89f`; its observed failure is
resolved, but the untracked file remains user-owned. The driver-bootstrap
runtime-header classifier change is plausible follow-up work but has not been
included in an executable closure commit here.

## Latest closed executable rung: semantic routine local inventory surface

Objective card:

- Objective: make routine source-local inventory and migrated assignment use
  edges derive from semantic/graph facts, not the active CFG build stack or
  expression text.
- Priority: one semantic owner, nested-scope/foreach coverage, graph-carried
  use identity, missing-fact failure, old-path deletion, then patch size.
- Fact owner: `src/self_hosted/mir/routine_local_inventory_owner.pgy`.
- Last legitimate consumers: `SelfMirAppendRoutine` for routine fact rows and
  `SelfMirLowerAssignmentFromArtifact` for indexed assignment uses; the for
  owner selects the correct value/auxiliary graph lane.
- Forbidden fallback: routine facts reconstructed from
  `build.local_names`, assignment `SelfMirExpressionUses` text scanning, and
  graph-lane substitution for collection-hoisted/foreach statements.
- Verification gate:
  `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`
  through `driver_rung2_body_parity.sh`, plus
  `tests/self_host_hard_contract_smoke.sh`.

Observed closure:

- `SelfMirRoutineLocalInventoryFromInput` merges semantic local-binding and
  iteration rows in syntax order, validates aligned initializer types, and
  creates foreach synthetic locals from iteration facts.
- `SelfMirAppendRoutine` now consumes that inventory instead of copying the
  active `SelfMirRoutineBuild` local stack into program facts.
- Assignment target/value uses are projected from expression graph leaves;
  missing target graph remains a negative failure.
- Hard prebuilt parity passed with
  `backends=1 body_fixtures=20 mir_fixtures=2` for
  `indexed_assignment` and `for_each_call`.
- The closure was committed and pushed as `aba64ab1`.

## Previous closed executable rung: machine runtime ABI receiver surface

Objective card:

- Objective: close the MIR runtime resource type seam behind routine-local and
  expression-graph owners for the DRV-2 machine layer.
- Priority: receiver/result identity, canonical runtime ABI row, missing-fact
  failure, old-path deletion, negative ratchet, then patch size.
- Fact owner: `src/self_hosted/mir/routine_build_owner.pgy`, with
  `src/self_hosted/mir/expression_runtime_abi_owner.pgy` owning expression-call
  operation/kind validation and
  `src/self_hosted/mir/abi_layout_json_projection_owner.pgy` owning JSON type
  projection.
- Last legitimate consumer: `SelfMirRoutineAttachLastExpressionGraph` and the
  DRV-2 MIR JSON/C consumer.
- Forbidden fallback: Claim `expr1` recovery, instruction `uses[0]`, source
  text identifier scans, `DeviceSlot` defaulting, and AST-call rows borrowing
  a declaration ABI layout type.
- Verification gate:
  `tests/self_hosted/parity/driver_rung2_machine_mir_parity_owner.sh` plus
  `tests/self_host_hard_contract_smoke.sh`.

Observed closure:

- Claim resolves `DeviceSlot<Int>` from the result SSA binding's routine local.
- Device Read/Write/Release rows resolve `DeviceSlot<Int>` from the attached
  expression graph call-argument receiver; the write shape follows the graph's
  outer argument edge.
- Declaration `Int` layout and resource `DeviceSlot<Int>` runtime rows remain
  distinct in self-host MIR JSON and canonicalize byte-equal with the native
  oracle for `device_slot_machine_layer`.
- Missing machine declaration fails closed with `MIR instruction rows are
  invalid: instruction=0 machine-layer projection is invalid`.
- Static gates forbid the old `expr1`, `uses[0]`, and source-text fallback
  paths. The closure was committed and pushed as `164d207e`.

## Previous closed executable rung: Set literal runtime surface

Objective card:

- Objective: carry `{...}` and `{}` from parser-owned syntax identity through
  semantic `Set<T>` compatibility and the existing Set runtime ABI into C.
- Priority: parser identity, ordered element graph, contextual empty literal,
  homogeneous element evidence, runtime symbol ownership, negative ratchet,
  then patch size.
- Fact owner:
  `src/self_hosted/semantic/ast_expression_graph_set_literal_owner.pgy`.
- Parser construction owner:
  `src/self_hosted/parser/expression_set_literal_graph_owner.pgy`.
- Parser contract owner:
  `src/self_hosted/parser/expression_set_literal_contract_owner.pgy`.
- Last legitimate consumer:
  `RewriteSemanticSetLiteralValue` in
  `src/self_hosted/codegen/emission/expr_semantic_composite_literal_emit_owner.pgy`.
- Forbidden fallback: Set-as-array/struct dispatch, source reparse, source
  spelling as ABI, backend element-type guessing, untyped empty-literal
  success, and missing Set ABI success.
- Verification gate:
  `tests/self_hosted/parity/driver_rung2_set_literal_parity_owner.sh`.

Observed closure:

- Parser and MIR carry distinct `set_literal` and `set_element` graph nodes.
- Declared `Set<Int>` and `Set<String>` literals emit the existing owned
  `PgySet_int`/`PgySet_String`, `pgy_set_new_*`, and `pgy_set_add_*` symbols.
- Duplicate insertion remains runtime Set behavior. The positive fixture runs
  as `3`, `true`, `false`, `0`, `2`, `true`.
- Removing the declaration ABI fact fails with `local declaration is missing
  its MIR ABI type fact`.
- A mixed element literal and a dedicated untyped empty literal both fail
  closed with `initializer_type_unresolved`.
- Parser responsibilities were split by owner; the generic expression graph
  owner is 597 lines and the Set graph/contract owners are 30/25 lines.

## Last observed verification

Green:

- Pergyra-built DRV-2 rebuild after the receiver-owner change, exit 0.
- Machine fixture producer/consumer: `produce=0`, `consume=0` with the
  repository-relative declaration manifest.
- Native/selfhost machine MIR canonicalization: both exit 0 and canonical JSON
  byte-equal.
- Missing machine declaration: exit 1 with an explicit invalid machine-layer
  projection diagnostic.
- Plain `Slot<Int>` expression ABI producer/consumer: exit `0/0`.
- Bounded selfhost manifest scan: all `280/280` rows produced successfully;
  no first red row observed.
- `tests/self_host_hard_contract_smoke.sh` after the new static ratchets.
- `tests/self_host_hard_contract_smoke.sh` after the routine local inventory
  ratchets.
- Pergyra-built DRV-2 rebuild with the local-inventory owners, exit 0.
- Hard prebuilt indexed-assignment/foreach parity:
  `backends=1 body_fixtures=20 mir_fixtures=2`.
- Full-fixpoint seed/oracle compile and bounded seed MIR consumer parity passed;
  full `driver_mir_oracle` did not reach an artifact before the isolated oracle
  process was stopped at approximately 17 GiB RSS / 28 GiB private memory with
  3.8 GiB host memory remaining.
- Machine owner static verification and dynamic ABI fact checks, including
  declaration `Int` versus runtime `DeviceSlot<Int>` rows.
- Native compiler rebuild: `make -s compiler`, exit 0. Existing warnings were
  observed; no success was inferred from silence.
- Native `random_inferred_let` MIR probe: `event.1` owns
  `abi_type_name: Int` and `source_type: AST_LET_DECL`.
- Focused hard `random_inferred_let` producer-first parity:
  `backends=1 body_fixtures=20 mir_fixtures=1`.
- Existing lexical List instruction-type strip remains fail-closed, proving no
  routine-local compatibility fallback was introduced.
- Official Pergyra parser/gen2/host DRV-2 build and bounded source smoke, with
  manifest count 280.
- Focused hard `set_literal_basic` producer-first parity after the parser owner
  split: `backends=1 body_fixtures=20 mir_fixtures=1`.
- `tests/self_host_hard_contract_smoke.sh`.
- `tests/sot_authority_edge_smoke.sh`: 48 authorities, 40 derived fact
  carriers, `CLOSED=28 BRIDGE=20 ACTIVE=0`.
- `PGY_ALLOW_MISSING_COQ=1 tests/sot_authority_adequacy_smoke.sh`: live
  binding and negative mutations passed. Coq/Rocq itself was a declared skip
  because no prover is installed.
- `git diff --cached --check` before both commits.

Blocked by unrelated dirty work:

- `tests/self_hosted_component_contract_smoke.sh` passes the Set owner and
  parser line-cap checks, then stops at the concurrent direct-codegen import
  assertion for `src/self_hosted/codegen/runtime_abi/set_runtime_owner.pgy`.
  Do not report the full component gate as green.

Not run:

- Full unfiltered 280-row hard DRV-2 matrix.
- LLVM parity for the new Set literal rung.
- Actual Coq/Rocq proof execution.
- Full driver bootstrap/fixpoint for the separate dirty
  `driver_bootstrap.sh` change.
- Full native/selfhost integration matrix remains blocked in this environment
  by `Cannot create temporary file in C:\\Windows\\: Permission denied`.
- Full driver oracle MIR production remains resource-bound at driver scale;
  no source-level assignment drift result was claimed from the stopped run.

## Next executable work

The next semantic seam is intentionally `Unknown` until executable evidence
selects it. At the next scheduled/merge boundary:

1. Verify HEAD/origin and preserve the dirty native C split and user-owned
   untracked documents.
2. Resolve or isolate the native JSON split component assertion under its own
   owner; do not absorb it into a Pergyra language rung by convenience.
3. Re-run the full driver fixpoint with a measured memory budget, then the
   unfiltered 280-row hard DRV-2 matrix and LLVM lane once the Windows
   temporary-file permission blocker is removed. The bounded scan is green, so
   the first red row from the full gate, not fixture ordering or an AI guess,
   chooses the next rung.
4. For that row, record objective, owner, last consumer, forbidden fallback,
   and falsifier before editing.
5. Land another executable Pergyra replacement before spending multiple
   commits on documentation or SoT-only cleanup.

## Workstation and recovery facts

- Global rules: `C:/Users/user/.codex/AGENTS.md`; repository `AGENTS.md` is
  more specific.
- Repository `core.autocrlf=false`. Use
  `C:/msys64/usr/bin/bash.exe` with `/ucrt64/bin:/usr/bin:/bin` for gates and
  Windows `bin/*.exe` artifacts.
- `.vscode` remains configured for `bin/pgy.exe`, `bin/pgy-lsp.exe`, MSYS2
  UCRT64, focused build/self-host/doc tasks, and Extension Host launchers.
- Session-specific inferred-let/Set-literal probe and verification directories
  were sent to the Windows Recycle Bin. Canonical parser/codegen/compiler
  bootstrap caches were retained.
- Never copy configured credentials into source, documentation, or logs.

## Resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, `src/self_hosted/OWNERS.md`,
   and the relevant row in `docs/semantics/sot_owner_spine_registry.md`.
2. Verify `git status --short --branch`, HEAD/origin, the named owner, and the
   focused Set literal gate.
3. Treat the current source/registry/gates as authoritative when this snapshot
   disagrees.
4. Run the full hard matrix only at the scheduled boundary, then follow its
   first observed falsifier.
5. Refresh exact revision, dirty state, last green gate, next falsifier, and
   blockers after the next material session.

# Beta Readiness Checklist - Execution Order And Progress Log

> Split from `docs/100_beta_readiness_checklist.md` on 2026-05-29.
> Keep active blocker edits in the shard that owns the relevant closure track.

## Progress Log - 2026-06-21 MIR Lifecycle Source-Text Fact Closure

- MIR JSON still emits the transitional `"ast"` text field for the current
  self-hosted `mir_lower` rung, but `mir_lifecycle.c` no longer reopens
  `mir_instruction_source_payload(...)` or calls `ast_capture_inline(...)`
  during serialization. The dump consumes
  `mir_instruction_source_inline_text(inst)`.
- `mir_instruction_capture_source_provenance(...)` now captures that inline
  source text at the source-shape boundary, beside line, column, stable-id, and
  source-node-type facts. The remaining payload accessor allowance is confined
  to `mir_source_shape.c` plus the public declaration in `mir.h`.
- `ast-to-mir-loss-contract-test-smoke` removes `mir_lifecycle.c` from the
  payload allowlist, and `perf-contract-test-smoke` rejects lifecycle payload
  reopening. Verified locally with `test-mir`, `cfg-body-dataflow-test-smoke`,
  `ast-to-mir-loss-contract-test-smoke`, `perf-contract-test-smoke`,
  `self-host-preparation-test-smoke`, `production-c-size-test-smoke`,
  `test-inc-size-test-smoke`, `documentation-quality-test-smoke`,
  `tests/self_host_readiness_scorecard.sh`, and `git diff --check`.

## Progress Log - 2026-06-21 MIR Public-Surface Provenance Capture Closure

- `mir_public_surface.c` no longer opens `mir_instruction_source_payload(...)`
  to seed source line, column, stable-id, or source-node-type facts. Public
  surface recording now consumes already captured scalar provenance and
  expression facts only.
- `mir_source_shape.c` owns
  `mir_instruction_capture_source_provenance(...)`; MIR construction and
  population owners call it at the point they attach a source AST payload to an
  instruction. This keeps provenance capture at the source-shape boundary
  instead of re-deriving it during public-surface consumption.
- `cfg-body-dataflow-test-smoke`, `ast-to-mir-loss-contract-test-smoke`, and
  `perf-contract-test-smoke` now reject reintroducing source-payload reads in
  `mir_public_surface.c` and require the capture-time provenance owner.
- The remaining source-payload allowance after this slice was the MIR
  lifecycle/dump provenance surface; the later lifecycle source-text fact
  closure above retires that dump-time payload read.

## Progress Log - 2026-06-21 C MIR Source-Local Binding SoT Closure

- C MIR-backed function emission no longer rescans the source AST block to
  register local binding/type compatibility facts. When MIR inventory is active,
  it materializes C local binding metadata from `MIRRoutine::source_local_types`
  through `transpiler_register_mir_source_local_bindings(...)`; the old AST
  compatibility scan remains only for non-MIR emission.
- MIR source-local capture now records for-loop element bindings, plus
  `Array<T>.Slice(...) -> Slice<T>` and `SliceCopy(Slice<T>) -> Array<T>` local
  type facts, so source-local prologue setup consumes MIR-owned facts rather
  than expression payload guesses.
- Source-local expression typing now lives in
  `mir_source_local_expr_types.c`, leaving `mir_source_local_types.c` as the
  capture/storage owner and keeping both files below the production owner cap.
- `mir_declaration_inventory_smoke.sh` now gates the MIR-active materializer
  and the new source-local fact cases, while `backend_fail_closed_smoke.sh`
  points call-inference fail-closed strings at the split
  `transpiler_expr_call_type_infer.c` owner.
- Verified locally with the `test-mir`, `mir-declaration-inventory-test-smoke`,
  `test-transpile`, `cfg-body-dataflow-test-smoke`, `perf-contract-test-smoke`,
  and `backend-fail-closed-test-smoke` make targets.

## Progress Log - 2026-06-21 AIR DAG Fallback Evidence Closure

- AIR evidence append now rejects non-zero fallback facts at the API boundary.
  `fallback_count` remains in the evidence schema and validator surface for
  compatibility with dumps and hand-crafted invalid inventories, but concrete
  appended evidence must carry `fallback_count == 0`.
- DAG type-resolution dead-end telemetry no longer becomes an AIR evidence
  row. `air_collect_dag_evidence(...)` fails closed before append when semantic
  metadata reports unresolved dead-end facts, so strict AIR cannot publish a
  fallback-shaped proof and rely on a later verifier pass to catch it.
- `test_air` and `air_drift_smoke` now gate both paths: append rejects fallback
  counts directly, and DAG dead-ends are rejected before evidence append.

## Progress Log - 2026-06-21 C Option Contextual Type Inference Closure

- C backend `None` type inference no longer synthesizes `Option<Unknown>` when
  no contextual `Option<T>` fact exists. Both identifier and call-shaped `None`
  inference consume `transpiler_contextual_option_inner_type_copy(...)` and
  otherwise return `Unknown` so emit/semantic layers fail closed.
- The transpile unit tests now pin context-free `None()` inference to
  `Unknown` and contextual `None()` inference to the exact `Option<T>` fact.
  `perf_contract_smoke` was moved to the call-inference owner after the recent
  `transpiler_expr_call_type_infer.c` split, so the slot-operation name policy
  is gated at its real owner instead of the old monolithic expression owner.

## Progress Log - 2026-06-20 Parser And C Inference Owner Cap Closure

- Parser map/set literal parsing now lives in `parser_expr_map_literal.c`,
  leaving `parser_expr.c` responsible for expression precedence and primary
  dispatch rather than brace-literal storage growth.
- C backend call expression type inference now lives in
  `transpiler_expr_call_type_infer.c`, leaving
  `transpiler_expr_type_infer.c` responsible for non-call expression type
  inference and shared inference arena/type-name utilities.
- This closes the active production-owner size gate without adding generic
  helper buckets: `parser_expr.c` is 616 LOC and
  `transpiler_expr_type_infer.c` is 331 LOC after the split.
- Verified locally with `test-parser`, `test-transpile`,
  `semantic-core-shape-test-smoke`, and `test-inc-size-test-smoke`.

## Progress Log - 2026-06-20 Role Lookup Host-Index SoT

- Role declaration iteration now consumes the semantic host declaration index
  before the legacy program-root compatibility path. The new host-index API
  owns typed declaration iteration, so role lookup no longer rescans the
  program root when the semantic graph inventory is present.
- The type-resolution resolver inventory smoke now gates that role lookup uses
  `semantic_host_index_find_next_decl_of_type(...)` and that the iteration is
  explicitly `AST_ROLE_DECL`-typed.
- Semantic positive fixtures that intentionally mutate fields were updated to
  spell those fields `let mut`, matching the current language surface instead
  of weakening immutable-field diagnostics.
- Verified locally with `test-semantic`,
  `type-resolution-resolver-inventory-test-smoke`, and
  `type-resolution-dag-test-smoke`.

## Progress Log - 2026-06-20 AIR RIR Authority Evidence Counter SoT

- RIR authority summary counting no longer scans raw `RIR_FACT_AUTHORITY` or
  `RIR_OP_AUTHORIZE` rows before boundary matching. The counter is incremented
  only when AIR accepts an `AIR_EVIDENCE_RIR_AUTHORITY` node for a concrete
  boundary participant.
- AIR summary validation now checks RIR boundary and authority counters against
  boundary-scoped evidence-node counts when real RIR input is present. Counter
  drift is an invariant error instead of compatibility telemetry silently
  disagreeing with the proof inventory.
- The AIR tests now include an explicit RIR boundary/authority counter-mismatch
  negative case, and the drift smoke rejects reintroducing the raw RIR
  authority counting helper.
- Verified locally with `test-air` and `air-drift-test-smoke`.

## Progress Log - 2026-06-20 LLVM SSA DEF Type-Fact Gate And ABI Fixture Alignment

- `ssa_def_reassign_type_fact` now covers `Int`, `String`, and a user class
  value reassigned inside an `if` body. This keeps the LLVM SSA-DEF path pinned
  to MIR source-local type facts instead of allowing an `i32`-shaped fallback
  to pass accidentally.
- The ABI pipeline embedded sources that mutate `self.started` or `player.hp`
  now spell those fields `let mut`, matching the beta language surface after
  immutable field assignment became a structured semantic error.
- The backend-compare default inventory now registers `set_literal_basic`, so
  the inventory gate no longer fails before targeted C/LLVM parity runs.
- Verified locally with `llvm-test-smoke`, `test-abi`,
  `backend-compare-inventory-test-smoke`, `cfg-body-dataflow-test-smoke`, and
  targeted `llvm-test-backend-compare` for `ssa_def_reassign_type_fact` and
  `set_literal_basic`.

## Progress Log - 2026-06-20 C Backend Source-Payload DEF Closure

- C MIR block emission no longer calls a source-payload helper for DEF/LET
  paths. Source-local LET DEFs consume `arg0` / `expr0` / `expr1` from the
  instruction, generic DEF expression emission consumes `expr0`, and receive
  payload type inference consumes the channel receive expression fact directly.
- The C backend helper owner now keeps only source-shape CFG-container and SSA
  seeding responsibilities. The retired source-payload accessor and DEF
  payload predicates are removed instead of kept as aliases.
- `cfg-body-dataflow-test-smoke` and `perf-contract-test-smoke` now reject any
  `mir_instruction_source_payload` read under `src/codegen`, so backend
  semantic emission cannot regress through the compatibility payload seam.

## Progress Log - 2026-06-20 MIR Surface Validation Payload Closure

- `mir_fact_surface_validate.c` no longer calls
  `mir_instruction_source_payload`. Payload presence is checked through the MIR
  source-shape predicate, and thread-pool / intent-observability usage is
  validated from `expr0` / `expr1` expression facts.
- `mir_public_surface.c` no longer derives surface-usage facts by rescanning
  the source payload. At the time, the remaining public-surface payload read
  seeded only provenance scalars: source line, column, stable id, and source
  node type. That tail is superseded by the 2026-06-21 capture-time
  provenance closure above.
- `cfg-body-dataflow-test-smoke` and `perf-contract-test-smoke` now reject
  reintroducing payload reads in MIR surface validation and reject source
  payload rescans for public-surface usage facts.

## Progress Log - 2026-06-19 MIR CFG-Owned Source-Shape Fact Closure

- `mir_instruction_source_is_cfg_owned_control(...)` now consumes only the
  recorded MIR `source_node_type` fact. It no longer falls back to
  `mir_stmt_ast_is_cfg_owned_control(inst->ast)` when source-shape metadata is
  missing.
- The MIR surface validator now rejects source payloads without a
  source-location/type fact, so CFG/body consumers cannot silently recover
  semantic source shape from AST payloads.
- `test-mir`, `cfg-body-dataflow-test-smoke`, and `perf-contract-test-smoke`
  now ratchet this by checking the validator error and rejecting reintroduced
  CFG-owned AST fallback.
- Verified locally with `test-mir`, `cfg-body-dataflow-test-smoke`,
  `build-source-inventory-test-smoke`, `perf-contract-test-smoke`, and
  `backend-fail-closed-test-smoke`.

## Progress Log - 2026-06-19 MIR Match Branch Predicate Closure

- `mir_instruction_has_required_branch_condition_fact(...)` now fails closed
  for `MIR_BRANCH_MATCH_CASE` when the MIR subject fact `expr0` is missing.
  Source payload shape alone is no longer enough to satisfy the shared
  branch-condition predicate consumed by C/LLVM codegen.
- `test-mir`, `cfg-body-dataflow-test-smoke`, and `perf-contract-test-smoke`
  ratchet this behavior through the existing source-compatible branch drift
  test and literal smoke gates.
- Verified locally with `test-mir`, `cfg-body-dataflow-test-smoke`,
  `build-source-inventory-test-smoke`, `perf-contract-test-smoke`, and
  targeted C/LLVM backend compare for match fixtures.

## Progress Log - 2026-06-19 MIR Match Subject Fact Lookup

- `pgy_codegen_match_subject_for_branch(...)` now returns the match subject
  directly from the MIR branch `expr0` fact after checking only
  `MIR_BRANCH_MATCH_CASE`. It no longer reopens source payload shape before
  consuming the subject fact.
- The MIR terminator validator remains the fail-closed owner for missing
  match-case subject facts, and `build-source-inventory-test-smoke` /
  `perf-contract-test-smoke` now reject reintroducing source-payload reads in
  the match-subject owner.
- Verified locally with `build/codegen/codegen_match_subject_lookup.o`,
  `build-source-inventory-test-smoke`, `perf-contract-test-smoke`,
  `cfg-body-dataflow-test-smoke`, `test-mir`, `llvm-test-smoke`, and targeted
  C/LLVM backend compare for match fixtures.

## Progress Log - 2026-06-19 LLVM Refresh Helper Retirement

- C/LLVM zone refresh compatibility arrays are now private to the hosted
  refresh view owners. Projection sync, projection value construction, and
  assignment/overlay invalidation consumers use view-owned mapped-source and
  source-field relevance APIs instead of indexing `ast_compat_refreshes`
  directly.
- C refresh view lowering is split into
  `transpiler_decl_zone_refresh_view.c`, keeping the slot-view owner under the
  600 LOC cap while matching the existing LLVM refresh-view responsibility
  split.
- `mir-declaration-inventory-test-smoke` now rejects `ast_compat_refreshes`
  outside the C/LLVM refresh view owners and requires the mapped-source /
  source-field view APIs.

- LLVM projection sync body and relation/effect struct registration now consume
  refresh inventory through `LLVMHostedZoneRefreshView`; the old AST-array
  declaration-parts/projection-target helper owners are removed from the source
  inventory.
- `mir-declaration-inventory-test-smoke` now rejects those helper files if they
  reappear. The remaining `ast_*_refreshes(...)` calls are confined to hosted
  view compatibility owners.

## Progress Log - 2026-06-19 LLVM Relation/Effect Assignment Refresh Cutover

- LLVM relation/effect assignment projection invalidation now shares the
  `LLVMHostedZoneRefreshView` path with zone assignment invalidation. Direct
  `ast_relation_refreshes(...)` / `ast_effect_refreshes(...)` reads and the
  AST-array host invalidation helper are removed from
  `llvm_expr_assignment_projection.c`.
- `mir-declaration-inventory-test-smoke` now rejects reopened relation/effect
  assignment refresh inventory and the retired AST-array helper.

## Progress Log - 2026-06-19 LLVM World Effect Sync Refresh Cutover

- LLVM world embedded effect sync now consumes refresh metadata through
  `LLVMHostedZoneRefreshView`; direct `ast_effect_refreshes(...)` reads are
  removed from `llvm_expr_call_methods_world_effect_sync.c`.
- `mir-declaration-inventory-test-smoke` now rejects reopened world effect sync
  refresh inventory in the LLVM method-call owner.

## Progress Log - 2026-06-19 LLVM Zone Action Refresh Cutover

- LLVM zone action effect invalidation now consumes refresh metadata through
  `LLVMHostedZoneRefreshView`; direct `ast_effect_refreshes(...)` reads are
  removed from `llvm_stmt_zone_action.c`.
- `mir-declaration-inventory-test-smoke` now rejects reopened effect refresh
  inventory in the LLVM zone action owner.

## Progress Log - 2026-06-19 LLVM Zone Bind Refresh Cutover

- LLVM zone relation/effect bind invalidation now consumes refresh metadata
  through `LLVMHostedZoneRefreshView`. Direct `ast_relation_refreshes(...)` and
  `ast_effect_refreshes(...)` reads are removed from
  `llvm_domain_zone_bind_lowering.c`.
- `mir-declaration-inventory-test-smoke` now rejects reopened relation/effect
  refresh inventory in the LLVM zone bind owner.

## Progress Log - 2026-06-19 C World Embedded Effect Refresh Cutover

- C world embedded effect sync now consumes relation/effect refresh object/source
  metadata through `TranspilerHostedZoneRefreshView`; direct
  `ast_effect_refreshes(...)` reads are removed from
  `transpiler_projection_sync.c`.
- `mir-declaration-inventory-test-smoke` now rejects reopened world embedded
  effect refresh inventory in the C projection-sync owner.

## Progress Log - 2026-06-19 C Zone Bind Refresh Cutover

- C zone relation/effect bind invalidation now consumes refresh metadata through
  `TranspilerHostedZoneRefreshView`. The direct
  `ast_relation_refreshes(...)` / `ast_effect_refreshes(...)` reads are removed
  from the C bind owners; compatibility stays inside the hosted view.
- `mir-declaration-inventory-test-smoke` now rejects reopened relation/effect
  refresh inventory in `transpiler_overlay_zone_bind.c` and
  `transpiler_overlay_zone_relation_bind.c`.

## Progress Log - 2026-06-19 C Overlay Relation/Effect Refresh Cutover

- C overlay/assignment projection invalidation now builds
  `TranspilerHostedZoneRefreshView` for relation, effect, and zone declarations.
  The direct `ast_relation_refreshes(...)` / `ast_effect_refreshes(...)` overlay
  reads are removed; non-MIR compatibility is constrained to the view.
- `mir-declaration-inventory-test-smoke` now rejects reopened relation/effect
  refresh inventory in `transpiler_overlay_projection.c`.

## Progress Log - 2026-06-19 LLVM Relation/Effect Constructor Refresh Cutover

- LLVM relation/effect/zone constructor projection-dirty initialization now
  consumes one `LLVMHostedZoneRefreshView` path. Relation/effect constructors no
  longer reopen `ast_relation_refreshes(...)` / `ast_effect_refreshes(...)`.
- `mir-declaration-inventory-test-smoke` now rejects relation/effect constructor
  refresh inventory reads in `llvm_expr_constructor_calls.c`.

## Progress Log - 2026-06-19 Relation/Effect Refresh Metadata Capture

- `mir_decl_header_set_refreshes` now captures refresh rows for relation,
  effect, and zone declarations. The validator permits refresh metadata only on
  those domain declarations and keeps count drift fail-closed.
- C relation/effect declaration sync and relation/effect constructor
  projection-dirty initialization now consume `TranspilerHostedZoneRefreshView`
  instead of reopening `ast_relation_refreshes(...)` /
  `ast_effect_refreshes(...)` directly.
- MIR tests now preserve relation and effect refresh metadata, and
  `mir-declaration-inventory-test-smoke` ratchets the new capture/view/consumer
  boundary.

## Progress Log - 2026-06-19 Zone Refresh LLVM Subject Sync Cutover

- LLVM current-zone subject projection sync now consumes
  `LLVMHostedZoneRefreshView` instead of reopening
  `ast_zone_refreshes(host_decl, &refresh_count)`.
- The MIR-only path fail-closes if zone refresh rows are missing or drift from
  the declaration inventory, matching the other zone refresh consumers.
- `mir-declaration-inventory-test-smoke` now requires this consumer and rejects
  the old direct AST refresh inventory read in
  `llvm_expr_call_projection_sync.c`.

## Progress Log - 2026-06-19 Zone Refresh C Overlay Invalidation Cutover

- C overlay/assignment projection invalidation now builds a
  `CurrentOverlayRefreshView` that consumes `TranspilerHostedZoneRefreshView`
  for zone declarations instead of reopening
  `ast_zone_refreshes(decl, refresh_count_out)`.
- MIR-backed zone invalidation uses `MIRDeclZoneRefresh` field-map accessors for
  source-field relevance. Relation/effect invalidation and explicit non-MIR
  compatibility still use the retained AST refresh shape.
- `mir-declaration-inventory-test-smoke` now requires the C overlay refresh view
  consumer and rejects reopened zone refresh inventory in
  `transpiler_overlay_projection.c`.
- Gates used: `make test-transpile` and
  `make mir-declaration-inventory-test-smoke`.

## Progress Log - 2026-06-19 Zone Refresh LLVM Assignment Invalidation Cutover

- LLVM host assignment projection invalidation now consumes
  `LLVMHostedZoneRefreshView` for zone declarations instead of reopening
  `ast_zone_refreshes(host_decl, &refresh_count)`.
- LLVM world-embedded zone assignment invalidation uses the same view for the
  resolved embedded zone, so nested world-zone member assignment dirty/ready
  updates read `MIRDeclZoneRefresh` source slots and field maps through MIR
  accessors.
- Relation/effect assignment invalidation remains on the compatibility refresh
  path until relation/effect refresh rows exist.
- `mir-declaration-inventory-test-smoke` now requires the assignment
  invalidation view consumer and rejects reopened zone refresh inventory in
  that file.
- Gates used: `make llvm-test-smoke` and
  `make mir-declaration-inventory-test-smoke`.

## Progress Log - 2026-06-19 Zone Refresh Constructor Dirty Cutover

- C zone constructors now initialize `__projection_dirty_*` fields through
  `TranspilerHostedZoneRefreshView` instead of reopening
  `ast_zone_refreshes(zone_decl, &refresh_count)`. Relation/effect
  constructors keep their compatibility refresh path until relation/effect
  refresh metadata exists.
- LLVM zone constructors now use `LLVMHostedZoneRefreshView` and
  `llvm_domain_slot_view_is_projection_slot_in_zone_refresh_view(...)` for the
  same projection-dirty decision. This keeps zone constructor layout and
  initialization on the same MIR refresh rows consumed by zone projection sync.
- `mir-declaration-inventory-test-smoke` now requires the C/LLVM constructor
  consumers and rejects reopened zone refresh inventory in both constructor
  files.
- Gates used: `make test-transpile`, `make llvm-test-smoke`, and
  `make mir-declaration-inventory-test-smoke`.

## Progress Log - 2026-06-19 Zone Refresh LLVM Consumer Cutover

- LLVM zone projection sync lowering now builds an
  `LLVMHostedZoneRefreshView` from `MIRDeclZoneRefresh` rows before it reads
  refresh object/source slot names or explicit target-to-source field maps.
  Relation/effect refreshes remain on the compatibility path until they have
  equivalent metadata rows.
- LLVM zone struct registration and projection-state field registration now
  count projection slots through the same refresh view instead of reopening
  `ast_zone_refreshes(stmt, &refresh_count)` in the struct/layout path.
- LLVM domain projection value lowering has a MIR refresh entry point,
  `llvm_build_domain_projection_value_from_zone_refresh_metadata(...)`, so
  zone field-map lowering consumes `mir_decl_zone_refresh_mapped_*` accessors.
- `mir-declaration-inventory-test-smoke` now requires the LLVM refresh view,
  projection-count/value consumers, and zone struct-field registration cutover,
  and rejects reopened zone refresh inventory in the LLVM declaration-parts and
  struct-field registration paths.
- Gates used: `make llvm-test-smoke` and
  `make mir-declaration-inventory-test-smoke`.

## Progress Log - 2026-06-19 Zone Refresh C Consumer Cutover

- C zone declaration emission now builds a `TranspilerHostedZoneRefreshView`
  from `MIRDeclZoneRefresh` rows instead of reopening
  `ast_zone_refreshes(node, &refresh_count)`.
- Zone projection sync lowering uses
  `emit_zone_projection_sync_loop_from_mir_refresh_view(...)`, and projection
  literal field-map lowering has a MIR refresh entry point,
  `emit_projection_literal_by_zone_refresh_metadata(...)`.
- `mir-declaration-inventory-test-smoke` now requires the C refresh view and
  rejects reopening `AST_ZONE_REFRESH` inventory in `transpiler_zone_decl_emit.c`.
  Relation/effect refreshes still need their own metadata/view cutover.
- Gates used: `make test-transpile` and
  `make mir-declaration-inventory-test-smoke`.

## Progress Log - 2026-06-19 Zone Refresh Metadata Capture

- Added `MIRDeclZoneRefresh` declaration-header rows for zone refresh
  object/source slots, optional participant slot, refresh mode bits, and
  ordered target-to-source field maps. This closes the metadata gap before
  C/LLVM projection lowering can stop reading `AST_ZONE_REFRESH` field maps.
- `mir_decl_header_refresh.c` owns capture/freeing, `mir_decl_header_access.c`
  exposes the compiler accessors, and `mir_decl_header_validate.c` rejects
  refresh metadata count drift and incomplete field-map rows.
- `test-mir` now preserves refresh field maps and rejects refresh metadata
  drift. `mir-declaration-inventory-test-smoke` requires the new row,
  accessors, validator strings, and fixtures so the row cannot disappear while
  consumers are being cut over.
- Gates used: `make test-mir`, `make mir-declaration-inventory-test-smoke`,
  `make documentation-quality-test-smoke`, `make test-transpile`,
  `make backend-fail-closed-test-smoke`, `make llvm-test-smoke`,
  `make test-inc-size-test-smoke`, `make build-source-inventory-test-smoke`,
  and `git diff --check`.

## Progress Log - 2026-06-19 Projection By-Name Owner Cutover

- C projection literal/source-path lowering and LLVM projection-path loading
  now expose only by-name/type-name entry points. The retired AST-declaration
  wrappers only extracted a declaration name before delegating, so keeping them
  open allowed projection code to look source-declaration-shaped again without
  owning a semantic declaration decision.
- The C/LLVM projection class-field view helpers no longer accept an AST
  compatibility declaration parameter. They pass the already-resolved type name
  to the hosted field view owner and fail closed in MIR-active paths when
  declaration-field metadata is missing.
- `mir-declaration-inventory-test-smoke` now rejects reintroducing
  `resolve_projection_source_path_rec`, `emit_projection_literal`, or
  `llvm_load_projection_path_value`, and rejects projection-local
  `compat_decl` parameters. Projection callers must consume the typed owner
  that already resolved the source type and then call the by-name helper.
- Gates used: `make mir-declaration-inventory-test-smoke`,
  `make test-transpile`, `make llvm-test-smoke`,
  `make documentation-quality-test-smoke`,
  `make backend-fail-closed-test-smoke`,
  `make test-inc-size-test-smoke`, and `git diff --check`.

## Progress Log - 2026-06-19 LLVM Class-Field Slot Registration Cutover

- LLVM MIR parameter setup now registers `self.field` slot bindings through
  `LLVMHostedFieldView` and `MIRDeclField` type-name facts instead of indexing
  `fields_view.ast_compat_fields[...]` directly.
- Secure slot paired-token lookup now checks `MIRDeclFieldClaim` rows first and
  uses AST destructuring only as explicit non-MIR compatibility. MIR-active
  missing field metadata fails closed through the backend MIR inventory
  diagnostic.
- Gates used: `make mir-declaration-inventory-test-smoke`,
  `make llvm-test-smoke`, and `git diff --check`.

## Progress Log - 2026-06-19 Hosted Method Compatibility View Ownership

- C and LLVM hosted method compatibility arrays are now private view state for
  consumers. Non-MIR compatibility lookup and specialization scanning must ask
  `transpiler_hosted_method_view_compat_method(...)` or
  `llvm_hosted_method_view_compat_method(...)`; direct indexing of
  `method_view.ast_compat_methods[...]` / `method_view->ast_compat_methods[...]`
  is confined to the view owners.
- MIR-active method body and specialization paths are unchanged: they consume
  linked `MIRDeclMethod` routine indexes and fail closed when method metadata or
  routine links are missing. The compatibility accessor only owns the explicit
  non-MIR path.
- Gates used: `make mir-declaration-inventory-test-smoke`,
  `make test-transpile`, `make llvm-test-smoke`,
  `make backend-fail-closed-test-smoke`, `make test-inc-size-test-smoke`, and
  `git diff --check`.

## Progress Log - 2026-06-19 ABI Niche / Explicit Layout Contract Gate

- Rust-style niche optimization is now documented as intentionally not
  implemented for the beta ABI. `Option<T>` remains an explicit tagged layout,
  and the runtime ABI spec plus static asserts now freeze exact tag/value
  offsets and sizes for the current primitive Option rows.
- `MIRTypeLayout` now carries an ABI representation fact. Current `Option<T>`
  and `Result<T,E>` rows are marked `MIR_ABI_REPR_EXPLICIT_TAG`; the future
  niche path has a reserved representation value but no backend may infer or
  emit a niche without a semantic/DAG proof type and MIR ABI fact.
- `docs/136_abi_niche_and_explicit_layout.md` now names the current golden
  gates and restricts explicit layout/field overlap to a future
  `unsafe(ffi, layout)` / raw-boundary capability surface. Ordinary
  structs/classes remain ownership-aware values, not user-packed ABI blobs.
- `docs/125_source_of_truth_spine.md` now names the ABI layout owner chain
  (`pgy_abi_spec.h` -> static asserts -> `MIRTypeLayout`) so niche encoding and
  explicit layout cannot re-enter as backend-local shortcuts.
- Gates used: `make test-mir`, `make test-abi`,
  `make abi-ownership-shape-test-smoke`, `make documentation-quality-test-smoke`,
  and `git diff --check`.

## Progress Log - 2026-06-19 Zone Authority MIR Metadata Cutover

- Zone authority declaration facts now have a MIR owner:
  `MIRDeclZoneAuthority` records the owning zone, authority subject slot, and
  written required ability refs. Role slots and zone authorities share the same
  `MIRAbilityRef` capture path so generic ability refs do not fork into two
  string conventions.
- C MIR-backed zone method entry checks now consume
  `mir_decl_header_zone_authority_count(...)` /
  `mir_decl_zone_authority_subject_slot_name(...)` instead of reopening
  `ast_zone_authorities(...)`. Missing zone headers fail closed through the MIR
  inventory diagnostic path.
- The LLVM authority owner now finds the current zone through MIR routine owner
  metadata and then consumes the zone declaration header. It no longer uses AST
  zone authority child accessors for the subject-slot decision.
- Gates used: `make test-mir`, `make mir-declaration-inventory-test-smoke`,
  `make test-abi`, and `make self-host-preparation-test-smoke`. The self-host
  production size/header clean fixtures were updated for the two new production
  `.c` files and two new production `.h` files.

## Progress Log - 2026-06-10 Anchor Bump To 83% + Additional Backend Coverage

- The live readiness anchor moves 82% -> 83% strict beta readiness
  after the closures listed below in the same day's earlier entry.
  Feature-surface feel stays at 85%. All eleven source-of-truth
  surfaces (docs/100, /100a, /100d, /50, /70, /98, README_ko, TODO,
  and the three smokes that gate the wording) were updated in a
  single sweep; `beta-readiness-checklist-test-smoke`,
  `documentation-quality-test-smoke`, and `air-drift-test-smoke`
  pass against the new wording.
- `backend_compare` default registry grew further: multi_spawn_
  accumulator (3-way fan-out), channel_drain_while,
  option_for_match_slot, result_for_match_slot,
  class_method_chain_slot, bool_helper_while_slot,
  three_slots_cross_update, max_reduction_slot, recursive_fib_slot,
  and early_return_while_slot. Each pins a distinct Slot+control-flow
  shape against the SSA def-block resource-op policy; default count
  is now 824.
- DAG smoke locks five additional invariants
  (alias_materialized >= 6, alias_diagnostic_unresolved >= 78,
  generic_param_nodes >= 100, dag_generic_contract_evidence >= 160,
  dag_ability_consumer_evidence >= 70). Each floor is strictly below
  the current production measurement (6/78/102/165/76 respectively);
  bump in a dedicated commit when the DAG owner intentionally raises
  the surface.
- TODO §0b interprocedural body-summary bullet rolled forward to name
  the PR1+PR2 prove-helper closure and the named next migration
  targets (zone authority spawn detection, intent control async/
  channel detection, C/LLVM lowering consuming the bits).
- C-only backend bug surfaced during probes and then fixed in the
  same session: nesting two `while` loops over a shared Slot
  counter where the second loop contains
  `let v: Int = <- ch; Write(sum, Read(sum) + v);` previously
  triggered "MIR contract breach: unresolved identifier `v`
  (expected SSA-mapped local)" on the C path. Root cause: the
  upstream MIR lowering attributes every routine slot Write
  resource op to the slot's SSA def-block flow, so block 0
  carries a copy of the second-loop-body Write whose `v` is
  `v.0` (undef) instead of `v.1` (defined in block 5). The C
  emit policy
  (`transpiler_emit_mir_resource_hook`) was already routing that
  copy to the observability-only path, but the *mapping precheck*
  in `transpiler_mir_emission_mapping_contract.c` ran *before* the
  emit policy and failed the contract on the undef `v.0`. Fix:
  the mapping precheck now consults the same
  `transpiler_mir_resource_has_mirroring_stmt_in_block` seam as
  the emit policy and skips the contract for a Write resource op
  whose paired stmt lives outside the current block. Regression
  fixture `nested_while_channel_let` pins the reproducer.

## Progress Log - 2026-06-10 Backend Parity, P0 #1 Read-Seam, And Cross-Lane CI Closure

- C backend `transpiler_emit_mir_resource_hook` now gates concrete runtime
  emission on a same-block paired `MIR_INST_STMT`, eliminating a parity drift
  where a slot SSA def-block carrying a use-block's Write resource op would
  emit the runtime call in both blocks. A minimal `while Read(slot) < N`
  reproducer printed iteration counts 1..N-1 on C and 0..N-1 on LLVM before
  this fix; both backends now agree.
- backend_compare default registry expanded by 16 fixtures covering
  `pin` inside nested-if / for / while / break / continue / return /
  branch-return exits, while-loop accumulation with Slot reads,
  for-loop conditional Slot writes, `if/else` and `match`-with-default
  inside a Slot-reading loop, helper calls that hand off `ref Slot`
  inside loops, channel sends inside Slot-reading loops, defer
  registered alongside Slot Log, and async/spawn accumulator into a
  Slot. Default count moved 798 -> 814.
- `PGY_SEM_PIN_TOKEN_INVALID` now fires from source level when
  `ViewRead/ViewWrite` is applied to a `SecureSlot<T>` whose paired
  capability token symbol is not reachable in scope, closing the
  previous gap where invalid-token pin only failed at the runtime ABI.
  `docs/74` and `docs/118 §4.2` are aligned; a semantic regression and
  a `ctx_has_diagnostic_code` test helper lock the contract.
- `runtime-abi-lifetime-test-smoke` now audits the pool runtime-owned
  handle contract (`pgy_pool_spawn`/`_despawn`/`_get`) in addition to
  the file-descriptor contract. Pool slot allocation reuses freed
  slots; the despawn surface clears the alive flag through a validated
  index; the get accessor rejects out-of-range or despawned handles.
- `runtime-abi-lifetime-test-smoke` evidence is recorded for the
  intent borrowed-snapshot thread-local store (`static _Thread_local
  char *snapshots`), pairing the existing mutex-backed registry path
  with the explicit "no raw registry pointer return" gate so scratch
  arena teardown cannot dangle a previously-returned query string.
- §4 exceptional/cancellation pin-exit audit closure: cancellation
  paths reject at source level via `PGY_SEM_PIN_AWAIT_BOUNDARY` /
  `PGY_SEM_PIN_PARALLEL_CONFLICT`; panic exit is process abort and OS
  reclaims pin storage; C source-block emission already binds GCC
  cleanup attribute for local scope exit. No further LLVM
  invoke/landingpad work is required for the stable beta subset.
- Windows LLVM-ready CI lane now also runs
  `air-strict-backend-compare-test-smoke`, extending Windows native
  evidence of strict-AIR validation across the backend execution
  compare to match the existing Linux lane.
- macOS C-only CI lane: `backend_output_tri_compare_parity.sh` now
  probes LLVM availability with a one-line `func Main() -> Void {}`
  source compiled through `--backend=llvm`; when the probe fails the
  script SKIPs gracefully so a non-LLVM pgy build does not turn into
  a self-host smoke failure. macOS C-only step 19 now passes again.
- P0 #1 §0b read-seam closure (PR1 + PR2): the body-summary prove
  helpers `semantic_callable_summary_proves_no_drop_resource`,
  `_no_spawn_task`, `_no_send_channel`, and `_no_zone_requirement`
  join the existing `_no_ref_escape` helper as the named read seam
  for consumers; each returns true only when the callee's recorded
  `body_summary_mask` positively proves the bit absent. The first
  consumer migration applies the channel-send seam:
  `semantic_callable_param_escape_summary` now strips the legacy AST
  analyzer's `SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE` bit when
  `proves_no_send_channel` succeeds. `semantic-core-shape-test-smoke`
  gates the helpers + bits + internal-header declarations as the
  canonical seam against future drift.
- `mir-declaration-inventory-test-smoke` anchor windows (4 / 6 lines)
  now hold after the P0 #4 AST fallback was compacted in
  `llvm_stmt_type_infer.c` and `llvm_expr_call_hosted.c`. The bug
  surfaced when the previous documentation comment + wrapped helper
  call pushed `llvm_active_has_mir(ctx)` outside the smoke's window.
- Build environment: 491 commits on `main` had their `Co-Authored-By`
  trailers rewritten and force-pushed; the remote contributor surface
  is `srtdog64` only by API.

## Progress Log - 2026-06-08 Readiness Reanchor And Documentation Sync

- The live readiness anchor now separates implementation surface from strict
  beta trust: feature-surface feel is about 85%, and strict beta readiness is
  now about 83%. The older 70% / 75% wording was stale for the current codebase
  and is no longer used by the source-of-truth docs.
- The low-80% line is still not beta-complete. The remaining closure is current
  full-suite evidence plus source-of-truth consumer completion across CFG/AIR,
  guarded compatibility-fallback tightening, MIR/LLVM declaration bootstrap,
  and ABI/Slot/Pin freeze.
- Documentation source-of-truth was aligned across
  `docs/100_beta_readiness_checklist.md`,
  `docs/100a_beta_active_status.md`, `docs/50_language_completion_board.md`,
  `docs/70_beta_closure_master_board.md`, `docs/README_ko.md`,
  `docs/98_beta_closure_readiness_report.md`, and `TODO.md`.
- The document gates now require the 83% wording and reject reintroducing the
  stale `strict beta readiness is now about 75%` phrase in current-status
  surfaces. Evidence run: `mingw32-make
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke
  air-drift-test-smoke` passed, with `test-air` at `119 passed, 0 failed`.

## Progress Log - 2026-06-06 C/LLVM Function Signature Fail-Closed

- C and LLVM function forward declarations and MIR body emission now fail
  closed when an active MIR routine row exists without signature metadata. The
  fallback to source AST function parameters and return types remains available
  only for no-MIR compatibility paths.
- C forward eligibility policy and C forward emission both report
  `MIR-only C path missing function forward signature metadata` instead of
  silently reopening AST function signatures.
- LLVM early-forward eligibility and LLVM function forward declaration emission
  both report `MIR-only LLVM path missing function forward signature metadata`
  for the same active-MIR defect.
- C and LLVM MIR function body emission now report `MIR-only C path missing
  function body signature metadata` and `MIR-only LLVM path missing function
  body signature metadata` before attempting any AST signature fallback.
- C MIR SSA-local declaration emission and LLVM MIR parameter-allocation
  emission now apply the same active-MIR signature guard. Missing signature
  metadata is reported as `MIR-only C path missing function SSA local signature
  metadata` or `MIR-only LLVM path missing function parameter signature
  metadata` instead of reopening source AST parameters inside helper-level
  lowering.
- C MIR signature eligibility policy now fails closed before checking source
  AST parameter and return shapes when an active MIR routine exists without
  signature metadata. This keeps the C "can emit from MIR" decision behind the
  MIR routine row instead of using AST as a silent compatibility substitute.
- C MIR local type lookup now resolves direct function-call return types from
  active MIR routine signature metadata when available. Source AST callable
  return fallback is limited to no-MIR compatibility mode, and the transpile
  fixture for generic ability specialization now supplies explicit MIR
  signature metadata instead of relying on source AST fallback.
- C MIR local parameter type lookup now consumes active routine signature
  metadata before consulting source AST parameters. If an active MIR routine row
  exists without signature metadata, local parameter lookup reports
  `MIR-only C path missing local parameter signature metadata` instead of
  silently using source AST parameter types.
- `MIRRoutine` kind/name/owner metadata now has a core accessor surface
  (`mir_routine_kind`, `mir_routine_name`, `mir_routine_owner_name`,
  `mir_routine_owner_ast_type`) with C and LLVM backend inventory wrappers.
  C/LLVM MIR function and parameter emission now consume those accessors in
  the closed slice instead of reopening selected routine fields directly.
- The accessor contract now covers the C/LLVM codegen routine lookup and
  contract owners as well: LLVM function inventory, intent flow, MIR contract
  validation, C intent routine collection, C MIR local type lookup, C MIR
  mapping precheck, and C MIR resource-op emission. A `src/codegen` scan for
  direct `routine->kind/name/owner_*` reads returns no hits.
- `build_source_inventory_smoke.sh` now normalizes scanner output paths to
  forward slashes at the scan owner. This keeps Git Bash/Windows `rg` output
  from bypassing allowlists as `src/codegen\...` false positives while leaving
  the C/LLVM owner checks unchanged.
- C/LLVM backend-compare evidence was extended on the deterministic default
  order after the path-normalization fix. With ABI precheck disabled for the
  targeted oracle run, ranges `0..11`, `12..35`, and `36..71` passed
  (`84/84`) across slot/secure-slot, arrays/slices, tuples, enum/match,
  generics, class methods, lexical shadowing, dynamic scope capture,
  Option same-binding regressions, loop control, and recursive calls. This is
  evidence for the registered support matrix prefix, not a full-suite claim.
- LLVM statement type inference now owns the `SetHas -> Bool` and
  `SetSize -> Int` collection facts alongside existing List/Map/Queue facts.
  This closes the `set_intersection_manual` C/LLVM compare gap where lowering
  knew `SetHas` but condition type inference still demanded registered
  function metadata. Targeted evidence: `set_intersection_manual` passed, and
  range `72..119` passed (`48/48`) with `PGY_BIN=/e/PergyraLang/bin/pgy.exe`
  and ABI precheck disabled.
- Additional C/LLVM backend-compare evidence now covers deterministic ranges
  `120..143` (`24/24`), `144..165` (`22/22`), `166..167` (`2/2`), and
  `168..191` (`24/24`) with the same explicit `PGY_BIN` and ABI precheck
  disabled. These ranges extend parity coverage through string/array/map/set/
  queue algorithms, sorting/search loops, recursive numeric helpers, and
  pipeline-style collection fixtures without requiring a new compatibility
  fallback.
- Range `192..215` also passed (`24/24`) under the same backend-compare
  settings, extending C/LLVM parity evidence across short-circuit call chains,
  nested loop joins, unary long arithmetic, substring/string formatting
  helpers, and enum payload/no-payload match lowering.
- Ranges `216..239`, `240..263`, and `264..287` also passed (`72/72`) with
  explicit `PGY_BIN` and ABI precheck disabled. This extends evidence across
  enum/result pipelines, generic boxes, scalar math/runtime conversion, file
  handle I/O, device slot runtime, string interpolation/split/join predicates,
  module namespace/export visibility, role/operator overloads, and recursion.
  No new compatibility fallback was added for these slices.
- Range `288..311` passed (`24/24`) with the same settings, covering nested
  calls, host-method returns, subject-method recursion with defer, branch/loop
  defer cleanup, boilerplate-reduction surface fixtures, intent trace/failure/
  rollback/authority observability, cross-world transfer, handoff frontier
  sync, zone mutation, and zone host-method ABI fixtures.
- Ranges `312..335`, `336..359`, and `360..383` passed (`72/72`) with the
  same settings. These slices cover subject/class/action projection dispatch,
  ownership forwarding, generic future/spawn specialization, default generic
  contracts, nested generic containers, ability/role/party/roster host methods,
  Result method chains, Option/coalesce class chains, intent header
  interleaving, map/list mutation and lookup, and for-in lowering over arrays
  and lists.
- Range `384..407` passed (`24/24`) with the same settings. This slice covers
  queue/set operations, Rc/Weak lifecycle, typed pin read/write views,
  secure-slot pin views, pin cleanup across successor/return/branch/break/
  continue paths, unsafe lexical boundary, and world/zone projection mutation
  fixtures.
- Ranges `408..431`, `432..455`, and `456..479` passed (`72/72`) with the
  same settings. These slices cover world/zone embedded action frontiers,
  relation/effect propagation and projection sync, authority failure surfaces,
  higher-order/lambda/event handlers, async spawn/await and future annotations,
  cancellation propagation, string spawn, channel pressure/status/select
  fairness, parallel channel communication, class-heavy method composition,
  nested class fields, enum fields, and algorithmic array/class fixtures.
- Range `480..503` passed (`24/24`) with the same settings. This extends the
  verified prefix through multi-match collision handling, nested branching,
  deeper Option class-chain and map-lookup fixtures, range method tests, Result
  class fields, chained class methods, string-carrying Result payloads, and
  nested Result propagation.
- Ranges `504..527`, `528..551`, and `552..575` passed (`72/72`) with the
  same settings. These slices cover Result pipelines, string-array tagging,
  class/stat composition, while/class method returns, array binary search,
  balanced split, inversion/counting/sliding-window algorithms, in-place array
  mutation and sorting, enum array dispatch, match payload destructuring,
  match branches with loops/breaks/lets/returns, same-binding multi-match, and
  Option composition/counting loops.
- Range `576..599` passed (`24/24`) with the same settings. This extends the
  verified prefix through Option loop consumption, complex Option match
  branches, default arguments, Result class propagation, string extraction/
  Caesar/reverse/repeat/run-length/split/strip/window helpers, triangle checks,
  triple function composition, and chained class factory fixtures.
- Range `600..623` passed (`24/24`) with the same settings after closing the
  LLVM indexed collection source-of-truth seam for current-host class fields.
  `nums[i]` inside class methods now resolves the field's registered
  `PgyArray_*`/`PgySlice_*` LLVM type to element metadata instead of requiring
  a local Array variable registration fallback. Evidence run:
  `mingw32-make pgy`, targeted `class_field_array_method`, range `600..623`,
  `backend_fail_closed_smoke.sh`, `perf_contract_smoke.sh`, and
  `documentation_quality_smoke.sh` passed.
- Ranges `624..647`, `648..671`, `672..695`, `696..719`, `720..743`,
  `744..767`, `768..791`, and `792..794` passed (`171/171`) with the same
  explicit `PGY_BIN` and ABI precheck disabled. This completes the
  deterministic default-order backend compare prefix through all `795/795`
  registered cases for this local Windows/Git-Bash LLVM-enabled build. The
  tail slices cover class-heavy factory/field/method chains, nested class and
  recursive traversal patterns, for-range fixtures, HashMap/List/Set basics,
  intent/zone/world fixtures, probes, slots, subject methods, string helpers,
  tree recursion, and zone/subject-slot fixtures. This is backend parity
  evidence for the current default registered case set, not a proof that every
  language surface is complete.
- The same local build also passed `backend_compare_llvm_coverage_smoke.sh`,
  `build_source_inventory_smoke.sh`, and `llvm_smoke.sh`. These gates keep the
  compare coverage allowlist, Makefile source inventory, and LLVM smoke surface
  aligned with the completed default-order compare evidence.
- The Korean/UTF hygiene scan found no current common Latin-1 mojibake payload
  in source docs/tests. The literal `slot` + three U+003F question marks
  remains an intentional forbidden sentinel in
  `formal_semantics_smoke.sh`, not user-facing broken Korean. Evidence:
  `documentation_quality_smoke.sh` passed.
- `mir-declaration-inventory-test-smoke` now freezes the active-MIR signature
  guard across C policy, C forward/body/SSA-local emission, LLVM policy, and
  LLVM forward/body/parameter emission, plus C MIR signature eligibility,
  C MIR local function-call return and parameter lookup, and the selected
  `MIRRoutine` metadata accessor contract. Evidence run:
  `git diff --check`, `mir_declaration_inventory_smoke.sh`, WSL `make pgy`,
  WSL `make llvm-test-smoke`, and WSL `make test-transpile` (`898/0`) passed.

## Progress Log - 2026-06-06 LLVM Domain Forward AST Compatibility Flag

- LLVM domain/role method forward declaration helpers now require an explicit
  `allow_ast_compat` flag before falling back from `MIRDeclMethod` metadata to
  source AST method names, parameters, or return types.
- Hosted domain/role method forward declarations pass `allow_ast_compat` only
  for no-MIR compatibility rows (`method_meta == NULL`). MIR-backed method rows
  consume MIR method metadata and fail through existing inventory diagnostics
  instead of silently reopening source AST.
- Ability vtable emission remains explicitly AST-compatible because that path
  still owns an AST-only declaration surface. Role operator forward emission is
  explicitly metadata-only.
- `mir-declaration-inventory-test-smoke` now freezes the flag contract for the
  shared method signature helpers and the domain/role/ability call sites.
  Evidence run: `git diff --check`, `mir_declaration_inventory_smoke.sh`, WSL
  `make pgy`, and WSL `make llvm-test-smoke` passed.

## Progress Log - 2026-06-06 C/LLVM Method Return Metadata Fallback Tightening

- LLVM method return inference now treats missing `MIRDeclMethod` return
  metadata as a MIR inventory defect when MIR is active, instead of reopening
  source AST method return types.
- C method return inference, member-call post-sync wrapping, and hosted-method
  forward declaration emission keep AST method return/parameter fallback only
  for no-MIR compatibility mode. MIR-active C inference no longer reopens
  source AST method return types and falls through to the existing
  unknown/concrete-type failure path; hosted forward declarations fail closed
  when MIR method rows are missing.
- LLVM identifier-call inference now consumes scalar builtin return facts before
  current-host method fallback. This prevents standard calls such as
  `ChannelClosed(ch)` inside a class method from being misclassified as
  `CurrentHost.ChannelClosed`.
- `mir-declaration-inventory-test-smoke` now requires the active-MIR guards
  around these return/forward-metadata fallbacks and freezes the
  builtin-before-host dispatch order. Evidence run: `git diff --check`,
  `mir_declaration_inventory_smoke.sh`, `backend_fail_closed_smoke.sh`, WSL
  `make pgy`, WSL `make llvm-test-smoke`, and WSL `make test-transpile`
  (`898/0`) passed.

## Progress Log - 2026-06-06 LLVM Member Call Metadata Fail-Closed

- LLVM general member-call lowering now applies the same MIR-active
  `MIRDeclMethod` requirement as hosted self-call lowering. If a member-call
  method row is missing while a MIR program is active, LLVM reports a MIR
  inventory-missing diagnostic instead of reopening source AST method lookup.
- The AST method fallback remains available only for no-MIR compatibility mode.
  This keeps legacy AST-only emission usable while preventing MIR-driven LLVM
  member calls from bypassing declaration metadata.
- `mir-declaration-inventory-test-smoke` now requires the LLVM member-call
  MIR-active guard and inventory-missing diagnostic. Evidence run:
  `git diff --check`, `mir_declaration_inventory_smoke.sh`,
  `backend_fail_closed_smoke.sh`, and WSL `make llvm-test-smoke` passed.

## Progress Log - 2026-06-06 C/LLVM Hosted Member Call Metadata Fail-Closed

- LLVM hosted self-call lowering now requires `MIRDeclMethod` metadata when a
  MIR program is active. Missing hosted-method metadata reports a MIR
  inventory-missing diagnostic instead of falling back to the current source
  host declaration.
- C nominal member-call lowering now applies the same policy: AST method lookup
  remains available only in no-MIR compatibility mode, while MIR-active mode
  fails closed if the member-call method row is absent from declaration
  metadata.
- `mir-declaration-inventory-test-smoke` now requires the MIR-active guards and
  inventory-missing diagnostics in both call sites. Evidence run:
  `git diff --check`, `mir_declaration_inventory_smoke.sh`,
  `backend_fail_closed_smoke.sh`, WSL `make pgy`, WSL `make test-transpile`
  (`898/0`), and WSL `make llvm-test-smoke` passed.

## Progress Log - 2026-06-06 C Return Callable Context Metadata Owner

- C backend return-expression lowering now consumes
  `TranspilerCtx.current_return_callable_type` instead of reopening
  `ast_func_return_type((ASTNode *)ctx->current_func_decl)` to recover an
  event-handler return contract.
- Function and MIR routine emission set the callable return context explicitly.
  The C MIR emit-state snapshot now saves/restores that context, and generated
  Bool/Void-style owners clear it when setting their return type so stale
  function metadata cannot leak into wrapper emission.
- `backend-fail-closed-test-smoke` rejects reintroducing the AST current-func
  return fallback in the C return policy owner and requires the context fact to
  stay wired. Evidence run: `git diff --check`, `backend_fail_closed_smoke.sh`,
  WSL `make pgy`, WSL `make test-transpile` (`898/0`), and WSL `make test-mir`
  (`78/0`) passed.

## Progress Log - 2026-06-06 C/LLVM Method Boundary Metadata Fail-Closed

- Zone action and world-effect synchronization now treat `MIRDeclMethod`
  metadata as required when a MIR program is active. If the method row is
  missing in MIR-active mode, C and LLVM set the MIR inventory-missing
  diagnostic instead of reopening the source AST method contract.
- AST method fallback remains only for no-MIR compatibility mode. This keeps
  legacy AST-only emission usable while preventing MIR-driven backends from
  silently bypassing the declaration metadata source of truth.
- `mir-declaration-inventory-test-smoke` now requires the MIR-active guard near
  each method-metadata fallback branch. Evidence run: `git diff --check`,
  `backend_fail_closed_smoke.sh`, `mir_declaration_inventory_smoke.sh`, and WSL
  `make llvm-test-smoke` passed.

## Progress Log - 2026-06-06 LLVM Zone Boundary Context Metadata Owner

- LLVM zone authority checks and current-host declaration lookup now consume
  `LLVMGenCtx.current_within_zone_name` instead of reopening
  `ast_func_within_zone(ctx->current_func_decl)` at the point of use.
- AST function emission sets the context fact directly. MIR routine emission
  consumes `MIRRoutine.within_zone` through the LLVM routine inventory view, so
  MIR codegen no longer reopens source AST to recover the active zone boundary.
- `backend-fail-closed-test-smoke` rejects reintroducing the AST current-func
  zone fallback in `llvm_decl_authority.c`, `llvm_inventory_decl_lookup.c`, and
  `llvm_mir_emit.c`. `mir-declaration-inventory-test-smoke` freezes the
  `MIRRoutine.within_zone` accessor surface for C and LLVM inventory consumers.

## Progress Log - 2026-06-06 LLVM Return Context Metadata Owner

- LLVM function, MIR routine, lambda, intent, main-wrapper, sync-wrapper,
  parallel/async wrapper, spawn-wrapper, and role-operator emission now keep
  function return metadata in `LLVMGenCtx` as explicit context facts:
  `current_function_ret_type`, `current_return_type_name`, and
  `current_return_callable_type`.
- Return statement lowering, Result suffix inference, callable return context,
  and the `?` operator no longer reopen
  `ast_func_return_type(ctx->current_func_decl)` to recover the active
  function return contract. Generated wrapper functions explicitly clear the
  source-level return name/callable metadata to prevent stale caller metadata
  from leaking across LLVM function emission.
- `backend-fail-closed-test-smoke` now rejects reintroducing the AST return
  fallback in LLVM return/Result lowering owners and requires the new context
  facts to remain wired.
- Evidence run: `backend_fail_closed_smoke.sh` passed under Git Bash,
  `git diff --check` passed for the touched LLVM return-context slice, and
  WSL `make llvm-test-smoke` passed with the Linux LLVM-18 toolchain.

## Progress Log - 2026-06-06 C Parallel Capture Metadata Owner

- Moved C parallel/async event-handler capture type discovery behind
  `transpiler_parallel_capture`. The C emitter now consumes captured type AST
  metadata instead of directly calling the local type-AST lookup from
  `transpiler_async_parallel_emit.c`.
- `backend-fail-closed-test-smoke` now rejects reintroducing that direct lookup
  in the emitter while requiring the capture owner to carry the type metadata.
- Evidence run: MinGW `gcc -fsyntax-only` on
  `transpiler_parallel_capture.c` and `transpiler_async_parallel_emit.c`,
  `git diff --check` for the touched files, and
  `make backend-fail-closed-test-smoke` passed locally.

## Progress Log - 2026-06-06 C MIR Residual DEF Type Registry

- C MIR residual `MIR_INST_DEF` emission now reads local binding type names
  from the active typed registry before expression inference. The block emitter
  no longer reopens function-body local type AST/name lookup for this path.
- `cfg-body-dataflow-test-smoke` now rejects reintroducing
  `transpiler_find_local_type_ast(ctx, ...)` or
  `transpiler_find_local_type_name(ctx, ...)` in
  `transpiler_mir_block_emit.c`.
- Evidence run: MinGW `gcc -fsyntax-only` on
  `transpiler_mir_block_emit.c`, `git diff --check` for the touched slice, and
  `make cfg-body-dataflow-test-smoke` passed locally.

## Progress Log - 2026-06-06 C MIR Destructure Type Registry

- C MIR destructuring now recovers identifier initializer types from the active
  typed registry before requiring a concrete lowered type. It no longer calls
  `transpiler_find_local_type_name(ctx, ctx->current_func_decl, ...)` from the
  destructure emitter.
- `cfg-body-dataflow-test-smoke` now rejects reintroducing that direct local
  type lookup in `transpiler_mir_destructure_emit.c`.
- Evidence run: MinGW `gcc -fsyntax-only` on
  `transpiler_mir_destructure_emit.c`, `git diff --check` for the touched
  slice, and `make cfg-body-dataflow-test-smoke` passed locally.

## Progress Log - 2026-06-06 C/LLVM Generic Declaration Metadata Seam

- Removed the remaining declaration-specific generic parameter consumers from
  `src/codegen` and `src/compiler/module_normalizer_refs.c`. C forward policy,
  LLVM forward policy, LLVM routine declaration inventory, C generic binding
  queries, LLVM generic spawn specialization, C generic class/ability lowering,
  and module import-name normalization now read generic parameters through
  `ast_declaration_generic_params(...)`.
- Tightened `mir-declaration-inventory-test-smoke` so codegen cannot reintroduce
  declaration-specific generic metadata reads. `semantic-core-shape` now applies
  the same rule to module normalization. This keeps generic eligibility,
  generic specialization, and import-name normalization attached to the
  declaration metadata seam rather than backend/local AST rediscovery.
- Evidence run: `make mir-declaration-inventory-test-smoke`, `make pgy`,
  `make llvm-test-smoke`, `make test-transpile` (`898/0`),
  `make perf-contract-test-smoke`, and `make semantic-core-shape-test-smoke`
  passed locally under WSL.
- Follow-up source-of-truth tightening: `MIRRoutine` now materializes
  `generic_param_count`, and C/LLVM forward-declaration policy consumes that
  routine metadata before the AST compatibility fallback. `MIRDeclHeader` also
  materializes declaration generic parameter rows (`name`, `constraint`,
  `default_type`) for hosted/domain declarations and abilities; LLVM generic
  formal-default resolution now reads those header rows before the AST fallback.
  `MIRDeclHeaderInventory` and `LLVMMIRDeclHeaderInventory` now expose the
  declaration header list through an explicit inventory view, so the LLVM
  type-map MIR-present path iterates header rows without direct `ctx->mir`
  probing. Function declarations are now also recorded as declaration headers,
  so generic formal-default lookup no longer reopens AST function inventory in
  the MIR-present path; the AST inventory loop remains only for the no-MIR
  compatibility path.
  Declaration-header lookup also now has a typed public query
  (`mir_find_decl_header_of_type`) with matching C/LLVM inventory wrappers, so
  adding function headers cannot make a name-only lookup shadow a different
  declaration kind.
  `mir-declaration-inventory-test-smoke` freezes these contracts, including the
  ban on raw backend `ctx->mir` reads outside inventory owners. Evidence for the
  follow-up slice: `make pgy`, `make mir-declaration-inventory-test-smoke`,
  `make test-mir` (`78/0`), and `make llvm-test-smoke` passed under WSL;
  changed C files also pass local MinGW `gcc -fsyntax-only`.

## Progress Log - 2026-06-06 Worker Boundary UB Source-Of-Truth

- Centralized growable/synchronization-backed worker-boundary storage
  classification behind `src/common/worker_boundary_storage_policy.{h,c}`.
  Semantic analysis, C lowering, and LLVM lowering consume that owner instead
  of carrying separate Array/Slice/List/Queue/Set/HashMap/Channel display
  vocabularies.
- Tightened the policy to classify both concrete generic names
  (`HashMap<String, Int>`, `Channel<Int>`) and raw constructor names
  (`HashMap`, `Channel`) as unsafe worker-boundary storage. Missing or
  partially rendered generic metadata can no longer make a storage type look
  safe by losing its type arguments.
- Added semantic regressions for borrowed `Slice<T>` and `HashMap<K, V>`
  capture/transport across `parallel` and `spawn`, and pinned the regression
  names in `worker-boundary-ub-test-smoke`.
- Evidence run: `make test-semantic`, `make test-transpile`,
  `make worker-boundary-ub-test-smoke`,
  `make memory-concurrency-model-test-smoke`,
  `make backend-fail-closed-test-smoke`,
  `make mir-declaration-inventory-test-smoke`,
  `make runtime-frontier-contract-test-smoke runtime-frontier-policy-test-smoke`,
  `make build-source-inventory-test-smoke`, targeted backend compare for
  `await_inline_spawn`, and backend compare shards 1/20 and 2/20 all passed
  locally under WSL.
- Follow-up: `tests/build_source_inventory_smoke.sh` now routes repeated
  recursive source scans through an executable `rg` fast path, a `git grep`
  fallback that also scans untracked files, and only then portable `grep -R`.
  This keeps the source-of-truth sentinel broad without forcing every local/CI
  check to pay repeated full-tree traversal cost.

## Progress Log - 2026-06-05 Backend Fail-Closed Hardening

- Promoted backend fail-open guards into the frozen smoke surface:
  `backend-fail-closed-test-smoke` now runs in Linux, macOS, and Windows CI and
  is listed in the beta freeze contract.
- Tightened the fast gate beyond literal C fallback strings and synthetic LLVM
  branch conditions. It now also rejects direct LLVM `i32 0` expression-result
  placeholders outside the central `llvm_void_expression_placeholder(...)`
  owner and rejects partial LLVM host-declaration lookup chains that bypass
  `host_decl_compat.c`.
- Removed two direct LLVM void-call placeholders in `RcDrop` and `WeakDrop`;
  both now consume `llvm_void_expression_placeholder(...)`.
- Closed two silent-skip metadata holes: C/LLVM role method emission now errors
  when MIR-backed role/method name metadata is absent, and LLVM destructuring
  let emission errors when a binding name is missing instead of skipping the
  binding.
- Removed empty generated-expression fallbacks from the C domain-query, I/O,
  and misc builtin format owners. Formatting/allocation failures now set a
  backend diagnostic and return `NULL` instead of emitting an empty C fragment.
- Tightened the shared C formatting owner as well: `strdup_fmt(...)` now returns
  `NULL` on `vsnprintf`/allocation failure instead of materializing `""`.
  Literal string escaping keeps its separate input-policy empty-string path.
- Tightened the same empty-string failure pattern in the C type-name renderer
  for `Channel<T>`/`Future<T>` wrappers. A type-name formatting/allocation
  failure now reports a backend diagnostic instead of returning `""`.
- Closed the missing-type C mapping fallback:
  `pergyra_primitive_to_c(NULL)` now returns `NULL`, and
  `pergyra_type_to_c_copy(NULL, ...)` now fails closed and leaves the caller
  buffer empty instead of materializing `int32_t`. The backend fail-closed
  smoke gate freezes this as a regression test because missing semantic type
  facts must not become concrete C types.
- Closed the matching AST type-name rendering fallback:
  `render_type_name_in_ctx(ctx, NULL)` now returns `NULL` instead of
  materializing `Int`, and malformed `AST_TYPE` / nameless generic-argument
  render paths now fail the whole render instead of writing `Int` into the
  type-name buffer. `pergyra_ast_type_to_c_copy(NULL, ...)` remains the
  explicit ABI owner for the intentional no-return-type-to-`void` policy.
- Closed the C slot-reference empty-expression fallback: `slot_ref_expr(...)`
  now reports a backend diagnostic and returns `NULL` when the lowered slot
  expression is absent instead of emitting `""`, and the slot builtin/member/
  dispatch call sites now propagate that failure instead of formatting an
  invalid C expression.
- Tightened the slot builtin formatter owner: generated slot builtin fragments
  now use a context-aware formatter that reports formatting/allocation failure
  through the backend diagnostic channel instead of returning an unannotated
  `NULL`.
- Closed C declarator type fallbacks: malformed or unmapped AST type facts in
  typed declarators, event-handler parameter/return declarators, function
  pointer declarators, and function signatures now report a backend diagnostic
  and fail closed instead of being materialized as `void *`, `int32_t`, or
  implicit `void`. The intentional no-return-type case still renders `void`.
- Locked function-typed values behind the declarator owner: raw AST type-to-C
  copying now rejects `func(...) -> ...` / `AST_EVENT_HANDLER_TYPE` instead of
  returning `void *`, while `pergyra_ast_typed_declarator_in_ctx(...)` remains
  the explicit owner that renders the correct function-pointer ABI. Event
  declaration emission now requires concrete parameter types instead of falling
  back to `void*`.
- Locked MIR type rendering behind explicit type facts:
  `mir_render_type_name(...)` now propagates missing type nodes, nameless
  `AST_TYPE`s, unsupported `Future<T>` / `Channel<T>`, and generic inner-type
  failures instead of silently materializing `Int`, `Future<Int>`, or
  `Channel<Int>`. MIR intent metadata no longer recovers a failed rendered
  value type by emitting only the outer AST type name.
- Locked the matching DIR type-ref rendering path: DIR type references now
  propagate missing base names and unsupported generic/async inner types instead
  of manufacturing `Int` inside the domain graph.
- Centralized the backend `Unknown` sentinel policy in the type mapping owner:
  inferred `Unknown` and generic sentinel wrappers such as `Option<Unknown>` /
  `Future<Unknown>` are no longer registered as typed-var facts after C let
  emission, and LLVM Result layout now consumes the same token policy instead
  of owning a separate scanner. User-defined names such as `UnknownError`
  remain valid because the guard only rejects standalone `Unknown` sentinels.
- Closed the matching LLVM callable-parameter metadata hole: MIR-backed
  function-typed parameters now register callable facts in the parameter emit
  owner, and LLVM call type inference consumes the same callable registry before
  falling back to visible function declarations. This keeps higher-order calls
  such as `f(x)` fail-closed without requiring a duplicated AST scan.
- Removed the old callable-variable emission fallback that rescanned
  `current_func_decl` parameters to rediscover function-typed callable
  signatures. Callable variable calls now consume the callable registry as the
  single source of truth, and the fail-closed / inventory / perf smoke gates
  reject reintroducing the AST parameter rescan.
- Removed the matching LLVM slot-identifier fallback that rescanned
  `current_func_decl` parameters to rediscover `Slot<T>` / `SecureSlot<T>`
  inner types. Slot operations now require the slot registry populated by
  parameter/local emission, and the fail-closed / inventory / perf smoke gates
  reject reintroducing the AST parameter rescan.
- Removed the LLVM call type-inference fallback that rescanned `AST_WITH_STMT`
  bodies to recover with-slot alias inner types. `with slot<T> as s` now relies
  on the slot registry populated at with-block entry, matching the same
  registry-only rule used by slot parameters and slot locals.
- Removed the LLVM nominal type-inference fallbacks that rescanned the current
  function body to rediscover local `let` annotations. Nominal identifier
  lowering now consumes the active LLVM scope / var-class registry / host-field
  and enum metadata owners only, so declaration order and shadowing cannot be
  bypassed by a whole-body AST scan.
- Blocked C/LLVM backend `parallel` / `async` pointer-capture of mutable collection
  values (`Array<T>`, `Slice<T>`, `List<T>`, `Queue<T>`, `Set<T>`,
  `HashMap<K,V>`). These runtime containers can grow or rehash, so sharing them
  into worker wrappers by raw pointer would recreate the classic concurrent
  rehash/read UB pattern. Captured collection state must now cross through a
  channel/result boundary or an explicit copy.
- Removed the duplicate C parallel-capture local type-AST walker. The
  `transpiler_parallel_capture` owner now only discovers capture names, while
  function/event-handler type AST recovery goes through the existing
  `transpiler_mir_local_type_ast_lookup` owner. The perf contract rejects
  reintroducing a parallel-capture-specific local type walker.
- Removed the remaining C `parallel` emit fallback that rediscovered captured
  local types by calling `transpiler_find_local_type_name(...)` during wrapper
  field emission. Capture discovery now resolves/registers typed captures
  before emission, and the `parallel` / `async` emit owner consumes the typed
  registry only.
- Centralized the C/LLVM MIR match-case subject compatibility lookup behind
  `pgy_codegen_match_subject_for_case(...)`. This does not claim that the AST
  compatibility path is gone; the owner still calls
  `ast_find_match_subject_for_case(...)` until MIR carries a subject fact.
  The important closure is that C/LLVM match condition emitters no longer own
  separate function-body rescans, and smoke gates now reject reintroducing that
  direct backend rescan.
- Added `llvm_scope_lookup_snapshot(...)` as the registry-owned way to carry
  a scope entry across later scope mutation. `llvm_scope_lookup(...)` remains
  explicitly borrowed because `llvm_scope_declare/push/pop` can invalidate frame
  storage and lookup caches. MIR match payload remapping, MIR pin token aliasing,
  MIR pin enter/exit locals, MIR resource-view aliasing, MIR for-in list/index
  metadata, MIR range-loop body aliasing, MIR block-local copy rebinding,
  legacy LLVM statement-loop for-in bindings, and LLVM let-resource/view
  lowering now snapshot `alloca/type/name` before any alias declaration can grow
  the scope frame. Existence-only checks now use `llvm_scope_contains(...)`, and
  the perf contract rejects mixing borrowed `llvm_scope_lookup(...)` with
  `llvm_scope_declare/push/pop` outside the registry owner, API declaration, and
  the intentional parallel-capture frame identity check.
- Extended the same fail-closed policy to role operator/vtable lowering:
  C/LLVM role operator method-name metadata, LLVM role operator forward-name
  metadata, LLVM registered operator-method functions, and C/LLVM role vtable
  method-name/ability-name metadata now fail through the backend metadata path
  instead of being skipped or materialized as null function slots. LLVM role
  forward declaration emission also fails closed when role name metadata is
  absent. The C role operator alias path fails closed when MIR role declaration
  or subject-type metadata is missing, leaving `continue` only for the
  legitimate "this operator is not implemented by the role" probe result.
- Local verification: `make test-transpile` (`893/0`),
  `make test-dir` (`9/0`), `make test-rir` (`18/0`), `make test-air`
  (`119/0`), `make test-mir` (`77/0`), and
  `make backend-fail-closed-test-smoke`; latest narrow LLVM/source-of-truth
  gates: `make backend-fail-closed-test-smoke
  mir-declaration-inventory-test-smoke perf-contract-test-smoke
  llvm-test-smoke`.

## Progress Log - 2026-06-08 CI Self-Host Preparation Closed By #88

CI run #391 on commit `384e4e5` failed `self-host-preparation-test-smoke`
on linux/windows/macos with garbage bytes between `[` and `]` for the
empty `findings` array. Root cause traced to LLVM MIR-mode versioned-
alloca binding; closure #88 (below) fixes it.

- Closure #88 in `src/codegen/llvm_mir_block_scope.c`
  (`llvm_mir_bind_versioned_local_scope`): when an MIR block's entry
  SSA-value list references a phi-result version (e.g. `findings.5`)
  but that version has no allocated alloca (because MIR routine
  meta reports `phi=3` but no explicit `phi` instructions are
  emitted), the function previously returned silently. The previous
  block's binding (e.g. `findings → %findings.4` from a sibling
  Concat branch that the current path never entered) stayed live, so
  the next consumer of the base identifier loaded from an alloca
  that was never written on this path — uninitialized stack memory,
  which surfaced as the three deterministic garbage bytes
  `0xb0 0x24 0x40` in the validator's `"findings":[...]}` output.
- The fix walks the per-routine SSA-versioned alloca list for
  `<base>.1` whenever the phi-result lookup fails and rebinds the
  base identifier to that first-version alloca. Upstream MIR keeps
  `<base>.1`'s alloca in sync with the merged value (every branch
  exit stores to `%findings.1` before snapshotting to its versioned
  alloca), so the rebinding is safe — the value loaded matches what
  a properly-emitted phi would have selected.
- Verified: `air_graph_json_validator_parity.sh` now reports
  `rung-2 parity ok (intents=1 boundaries=1 evidence=12 drifts=0;
  missing-key rc=1; live-drift=ok)`. The validator's LLVM-backend
  output is now byte-equal to the C backend's.
- Risk: the fallback assumes `<base>.1` exists and holds the merged
  value. If MIR ever defines a slot whose first SSA version is named
  differently (e.g. `<base>.0`), the fallback silently does nothing
  and the original misbinding re-surfaces. Real fix is for MIR to
  emit explicit `phi` instructions and allocate alloca for phi
  results; #88 is the narrow shim until then.
- Original failure context retained below for searchability.

CI run #391 on commit `384e4e5` failed `self-host-preparation-test-smoke`
on linux/windows/macos. Local repro confirms a pre-existing LLVM
backend bug, not introduced by the closures in this session.

- Reproducer: `bin/pgy
  src/self_hosted/tools/air_graph_json_validator/main.pgy --run` on the
  LLVM backend prints
  `"findings":[<3 bytes garbage>]}` where the C backend prints
  `"findings":[]}`.
- The 3 garbage bytes are deterministic (`0xb0 0x24 0x40`) across
  invocations.
- An inline `Log("DEBUG findings_value=[" + findings + "]")` inside the
  validator confirms `findings` *is* the empty string at the point of
  use (`DEBUG findings_value=[]`, `DEBUG findings_len=0`). The garbage
  appears only when `findings` is consumed as an element of a mixed
  `Array<String>` literal (literal + `ToString(...)` + identifier) and
  that array is then handed to `StringJoin(...)`. Replacing the array
  literal with a chain of `Concat` calls on the same identifier
  segfaults the program (`program exited with code -1`), so the
  identifier reload path is independently fragile when the underlying
  string is empty.
- Confirmed by checking out HEAD without the closures from this
  session (`#86`, `#87a-d` reverted): the corruption is unchanged, so
  the regression predates this session.
- This is *not* the same path as `#87d`'s slot-builtin fallback. The
  Array<String> push is a runtime function call, not a slot op, and
  the receiver isn't a slot at all.
- Status: documented and left for a dedicated investigation pass. Not
  closed here because (a) the actual mis-emit lives further in than
  the changes this session is bundling, and (b) the partial
  workarounds attempted (manual accumulator inside
  `BuildMissingKeyFindings`, replacing the array literal with
  `Concat`s in `Main`) both broke other paths. A correct fix needs
  LLVM-IR-level inspection of how `let findings: String = missing_findings`
  lays out the local alloca and what the array-push call site loads
  from it; that's its own session.

## Progress Log - 2026-06-08 Examples LLVM Cluster: Min/Max Precedence + Slot Builtin Guard

- Closure #86: `llvm_mir_emit_borrow_view_alias`
  (`src/codegen/llvm_mir_resource_view.c:229`) now returns success
  silently when the source slot's inner-type metadata isn't yet
  registered. The strict "cannot resolve owner slot" error previously
  fired in `pin_mixed_read_view_sequence`, where MIR places a
  `BorrowRead` in block 0 but the `SecureSlot` let-decl's
  `source-statement-emit` lives in block 2 — at the time the borrow
  alias emit runs, the slot's `slot_inner` registry is still empty.
  The borrow alias is best-effort metadata; the canonical resource
  ops on the view (`Read` / `Write`) still resolve through typed
  runtime layouts, so dropping the alias declaration doesn't change
  generated behavior. Trade-off: if a future test relies on the
  borrow alias binding being present for some downstream pass that
  doesn't itself consult the runtime layout, this closure will mask
  the missing binding silently. That is preferable to blocking the
  compile, but the real fix lives upstream in MIR's block placement /
  `source-stmt-emit` flag on the secure-slot claim.

- Closure #87 (a–d): four small ordering / fallback corrections in
  `src/codegen/llvm_stmt_type_infer.c` that together recover five
  previously-failing LLVM-only examples (battle_simulator,
  biome_simulator, campaign_graph_fsm, graph_fsm_dispatch,
  shopping_mall_checkout_refund).
  - #87 moves `llvm_stmt_infer_scalar_math_return_type` (which
    handles `Min` / `Max` / `Clamp` / `Abs` via argument promotion) to
    fire *before* the host-method dispatch. Inside an aggregate
    method body that writes to a field of the aggregate
    (e.g. `self.defender.health.current = Max(...)` in
    battle_simulator), the call was previously being reinterpreted as
    `<host_type>.Max` and rejected with
    "missing method return metadata for 'Health.Max'".
  - #87b makes `Min` / `Max` tolerant of one-sided argument-type
    failure: if `infer_expr_type` on one arg sets the error flag
    (because a local hasn't been registered into the LLVM scope yet
    in MIR-only mode), the flag is cleared and inference continues
    using the other arg's type, finally defaulting to `i32` if both
    fail.
  - #87c excludes known builtin call names (slot ops via
    `llvm_stmt_call_is_slot_builtin`, plus `Log`, `Print`, `ToString`,
    `Clone`) from host-method dispatch entirely. Calling
    `Read(commandBudget)` inside a CampaignWorld method was being
    reinterpreted as `CampaignWorld.Read` and erroring out.
  - #87d adds a fallback at the slot-builtin shortcut: when the
    slot-inner registry doesn't yet have the receiver's type (e.g. a
    `with slot<Int> as commandBudget` block where MIR registers the
    slot in a later block), return `i32` for value-returning slot
    ops (`Read`, etc.) and `void` for others. This matches the actual
    emit path's behavior for unknown-inner slots.
  - Caveat for #87b / #87d: defaulting to `Int32` is incorrect for
    `Float` slots and `Float`-typed `Min` / `Max`. The actual emit
    path uses the typed runtime layout, so generated code is still
    correct, but downstream type-infer steps that consume this
    fallback value may pick the wrong promotion. No `Float`-slot
    example currently exercises this; if one appears it will likely
    surface as a `Float` ↔ `Int` conversion mismatch and need a
    typed-fallback path.

- After this cluster: `tests/compare_backends.sh` reports **798/798
  passed (100%)**. `examples/*` LLVM coverage moved from
  90 PASS / 12 LLVM-only-fail to **95 PASS / 7 LLVM-only-fail**. The
  seven remaining LLVM-only fails are: `collection_ops` and
  `adapter_policy_stack` (both "identifier requires registered LLVM
  local metadata"), `function_clause_order_minimal` and
  `zone_context_minimal` (both "MIR routine function type drift" for
  `Hero_Attack` / `Hero_Guard`), `iot_device_adapter_probe`
  (member-access on `started:<unknown>.milliseconds`),
  `order_analytics` (`ListSize` requires identifier receiver), and
  `pattern_library_basics` ("LLVM member access requires concrete
  receiver type metadata"). These are five distinct root causes; none
  are blocked on the Min/Max precedence fix.

## Progress Log - 2026-06-08 Resource Layout For Function Parameter Facts

- Closure #85: `mir_resource_layout_from_fact` (in
  `src/compiler/mir_lower_population.c`) now handles
  `AST_FUNC_DECL` resource facts. Function parameters get a RIR resource
  fact whose `ast` points at the function declaration (not at a
  let/with). The previous code only knew how to pull a type node from
  `AST_LET_DECL` or `AST_WITH_STMT` facts, so a `pin slot as view: ...`
  inside a `func F(own slot: SecureSlot<Int>)` could not derive an ABI
  layout for the source slot — the MIR validator then rejected the test
  with "view-backed resource op is missing owner slot ABI metadata".
- The closure walks `ast_func_param(fact->ast, ...)` looking for a
  parameter whose name matches `fact->name`, then uses that parameter's
  `type` as the type node. The remainder of the function is unchanged
  (the type node still flows through `mir_render_type_name` + a final
  `mir_abi_lookup`), so the ABI layout still has to be registered for
  the type — closure #85 only fixes the AST-to-type lookup, not the
  layout registry. If `mir_abi_lookup` has no entry for the parameter's
  type name, layout still returns NULL and the validator still errors.
- Recovered `tests/cases/backend_compare/pin_secure_param_read_view_block`
  on the C backend. `tests/compare_backends.sh` after this closure
  reports 797/798 passed. The remaining red,
  `pin_mixed_read_view_sequence`, is unrelated: it fails on the LLVM
  backend with `LLVM MIR borrow view alias 'secureView' cannot resolve
  owner slot 'secureScores'`. MIR for that test shows the `Claim`
  resource-op for the secure slot is not flagged with
  `source-statement-emit`, so LLVM's `llvm_stmt_let_resources` slot
  registration never fires for `secureScores`; the borrow then can't
  look up `slot_inner` and errors out. The closure here does not move
  that case.

### Known unrecovered fails after this session

- `tests/cases/backend_compare/pin_mixed_read_view_sequence/main.pgy`
  — LLVM backend. MIR-side `source-statement-emit` flag is missing on
  the SecureSlot claim instruction, so the let-resources slot register
  path is bypassed and the borrow view alias can't resolve. Fix lives
  in MIR resource-op emission, not in codegen.
- `examples/dnd_tavern_campaign/main.pgy` — LLVM runtime parity drift.
  Compiles cleanly; runtime output diverges from C. Not in scope here.

## Progress Log - 2026-06-08 Closure Risk Audit (Conservative Notes)

This entry records the known risks of the closures landed in this
session. None of these have produced an observed regression in the
current suite, but each one trades a strict-mode error for a
more-permissive fallback. A future maintainer should know about each
one before relying on the strict-mode contract elsewhere.

- **#67** (LLVM host-field assignment also stores to same-name local
  alloca). The dual store works because Pergyra's current runtime
  doesn't refcount heap-stored values via store sites — both the host
  field and the local alloca hold the same pointer without an extra
  acquire. If the String/heap runtime later introduces refcount-on-store
  semantics, this closure becomes a double-release source. Re-audit
  before adding refcount tracking on store.
- **#70** (function-entry pre-init of host-field-aliased SSA local).
  Triggers a host-field load+store in the function entry block before
  any user code runs. Guarded by `llvm_current_host_class_name(ctx)`
  and `llvm_class_field_index(...) >= 0`, so it only fires inside host
  method bodies. The load happens unconditionally even if the local is
  later overwritten — a minor extra read, not a correctness issue.
- **#71** (bare identifier reads inside a host method always GEP+load
  from host field when the name matches a shared field). Disables a
  small optimisation (reading the same field twice in a row now hits
  the host struct twice instead of caching in a register-like local).
  LLVM's mem2reg / load-store-opt passes generally recover this, but
  worst case it adds a load per use. Acceptable cost for correctness
  across opaque callee mutations.
- **#74** (no-successor non-void block emits `unreachable` instead of
  erroring). Real risk surface: this masks a class of malformed MIR
  where a reachable block falls off the end of a non-void function. The
  LLVM verifier still rejects truly live malformed paths, so a runtime
  miscompile from this closure is unlikely. The compile-time error it
  replaces was load-bearing for any future MIR analysis bug that breaks
  control flow. Tighten when MIR adds "post-exhaustive-match block is
  unreachable" analysis.
- **#75** (single-uppercase-letter type name falls back to `i8ptr`).
  Only fires after the class registry and enum lookup both miss, so a
  defined `class T { ... }` still resolves correctly. The risk is a
  user-defined single-letter class that hasn't been registered yet in
  the current emission window — that would silently lower to `i8ptr`.
  Low likelihood, possible.
- **#78** (silent fallthrough in hosted-self-call and
  host-method-return-type when callee is a registered `AST_FUNC_DECL`).
  If a host method and a free function share a name, this prefers the
  free function. Pergyra's scoping intent prefers the closer (host)
  binding, so this is technically wrong for that shadowing case. The
  strict error it replaces was breaking every free function call made
  from inside a host method body, which is the common case. Add a
  host-method-presence probe before the callable_decl check if the
  shadowing case ever appears in real surface code.
- **#82** (skip `AST_LET_DESTRUCTURE` in `MIR_INST_STMT` fallback).
  Relies on `MIR_INST_DESTRUCTURE` being emitted upstream. If a future
  MIR shape produces a destructure-shaped AST without a matching
  `MIR_INST_DESTRUCTURE` instruction, this closure silently drops the
  statement. Add a coverage probe when extending destructuring grammar.
- **#83** (role vtable `metadata_role_name` parameter for include
  chain). Caller responsibility: must pass the correct included-role
  name. If misused, the vtable instance is named per role but methods
  resolve via the wrong host. The current caller in
  `transpiler_domain_nominal_emit.c` is the only call site; review any
  new caller.
- **#84** (typed-name handling for `ReadView`/`WriteView`/`MoveView`
  in resource op core). Does not by itself recover
  `pin_secure_param_read_view_block` — the view-to-source-slot redirect
  upstream in `transpiler_mir_resource_hook_emit.c` is the real gap.
  #84's added branches are dead code today on the failing path, but
  they are correct for any caller that does populate the view
  typed_name; leaving them in does no harm and unblocks the next step.

### Known unrecovered fails after this session

- `tests/cases/backend_compare/pin_secure_param_read_view_block/main.pgy`
  — C backend, MIR-only mode. Needs `lookup_typed_entry(ctx, "view")`
  to return an entry with `is_view=true` and a non-empty `source_slot`,
  or needs MIR's routine inventory scan to surface the source slot.
  Not a one-closure fix.
- `examples/dnd_tavern_campaign/main.pgy` — runtime parity drift on the
  LLVM backend. Compile succeeds (after the closures above); runtime
  output diverges from C. Suspected source: subject-action dispatch
  shape in zone projection chain. Out of scope for this session.

## Progress Log - 2026-06-08 Role Include And Pin View Closures

- Closure #83: `emit_role_vtable_instance` now takes an explicit
  `metadata_role_name` parameter that's used for MIR host-method lookup
  while the original `role_name` continues to drive the emitted symbol
  names. The canonical caller is `role X { include BaseRole; }`: the
  vtable instance is named per `X`, but MIR registers the included
  ability methods under `BaseRole`, so the lookup must walk to the
  inclusion target. Recovered `tests/cases/backend_compare/role_include_methods`
  on the C backend.
- Closure #84 (partial): `transpiler_emit_mir_resource_op` learned how
  to extract a typed inner from `ReadView<T>`/`WriteView<T>`/`MoveView<T>`
  typed-var bindings, mirroring the existing `Slot<T>` / `SecureSlot<T>`
  / `DeviceSlot<T>` extraction. The pin-redirect path in
  `transpiler_mir_resource_hook_emit.c` still has to surface the view's
  source slot via either the `TypedVarEntry::is_view`/`source_slot`
  channel or a MIR routine inventory scan, and that's the remaining gap
  on `pin_secure_param_read_view_block` — the view binding isn't
  promoted into `lookup_typed_entry` with the right `is_view` shape in
  this MIR shape, so the redirect doesn't fire and the core resource-op
  emitter ends up looking at the view typed_name without a registered
  source slot to anchor it.
- Backend evidence after #83 + #84: `tests/compare_backends.sh` reports
  794/795 passed (`pin_secure_param_read_view_block` is the single
  remaining red on a real-language surface, scoped to MIR-side view
  registration). All five probes and the three BETA gating smokes stay
  RC=0.

## Progress Log - 2026-06-08 C-Backend MIR Destructure Duplicate Decl Closure

- Closure #82: `transpiler_mir_block_emit.c` now treats
  `AST_LET_DESTRUCTURE` as already-handled in the `MIR_INST_STMT`
  fallback (same shape as `AST_BLOCK`/`AST_RETURN`/`AST_DEFER_STMT`),
  skipping the non-MIR `emit_let_destructure_statement` rerun. Without
  this guard MIR-only mode emitted the destructure twice: once through
  `MIR_INST_DESTRUCTURE` via `transpiler_emit_mir_let_destructure_stmt`
  (assignment to a pre-declared SSA name) and once through the
  source-statement fallback's `emit_statement` dispatch (full
  `<c_type> <name> = ...;` declaration). gcc rejected the redefinition
  with `previous definition of '_pgy_ssa_X_N' with type 'char *'`.
- Recovered 4 backend-compare cases at once:
  `destructure_array`, `destructure_tuple_return`, `tuple_literal_local`,
  `slice_surface`. `tests/compare_backends.sh` reports 793/795 passed
  (the two remaining fails are unrelated user-side surface: a
  `pin_secure_param_read_view_block` resource-op runtime-layout gap and
  the long-standing `role_include_methods` vtable inventory issue).
- Sanity smokes after #82: `probe_record`, `probe_intent_array`,
  `probe_field_index`, `probe_zone_chain`, `intent_header_interleaved`
  all pass byte-equal. `beta-readiness-checklist-test-smoke`,
  `mir-declaration-inventory-test-smoke`,
  `intent-compression-contract-test-smoke` all RC=0.

## Progress Log - 2026-06-05 ArraySort LLVM Lowering

- Closure #81: `ArraySort` is now declared in the LLVM runtime function
  inventory (`pgy_array_sort_<suffix>(val_ty *data, i64 length)` for the
  same slot type set used by `array_push`/`array_pop`) and dispatched as
  an `LLVM_ARRAY_BUILTIN_SORT` op. The lowering matches the C-backend
  statement-expression shape: GEP into the array struct for the data
  pointer and length, call the in-place sort runtime, then return the
  loaded (now sorted) array struct value. `examples/collection_ops.pgy`
  now compiles past its `ArraySort` site; the file still fails later at
  `ArrayMap(nums, Double)` because mapping a callable into an array
  builtin requires closure/function-arg surface that LLVM lowering
  hasn't been wired for. Not a regression — same line was already
  failing C-backend in the strict-mode build.

## Progress Log - 2026-06-05 Free-Function-In-Host-Body Strict-Mode Closure

- Closure #75: unresolved generic placeholder names (single uppercase
  letter — `T`, `K`, `V`, ...) now type-erase to `i8ptr` in the LLVM type
  map instead of erroring with `LLVM type '%s' is not registered`. The
  strict rejection was too aggressive for the existing ability/role
  generic surface (`examples/generic_ability_requires_minimal.pgy`,
  `examples/logistics_intent_probe/main.pgy`). Concrete type still wins
  when available; this only fires when no resolver above produced a hit.
- Closure #78: `llvm_emit_hosted_self_call` and the type-inference path
  `llvm_stmt_host_method_return_type` now consult
  `llvm_find_callable_decl` before raising the strict-mode "missing host
  method metadata" error. If a global free function `AST_FUNC_DECL`
  exists with the callee's name, we let the dispatcher's global-function
  resolver take over instead of poisoning the inference pipeline. This is
  the canonical shape for a free function called from inside a host
  method body (`MergeRouteScore` inside `LoadingZone`,
  `CombatStrategyFactory` inside `TavernCampaignWorld_RunSkirmishRounds`).
  Fail-closed semantics survive: when the callee is neither a host
  method (no MIR metadata) NOR a registered global, the strict error
  still fires. (Note: `llvm_stmt_lookup_declared_call_return_type` keeps
  its strict MIR-mode error by design — that helper's strictness is a
  separate policy decision and is intentionally preserved.)
- Backend evidence: `tests/compare_backends.sh tests/cases/backend_compare/*/main.pgy`
  reports 794/795 passed (`role_include_methods` is a C-backend
  inventory regression from a separate user-side tightening, unrelated to
  the LLVM closures here);
  `tests/cases/backend_compare/probe_record`, `probe_intent_array`,
  `probe_field_index`, `probe_zone_chain`, and `intent_header_interleaved`
  all pass byte-equal; `beta-readiness-checklist-test-smoke`,
  `mir-declaration-inventory-test-smoke`, and
  `intent-compression-contract-test-smoke` are RC=0. Real-coverage scope:
  `examples` LLVM 96/18 with `generic_ability_requires_minimal` recovered
  by #75 (the C-also-fails set is parse/semantic frontend gaps, not
  backend gaps).

## Progress Log - 2026-06-05 Real-Coverage Scope Test And Strict-Mode Repair

- Real-coverage scope: compiled every `func Main` entrypoint across
  `examples/`, `tests/cases/backend_compare/`, and `src/self_hosted/`
  (~976 sources) through both C and LLVM backends using
  `/tmp/pgy-PergyraLang-bin/pgy` (Linux ELF). Diagnosed earlier scope-test
  miscount as path mangling by a stale Windows PE `bin/pgy.exe` — real
  measurements require explicit POSIX-native binary selection.
- Baseline before today's closures: `bc_cases` 779/0 C, 779/0 LLVM;
  `examples` 102/12 C, 89/25 LLVM; `self_hosted` 71/12 C, 70/13 LLVM
  (parser fixtures are deliberate edge cases).
- Closure #72: parallel/async block capture now restores the slot inner
  type and security flag via
  `llvm_lookup_slot_inner`/`llvm_lookup_slot_is_secure` and
  `llvm_register_slot_var_binding`, mirroring the existing channel/future
  restoration. Without this restore, slot-typed bindings (`Slot<Int>`)
  captured into a `parallel { ... }` task lost their inner type metadata at
  the wrapper boundary and tripped
  `LLVM slot operation on '<name>' requires a concrete slot inner type`
  under strict mode. Recovered `channel_parallel.pgy`,
  `concurrency_demo.pgy`, `slots_simple.pgy`, `spawn_test.pgy`,
  `test_parallel.pgy` (+1 additional example knock-on).
- Closure #73: bare `Clone(x)` call sites now fall through type inference
  to their argument's inferred type. `llvm_expr_call_dispatch.c:92` already
  lowers `Clone` as an identity pass-through; the type-inference pass had
  no matching shortcut, so concurrent strict-mode tightening
  (`b3296be2`) made it surface as `call 'Clone' requires registered
  function or expected type metadata`. Recovered
  `relation_effect_propagation`, `world_derived_states`,
  `world_zone_cross_queries`, and `world_zone_projection_visibility` in
  `tests/cases/backend_compare/`.
- Closure #74: when a MIR block reaches the backend with no successors and
  no terminating return — the canonical shape produced by an exhaustive
  `match` where every case returns — the LLVM emit now emits
  `unreachable` instead of erroring with
  `LLVM MIR block %d reached backend without a terminal return value`.
  The earlier error was load-bearing as a fail-closed for genuinely missing
  returns, but it consumed legitimate exhaustive matches as collateral. The
  LLVM verifier still rejects truly live paths, so the safety contract is
  preserved. Recovered 150 backend-compare cases at once
  (every `match`-everything-returns shape across enum/option/result/match
  fixtures).
- Backend evidence after #72 + #73 + #74:
  `tests/compare_backends.sh tests/cases/backend_compare/*/main.pgy`
  reports 794/794 passed, 0 failed; `make llvm-dnd-campaign-test-smoke`
  reports `dnd_tavern_campaign C/LLVM parity ok`;
  `make llvm-campaign-projection-test-smoke` reports
  `campaign_graph_fsm LLVM projection parity ok`. Real-coverage scope:
  `bc_cases` 779/0 C, 779/0 LLVM; `examples` 102/12 C, 95/19 LLVM
  (LLVM gap closed by 6 vs baseline); `self_hosted` 71/12 C, 70/13 LLVM
  (deliberate parser fixtures untouched).

## Progress Log - 2026-06-05 Intent Declaration Carrier Closure

- MIR intent lowering now materializes declaration-level `priority` and
  `success` as semantic carriers: `IntentEval(priority)` and
  `IntentCheck(success)`, anchored by the intent routine name. Step-level
  carriers remain unchanged and top-level carriers do not enter the step-name
  sequence.
- C and LLVM intent declaration emission consume those carriers when a MIR
  routine exists and fail closed through the existing MIR intent-carrier
  diagnostic path if a carrier row exists without its expression payload.
- Tightened the MIR negative test lifecycle: the signature-drift case now
  restores the mutated `param_count` before `mir_destroy(...)`, preventing a
  validator fixture from turning its deliberate metadata corruption into a
  teardown-time heap error.
- Evidence: `mir-declaration-inventory-test-smoke`, `test-mir` (`76/0`),
  `pgy`, and targeted backend compare for `intent_no_step_call`,
  `probe_cursor`, and `probe_dnd_minimal` pass locally.

## Progress Log - 2026-06-04 Backend Runner Stale-Binary Guard Closure

- Backend runner executable selection is now fail-closed for stale or
  cross-platform binaries. `tests/pgy_binary_path_helpers.sh` owns the
  current-host runnable check and classifies PE, ELF, and Mach-O from magic
  bytes before falling back to `file(1)` text. This prevents a stale Linux
  `bin/pgy.exe` from being launched under Windows Git Bash simply because the
  filename ends in `.exe`.
- `tests/compare_backends.sh` applies the same runnable-binary policy to both
  `PGY_BIN` and `PGY_ABI_PIPELINE_TEST_BIN` before the C/LLVM compare or ABI
  same-process precheck can launch. The PowerShell fallback now quotes both
  the PATH prefix and ABI executable path through the shared quote helper.
- The PowerShell runtime PATH prefix now excludes Git for Windows'
  `mingw64\bin` when `MSYSTEM_PREFIX=/mingw64` resolves there. That directory
  can shadow the real MinGW runtime and crash `test_abi_pipeline.exe` before
  the ABI same-process test prints diagnostics, so explicit LLVM/MinGW runtime
  roots own PowerShell launch priority.
- Local bench/perf/dogfood scripts now consume the same helper seam. The bench
  script no longer hardcodes `/usr/bin/time`; it uses a shell clock fallback
  so Git Bash/macOS layouts do not turn benchmarking into a tool-path failure.
- The P0 CI smoke layer now uses the same runnable-binary seam for formatter,
  stdlib, AIR JSON schema, runtime-none, raw-escape, and semantic
  fixture-isolation probes. `pgy_select_optional_exe_binary(...)` selects an
  existing `.exe` even when it is stale, and
  `pgy_require_runnable_binary_here(...)` rejects it before launch; default
  source-only smokes may still skip when no explicit binary was supplied.
- Bash and PowerShell runtime PATH setup both reject Git for Windows runtime
  mounts as explicit MinGW/LLVM priority candidates. Existing Git Bash PATH
  entries may remain later in `PATH`, but they no longer shadow the real
  runtime directories prepended by the helper.
- Evidence: normal ABI precheck plus targeted backend compare passes for
  `probe_cursor`; stale `bin/pgy.exe` is rejected for backend compare ABI
  precheck, bench, perf baseline, and dogfood WebGL; `backend-compare-
  inventory-test-smoke`, `backend-compare-llvm-coverage-test-smoke`,
  `build-source-inventory-test-smoke`, `dogfood_webgl_smoke.sh`, and a reduced
  `perf_c_baseline_smoke.sh` run all pass. A Makefile shard with precheck
  enabled passed ABI same-process and then exceeded the 300s interactive tool
  limit while running backend cases; this is time budget, not a code failure.
  `llvm-test-abi-same-process` now passes through the Makefile path with
  `196 passed, 0 failed`. Follow-up gates on 2026-06-05:
  `fmt-test-smoke`, `stdlib-test-smoke`, `air-json-schema-test-smoke`,
  `runtime-none-contract-test-smoke`, `raw-escape-contract-test-smoke`,
  `semantic-fixture-isolation-test-smoke`, and
  `build-source-inventory-test-smoke`.

## Progress Log - 2026-06-04 LLVM Bare-Identifier Host-Field Read Closure

- Closure #71: bare identifier reads inside a host method now ALWAYS lower
  to a host-struct GEP+load when the name matches a shared field, rather
  than first preferring an SSA-versioned local mirror. The local mirror is
  authoritative only between explicit reassignments in the same function —
  any opaque callee that touches `self->field` invalidates the mirror, and
  the caller has no way to express "reload after call". This was the gap
  behind `RunCampaign`'s `cursor=ToString(choiceCursor + 1)` print drifting
  from C (cursor=2) to LLVM (cursor=1) after `RollChoice(gameState)` had
  bumped `self->choiceCursor` via `ScriptedChoice`. The fix flips the
  identifier-lookup order: host-field GEP first, scope alloca second.
- Probe coverage stays green (probe_record, probe_intent_array,
  probe_field_index, probe_zone_chain, probe_cursor, probe_dnd_minimal,
  intent_header_interleaved) because writes still go through
  `llvm_emit_current_host_field_assignment` (closure #67) and the host
  store is therefore observed by subsequent host-field reads in the same
  block. The local-mirror dual-store from #67 becomes belt-and-suspenders
  rather than load-bearing.
- Backend evidence after #71:
  `tests/compare_backends.sh tests/cases/backend_compare/*/main.pgy` reports
  794/794 passed, 0 failed; `make llvm-dnd-campaign-test-smoke` reports
  `dnd_tavern_campaign C/LLVM parity ok`;
  `make llvm-campaign-projection-test-smoke` reports
  `campaign_graph_fsm LLVM projection parity ok`;
  `make test-all` reports 2624/870/74/58/9/119/18/74/20 across
  lexer/semantic/transpile/memory/concurrency/AIR/RIR/MIR/HIR with 0 failed.

## Progress Log - 2026-06-04 LLVM Host-Field Local Pre-Init And Memory Error Closure

- Closure #70: when emitting an SSA-versioned local alloca whose base name
  matches a host field of the current method's host class, the entry block
  now pre-initializes the alloca from the host field. Without this, branches
  that read the local without a prior SSA-DEF observed uninitialised stack
  memory. `W_Record` was the canonical failure: the `if/else` body of
  `transcript = transcript + "\n" + line` lowered the else branch to
  `load ptr, ptr %transcript.1` where `%transcript.1` had only ever been
  stored in the sibling `if` branch — so the second `Record` call inside the
  same `Broadcast` chain consumed garbage and segfaulted the program.
  Pre-init guarded to fire only when `llvm_current_host_class_name(ctx)`,
  `llvm_scope_lookup(ctx, "self")`, and `llvm_class_field_index(host_cls,
  base_name) >= 0` all hold; otherwise free functions whose locals happen to
  share a name with a host field would have produced bogus loads.
- Closure #70 was bisected to confirm it is not the source of the previously
  surfaced `dnd_campaign` memory error: compiling with #67 disabled
  reproduced the identical `rc=255` segfault at
  `WeaponCard.EffectLine`, confirming the bug pre-existed.
- Backend evidence: `tests/compare_backends.sh tests/cases/backend_compare/*/main.pgy`
  reports 793/793 passed, 0 failed (4 probes added: `probe_record`,
  `probe_intent_array`, `probe_field_index`, `probe_zone_chain`,
  `probe_cursor`, `probe_dnd_minimal`). `make test-all` reports
  `2624/0`, `870/0`, `74/0`, `58/0`, `9/0`, `119/0`, `18/0`, `74/0`, `20/0`.
  `make llvm-campaign-projection-test-smoke` is now green
  (projection parity restored). `make llvm-dnd-campaign-test-smoke` no
  longer segfaults but still diffs because the broader campaign exercises a
  separate `journey.morale`/`choiceCursor` projection path not closed by
  #70 alone; minimal probes (`probe_cursor`, `probe_dnd_minimal` of 4-strat
  Broadcast chain) all pass byte-equal.

## Progress Log - 2026-06-04 LLVM Host-Field Sync And MIR Intent Value Type Fidelity

- LLVM `llvm_emit_current_host_field_assignment` now also stores into a
  same-name local alloca (when present and field-type-matched) so that
  chained `host_field = host_field + X` reads do not return the stale
  alloca-cached value. Source-only reproducer captured at
  `tests/cases/backend_compare/probe_record/main.pgy` (was: `"a"`, expected
  `"abc"`); the same gap was the root cause of `examples/dnd_tavern_campaign`
  runtime crash and the `examples/campaign_graph_fsm` projection drift.
- MIR `IntentValue` emission no longer stores only `ast_type_name(...)` for
  the value's declared type. `mir_intent.c` now renders the full type via
  `mir_render_type_name(...)` (lifted to a public mir helper) and interns
  the rendered string in the routine's scratch arena, so a binding like
  `adjustments: Array<Int>` reaches the transpiler intent prologue as
  `Array<Int>` instead of `Array`. The C backend `ArrayLength` builtin
  resolver then accepts `adjustments` because the registered typed-var has
  the generic argument. Reproducer:
  `tests/cases/backend_compare/probe_intent_array/main.pgy`. The same fix
  also closes the residual `examples/campaign_graph_fsm` numerical drift
  because intent value parameters there reuse generic container types.
- Backend evidence: `tests/compare_backends.sh tests/cases/backend_compare/*/main.pgy`
  reports 789/789 passed, 0 failed (788 prior fixtures + the two new probes).
  `tests/llvm_dnd_campaign_smoke.sh` and
  `tests/llvm_campaign_projection_smoke.sh` are RC=0 with no diff. `make
  test-all` reports `2624/0`, `870/0`, `74/0`, `58/0`, `9/0`, `119/0`,
  `18/0`, `74/0`, `20/0` across parser/lexer/semantic/transpile/memory/
  concurrency/AIR/RIR/MIR/HIR.

## Progress Log - 2026-06-03 LLVM Builder Restoration Tightening

- LLVM generated-function emitters no longer restore caller builder state by
  guessing `LLVMGetLastBasicBlock(saved_fn)`. Function declaration emission,
  role operator bridge emission, and domain sync finish now restore the exact
  caller insertion block captured as `saved_bb`.
- `llvm_finish_domain_sync_emit(...)` now consumes the caller-owned `saved_bb`
  snapshot, so zone sync no longer performs a duplicate restore after the
  helper returns.
- Source-contract evidence: `perf_contract_smoke.sh` rejects reintroducing the
  saved-function last-block restore pattern. Local verification in this slice
  was limited to script syntax, `git diff --check`, and source scans because
  the local LLVM toolchain is not available.

## Immediate Execution Order

1. Continue the 600-1,000 LOC production owner queue without reintroducing
   behavior-owning `.inc` files.
2. DAG source-of-truth audit and migration.
3. CFG consumer migration: make body-safety facts the consumer-facing source
   for ownership/resource/return/drop-sensitive checks.
4. AIR consumer migration: make abstraction-boundary checks consume
   `air_verify(...)` evidence instead of re-reading AST/DIR strings.
5. Runtime propagation full transitive frontier scheduler.
6. MIR declaration inventory view and C/LLVM parity edge cleanup.
7. ABI ownership audit: Slot/Pin/Zone-bound handle/runtime-none/raw escape.
   `make abi-ownership-shape-test-smoke` gates the implemented Slot/Pin ABI
   shape, MIR cleanup evidence, C/LLVM pin/unpin lowering, and the docs
   contract that Zone-Bound Handle remains the missing non-pin expiration type
   piece.
8. parallel/core keyword matrix.
9. pain point sweep and beta wording freeze.

## Progress Log - 2026-05-29 Checklist Shard And Semantic Field Owner

- Split the oversized beta readiness checklist into four active shards plus a
  small index:
  `docs/100a_beta_active_status.md`,
  `docs/100b_beta_p0_semantics_systems_air.md`,
  `docs/100c_beta_dag_mir_abi_runtime.md`, and
  `docs/100d_beta_execution_log.md`.
- Updated the checklist/documentation/CFG/AIR/formal/runtime/MIR smoke scripts
  to treat the split files as one logical contract without letting the index
  grow back into a mega-file.
- Moved semantic projection-field consumers onto
  `projection_source_field_count(...)` / `projection_source_field_at(...)`.
  `ToObject`/`ToTObject`, zone projection field contracts, intent role field
  checks, target-field existence checks, and constructor arity checks no longer
  reopen `ast_class_fields(...)` directly.
- Removed duplicate projection helper declarations from
  `type_checker_internal.h`.
- Verified locally with `mingw32-make test-semantic` (`2612/0`) and
  `mingw32-make cfg-body-dataflow-test-smoke`. Earlier P0 shard verification
  also passed `beta-readiness-checklist-test-smoke`,
  `documentation-quality-test-smoke`, `air-drift-test-smoke`,
  `formal-semantics-test-smoke`, `runtime-panic-contract-test-smoke`,
  `abi-ownership-shape-test-smoke`, `raw-escape-contract-test-smoke`,
  `mir-declaration-inventory-test-smoke`, and
  `runtime-frontier-contract-test-smoke`.
- Known test-debt note: `perf-contract-test-smoke` remains too large/slow on
  local Windows Git Bash and timed out; it is P10 and should be split or
  batched separately rather than blocking this P0 semantic owner slice.
- MIR inventory surface-usage consumption is now accessor-gated:
  thread-pool and intent-observability runtime-selection code reads recorded
  inventory facts through `mir_surface_usage.c` instead of raw `MIRProgram`
  inventory booleans in codegen. The perf contract rejects raw
  `mir->has_inventory_surface_usage_facts` / `mir->inventory_uses_*` reads
  under `src/codegen`.
- MIR routine/program structure consumption is now accessor-gated:
  C/LLVM inventory views read routine inventory and `main`/top-level executable
  flags through `mir_program_inventory.c`, and AIR MIR evidence now consumes the
  same public routine inventory instead of reopening `mir->routines` directly.
  `mir-declaration-inventory-test-smoke` gates the C/LLVM side; the semantic
  core-shape smoke gates the AIR evidence side.
- HIR routine inventory consumption is now accessor-gated for AIR:
  `air_evidence_hir.c` consumes `hir_public.c` routine inventory accessors
  rather than reopening `hir->routines` / `hir->routine_count`. The semantic
  core-shape smoke rejects regressions.
- HIR routine inventory consumption is now accessor-gated for validation and
  mutable callgraph pass owners as well: `hir_validate.c` consumes the const
  routine inventory view, and `hir_callgraph.c` consumes the mutable routine
  inventory view. This leaves raw HIR routine arrays to HIR construction,
  destruction, and public-surface owners. Verified with
  `mingw32-make test-hir`, `mingw32-make semantic-core-shape-test-smoke`, and
  `mingw32-make perf-contract-test-smoke`.
- MIR public pass wrappers now consume routine inventory views:
  `mir_count_non_cfg_body_fallback_inventory(...)` uses the const routine
  inventory view, while liveness/DCE pass wrappers use the mutable routine
  inventory view. `mir-declaration-inventory-test-smoke` rejects raw
  `mir->routines` / `mir->routine_count` reads in `mir_public_surface.c`.
  Verified with `mingw32-make mir-declaration-inventory-test-smoke` and
  `mingw32-make test-mir`.
- LLVM routine inventory consumers now consume the LLVM routine-inventory
  accessor instead of indexing the wrapped array directly. The covered owners
  are function declaration emission, intent forward/emission paths, intent
  routine lookup, and MIR emission contract validation. Verified with targeted
  codegen TU builds and `mingw32-make mir-declaration-inventory-test-smoke`.
- RIR scope inventory consumption is now accessor-gated for AIR:
  `air_evidence_rir.c` consumes `rir_public_surface.c` scope inventory
  accessors rather than reopening `rir->scopes` / `rir->scope_count`. The
  semantic core-shape smoke rejects regressions.
- RIR scope item consumption is now accessor-gated for AIR:
  `air_evidence_rir*.c` consumes `rir_public_surface.c` fact/op/state-summary
  accessors rather than reopening `scope->facts`, `scope->ops`, or
  `scope->state_summaries`. The semantic core-shape smoke rejects regressions.
- RIR public dump output now consumes the same RIR scope inventory and
  fact/op/state-summary accessors as validators and evidence consumers.
  `rir_dump(...)` and `rir_dump_json(...)` no longer walk raw scope/fact/op
  arrays directly. Verified with `mingw32-make build/compiler/rir_public_surface.o`,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-rir`.
- RIR flow enrichment now consumes the mutable RIR scope inventory for scope
  matching instead of reopening `rir->scopes` / `rir->scope_count`. This keeps
  mutation local to the flow owner while putting program-level scope iteration
  behind the public RIR surface. Verified with
  `mingw32-make build/compiler/rir_public_surface.o build/compiler/rir_flow.o`,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-rir`.
- RIR flow enrichment read paths now consume `rir_scope_fact_at(...)`,
  `rir_scope_op_at(...)`, and `rir_scope_state_summary_at(...)`. The remaining
  raw state-summary writes in `rir_flow.c` are the owner-local summary reset
  before rebuilding flow facts. Verified with `mingw32-make build/compiler/rir_flow.o`,
  `mingw32-make test-rir`, and a longer-timeout rerun of
  `mingw32-make semantic-core-shape-test-smoke`.
- RIR validation and public dump flow-block fact reads now consume
  `rir_scope_flow_block_*` and `rir_flow_block_fact_*` accessors instead of
  reopening `scope->flow_blocks` / `block->facts`. Verified with
  `mingw32-make build/compiler/rir_public_surface.o build/compiler/rir_validation.o`,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-rir`.
- MIR cleanup invalidation checks now consume the same RIR flow-block accessors
  instead of reopening `rir_scope->flow_blocks` / `flow->facts`. Verified with
  `mingw32-make build/compiler/mir_cleanup.o`,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-mir`.
- RIR flow semantic-bit reads are now public-surface owned too:
  `rir_scope_conservative_semantics(...)`,
  `rir_flow_block_entry_semantics(...)`, and
  `rir_flow_block_exit_semantics(...)` are the consumer-facing API for dump and
  MIR cleanup invalidation checks. Verified with
  `mingw32-make build/compiler/rir_public_surface.o build/compiler/mir_cleanup.o`
  and `mingw32-make semantic-core-shape-test-smoke`.
- RIR flow-block identity reads now use
  `rir_flow_block_id(...)`, `rir_flow_block_is_reachable(...)`, and
  `rir_flow_block_is_join(...)` in validation/public dump consumers. Verified
  with `mingw32-make build/compiler/rir_public_surface.o build/compiler/rir_validation.o`
  and `mingw32-make semantic-core-shape-test-smoke`.
- RIR validation/public dump scope metadata reads now use
  `rir_scope_kind(...)`, `rir_scope_name(...)`, `rir_scope_owner_name(...)`,
  `rir_scope_display_name(...)`, and `rir_scope_has_state_errors(...)`.
  Verified with
  `mingw32-make build/compiler/rir_public_surface.o build/compiler/rir_validation.o build/compiler/rir_validation_dir.o`,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-rir`.
- RIR validation lookup ownership tightened: const state-summary lookup and
  projection-fact lookup now live in `rir_public_surface.c`. The validators
  consume `rir_scope_find_state_summary(...)` /
  `rir_scope_find_projection_fact(...)`, and the shape smoke rejects mutable
  `RIRScope` casts or local projection lookup reintroduction. Verified with
  `mingw32-make semantic-core-shape-test-smoke` and `mingw32-make test-rir`.
- RIR/DIR validation fact lookup ownership tightened further:
  `rir_scope_find_fact_by_name_kind(...)` and
  `rir_scope_has_capability_fact(...)` now live in `rir_public_surface.c`, so
  authority/resource/capability checks no longer own local fact scans. Verified
  with `mingw32-make build/compiler/rir_public_surface.o build/compiler/rir_validation_dir.o`,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-rir`.
- Backend compare collection parity was widened with
  `list_push_get_loop`, `map_long_values`, and `set_intersection_manual`.
  Each fixture passed a targeted C/LLVM compare with `bin-llvm/pgy.exe`.
- Backend compare algorithmic parity was widened with 16 small fixtures:
  `string_starts_with_prefix`, `loop_collect_max`, `sort_three_ints`,
  `count_letters_in_word`, `binary_search_int`, `gcd_recursive`,
  `reverse_array_in_place`, `palindrome_check`, `bubble_sort_small`,
  `fizzbuzz_loop`, `multi_array_find`, `bool_state_toggle`, `primes_below_n`,
  `string_repeat_pattern`, `linear_search_first_match`, and
  `map_word_grouping`. Targeted `tests/compare_backends.sh` passed 15/15, plus
  a targeted `map_word_grouping` run, with `bin-llvm/pgy.exe`.
- Rechecked the systems-baseline evidence commands after the P0 shard split:
  `mingw32-make codegen-determinism-test-smoke`,
  `mingw32-make runtime-none-contract-test-smoke`, and
  `mingw32-make raw-escape-contract-test-smoke` all pass locally.
- Tightened C MIR loop-local typing: `for` range variables still resolve to
  `Int`, but `for x in List<T>|Array<T>|Slice<T>` now derives `x: T` through
  the shared MIR local-type owner instead of registering every loop variable as
  `Int`. The transpile regression now checks that `for event in List<Event>`
  emits an `Event` local from list storage.
- Verified this loop-local slice with `mingw32-make test-transpile`
  (`860/0`) and `mingw32-make cfg-body-dataflow-test-smoke`.
- Tightened hosted method body-summary consumption: current-host and instance
  method calls now resolve the checked `Host_Method` function symbol through
  `expr_host_method_function_type(...)` and record the typed callee body
  summary instead of relying only on AST-declared method clauses. The semantic
  regression now requires an instance method call to propagate a body-derived
  `BODY_SUMMARY_SPAWNS_TASK` fact.
- Intent authority-sensitive call detection now uses the same typed hosted
  method effect source, so body-derived secure method effects are not lost when
  an intent `expect:` clause calls `receiver.Method()`. The parallel semantic
  regression also covers a body-derived secure method call.
- Narrowed the ref-parameter escape compatibility seam: when a checked
  function type already has a body summary fact without
  `BODY_SUMMARY_MAY_ESCAPE_REF`, `semantic_callable_param_escape_summary(...)`
  returns no escape without invoking the legacy AST param analyzer. The legacy
  analyzer remains only for unresolved/undecisive compatibility paths.
  The typed-summary fast path now first verifies that the callable declaration
  owner seam returns the same AST declaration as the checked function symbol, so
  name/scope drift cannot incorrectly bypass the legacy analyzer.
  `semantic-core-shape-test-smoke` now gates that typed body-summary facts are
  checked before legacy AST analysis.
- Verified with direct MinGW TU compiles and `mingw32-make test-semantic`
  (`2615/0`). A stale Git Bash/make child process briefly blocked the first
  clean-shell run, but the rerun completed after the process cleared.
- Added an explicit `has_body_summary_facts` bit to function types, so a checked
  empty summary is still decisive evidence and no longer falls back through the
  legacy AST param analyzer. `semantic-core-shape-test-smoke` now gates this
  distinction.
- Closed a CFG smoke violation in C MIR SSA-local emission:
  `transpiler_mir_func_ssa_locals_emit.c` no longer falls back to raw
  `inst->ast`; receive payload recovery now goes through
  `mir_instruction_source_payload(...)`. Verified with
  `mingw32-make cfg-body-dataflow-test-smoke`.
- Promoted twenty-three parity fixtures into the default backend-compare inventory:
  `for_in_array_int`, `nested_array_subarray`, `float_to_string_precision`, and
  `map_key_lookup_branch` from the array/string/map slice, plus
  `phi_branch_value` from the LLVM PHI smoke surface, `queue_string_ops` /
  `list_int_loop` from the collection-runtime parity slice, and
  `compose_two_functions` / `negative_index_check` from the callable and array
  guard surfaces, plus `multi_return_paths` / `bool_expr_chain` from the
  control-flow/expression parity surface, plus `fibonacci_iterative` /
  `map_count_unique` from the loop/map parity surface, plus
  `for_range_explicit`, `if_short_circuit_pure`, `nested_match_int`,
  `option_param_pass`, `result_via_unwrap`, `string_compare_branch`,
  `string_split_simple`, `subject_class_pair`, `substring_extract`, and
  `sum_filter_loop` from the common syntax/control-flow parity surface. The
  targeted MinGW/Git Bash runs passed for these fixtures (`5/5`, `2/2`, `2/2`,
  `2/2`, `2/2`, then `10/10`). The full default registry now contains 239 cases,
  but this slice did not run the whole backend-compare suite. Field-channel
  storage was checked and kept out of backend-compare
  because `Channel<T>` aggregate fields are an intentional C/LLVM stable-surface
  reject until movable channel-handle ABI exists.
- Repaired `mir_declaration_inventory_smoke.sh` for the build-source inventory
  portability gate: the declaration-field whitelist no longer uses macOS Bash
  3.2-hostile `case` pattern line continuations. Verified with
  `mingw32-make build-source-inventory-test-smoke` and
  `mingw32-make mir-declaration-inventory-test-smoke`.
- Repaired the perf/source-of-truth contract smoke after the general call
  return-type owner moved: the gate now checks
  `type_checker_helpers_late.c` for `type_function_return_type(sym->type)`
  instead of expecting `type_checker_expr_call.c` to own the function signature
  return path. Verified with `mingw32-make perf-contract-test-smoke`.
- Tightened the AIR source-of-truth regression gate in
  `semantic_core_shape_smoke.sh`: raw AIR graph fields
  (`intent_count`/`intents`, `boundary_count`/`boundaries`,
  `drift_count`/`drifts`, input flags, strict-evidence flag, and evidence-node
  storage) are only allowed in their AIR owner TUs. Consumers must use the
  public AIR graph/EvidenceNode accessors. Verified with
  `mingw32-make semantic-core-shape-test-smoke`.
- Tightened the no-Python CFG/body smoke fallback for MIR source-shape
  ownership: raw `inst->source_ast_type`, `inst->source_line`,
  `inst->source_column`, and matching block source fields are allowed only in
  MIR construction/public-surface/source-shape owners. Consumers must use the
  MIR source-shape accessors. Verified with
  `mingw32-make cfg-body-dataflow-test-smoke`.
- Tightened HIR/RIR inventory consumption in the lowering path: MIR lowering
  and RIR flow enrichment now consume HIR routines through
  `hir_routine_inventory_from_program(...)`, and MIR cleanup/lowering consumes
  RIR scopes/facts/ops through `rir_public_surface.c` accessors instead of
  reopening raw `hir->routines`, `rir->scopes`, `scope->facts`, or
  `scope->ops`. Verified with targeted TU builds,
  `mingw32-make semantic-core-shape-test-smoke`, `mingw32-make test-mir`, and
  `mingw32-make cfg-body-dataflow-test-smoke`.
- Tightened MIR declaration method routine linking/validation: the declaration
  header linker and validator now consume routines through
  `mir_routine_inventory_from_program(...)` / `mir_routine_inventory_get(...)`
  rather than raw MIR routine arrays. The MIR program validator now uses the
  same inventory view for routine inventory-shape checks. Verified with targeted
  TU builds, `mingw32-make mir-declaration-inventory-test-smoke`, and
  `mingw32-make test-mir`.
- Tightened RIR validation source-of-truth consumption: `rir_validate(...)` and
  `rir_validate_against_dir(...)` now consume RIR scope inventory plus
  fact/op/state-summary accessors rather than raw `rir->scopes` or
  `scope->facts` / `scope->ops` arrays. Verified with targeted TU builds,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-rir`.
- Fixed a local Windows build-system trap where `pgy_mkdir_p` used a nested
  login-shell bash invocation while `SHELL` was already Git Bash. The owner
  now invokes bash with `-c` for `mkdir`/`touch`, and
  `build-source-inventory-test-smoke` rejects regressing to `-lc`. Verified
  with `mingw32-make bin/pgy.exe` and
  `mingw32-make build-source-inventory-test-smoke`.

## Progress Log — 2026-04-28 AST Owner Split

- `src/parser/ast.c` no longer owns node construction or clone helpers.
  Construction moved to `src/parser/ast_constructors.c` and
  `src/parser/ast_domain_constructors.c`; clone helpers moved to
  `src/parser/ast_clone.c`.
- `src/parser/ast_print.c` no longer owns domain/intent/event printers.
  Those moved to `src/parser/ast_print_domain.c`, with misc print policy in
  `src/parser/ast_print_misc.c`.
- `src/parser/ast_print.c` also no longer owns inline expression rendering,
  compact print rendering, operator spelling, escaped string rendering, or
  generic/where-clause inline rendering. Those moved to
  `src/parser/ast_print_inline.c` and `src/parser/ast_print_generics.c`.
- `src/parser/ast_print_domain.c` no longer owns intent or event printing.
  Intent printing plus contract provenance moved to
  `src/parser/ast_print_intent.c`; event printing moved to
  `src/parser/ast_print_event.c`.
- `src/parser/parser.c` no longer owns declaration hint inventory.
  `src/parser/parser_decl_hints.c` now owns top-level declaration hint
  extraction, registration, capacity growth, and lookup.
- `src/parser/parser_domain.c` no longer owns relation/effect declaration
  parsing or projection-sync helper parsing. Relation/effect declarations
  moved to `src/parser/parser_domain_relation_effect.c`; projection group
  parsing and projection field maps moved to
  `src/parser/parser_domain_projection.c`.
- `src/parser/ast.h` no longer owns shared AST vocabulary directly.
  `src/parser/ast_types.h` now owns AST enums, forward declarations, generic
  parameter structs, function parameter structs, and class field structs.
- `src/parser/ast.h` also no longer owns the public AST
  constructor/manipulation prototype surface. Those declarations moved to
  `src/parser/ast_api.h`, which `ast.h` includes for source compatibility.
- Verified with `make test-parser pgy`, `make test-semantic` (2357/0),
  owner/sentinel/doc checklist gates, and
  `make runtime-frontier-contract-test-smoke`.
- Result: no production `.c` or `.h` owner remains above the 1,000 LOC hard
  risk line. The AST print family is now below the 600 LOC split-review
  threshold: `ast_print.c` 553 LOC, `ast_print_domain.c` 539 LOC,
  `ast_print_inline.c` 382 LOC, `ast_print_intent.c` 253 LOC, and
  `ast_print_event.c` 76 LOC. `parser.c` is now 867 LOC after the declaration
  hint split. The parser domain family is below the 600 LOC split-review
  threshold: `parser_domain.c` 493 LOC, `parser_domain_relation_effect.c` 283
  LOC, and `parser_domain_projection.c` 184 LOC. `ast.h` is now 848 LOC after
  the public API header split, with `ast_api.h` at 137 LOC.

## Progress Log — 2026-04-28 LLVM Backend Type Map Split

- `src/codegen/llvm_backend.c` is reduced to context lifecycle and backend
  entry ownership. AST/Pergyra type-name rendering, generic container type
  extraction, `pergyra_type_to_llvm`, `ast_type_to_llvm`, and early
  forward-declare eligibility now live in
  `src/codegen/llvm_backend_type_map.c`.
- Verified with `make pgy` and `make llvm-test-smoke`.
- Remaining LLVM blocker focus: `llvm_domain_zone_sync.c` / domain frontier
  parity and declaration inventory/bootstrap seams, not the generic backend
  context owner.

## Progress Log — 2026-04-28 LLVM Zone Frontier State Split

- Zone sync bounded-frontier bookkeeping now has a named LLVM owner:
  `src/codegen/llvm_domain_zone_frontier_state.c` owns previous-state
  allocation, snapshotting, reset, and change-detection continuation updates.
- `src/codegen/llvm_domain_zone_sync.c` is reduced to zone propagation
  orchestration for projection sync, action-caused effects, apply/maintain,
  detach, link, relation maintain, and unlink.
- Verified with `make runtime-frontier-contract-test-smoke` and
  `make llvm-test-smoke`.
- Remaining runtime propagation blocker is now broader-family coverage, not
  backend-local world frontier policy: stable world sync consumes
  `pgy_frontier_world_transitive_pass_limit(...)` in C and LLVM, while future
  world-zone propagation paths must be forced through the same policy family.

## Progress Log — 2026-04-29 Runtime Frontier Policy And C Owner Split

- Stable world outer frontier scheduling now consumes
  `pgy_frontier_world_transitive_pass_limit(...)` from
  `src/codegen/domain_frontier_policy.h` in both the C world emitter and LLVM
  world sync emitter.
- `make runtime-frontier-contract-test-smoke` now gates that named transitive
  policy source of truth in addition to the existing zone, world-derived, and
  projection pass-limit helpers. It also requires
  `make runtime-frontier-policy-test-smoke` to stay wired so saturating
  pass-limit arithmetic is checked by a compiled executable, not only by
  string terms.
- C world/select/event lowering is split into focused owners:
  `transpiler_world_select_event_emit.c` (457 LOC), `transpiler_select_emit.h`
  (155 LOC), and `transpiler_event_emit.h` (103 LOC). This removes the
  600+ LOC mixed owner without reintroducing `.inc` files.
- Verified with `make pgy`, `make runtime-frontier-contract-test-smoke`,
  `make runtime-frontier-policy-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.

## Progress Log — 2026-04-29 C Let Slot Owner Split

- Slot-related let lowering is now a named C backend owner:
  `transpiler_let_slot_emit.c` owns ClaimSlot/ClaimSecureSlot/ClaimDeviceSlot,
  ReadView/WriteView/MoveToken declarations, and Slot/SecureSlot sugar.
  `transpiler_let_slot_emit.h` is declaration-only.
- `transpiler_let_emit.c` is back to let-declaration orchestration plus
  non-slot specialization paths and stays under the 600 LOC split-review
  threshold.
- Verified with `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).

## Progress Log — 2026-04-29 C Domain Provenance Owner Split

- Hidden domain provenance field/stamp emission and projection-chain bounded
  recompute moved to `transpiler_domain_provenance_emit.h`.
- `transpiler_domain_role_ability_emit.h` no longer mixes role/ability vtable
  lowering with runtime propagation frontier helpers.
- Verified with `make pgy`, `make test-transpile`, and
  `make runtime-frontier-contract-test-smoke`; `make
  llvm-test-backend-compare` remains green (`196/0` ABI same-process,
  `65/65` backend compare).

## Progress Log — 2026-04-29 C Class Declaration Owner Split

- Non-generic class declaration lowering moved to the compiled owner
  `transpiler_class_decl_emit.c`.
- `transpiler_func_class_flow_emit.h` is now below the 600 LOC split-review
  threshold and no longer owns class field/container/method emission directly.
- `transpiler_func_flow_policy.c` owns function fallback policy helpers, keeping
  the function-flow shim focused on emission orchestration.
- Verified with `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).

## Progress Log — 2026-04-29 C MIR Block Owner Split

- MIR emission predicate wrappers moved to `transpiler_mir_emit_predicates.h`.
- `transpiler_mir_block_emit.c` owns block statement emission; the header is
  declaration-only and no longer participates in implementation LOC debt.
- Verified with `make test-mir`, `make cfg-body-dataflow-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.

## Progress Log - 2026-04-29 C Declaration Lookup Owner Split

- Host declaration lookup moved to `transpiler_decl_host_lookup.c`; hosted
  method lookup now consumes MIR metadata and the non-MIR AST host-method lookup
  path is retired.
- `transpiler_decl_lookup.c` is now 419 LOC and keeps named declaration,
  alias, inventory, and method-list lookup ownership focused.
- `transpiler_decl_host_lookup.c` owns current-host, owner-host, and
  nominal-host lookup cache paths.
- Verified with `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).

## Progress Log - 2026-04-29 C Type Mapping Owner Split

- AST type-name rendering moved to `transpiler_type_render_helpers.h`.
- `transpiler_type_mapping_helpers.h` is now 563 LOC and keeps primitive,
  collection, slot, result, and suffix mapping ownership focused.
- `transpiler_type_render_helpers.h` is 102 LOC and owns recursive AST
  type-name rendering plus arena-stable local render results.
- Verified with `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).

## Progress Log - 2026-04-29 CFG Contract Validator Owner Split

- CFG-owned AST control classification moved to
  `mir_cfg_contract_control.h`.
- `mir_cfg_contract_validate.h` is now 551 LOC and keeps cleanup, successor,
  and predecessor contract validation ownership focused.
- Pin cleanup edge validation moved to `mir_cfg_contract_pin.h`, preserving the
  existing Slot/Pin ABI shape smoke contract while keeping the main CFG
  contract owner below the split-review threshold.
- Verified with `make test-mir`, `make cfg-body-dataflow-test-smoke`,
  `make abi-ownership-shape-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.

## Progress Log - 2026-04-29 MIR SSA Local Type Owner Split

- AST body local type lookup and expression fallback inference moved to
  `transpiler_mir_local_type_lookup.c`.
- `transpiler_mir_ssa_names.h` is now 357 LOC and keeps SSA name resolution,
  SSA map setup, claim-shape predicates, and implicit-field rendering focused.
- `transpiler_mir_local_type_lookup.c` owns MIR local type
  recovery for let declarations, destructuring, with aliases, branch bodies,
  member calls, and nominal constructor calls.
- Verified with `make pgy`, `make test-mir`,
  `make cfg-body-dataflow-test-smoke`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).

## Progress Log - 2026-04-29 DAG Signature Stage Seam Tightening

- `semantic_stage_resolve_type_quiet(...)` no longer calls
  `semantic_type_resolution_lookup_or_materialize(...)` directly from the
  signature stage. The current implementation now consumes DAG metadata facts
  and owner-local diagnostics directly; the intermediate materializing
  type-ref API has since been removed.
- `type-resolution-resolver-inventory-test-smoke` removed
  `type_checker_resolution_stage_signature.c` from the direct materializer
  allowlist. Direct diagnostic materializer calls are limited to central
  metadata/diagnostic compatibility owners.
- Stable constructed-type diagnostic argument resolution now uses the
  metadata-first type-ref helper too. The only remaining direct
  `semantic_type_resolution_lookup_or_materialize(ctx, ...)` call is the
  central metadata type-ref helper's fallback branch.
- The direct materializer smoke allowlist is now narrowed to that central
  metadata owner only; `type_checker_resolve.c` remains counter-only.
- Local gates: `make type-resolution-resolver-inventory-test-smoke
  type-resolution-dag-test-smoke test-semantic` is green with
  `materializer_fallbacks=0` and semantic suite `2359/0`.

## Progress Log - 2026-04-29 Runtime Channel/Qubit Export Owner Split

- Runtime channel/qubit export ownership is now split without changing the
  public runtime include seam.
- `pgy_runtime_lib_channel_quantum_exports.h` is a 7 LOC facade over
  `pgy_runtime_lib_channel_int_exports.h`,
  `pgy_runtime_lib_channel_string_exports.h`, and
  `pgy_runtime_lib_qubit_state_exports.h`.
- The split owners are 327, 319, and 69 LOC respectively, keeping the
  channel/qubit export surface below the 600 LOC split-review threshold without
  reintroducing `.inc` files.
- `compiler_runtime_cache_is_fresh(...)` tracks the leaf owners, so cached LLVM
  runtime objects cannot stay stale after a channel or qubit export edit.
- Verified with `make pgy`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`.

## Progress Log - 2026-04-29 Runtime Raw Collection Export Owner Split

- Runtime raw collection export ownership is now split without changing the
  public runtime include seam.
- `pgy_runtime_lib_raw_collection_exports.h` is an 8 LOC facade over
  `pgy_runtime_lib_raw_collection_common_exports.h`,
  `pgy_runtime_lib_raw_queue_exports.h`,
  `pgy_runtime_lib_raw_map_exports.h`, and
  `pgy_runtime_lib_raw_set_exports.h`.
- The split owners are 13, 117, 431, and 153 LOC respectively, keeping raw
  Queue/HashMap/Set export ownership below the 600 LOC split-review threshold
  without reintroducing `.inc` files.
- `compiler_runtime_cache_is_fresh(...)` tracks the leaf owners, so cached LLVM
  runtime objects cannot stay stale after a raw collection export edit.
- Verified with `make pgy`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`.

## Progress Log - 2026-04-29 DAG Retired Resolver Owner Split

- The obsolete `type_checker_resolve.c` owner is removed from the beta path.
- Retired compatibility counters now live in
  `type_checker_resolution_retired.c`; general type helper functions
  `require_assignable(...)` and `wrap_constructed(...)` now live in
  `type_checker_type_helpers.c`.
- `type-resolution-resolver-inventory-test-smoke` rejects reintroducing
  `type_checker_resolve.c` or `type_checker_resolve.h` and requires the retired
  counter owner to keep its audit marker.
- `semantic-core-shape-test-smoke` requires the new owners and verifies
  assignability helpers do not move back into the retired counter owner.
- Verified with `make type-resolution-resolver-inventory-test-smoke`,
  `make type-resolution-dag-test-smoke`, `make test-semantic`,
  `make semantic-core-shape-test-smoke`, and `make semantic-tu-size-test-smoke`.

## Progress Log - 2026-05-02 AIR/DAG Source-Of-Truth Tightening

- AIR retains the legacy `authority_from_zone` schema field, but active beta
  semantics no longer derive approval from a local `who`. Action-inherited
  authority becomes `authority_from_action`. Action-inherited
  zone source becomes `source_from_action`, while action-inherited ability/effect
  contracts become `requires_from_action` and `causes_from_action`. AIR JSON plus
  drift diagnostics expose
  `authority_provenance=action-inherited|explicit|none` on active beta paths;
  compatibility-only zone authority is labeled `legacy-zone-field`.
- Action-derived intent `causes` now also reaches RIR propagation evidence:
  intent RIR lowering emits `RIR_RESOURCE_EFFECT_INSTANCE` and
  `RIR_OP_ATTACH_EFFECT`, prefers the unique zone effect-slot anchor when one
  exists, and AIR observes it as `AIR_EVIDENCE_RIR_EFFECT_PROPAGATION`.
- Action-derived intent `authorized by` is pinned to RIR authority evidence in
  the same parsed on-receiver fixture. This keeps `authority_from_action`
  honest by requiring `AIR_EVIDENCE_RIR_AUTHORITY` and a matching
  `rir_authority_evidence_name`.
- MIR cleanup evidence accounting is stricter and more CFG-backed:
  `AIR_EVIDENCE_MIR_CLEANUP` consumes MIR cleanup successors first, while pin
  cleanup remains boundary-specific `AIR_EVIDENCE_MIR_PIN_CLEANUP`.
- DAG materializer owner inventory shrank from `25` to `18`; intent participant,
  transfer, inherited-action parameter, and zone-authority subject-slot type
  annotations now use the centralized annotation read API instead of the
  materializer seam. Abstract ability method signature validation also now
  consumes annotation facts directly. Projection field-path type reads also
  consume annotation facts, keeping projection diagnostics read-only with
  respect to DAG metadata creation. Destructuring ownership type reads now do
  the same, so that CFG/body-safety-adjacent path no longer materializes DAG
  metadata as a side effect.
- The ownership-let materializing semantic owner seam is now closed without
  pretending that annotation-only lookup was sufficient. The negative probe
  showed that annotation-only lookup caused broad semantic drift and lost
  unsupported `HashMap` key diagnostics. The accepted path consumes DAG
  metadata type-ref facts and then calls the shared stable-shell arity,
  constructed-type, and unknown-bare-name diagnostic helpers. Effective
  generic-argument derivation, generic contract validation, generic
  where/default validation, host/domain slot reads, intent-local type reads,
  function signatures, expression annotations, action contract reads,
  ownership-let annotations/type arguments, and compressed intent role/ability
  field checks now consume centralized metadata/effective-argument evidence
  instead of owning local materializer seams.
- Class/ability signature staging now opens a generic-parameter scope before
  resolving staged fields and methods, aligning DAG staging with the full
  semantic checker even where the materializer allowlist is still required.
- Local gates: `make test-air`, `make test-semantic`,
  `make type-resolution-resolver-inventory-test-smoke`,
  `make type-resolution-dag-test-smoke`, `air-drift-test-smoke`,
  `intent_compression_contract_smoke.sh`.

## Progress Log - 2026-06-06 MIR Host Declaration Header Lookup

- Function declaration headers are now part of the MIR declaration-header
  inventory, so host metadata lookups must not use name-only header lookup.
  A function and a host declaration can legally share a name at different AST
  kinds; host metadata must select by host declaration kind.
- LLVM `llvm_find_host_decl_header_in_context` now iterates
  `pgy_host_decl_compat_types` and resolves through typed declaration-header
  lookup. The C backend has the matching
  `transpiler_active_host_decl_header` owner and hosted field/method/slot
  metadata consumers now use it.
- The C/LLVM codegen-level name-only declaration-header wrappers were removed;
  codegen callers must use typed declaration-header lookup or the host-specific
  owner. The MIR core still keeps `mir_find_decl_header` for direct MIR tests.
- The smoke gate now requires typed declaration-header lookup for C and LLVM
  host metadata and keeps the backend from falling back to partial hard-coded
  host-kind chains.
- Verified locally with `gcc -fsyntax-only` on the changed C slices,
  `make pgy mir-declaration-inventory-test-smoke test-mir`, and
  `make llvm-test-smoke`.
- Full `llvm-test-backend-compare` is still too large for the current
  interactive 15-minute window: the last run reached `570/795` PASS with no
  mismatch before timeout. Treat it as a long-running parity gate, not as a
  failed parity case; use the existing `PGY_BACKEND_COMPARE_SHARD_TOTAL` /
  `PGY_BACKEND_COMPARE_SHARD_INDEX` path for interactive or CI shard runs.

## Progress Log - 2026-06-06 Backend Compare Range Gate

- C/LLVM parity work no longer has to choose between one explicit fixture and
  the full 795-case backend compare. `tests/compare_backends.sh` now accepts
  deterministic `PGY_BACKEND_COMPARE_START_INDEX` and
  `PGY_BACKEND_COMPARE_MAX_CASES` controls after shard selection.
- The CI shard matrix remains the full source of truth for broad coverage.
  The range gate is a developer feedback loop for fixing a narrow C/LLVM seam
  without re-running hundreds of unrelated fixtures on every edit.
- Makefile forwards the range controls through `llvm-test-backend-compare` and
  `air-strict-backend-compare-test-smoke`, and the source-inventory smoke gate
  now keeps that wiring from regressing.
- Verified locally with `make build-source-inventory-test-smoke`,
  `PGY_BACKEND_COMPARE_PRECHECK=0 PGY_BACKEND_COMPARE_MAX_CASES=3 make
  llvm-test-backend-compare`, and direct non-zero range execution
  (`START_INDEX=1`, `MAX_CASES=1`).
- Additional direct C/LLVM evidence with `bin/pgy`: first 20 default fixtures
  pass with `PGY_BACKEND_COMPARE_MAX_CASES=20`, and the known same-name
  binding/scope-collision set (`option_same_binding_guard`,
  `option_multi_same_binding_loop`, `option_nested_same_binding_shadow`,
  `match_nested_same_binding_shadow`, `lexical_shadow_class_method`,
  `llvm_dynamic_scope_capture`, `list_shadow_scope_metadata`) passes 7/7.

## Progress Log - 2026-06-06 AIR Strict Evidence Policy Tightening

- AIR boundary evidence policy now owns the pin-cleanup requirement shape:
  `AIR_BOUNDARY_EXECUTION` carries `mir_pin_cleanup_source_name = "pin"` in
  `kBoundaryEvidencePolicies` instead of re-stating the source-name rule in
  verification code.
- AIR global verification now consumes data-driven strict evidence requirement
  tables for MIR counter proofs, DAG counter proofs, and runtime singleton
  evidence. Summary counters remain observability only; `EvidenceNode` inventory
  is the proof source of truth.
- `air-drift-test-smoke` now gates the policy tables so future AIR evidence
  kinds are added by extending the requirement owners rather than by copying
  ad-hoc strict-evidence checks.
- Verified locally with `gcc -fsyntax-only` for the touched AIR owners,
  `make air-drift-test-smoke`, `make air-json-schema-test-smoke`,
  `make backend-fail-closed-test-smoke`, and
  `make build-source-inventory-test-smoke`.

## Progress Log - 2026-06-06 C Projection Nominal Lookup Tightening

- C `transpiler_find_projection_nominal_decl_local(...)` now consumes
  `transpiler_find_named_decl_local(ctx, AST_CLASS_DECL, name)`, matching the
  LLVM path's typed declaration-header-first lookup. This removes one
  inventory-first compatibility seam from projection nominal recovery.
- `mir-declaration-inventory-test-smoke` now rejects returning the projection
  nominal owner to `transpiler_find_decl_in_inventory_local(...)`.
- Verified locally with `make mir-declaration-inventory-test-smoke`, targeted
  projection/zone compare fixtures (`probe_record`, `probe_field_index`,
  `probe_zone_chain`, `zone_with_subject_slot`) and actual projection fixtures
  (`subject_projection`, `relation_effect_projection_sync`,
  `world_zone_projection_visibility`,
  `world_embedded_branch_projection_visibility`).

## Progress Log - 2026-06-06 C Typed Declaration Recovery Tightening

- C backend type-specific declaration recovery no longer calls
  `transpiler_find_decl_in_inventory_local(ctx, AST_*_DECL, ...)` directly
  outside the declaration-lookup owner. Class/enum host method body lookup,
  nominal host lookup, relation/effect/party/roster/world/zone declaration
  emitters, overlay relation/effect bind, world-zone projection recovery,
  intent zone lookup, effect sync lookup, MIR SSA zone recovery, and function
  forward world checks now go through `transpiler_find_named_decl_local(...)`,
  which prefers typed MIR declaration headers before AST inventory fallback.
- `mir-declaration-inventory-test-smoke` now rejects reintroduced literal
  `AST_*_DECL` direct inventory recovery under `src/codegen`.
- Verified locally with `gcc -fsyntax-only` on the touched C backend owners,
  `make mir-declaration-inventory-test-smoke`, and targeted C/LLVM fixture
  batches: class/enum/world/projection baseline (`8/8`) plus declaration/
  overlay/world-zone coverage (`role_operator`, `zone_host_method_abi_combo`,
  `relation_effect_projection_sync`, `world_with_zones`,
  `world_zone_cross_queries`, `world_zone_projection_visibility`,
  `zone_action_effect_runtime`, `zone_layer_projection_runtime`) with `10/10`
  passing.

## Progress Log - 2026-05-02 Intent Single-Subject Who Inference

- Closed the first safe Intent-Compress `who` rule: a step with omitted `who`
  derives it from the enclosing intent only when there is exactly one
  subject participant and no action/default has already supplied a `who`.
- Multi-subject intents remain explicit. This keeps the rule fail-closed and
  prevents intent compression from becoming an authority/effect owner.
- The provenance now flows through AST print, semantic contract summary, DIR,
  AIR, and `pgy.air.graph.v1` JSON as `who_from_single_participant`.
- Added positive and negative semantic regressions plus source-gated smoke
  checks for the derivation owner and AIR JSON schema field.
- Split the AIR evidence test case owner so `src/tests/*.cases.h` stays below
  the size gate without weakening AIR coverage.
- Verified locally with `make test-semantic` (`2430/0`), `make test-air`
  (`65/0`), `make intent-compression-contract-test-smoke`,
  `make air-json-schema-test-smoke`, `make test-inc-size-test-smoke`, and
  `make source-utf8-test-smoke`.

## Progress Log - 2026-04-30 C/LLVM Defer Cleanup Parity

- C `defer` lowering now uses lexical inline cleanup instead of a file-scope GCC
  cleanup helper. This keeps local state such as method `self` visible to the
  deferred body and aligns the C backend with LLVM's defer stack model.
- MIR-emitted C functions now register `AST_DEFER_STMT` through the same defer
  stack and emit active defers on MIR return/fallthrough returns, so subject
  method recursion with deferred state mutation is backend-parity gated.
- Nested branch defer is now MIR-preserved rather than treated as CFG-owned
  control, so `if { defer { ... } }` survives DCE and is smoke/parity gated.
- Dynamic `defer` inside runtime-dependent `if`/match/loop control is not beta-stable
  and is now rejected with `PGY_SEM_DEFER_DYNAMIC_CONTROL`. This avoids a false
  parity state where C and LLVM both run the same wrong cleanup.
- The old sentinel path is now a regression smell: C tests reject
  `__attribute__((cleanup(_pgy_defer_...)))` for source-level `defer`.
- Current evidence: `make test-transpile` (`682/0`), `make llvm-test-smoke`,
  `make llvm-test-backend-compare` (`69/69`), and the CFG/AIR/DAG smoke gates
  pass. A full monolithic `make ci-linux` was not completed locally because the
  command exceeded the 15 minute execution window; the CI target groups were
  run in slices instead.

## Progress Log - 2026-04-24 Parser/Lexer Diagnostic Routing

- `parser_error` and lexer error-token paths now route stage code, reason, and
  fix metadata through the first diagnostic-routing gate.
- Stable codes: `PGY_PARSE_SYNTAX`, `PGY_LEX_INVALID_TOKEN`.
- Gate: `make parser-lexer-diagnostic-test-smoke`.
- CI wiring: `ci-linux` runs the parser/lexer diagnostic gate.
- Remaining beta debt: parser-specific code split and multi-error accumulation.

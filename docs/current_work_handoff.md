# Current Work Handoff

Updated: 2026-07-24 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. Verify it against the
current source, `git status --short --branch`, the SoT registry, the active
owner, and the named executable gate.

## Resume checkpoint

- The latest pushed implementation checkpoint is `18521263` (`Close semantic
  normalization allocation SoT`), and `HEAD` equals `origin/main`. This unit
  replaces allocation-returning character scans and unconditional trim copies
  in `SemanticStripOuterParens` with byte views and source-string reuse. Its
  focused owner gate and C initializer-projection parity pass; the full-driver
  pressure gate remains red as recorded below.
- `0fdaf851` closes the shared collection-call protocol for List/Queue/Set:
  family, operation, arity, argument positions, and return shape now have one
  Pergyra semantic owner consumed by the semantic and C-emission lanes. This
  is a bounded String-backed protocol closure; the production-bar TypeId and
  RuntimeCallAbiId replacement remains open.
- `9b002796` adds measured, fail-closed 3 GiB Windows pressure boundaries for
  native and bounded Pergyra-built compiler builds, attributes reparented MSYS
  compiler workers, and rejects unfiltered Git Bash execution of the 280-row
  DRV-2 matrix. `05b2da48` extends that owner around the full driver fixpoint.
  `81657340` adds the binary-level accidental-direct-run interlock, and
  `a2de312d` corrects the executable CLI evidence. These are operational
  hardening, not self-host substitution.
- `a42616b7` is the latest executable closure: concrete by-value wrapper
  declarations are projected from semantic type/signature/specialization facts;
  generic formal templates are excluded; specialized concrete return/parameter
  types are restored; and generic actual symbol mangling preserves the
  terminal constructed-type separator. `96a96868` closes the discovered
  missing-fact diagnostic/import boundary without changing semantic ownership.
- The preceding `aba64ab1` closure projects routine local inventory from
  semantic binding, initializer, and iteration facts; indexed assignment use
  edges consume attached expression graphs instead of text scans; and the
  for/foreach graph lane preserves the same owner boundary.
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

## Exact remaining dirty state after the pressure attribution

The following concurrent or user-owned work remains outside the pressure
commits:

- modified `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`;
- modified `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`;
- modified `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`.

The collection protocol owner, its migrated consumers, registry row, proof
mapping, and negative gate are clean at `0fdaf851`; they are not part of the
remaining dirty state above.

The pressure-diagnostic compiler and semantic owners
`driver_bootstrap_main.pgy`, `driver_rung2_owner.pgy`,
`ast_body_type_bundle_owner.pgy`, and
`ast_initializer_type_fact_owner.pgy` are clean at `18521263`; do not list
them as concurrent dirty work on resume.

The native C files are a concurrent file-split change. Do not discard, stage,
or fold them into a self-host rung without reviewing their owner and gate.
The raw full-matrix blocker note predates `6574f89f`; its observed failure is
resolved, but the untracked file remains user-owned. The driver-bootstrap
runtime-header classifier change is plausible follow-up work but has not been
included in an executable closure commit here.

## 2026-07-24 memory incident and architecture checkpoint

Observed facts:

- A forced native release rebuild completed in 1,576,373 ms. Its separately
  sampled final LTO relink peaked at 490.3 MB working set / 444.1 MB private.
- A fresh `make self-host-compiler` equivalent built the bounded Pergyra DRV-2
  in 351,507 ms and peaked at 1,343.8 MB working set / 1,412.2 MB private.
  `gen2.exe` owned 1,134.1 MB private while producing an approximately 3 MiB
  AST and 3 MiB C artifact. This is optimization debt, but not a 20 GiB build.
- The desktop incident included a Git Bash wrapper whose native DRV-2 worker
  survived reparented, a second full matrix started over it, other high-memory
  desktop/WSL processes, and D: at 97% use. No single currently sampled normal
  compiler build owned 20 GiB.
- A distinct earlier full-input `driver_mir_oracle` run is a confirmed real
  defect: it reached approximately 17 GiB RSS / 28 GiB private, produced no
  MIR artifact, and was stopped. This is unresolved full-driver MIR
  materialization amplification, not acceptable self-host overhead.
- Compiling the approximately 3 MiB driver source into a guarded oracle is
  itself heavy but bounded: the C backend completed in 74,025 ms at
  2,138.8 MB working / 2,145.6 MB private, and LLVM completed in 147,566 ms at
  2,228.2 MB working / 2,239.5 MB private. These are build measurements, not
  the full-input oracle execution that reached 28 GiB.
- The official Windows full-fixpoint target is now pressure-wrapped at 3 GiB,
  including reparented `driver_oracle`, `driver_seed`, and `driver_genN`
  processes. The direct script-plus-environment invocation is not an approved
  diagnostic path for this blocker.
- The guarded C- and LLVM-built oracles both reject a full-driver request
  without the pressure-owned runner token, reject token use on a bounded
  fixture, and emit the same 2,341-byte `let_log` MIR artifact. The token
  prevents accidental direct use; it is not a security boundary and does not
  replace the pressure wrapper.
- The stale pre-guard `.tmp/self_hosted/driver/bootstrap/driver_oracle.exe`
  was moved to `driver_oracle.unbounded-disabled-20260724.exe`; the official
  bootstrap must rebuild the canonical path before use.
- The CLI contract is `mode, source, output[, token]`. A temporary probe used
  `source, mode, output`, so its usage diagnostic was invalid evidence and has
  been discarded; there is no observed LLVM argv blocker from this session.
- The pressure-owned C oracle was then run on the full driver source. It was
  stopped after 170,534 ms at 3,079.2 MB process-tree private / 2,549.3 MB
  working set; the oracle itself owned 3,030.0 MB private. It produced no MIR
  artifact and left no oracle process. The cap works; materialization remains
  blocked.
- Pressure-only markers then proved that AST construction, typed semantic
  analysis, and driver readiness complete before the cap. The run enters body
  types and `SemanticAstInitializerTypeFactsFromArtifact`, but never completes
  the base-initializer projection and never enters iteration, MIR-fact, or JSON
  construction.
- A finer isolated C-oracle run completed initializer rows 0 through 5,003 and
  crossed the cap at row 5,004 before that row's environment marker. It was
  stopped after 165,336 ms at 3,074.4 MB private / 2,527.8 MB working set,
  with no artifact. The corresponding instrumented C build completed in
  76,854 ms at 2,253.3 MB private / 2,235.5 MB working set; the LLVM build
  completed in 132,825 ms at 2,253.6 MB private / 2,235.7 MB working set.
- A follow-up process audit caught that direct bypass running concurrently with
  a 95-fixture DRV-2 shard. A recursive make dry-run briefly added a third run;
  the exact 21-process Pergyra set was at 2,020.6 MB working set / 2,114 MB
  private when stopped. GNU make executes lines containing `$(MAKE)` even with
  `-n`; verify this pressure recipe through its static gate, not a dry-run.
- During this session, the confirmed runaway project process was terminated and
  available memory recovered to the normal tens-of-GB range. An isolated
  seed/DRV-2 build stayed below the incident scale: the largest observed
  `gen2`/driver worker was about 1.0 GB working set while available memory
  remained above 20 GB. A separate reparented full-matrix `driver_oracle`
  process previously contaminated the pressure wrapper; that was not evidence
  that the self-host compiler itself owns tens of GB.

Current measured attribution and remaining unknown:

- The earlier JSON-leading explanation is falsified for the current 3 GiB
  crossing: MIR facts and JSON projection have not started.
- The crossing is linear initializer-row accumulation, not one exceptional
  row. After the byte-view normalization closure, row 5,201 completed and row
  5,202 had not finished environment setup; the official pressure wrapper
  measured 2,535.6 MB working set / 3,082.6 MB private, with
  `driver_oracle.exe` at 3,071.6 MB private, and exited 88.
- Emitted C has no row-scope cleanup in the initializer owner. Its graph and
  verdict helpers create temporary arrays/strings; allocation-returning
  `CharAtN`, `Substring`, `StringTrim`, and `StringConcat` results are not
  reclaimed there, while `ArrayPop` only changes length.
- The exact split among repeated call-spine materialization, character scans,
  and other verdict helpers is still `Unknown`. JSON retains separate later
  optimization debt, but it is not the active first blocker.

Active memory objective card:

- Objective: make full-driver base-initializer projection complete under the
  3 GiB hard boundary without weakening semantic diagnostics.
- Priority: initializer fact identity, owner-directed graph/environment reuse,
  per-row lifetime closure, negative pressure ratchet, then speed and patch
  size.
- Fact owner: `SemanticAstInitializerTypeFacts` and the expression-graph facts
  it consumes; the body-type bundle is the last legitimate consumer.
- Forbidden fallback: raising the cap, tuning JSON before it executes,
  splitting the compiler into per-chunk subprocesses, or mirroring C backend
  fragments in Pergyra.
- Verification: guarded full-driver C and LLVM executions remain below 3 GiB,
  and the bounded `let_log` MIR artifacts remain byte-equal.

Latest observed gates for this slice:

- `build_pressure_contract_smoke.sh`, `documentation_quality_smoke.sh`, and
  `bash -n tests/self_hosted/parity/driver_bootstrap.sh` passed.
- `tests/self_hosted/parity/semantic_expression_normalization_owner_smoke.sh`
  passed, and
  `tests/self_hosted/parity/initializer_projection_probe_parity.sh` passed
  with the C backend.
- The official `make -s self-host-driver-bootstrap-full-test-smoke` pressure
  gate remains red: `driver_oracle.exe` crossed the 3 GiB private limit before
  iteration, MIR-fact, or JSON stages. This is the next falsifier for the
  initializer row-lifetime owner.
- C and LLVM guarded-oracle builds completed with zero diagnostics; their
  bounded `let_log` outputs were both 2,341 bytes and SHA-256-equal. Both
  binaries rejected an unowned full-driver request with exit 1 and no artifact.
- `self_hosted_component_contract_smoke.sh` is externally blocked by the
  concurrent `src/self_hosted/OWNERS.md` state: it is missing the existing
  `ast_expression_graph_collection_call_protocol_owner.pgy` term. Do not fix
  or stage that concurrent file as part of the pressure slice.

Architecture boundary:

- C and LLVM are peer production backends of the current native compiler.
- The Pergyra-built DRV-2 is still a bounded source/MIR-to-C replacement.
  Building that tool through C and LLVM proves parity; it does not make LLVM a
  Pergyra-owned self-host emitter and does not make the released compiler fully
  self-hosted.
- The next compiler-world closure must keep one backend-neutral semantic/MIR/
  ABI fact spine and let C and LLVM consume it as peers. Do not reproduce the C
  compiler's file fragmentation or create a mirrored C-shaped Pergyra tree.

## Latest closed executable rung: shared collection-call protocol surface

Objective card:

- Objective: close the duplicated List/Queue/Set call metadata seam behind one
  Pergyra-owned collection-call protocol consumed by semantic resolution and C
  emission.
- Priority: one operation protocol owner, consumer migration, missing-fact
  failure, old helper deletion, negative ratchet, then patch size.
- Fact owner:
  `src/self_hosted/semantic/ast_expression_graph_collection_call_protocol_owner.pgy`.
- Last legitimate consumers: the semantic List/Queue/Set call owners,
  collection verdict dispatcher, contextual literal emission, and the three
  collection call type/emit owners.
- Forbidden fallback: per-family name/arity/argument-position/return-shape
  helper redeclarations or final-emitter source-name routing.
- Verification gates:
  `tests/self_hosted/parity/collection_call_protocol_owner_smoke.sh`,
  `tests/self_host_hard_contract_smoke.sh`,
  `tests/sot_authority_edge_smoke.sh`, and
  `tests/sot_authority_adequacy_smoke.sh`.

Observed closure:

- `SemanticExpressionGraphCollectionCallProtocolFromName` owns the bounded
  List/Queue/Set operation rows, including constructor, receiver/index/value
  positions, arity, and return shape.
- The semantic and C-emission consumers read that protocol; the final direct
  dispatcher no longer branches on per-family source-name groups.
- The focused hard lane passed `backends=1`, `body_fixtures=20`, and the three
  selected MIR rows `list_ops`, `sequence_literal_list_queue`, and `set_ops`.
- Static protocol and hard-contract gates passed. Registry edge reported
  `49 authorities, 40 derived fact carriers, CLOSED=29 BRIDGE=20 ACTIVE=0`.
  Adequacy live binding and negative mutations passed; Coq/Rocq was a declared
  skip because no prover is installed.
- The full 280-row matrix and full-driver fixpoint were not run in this
  session. The protocol still uses String metadata, so the production-bar
  TypeId/RuntimeCallAbiId architecture is the next falsifier, not inferred as
  complete.

## Previous closed executable rung: generic wrapper materialization surface

Objective card:

- Objective: make C by-value Option/Result wrapper declarations consume one
  semantic concrete-type inventory while excluding generic formal templates and
  restoring concrete types from specialization facts.
- Priority: semantic signature/formal identity, concrete specialization facts,
  one wrapper owner, one generic symbol owner, missing-fact failure, negative
  ratchet, then patch size.
- Fact owners: `src/self_hosted/codegen/input/value_wrapper_usage_owner.pgy`,
  `src/self_hosted/codegen/input/semantic_signature_codegen_view_owner.pgy`,
  and `src/self_hosted/compiler/symbol_table_owner.pgy`.
- Last legitimate consumer: `GenerateCUnitFromSemanticFacts` and the
  dependency-ordered type declaration scheduler.
- Forbidden fallback: treating generic formal `Option<T>`/`Result<T,E>` rows as
  concrete C wrappers, reparsing call text for actual types, and trimming the
  terminal separator from generic actual symbol components.
- Verification gates:
  `tests/self_hosted/parity/wrapper_policy_probe_parity.sh`,
  `tests/self_hosted/parity/generic_return_probe_parity.sh`, and
  `tests/self_host_hard_contract_smoke.sh`.

Observed closure:

- `CodegenValueWrapperUsageSurfaceHasFormal` uses typed AST parent identity and
  structured signature type-expression facts to suppress formal templates.
- `CodegenValueWrapperUsageFactsFromSemantic` consumes resolved concrete return
  and parameter types from `CodegenGenericSpecializationFacts`.
- `CodegenSemanticFunctionParamFlatIndexOrDie` owns missing flat-parameter
  failure; semantic generic-default row mismatch exits explicitly in its
  semantic owner.
- C wrapper-policy output was `option-wrapper=graph`,
  `result-wrapper=graph`, `target-drift=reject`; generic-return output covered
  `Int`, `Option<Int>`, and `String` plus three mismatch negatives. Both C
  focused gates compiled with `0 error(s), 0 warning(s)` and the hard contract
  gate exited 0.
- `a42616b7` and the follow-up `96a96868` were committed and pushed; local and
  remote HEAD are equal.

## Previous closed executable rung: semantic routine local inventory surface

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

- Collection-call protocol static gate: shared owner, migrated consumers, and
  negative redeclaration ratchet passed.
- Isolated Pergyra-built DRV-2 collection gate: `exit 0`,
  `backends=1 body_fixtures=20 mir_fixtures=3` for `list_ops`,
  `sequence_literal_list_queue`, and `set_ops`.
- `tests/self_host_hard_contract_smoke.sh`: hard substitution contract wired.
- `tests/build_pressure_contract_smoke.sh`: native, bounded self-host, full
  driver-fixpoint, detached-worker, and Git Bash matrix boundaries wired.
- PowerShell parser validation for `scripts/measure_build_pressure.ps1`.
- `bash -n tests/self_hosted/parity/driver_rung2_body_parity.sh`.
- `tests/documentation_quality_smoke.sh`.
- Full-pressure body bypass negative: direct body invocation exits 1 with
  `full pressure body requires measure_build_pressure.ps1`.
- Pressure sentinel propagation probe: the measured child observed
  `PGY_BUILD_PRESSURE_ACTIVE=1` and exited 0.
- Actual isolated native LTO relink and bounded Pergyra-built DRV-2 build under
  the measured process-tree sampler, with the peaks recorded above.
- Guarded full-driver source compilation through C and LLVM stayed below the
  3 GiB ceiling at 2,145.6 MB and 2,239.5 MB private respectively.
- C-built guarded oracle negatives: direct full input and bounded-fixture token
  misuse both exited 1 without an artifact; bounded `let_log` emitted MIR.
- The same guarded-oracle cases passed through LLVM, and both backends emitted
  byte-sized parity evidence at 2,341 bytes for bounded `let_log`.
- Pressure-owned full C oracle execution stopped at the 3 GiB boundary after
  170,534 ms with no artifact and no remaining oracle process.
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
- C wrapper-policy focused parity: `option-wrapper=graph`,
  `result-wrapper=graph`, `target-drift=reject`; missing Option/Result
  negatives remained explicit.
- C generic-return focused parity: `generic-call=x type=Int`,
  `generic-call=wrapped type=Option<Int>`, `generic-call=first type=Int`, and
  `generic-call=explicit type=String`; target, nested, and explicit mismatch
  negatives remained rejected.
- Both wrapper focused C compiles reported `0 error(s), 0 warning(s)`; the hard
  contract smoke exited 0 after the closure ratchets.
- `tests/sot_authority_edge_smoke.sh`: 49 authorities, 40 derived fact
  carriers, `CLOSED=29 BRIDGE=20 ACTIVE=0`.
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

- Successful full-input driver fixpoint completion under the 3 GiB boundary.
  The bounded run reached the ceiling and was stopped without an artifact.
- Full unfiltered 280-row hard DRV-2 matrix.
- LLVM parity for the generic wrapper and generic-return focused rungs.
- LLVM parity for the new Set literal rung.
- Actual Coq/Rocq proof execution.
- Full driver bootstrap/fixpoint for the separate dirty
  `driver_bootstrap.sh` change.
- Full native/selfhost integration matrix remains blocked in this environment
  by `Cannot create temporary file in C:\\Windows\\: Permission denied`.
- Full driver oracle MIR production remains resource-bound at driver scale;
  no source-level assignment drift result was claimed from the stopped run.

## Next executable work

The active executable blocker is full-driver MIR artifact production; the next
owner seam is its unbounded projection/lifetime boundary, not a guessed new
semantic fixture. At the next scheduled/merge boundary:

1. Verify HEAD/origin and preserve the dirty native C split and user-owned
   untracked documents.
2. Resolve or isolate the native JSON split component assertion under its own
   owner; do not absorb it into a Pergyra language rung by convenience.
3. Add an observable stage boundary that separates MIR fact construction from
   `SelfMirJsonProgram` projection, then move JSON output behind a bounded or
   streaming Pergyra owner. The last consumer is the verified MIR artifact
   write; nested whole-program `Array<String>` assembly and a raised cap are
   forbidden fallbacks.
4. Re-run the full driver fixpoint only through
   `self-host-driver-bootstrap-full-test-smoke`. The falsifier is either a
   3 GiB stop or output/schema/parity drift; success requires a complete
   artifact under the existing ceiling.
5. Once that blocker is closed, run the unfiltered 280-row hard DRV-2 matrix
   from MSYS2 and its LLVM lane. The first red row, not fixture ordering or an
   AI guess, chooses the next semantic rung.
6. For that row, record objective, owner, last consumer, forbidden fallback,
   and falsifier before editing. Keep C/LLVM peer emitters behind one
   Pergyra-owned fact spine rather than mirroring C fragments.
7. Land another executable Pergyra replacement before spending multiple
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
2. Verify `git status --short --branch`, HEAD/origin, the named collection-call
   protocol owner, and
   `tests/self_hosted/parity/collection_call_protocol_owner_smoke.sh`.
3. Treat the current source/registry/gates as authoritative when this snapshot
   disagrees.
4. Run the full hard matrix only at the scheduled boundary, then follow its
   first observed falsifier.
5. Refresh exact revision, dirty state, last green gate, next falsifier, and
   blockers after the next material session.

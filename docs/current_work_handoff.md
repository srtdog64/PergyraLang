# Current Work Handoff

Updated: 2026-07-24 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. Verify it against the
current source, `git status --short --branch`, the SoT registry, the active
owner, and the named executable gate.

## Resume checkpoint

- Active implementation checkpoint: `6b96266d` (`Unify semantic expression
  topology under the program graph`). It moves the stable
  `AstExpressionArena` storage owner to
  `src/self_hosted/hir/program_graph_owner.pgy` and makes the semantic graph
  borrow that topology instead of copying node-kind and child arrays.
- The blocking graph gate now reports `phase=semantic-repointed` with exactly
  two structural stores: the program topology and the still-open MIR
  projection. The previous HIR and semantic structural stores are retired.
- `6263c490` also repoints the machine-resource receiver consumer from copied
  MIR graph rows to `SemanticExpressionGraphView`. The MIR aggregate store is
  still present, so this is consumer progress rather than the final two-to-one
  storage transition.
- C and LLVM remain peer production backends of the native compiler. The
  Pergyra-built DRV-2 is a bounded source/MIR-to-C replacement, not yet a fully
  self-hosted compiler or a Pergyra-owned LLVM emitter.
- `08c2b231` has a misleading title because two concurrent tasks raced on the
  shared Git index. Its actual content is the ability generic-bound lifetime
  closure and its smoke gate, not the program-graph migration. It is already
  pushed and must not be treated as graph evidence. `6b96266d` is the actual
  graph implementation commit.

## Active program-graph objective card

- Objective: carry one expression topology from parser/HIR through semantic
  and MIR consumers without whole-program structural copies.
- Priority: stable identity, owner-directed facts, old-store deletion,
  missing/foreign-handle failure, bounded lifetime, then file layout.
- Fact owner: `src/self_hosted/hir/program_graph_owner.pgy`, registered as the
  structural carrier for `selfhost.expression_graph` rather than a competing
  semantic authority.
- Last legitimate consumers: semantic expression verdicts, MIR
  instruction/root/origin projection, AIR evidence publication, and verified
  C/LLVM/self-hosted projection lanes.
- Forbidden fallback: dual structural reads, `new ? old`, expression-text
  recovery, per-routine whole-program graph copies, or raising the 3 GiB cap.
- Gate: `tests/self_host_program_graph_unification_smoke.sh`; current expected
  state is exactly two structural owners. The MIR migration must tighten the
  same gate to one.

## Implemented graph slice

- `AstExpressionArena`, its empty constructor, node count, row-alignment
  contract, and storage schema live in `hir/program_graph_owner.pgy`.
- `hir/ast_expression_graph_owner.pgy` imports the program owner and validates
  node invariants without redeclaring storage.
- `SemanticExpressionGraphArena` carries `topology: AstExpressionArena` plus
  semantic-only normalized-text, call-target, and place overlays.
- `SemanticExpressionGraphFactsFromAstRows` no longer copies `node_kinds`,
  `left_children`, or `right_children`; call-target capture preserves the same
  topology while replacing only overlays.
- Semantic, MIR-lowering, codegen-input, and probe consumers read topology
  through the new carrier. Structural negative probes use an explicit
  `SemanticExpressionGraphArenaFromRows` builder; production capture is
  ratcheted to `FromTopology`.
- The dashboard owns a blocking `PARTIAL` `program_graph_unification` row at
  13/13. Boundary migration and the derived-carrier registry record the owner
  move without inventing a second top-level fact family.

## Memory verdict

The graph substitution is real, but it does not close the 20+ GiB class defect.
The clean official pressure observation for a semantic-repointed snapshot
recorded:

- elapsed `608905 ms`;
- peak working set `2531.5 MB`;
- peak private memory `3076.7 MB`;
- top process `driver_oracle.exe` at `3065.9 MB` private;
- initializer row 5,214 complete and row 5,215 started;
- no MIR/JSON artifact; the pressure owner returned 88 after enforcing the
  3 GiB ceiling.

This falsifies the semantic topology copy as the sole memory cause. The native
compiler still produces the same composed-driver MIR in about 394 MB and 48 s,
with an approximately 120 MB golden artifact. The self-hosted failure remains
whole-program semantic fact retention before MIR/JSON, now measured as roughly
26x the native reference.

Later current-source pressure attempts are not compiler evidence: one narrowed
MSYS2 `PATH` hid PowerShell and entered the unbounded fallback, and later runs
were contaminated by concurrent compiler builds. Owned runaway processes were
stopped; external processes were not touched. The Make contract now fails
closed on Windows if the PowerShell pressure owner is unavailable, and
`tests/build_pressure_contract_smoke.sh` ratchets that rule.

## Last observed gates

Green on the graph commit slice:

- `tests/build_pressure_contract_smoke.sh`;
- `tests/self_host_program_graph_unification_smoke.sh` with
  `phase=semantic-repointed structural_owners=2`;
- `tests/boundary_migration_contract_smoke.sh` with six migration rows and
  negative mutation checks;
- `tests/self_hosted/parity/gate_dashboard_parity.sh` at 13/13;
- `tests/documentation_quality_smoke.sh`;
- `tests/self_hosted_component_contract_smoke.sh`;
- focused hard DRV-2 evidence: 20 body fixtures plus six selected graph-heavy
  MIR fixtures;
- codegen gen1/gen2 fixed point and bounded driver seed/oracle parity before
  the full-driver pressure stage.

Initializer projection passed its C/LLVM parity on the graph slice before the
concurrent lifetime-commit burst. During the moving-HEAD burst, official C/LLVM
invocations sometimes exited without diagnostics while imports changed. With a
fixed HEAD, LLVM IR emission (12,368,888 bytes), manual clang object emission,
and the complete native LLVM object/link path all succeeded. Re-run the
official paired parity once the workspace is exclusive; do not record the
moving-HEAD failures as a semantic regression or a green current-HEAD gate.

`PGY_ALLOW_MISSING_COQ=1 tests/sot_authority_adequacy_smoke.sh` previously
passed live owner/consumer binding and negative mutations. Coq/Rocq itself was
not installed, so the proof model was a declared skip, not a green prover run.

## Exact remaining dirty state after the handoff commit

Only these pre-existing/concurrent parity scripts should remain dirty. Do not
discard or fold them into another unit implicitly:

- `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`.

Verify this list after the handoff commit because another task may advance
`main` or add a new lifetime slice concurrently.

## Next executable work

1. Obtain an exclusive compiler-build window and verify HEAD/origin plus the
   exact dirty list.
2. Re-run the official initializer C/LLVM parity, then
   `self-host-driver-bootstrap-full-test-smoke` from an environment where
   PowerShell is discoverable. A Windows missing-PowerShell path must now fail
   before the full oracle starts.
3. Close the measured whole-program semantic lifetime boundary. The preferred
   unit is routine-scoped analysis/verify with owner-proved output ordering and
   comparison against the native 120 MB golden artifact. Do not tune JSON or
   raise the cap before execution reaches those stages.
4. Repoint remaining MIR instruction/root/origin consumers and delete
   `SelfMirExpressionGraphRows` structural topology. Tighten the graph ratchet
   from two stores to one; missing/foreign handles must fail closed.
5. Introduce the real revision owner and distinct stable identities
   (`CompilationRevisionId`, `ExpressionNodeId`, `SyntaxNodeId`, `TypeId`,
   `SymbolId`, `InstructionId`, `ValueId`) without aliasing them to one integer
   domain or inventing a compatibility identity.
6. After the full-driver artifact completes below 3 GiB, run the unfiltered
   280-row C/LLVM/self-hosted matrix. Its first red row chooses the next
   executable substitution rung.

## Resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, `src/self_hosted/OWNERS.md`,
   `docs/180_compiler_logical_spine_handles_gates.md`, and the
   `selfhost.expression_graph` registry rows.
2. Verify `git status --short --branch`, HEAD/origin, and the three named dirty
   parity scripts.
3. Run the graph ratchet and build-pressure contract before a broad build.
4. Confirm no other `pgy`, `genN`, `driver_oracle`, `gcc`, or `cc1` process is
   active before the pressure gate. Concurrent broad builds invalidate its
   attribution and may be terminated by the pressure owner.
5. Treat current source, registries, and executable gates as authoritative when
   this snapshot disagrees.

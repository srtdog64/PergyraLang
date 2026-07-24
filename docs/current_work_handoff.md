# Current Work Handoff

Updated: 2026-07-24 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. Verify it against the
current source, `git status --short --branch`, the SoT registry, the active
owner, and the named executable gate.

## Resume checkpoint

- Active implementation checkpoint: `fd2e0597` (`Close collection mutation
  graph ownership`). It carries the graph storage migration forward: the
  `AstExpressionArena` remains owned by
  `src/self_hosted/hir/program_graph_owner.pgy`, while MIR carries only
  semantic graph plus instruction root/range handles.
- The blocking graph gate now reports `phase=unified` with exactly one
  structural store: the program topology. The HIR, semantic, and MIR copied
  topology stores are retired.
- `20ba92fd` repoints the MIR let initializer's use-edge consumer from
  expression-text scanning to the semantic graph view. The let owner now
  fails closed when that graph fact is missing and attaches the same view to
  MIR. The remaining routine owners are still bridge consumers.
- `0367247b` repoints the MIR if-condition use-edge consumer in the same way:
  one semantic graph view is validated, used for SSA use derivation, and
  attached to MIR; a missing condition graph fails closed before instruction
  creation. The if owner no longer scans condition text.
- `ce6ee3cc` applies the same replacement to the MIR while-condition consumer.
  Its graph view is now the use-edge owner and missing graph facts fail closed;
  the while owner no longer recovers identifiers from condition text.
- `d9cb7f9b` closes the tracked value-return consumer. Bare returns retain an
  explicit `SelfMirNoUses()` path; value returns require an Atom graph,
  derive SSA uses from that view, and attach the same view. Missing value
  return graphs fail closed before instruction creation.
- `27102ed3` closes the MIR match-case subject consumer. Each case validates
  the match Atom graph before instruction creation, derives use edges from it,
  and attaches the same view; match subject text is no longer an SSA-use
  authority.
- `dccbfd41` closes the destructure initializer consumer. It validates the
  semantic Value graph, derives initializer use edges before registering new
  bindings, and attaches the same graph view. Missing graph facts fail closed,
  and initializer text is no longer an SSA-use authority.
- `2db972f9` closes both iteration text-use paths. Collection hoist uses the
  semantic Value graph, while foreach branch uses the semantic or synthetic
  graph view; range loops retain explicit no-use semantics. Missing source or
  foreach branch graphs fail closed before the affected instruction.
- `d05e653c` closes the graph-complete simple statement kinds (`Log`, bare
  call, and `Exit`) through a separate graph-owned lowering path. The path
  validates and reuses one Atom view for SSA uses and MIR attachment. The
- `fd2e0597` closes the remaining collection mutation statement bridge. The
  parser records receiver/value/index lane facts, MIR derives receiver uses
  from the semantic graph, Push/Set preserve their value/index graph slots,
  Pop remains receiver-use-only without a third MIR graph slot, and missing
  receiver facts fail closed. The old text-use owner is deleted and the
  collection graph-use gate is wired into the preparation contract.
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
  pushed and must not be treated as graph evidence. `6b96266d` is the prior
  graph implementation commit; `ec062184` is the MIR handle closure.

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
  state is exactly one structural owner. The companion
  `tests/self_hosted/parity/mir_expression_graph_projection_owner_smoke.sh`
  rejects copied MIR topology and raw graph reads.

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
- `SelfMirExpressionGraphRows` now stores only semantic graph handles and
  bounded source ranges. MIR JSON, assignment verification, and the
  initializer probe use semantic accessors; graph mismatch and invalid handles
  fail closed. The let initializer owner derives use edges from the same
  `SemanticExpressionGraphView` and rejects missing graph facts instead of
  falling back to source text. The graph gate reports one structural owner.
- `tests/self_hosted/parity/driver_rung2_let_graph_use_owner.sh` ratchets the
  let owner: it requires graph-owned initializer uses and rejects
  `SelfMirExpressionUses`/identifier-text recovery in that owner. The gate is
  wired into the preparation contract smoke.
- `tests/self_hosted/parity/driver_rung2_if_graph_use_owner.sh` applies the
  same negative ratchet to the if owner and is wired into the preparation
  contract smoke.
- `tests/self_hosted/parity/driver_rung2_while_graph_use_owner.sh` applies the
  same negative ratchet to the while owner and is wired into the preparation
  contract smoke.
- `tests/self_hosted/parity/driver_rung2_return_graph_use_owner.sh` ratchets
  the tracked return owner, requiring graph-owned value-return uses while
  preserving the explicit bare-return no-use fact. It is wired into the
  preparation contract smoke.
- `tests/self_hosted/parity/driver_rung2_match_graph_use_owner.sh` ratchets
  the match owner and is wired into the preparation contract smoke.
- `tests/self_hosted/parity/driver_rung2_destructure_graph_use_owner.sh`
  ratchets the destructure owner, including the ordering requirement that
  graph uses are resolved before destructured bindings mutate local state.
- `tests/self_hosted/parity/driver_rung2_iteration_graph_use_owner.sh` ratchets
  both iteration graph-use paths and is wired into the preparation contract
  smoke.
- `tests/self_hosted/parity/driver_rung2_simple_statement_graph_use_owner.sh`
  ratchets the graph-owned simple statement path and proves the collection
  text bridge is not used by that path. It is wired into the preparation
  contract smoke.
- `tests/self_hosted/parity/driver_rung2_collection_mutation_graph_use_owner.sh`
  ratchets receiver/value/index graph lanes, the missing-receiver guard, and
  the no-third-MIR-slot policy for collection mutations.
- The dashboard owns the blocking `program_graph_unification` row at 13/13.
  Boundary migration and the derived-carrier registry record the owner move
  without inventing a second top-level fact family.

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

The pressure observation predates the final MIR handle transition, so it is
not a measurement of the current one-owner snapshot. Source inspection also
shows why extracting the initializer-row body into a helper is insufficient:
the semantic environment clear path only pops array lengths, runtime
`Substring`/`StringConcat` allocate heap buffers, and no Pergyra-owned
last-consumer cleanup contract currently inserts the lower-level typed array
drop operations. The next memory rung therefore needs owner-proved reclamation
or region allocation for non-escaping temporaries, not a larger cap or an
ambient allocator fallback.

Until a real revision identity lands, foreign MIR graph rejection uses
`SemanticExpressionGraphFactsEqual`, a whole-graph comparison. That is the
current correctness bridge and an explicit performance falsifier; it must be
replaced by an owner-issued revision-scoped identity plus stale/foreign-handle
negative fixtures, not by a weak content shortcut.

Later current-source pressure attempts are not compiler evidence: one narrowed
MSYS2 `PATH` hid PowerShell and entered the unbounded fallback, and later runs
were contaminated by concurrent compiler builds. Owned runaway processes were
stopped; external processes were not touched. The Make contract now fails
closed on Windows if the PowerShell pressure owner is unavailable, and
`tests/build_pressure_contract_smoke.sh` ratchets that rule.

## Last observed gates

Green on the graph handle commit slice:

- `tests/build_pressure_contract_smoke.sh`;
- `tests/self_host_program_graph_unification_smoke.sh` with
  `phase=unified structural_owners=1`;
- `tests/self_hosted/parity/mir_expression_graph_projection_owner_smoke.sh`;
- `tests/boundary_migration_contract_smoke.sh` with six migration rows and
  negative mutation checks;
- `tests/self_hosted/parity/gate_dashboard_parity.sh` at 13/13;
- `tests/documentation_quality_smoke.sh`;
- `tests/self_hosted_component_contract_smoke.sh`;
- `tests/self_hosted/parity/driver_rung2_let_graph_use_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_if_graph_use_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_while_graph_use_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_return_graph_use_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_match_graph_use_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_destructure_graph_use_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_iteration_graph_use_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_simple_statement_graph_use_owner.sh`;
- current `bin/pgy.exe` DRV-2 owner import `--emit-c` with `0 error(s), 0
  warning(s)`;
- current component contract smoke with `RC=0` after registering the new
  lane-policy and MIR-lower bridge owners;
- current C DRV-2 `valid_array_builtins` MIR generation and MIR re-consumption,
  plus collection mutation parity, with `RC=0`;
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

The component contract smoke was started after the if change but produced no
result while an unrelated concurrent initializer build was active. It was
stopped before a result and is not recorded as green evidence for `0367247b`.
It has not been rerun on `ce6ee3cc`.
The return graph-use gate and direct DRV-2 owner import compile passed on
`d9cb7f9b`; the full component smoke remains unclaimed.
The match graph-use gate and direct DRV-2 owner import compile passed on
`27102ed3`; the full component smoke remains unclaimed.
The destructure gate passed, and both C- and LLVM-built self drivers passed all
20 body fixtures plus focused `array_destructure` canonical-MIR, emitted-C,
host-compile, negative, and runtime parity. The LLVM run crossed the unrelated
match-owner commit, but the destructure source/gate fingerprints remained
unchanged; this is focused destructure evidence, not a fixed whole-tree matrix.
The iteration graph-use gate and direct DRV-2 owner import compile passed on
`2db972f9`. The stale component assertion that required the retired iteration
text-use call was replaced with graph-use requirements; rerun results are
recorded below.
The simple-statement graph-use gate and direct DRV-2 owner import compile
passed on `d05e653c`. After replacing stale iteration and return assertions
and ratcheting the graph-owned simple-statement path, the full
`tests/self_hosted_component_contract_smoke.sh` passed on the current combined
source.

## Exact remaining dirty state after the handoff snapshot

After the graph-consumer documentation and component-gate commit, only these
concurrent parity changes should remain dirty. Do not discard or fold them
into another unit implicitly:

- `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`.

Verify this list after the handoff snapshot because another task may advance
`main` or add a new lifetime slice concurrently.

## Next executable work

1. Obtain an exclusive compiler-build window and verify HEAD/origin plus the
   exact dirty list. The attempted full self-host bootstrap was interrupted
   after concurrent bootstrap activity produced no attributable result; it is
   not green evidence for `d05e653c`.
2. Re-run the official initializer C/LLVM parity, then
   `self-host-driver-bootstrap-full-test-smoke` from an environment where
   PowerShell is discoverable. A Windows missing-PowerShell path must now fail
   before the full oracle starts.
3. Close the measured whole-program semantic lifetime boundary. The preferred
   unit is routine-scoped analysis/verify with owner-proved output ordering and
   comparison against the native 120 MB golden artifact. Do not tune JSON or
   raise the cap before execution reaches those stages.
4. Introduce the real revision owner and distinct stable identities
   (`CompilationRevisionId`, `ExpressionNodeId`, `SyntaxNodeId`, `TypeId`,
   `SymbolId`, `InstructionId`, `ValueId`) without aliasing them to one integer
   domain or inventing a compatibility identity.
5. After the full-driver artifact completes below 3 GiB, run the unfiltered
   280-row C/LLVM/self-hosted matrix. Its first red row chooses the next
   executable substitution rung.

## Resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, `src/self_hosted/OWNERS.md`,
   `docs/180_compiler_logical_spine_handles_gates.md`, and the
   `selfhost.expression_graph` registry rows.
2. Verify `git status --short --branch`, HEAD/origin, the named owner registry,
   and the three concurrent dirty parity files.
3. Run the graph ratchet and build-pressure contract before a broad build.
4. Confirm no other `pgy`, `genN`, `driver_oracle`, `gcc`, or `cc1` process is
   active before the pressure gate. Concurrent broad builds invalidate its
   attribution and may be terminated by the pressure owner.
5. Treat current source, registries, and executable gates as authoritative when
   this snapshot disagrees.

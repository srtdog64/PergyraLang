# Current Work Handoff

Updated: 2026-07-25 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. Verify it against the
current source, `git status --short --branch`, the SoT registry, the active
owner, and the named executable gate.

## Resume checkpoint

- Active implementation checkpoint: `14c1683b` (`Remove assignment target text
  duplicate`) on the isolated branch
  `codex/semantic-environment-owned-lifetime`. It removes the derived
  `SemanticAstAssignmentTypeFacts.target_texts` row and routes the codegen
  readiness check through the parser/assignment-owned
  `SemanticAstAssignmentFacts.target_texts` row. The preceding `938a5886`
  slice closes the initializer-text duplication seam, while `645d9f2c` closes
  the reachable artifact-owned callable table lifetime path through self-hosted
  codegen; ordinary `Array<String>` remains on the beta no-free policy.
- Prior graph checkpoint: `9f207fdc` (`Share artifact callable table
  across capture and body`), following `6433659b` (`Share callable table with
  match binding`), `561d8ae1` (`Share callable table with generic
  specialization`), `a1d508a0` (`Share callable table across assignment and
  statement passes`), `fa2d8383` (`Route call targets through shared callable
  table`) and `3ccbfd2d` (`Share semantic callable
  table across body passes`). It carries the graph storage
  migration forward: the
  `AstExpressionArena` remains owned by
  `src/self_hosted/hir/program_graph_owner.pgy`, while MIR carries only
  semantic graph plus instruction root/range handles.
- `3ccbfd2d` closes one executable body-analysis lifetime seam. The body owner
  creates one `SemanticAstExpressionFunctionTableFacts` value and routes it to
  initializer base, iteration, and initializer-refinement consumers. The
  callable-table fact owner validates aligned rows and fails closed on a
  missing or malformed fact; direct legacy entry points remain fixture-facing
  wrappers and are not used by the production body path.
- `fa2d8383` extends that same fact through
  `SemanticAstAnalysisResolveCallTargetsFromBody`. The resolver now consumes
  the body-owned table and fails closed on an invalid fact; its former
  per-pass `SemanticAstExpressionFunctionTables` rebuild is removed. The
  callable-table smoke now ratchets this consumer as well.
- `a1d508a0` extends the fact through assignment and statement type owners in
  the production body bundle. Contract rebuilds also pass an explicit table
  fact; both owners now fail closed on malformed rows and no longer rebuild
  the callable universe internally.
- `561d8ae1` extends the same fact through generic-specialization analysis.
  The generic owner now fails closed on a missing or malformed callable-table
  fact and reads names/returns only from that body-owned fact; its former local
  table rebuild is removed. The body route and callable-table smoke ratchet
  this consumer as well.
- `6433659b` extends the fact through match-binding environment seeding and
  expression-place analysis. All production body callers now pass the
  body-owned fact; match binding fails closed on a missing or malformed fact,
  and its former local table rebuild is removed. The callable-table smoke
  ratchets both the match-binding owner and its expression-place route.
- `9f207fdc` moves callable-table production to artifact analysis and carries
  the immutable fact through initial call-target capture into the body bundle.
  The capture owner and body owner no longer rebuild the callable universe;
  missing or malformed facts fail closed at the capture/consumer boundary.
  Remaining direct `SemanticAstExpressionFunctionTables` reads are limited to
  the fact producer/contract and fixture policy probes, outside the production
  body route.
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
  validates and reuses one Atom view for SSA uses and MIR attachment.
- `fd2e0597` closes the remaining collection mutation statement bridge. The
  parser records receiver/value/index lane facts, MIR derives receiver uses
  from the semantic graph, Push/Set preserve their value/index graph slots,
  Pop remains receiver-use-only without a third MIR graph slot, and missing
  receiver facts fail closed. The old text-use owner is deleted and the
  collection graph-use gate is wired into the preparation contract.
- `4ee38b73` keeps that closure within the component size contract by moving
  only the persisted `expr0`/`expr1` slot-requirement policy into
  `expression_graph_instruction_policy_owner.pgy`. The program graph remains
  singular; the new policy owns no graph storage or semantic identity.
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
- `expression_graph_instruction_policy_owner.pgy` is the bounded MIR-wire slot
  policy. `expression_graph_fact_owner.pgy` remains the schema-aware decoder
  and reconstructed NodeId binder, now below its 280-line component cap.
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

`3ccbfd2d` removes one repeated callable-table construction from the
production body path: initializer base, iteration, and refinement now borrow
one artifact-scoped table fact. This is a bounded allocation/lifetime
improvement, not evidence that the whole-program semantic retention defect is
closed; the next falsifier remains an exclusive full-driver pressure run below
the existing 3 GiB ceiling.

`fa2d8383` removes the same repeated table construction from body call-target
resolution, so the call-target fixpoint now borrows the already-produced fact.
This closes another consumer seam but does not change the whole-program
pressure conclusion.

`a1d508a0` removes the same per-pass table construction from assignment and
statement typing. Isolated C/LLVM initializer projection parity remains green;
the measured whole-program retention and reclamation problem remains open.

`561d8ae1` removes the same per-pass table construction from generic
specialization analysis. Isolated generic-return parity remains green; this is
another bounded allocation/lifetime improvement, not evidence that the
whole-program semantic retention defect is closed.

`6433659b` removes the same per-pass table construction from match-binding
environment seeding. Isolated match-binding semantic-to-MIR smoke remains
green; this is another bounded allocation/lifetime improvement, not evidence
that the whole-program semantic retention defect is closed.

`9f207fdc` removes the artifact-capture/body duplicate table construction by
carrying the artifact-owned fact through `SemanticAstArtifactAnalysis`. The
isolated initializer, assignment, and generic projection parities remain
green; the whole-program semantic retention defect remains open.

The exclusive official full bootstrap was then run from a detached
`bec2fca3` worktree with PowerShell discoverable, isolated `BUILD_DIR`/`BIN_DIR`,
and `PGY_BUILD_PRESSURE_LIMIT_MB=3072`. It returned `RC=2` after `548501ms`:
`peak_working_set_mb=2541.8`, `peak_private_mb=3091.3`, and
`top_private_process=driver_oracle.exe` at `3080.4MB`. The pressure owner
reported `limit_exceeded=true`; phase private peaks were orchestrate
`3091.35MB`, compile `1233.45MB`, and link `420.37MB`. This is not system-wide
memory exhaustion: Windows still reported approximately `24.88GB` physical
memory free at the peak. The displayed `0.51GB`-class value is a process/private
observation, while the compiler defect is the whole-program semantic lifetime
crossing the 3GiB ceiling by `19.3MB`. The detached worktree and its owned
processes were removed after the measurement without touching main-worktree
changes.

`6ef7641d` closes the next measured lifetime seam without changing the
ordinary collection policy. The semantic environment owner now uses
`ArrayPushOwnedString` for the temporary `names`, `types`, and `modes` rows and
`ArrayDropOwnedStrings` at its last consumer. The pair is registered in the
type checker, C transpiler, LLVM emitter/runtime, and both runtime surfaces;
the focused owner gate and a direct C push/drop execution harness passed. This
is a bounded reclamation unit, not evidence that the full-driver 3 GiB
pressure defect is closed. The next falsifier is an exclusive rebuilt
full-driver pressure run with the same 3072 MB ceiling.

`e5b8b4a3` extends that bounded reclamation to the artifact-owned callable
table. Its producer uses an explicit owned seed mode and owned row insertion;
the body type bundle releases `names`, `returns`, and `params` only after
`verdict:done`, then marks the fact invalid so stale consumers fail closed.
The callable-table gate proves there is no ordinary producer push and that the
release follows the final body consumer. This remains a focused lifetime
closure, not a full-driver pressure result.

`645d9f2c` completes the reachable owned-lifetime path through self-hosted
codegen. The C runtime projection duplicates owned strings, checks OOM and
capacity overflow, drops each owned element, and resets the array. The
codegen call owner emits the corresponding runtime symbols with explicit
`inout` addressability; the callable-table release snapshots struct fields
before crossing the inout boundary. This is still a bounded lifetime closure,
not a full-driver pressure result.

`938a5886` removes `SemanticAstInitializerTypeFacts.expression_texts`. That
row duplicated parser-owned `SemanticAstLocalBindingFacts.initializer_texts`
and had no live consumer beyond self-validation/probe constructors; readiness
now validates node identity against the parser owner without retaining a second
program-wide expression-text array. The initializer probe default/direct lanes,
seed bootstrap, initializer pressure-owner smoke, component contract, and
documentation quality gates passed. The member-call lane remains red with the
same Windows heap-corruption exit (`0xC0000374`) on both `938a5886` and the
preceding `645d9f2c` source checkpoint, so it is not attributed to this SoT
de-duplication.

`14c1683b` removes `SemanticAstAssignmentTypeFacts.target_texts`. Assignment
diagnostics and codegen now read target text from the parser/assignment fact
owner, and the type fact retains only type/verification/diagnostic rows. The
assignment projection parity, callable-table owner, component contract, and
UCRT self-host bootstrap seed gates passed. This is a bounded derived-fact
de-duplication; the latest exclusive pressure result is still the red
`aaf24849` observation below and must be rerun on this source checkpoint.

The later exclusive single pressure run at handoff HEAD `416e6aad` reproduced
the red result under the same `3072 MB` ceiling: `exit_code=-1`, elapsed
`548250 ms`, peak working set `2537.1 MB`, peak private memory `3084.2 MB`,
and `driver_oracle.exe` at `3073.4 MB` private across three processes. The
pressure owner reported `limit_exceeded=true`; phase private peaks were
orchestrate `3084.21 MB`, compile `1241.95 MB`, and link `295.39 MB`. The
make target returned `Error 88`. This is a compiler-process retention defect,
not system-wide free-memory exhaustion, and the `3072 MB` cap remains closed.

The pressure observation predates the final MIR handle transition, so it is
not a measurement of the current one-owner snapshot. Source inspection also
shows why extracting the initializer-row body into a helper is insufficient:
the semantic environment clear path only pops array lengths, runtime
`Substring`/`StringConcat` allocate heap buffers, and no Pergyra-owned
last-consumer cleanup contract currently inserts the lower-level typed array
drop operations. The next memory rung therefore needs owner-proved reclamation
or region allocation for non-escaping temporaries, not a larger cap or an
ambient allocator fallback.

The exclusive current-source pressure run at handoff HEAD `aaf24849` then
returned `exit_code=-1` after `549275 ms`: peak working set `2519.1 MB`, peak
private memory `3080.9 MB`, and `driver_oracle.exe` at `3070.1 MB` private
across three processes. The pressure owner reported `limit_exceeded=true`;
phase private peaks were orchestrate `3080.90 MB`, compile `1253.21 MB`, and
link `311.07 MB`, with `Error 88`. Relative to the preceding
`3084.2 MB`/`3073.4 MB` observation, the de-duplication reduced both peaks by
about `3.3 MB`, but the existing `3072 MB` ceiling remains red and must not be
raised.

Until a real revision identity lands, foreign MIR graph rejection uses
`SemanticExpressionGraphFactsEqual`, a whole-graph comparison. That is the
current correctness bridge and an explicit performance falsifier; it must be
replaced by an owner-issued revision-scoped identity plus stale/foreign-handle
negative fixtures, not by a weak content shortcut.

The earlier narrowed-PATH and concurrent-build pressure attempts remain
non-evidence. The official exclusive run above is attributable and red at the
existing ceiling. The Make contract still fails closed on Windows if the
PowerShell pressure owner is unavailable, and
`tests/build_pressure_contract_smoke.sh` ratchets that rule.

## Last observed gates

Green on the graph handle commit slice:

- `6ef7641d` semantic environment owned-lifetime gate:
  `tests/self_hosted/parity/semantic_expression_environment_owned_lifetime_smoke.sh`;
- `6ef7641d` GCC C syntax-only checks on the changed C backend, LLVM emitter,
  LLVM runtime, and collection type-checker files;
- `6ef7641d` direct C execution harness for the owned String push/drop pair;
- `e5b8b4a3` callable-table owner gate:
  `tests/self_hosted/parity/semantic_function_table_owner_smoke.sh`;

- `645d9f2c` owned-lifetime owner gates:
  `tests/self_hosted/parity/semantic_expression_environment_owned_lifetime_smoke.sh`
  and `tests/self_hosted/parity/semantic_function_table_owner_smoke.sh`;
- `645d9f2c` self-host component contract and documentation quality gates;
- `645d9f2c` `make -j2 self-host-codegen-bootstrap-seed-test-smoke`, with
  gen0/gen1 compile and gen2 seed artifacts ready;
- `938a5886` initializer expression-text de-duplication owner smoke,
  component contract, documentation quality, and
  `make -j2 self-host-codegen-bootstrap-seed-test-smoke`;
- `938a5886` initializer probe default and direct-call executable lanes passed;
  the member-call lane is an attributable red baseline blocker, not a green
  parity result.
- `14c1683b` assignment projection parity, assignment target-text negative
  ratchet, callable-table owner smoke, component contract, and
  `make -j2 self-host-codegen-bootstrap-seed-test-smoke` passed.
- The exclusive current-source full-driver pressure gate at `aaf24849` is red:
  `peak_private_mb=3080.9`, `top_private_mb=3070.1`, and `Error 88` at the
  unchanged `3072 MB` limit.

- `tests/self_hosted/parity/semantic_function_table_owner_smoke.sh` after
  adding the match-binding and expression-place consumers;
- `tests/self_hosted_component_contract_smoke.sh` after updating its body
  assertion to the shared callable-table production route;
- `bin/pgy.exe` DRV-2 owner import `--emit-c` with `0 error(s), 0 warning(s)`
  after the callable-table migration;
- GCC `-std=c11 -Isrc -Isrc/runtime -fsyntax-only` on the generated owner C;
- `tests/self_hosted/parity/semantic_expression_normalization_owner_smoke.sh`;
- `tests/self_hosted/parity/semantic_expression_validation_lifetime_owner_smoke.sh`;
- `tests/self_hosted/parity/semantic_initializer_pressure_owner_smoke.sh`;
- `tests/self_hosted/parity/initializer_projection_probe_parity.sh` with
  `RC=0` (C/LLVM semantic initializer and call-target projection parity);
- isolated `0d38b2f3` verification worktree with the `a1d508a0` patch:
  Pergyra owner import `0 error(s), 0 warning(s)`, GCC C syntax, component
  contract, callable-table smoke, and initializer C/LLVM projection all passed;
- isolated `2425f482` verification worktree with the `561d8ae1` patch:
  Pergyra owner import `0 error(s), 0 warning(s)`, GCC C syntax, callable-table
  smoke, generic-return parity, and component contract all passed;
- isolated `f493d0cb` verification worktree with the `6433659b` patch:
  Pergyra owner import `0 error(s), 0 warning(s)`, GCC C syntax, callable-table
  smoke, match-binding semantic-to-MIR smoke, and component contract all
  passed;
- isolated `ff30dfc1` verification worktree with the `9f207fdc` patch:
  Pergyra owner import `0 error(s), 0 warning(s)`, GCC C syntax, callable-table
  smoke, component contract, initializer C/LLVM projection, generic-return,
  and assignment projection all passed;

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
- detached `4ee38b73` component contract smoke with `RC=0` after moving all
  stale owner assertions to the lane-policy and MIR slot-policy owners;
- detached `4ee38b73` C and LLVM DRV-2 parity, each with 20 body fixtures and
  four focused MIR fixtures (`valid_array_builtins`, `ast_node_array_push`,
  `ast_node_array_set`, and `str_array`);
- detached `4ee38b73` SoT authority adequacy, program-graph unification
  (`phase=unified structural_owners=1`), and build-pressure contract gates;
- focused hard DRV-2 evidence: 20 body fixtures plus six selected graph-heavy
  MIR fixtures;
- codegen gen1/gen2 fixed point and bounded driver seed/oracle parity before
  the full-driver pressure stage.
- The exclusive current-head full-driver pressure gate was rerun once after
  all duplicate pressure trees were terminated: it reproduced `Error 88` at
  `peak_private_mb=3080.9` and `top_private_mb=3070.1`; this is the current
  falsifying fixture, not a green gate.

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

The current moving worktree is not an exclusive verification surface. Its
component smoke currently stops at a concurrent
`artifact_lower_owner.pgy` contract assertion, and its direct owner compile
stops at a concurrent borrowed-ref escape in
`SelfMirIterationSyntheticGraphView`; neither result is attributed to
`9f207fdc`. The isolated verification worktrees listed above are the
attributable evidence for the callable-table slices.

## Exact remaining dirty state after the handoff snapshot

At isolated HEAD `14c1683b`, the semantic environment, callable-table,
initializer-text, and assignment-target-text lifetime slices are clean
and pushed on `codex/semantic-environment-owned-lifetime`. `main` remains at
`2b95746d` and `origin/main` is aligned; the callable-table,
call-target, assignment, statement, generic-specialization, match-binding,
and artifact-capture slices are committed and pushed. The following concurrent
changes remain dirty and are intentionally excluded;
preserve them and re-check ownership before the next unit:

- `docs/91_build_troubleshooting.md`;
- `src/self_hosted/OWNERS.md`;
- `src/self_hosted/PROGRESS.md`;
- `src/self_hosted/hir/program_graph_owner.pgy`;
- `src/self_hosted/mir/artifact_lower_owner.pgy`;
- `src/self_hosted/mir/routine_for_owner.pgy`;
- `src/self_hosted/mir/routine_input_owner.pgy`;
- `src/self_hosted/mir/routine_iteration_owner.pgy`;
- `src/self_hosted/mir/routine_local_inventory_owner.pgy`;
- `src/self_hosted/semantic/ast_body_type_bundle_owner.pgy`;
- `src/self_hosted/semantic/ast_iteration_type_fact_owner.pgy`;
- `tests/self_hosted/parity/driver_rung2_foreach_call_type_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_iteration_graph_use_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`;
- `tests/self_hosted_component_contract_smoke.sh`;
- untracked `src/self_hosted/semantic/ast_iteration_graph_root_owner.pgy`.

The final process check observed concurrent compiler activity; do not stop it
without re-identifying ownership. The isolated verification processes have
ended.

## Next executable work

1. `14c1683b` is the latest executable source closure. Rerun the exclusive
   pressure target on this checkpoint to measure the bounded assignment
   de-duplication; the latest attributable baseline remains red at `3080.9 MB`.
   If it remains red, identify the next retained compiler-wide materialization
   owner and last legitimate consumer before adding another cleanup or
   representation change.
2. The production callable-table seam is closed through artifact capture and
   body analysis. Keep the remaining producer/fixture reads explicitly
   bounded, then obtain the official initializer C/LLVM parity on an exclusive
   compiler window.
3. The exclusive official full bootstrap was run with PowerShell discoverable;
   it is red at the 3GiB pressure ceiling (`peak_private_mb=3091.3`). Do not
   raise the cap or use a system-free-memory reading as the fix. Preserve the
   pressure contract and use the recorded peak as the falsifying fixture.
4. Close the measured whole-program semantic lifetime boundary. The
   initializer and assignment text duplications are now removed; the next
   preferred executable unit is a routine-scoped analysis/verify materialization
   with owner-proved output ordering and comparison against the native 120MB
   golden artifact. Do not infer a pressure improvement until the exclusive
   target is rerun.
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
2. Verify `git status --short --branch`, HEAD/origin, the named owner registry,
   and the exact concurrent dirty list above.
3. Run the graph ratchet and build-pressure contract before a broad build.
4. Confirm no other `pgy`, `genN`, `driver_oracle`, `gcc`, or `cc1` process is
   active before the pressure gate. Concurrent broad builds invalidate its
   attribution and may be terminated by the pressure owner.
5. Treat current source, registries, and executable gates as authoritative when
   this snapshot disagrees.

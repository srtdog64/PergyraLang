# Current Work Handoff

Updated: 2026-07-27 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. Verify it against the
current source, `git status --short --branch`, the SoT registries, the named
owner, and the named executable gate.

## Current resume checkpoint

- Implementation checkpoint: `adb9a502` on `main` (v65 one admitted MIR
  directly projects to C and LLVM). Its parent `20c4bf80` records the v64
  Pergyra-owned complete-source MIR production and formal bootstrap evidence.
- The Pergyra-built gen2 driver directly emitted one verified 54,205,046-byte
  MIR artifact from the current complete compiler source, SHA-256
  `3d6aa33595592f8af2c78a68c6d5fc9e5a242c15e55b9e5a8deb4fe60209083b`.
  It is byte-identical to the separate C-oracle artifact. The Pergyra-built
  seed consumes this Pergyra artifact to emit complete gen2 C; gen2 consumes
  the unchanged artifact and emits byte-identical gen3 C.
- Gen2/gen3 C is 3,378,704 bytes / 59,482 lines with SHA-256
  `6aaf915d67fb129fce6a85bece93d9c814c66dadf94578c8ee160e7b9e1f7087`.
  Both generations compile, and both reproduce the established 414-byte
  bounded artifact with SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
- This closes Pergyra-owned complete-source MIR production and the explicit
  gen2/gen3 fixed point. It does not replace the released/default C-owned
  `pgy`; released/default replacement remains 0%.
- The Pergyra-built integrated driver now admits the hello `pgy.mir.v1`
  artifact once and directly projects the same identity to C and LLVM without
  rebuilding AST or semantic artifacts. Both artifacts compile, run, and
  match the native C runtime oracle. The admitted artifact SHA-256 is
  `1c31e768cecf6650710d6a77745a4b0aae34d1fe0ee71acf96fd23d9c76e0c34`.
  This closes only the one-routine/one-block/ASCII literal-`Log` shape; general
  backend admission and default selection remain open.
- The v59-v64 memory conclusion is stable: cumulative graph copying and
  repeated whole-arena/readiness validation caused the historical 20+ GiB /
  3 GiB symptom. The Pergyra full-source producer used 1,091.0 MB peak private;
  all complete producer, consumer, and fixed-point legs stayed below the
  unchanged 3,072 MB cap.

## Exact dirty state at this handoff

After the handoff-only successor is committed, `main` and `origin/main` should
be synchronized. The following unstaged files are concurrent user work and
must remain unmodified and excluded from task commits:

- `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`.

No v63-v65 implementation or documentation file should remain dirty.

## Active executable objective card

- Objective: widen the same backend-neutral direct consumer from literal
  `Log` to `src/self_hosted/mir_lower/fixture/let_log.pgy`: an integer local,
  addition, direct `ToString`, and `Log`, with one unchanged Pergyra MIR driving
  C and LLVM.
- Priority: preserve one MIR identity, admit local/use/expression/call facts
  once, project both targets, fail closed on a missing fact, then carry the
  AIR-certified projection/spawn/parallel/region plans required by broader
  programs. Default-driver promotion comes only after that shared admission.
- Fact owner: `SelfMirProgramFacts`, the machine-admitted `pgy.mir.v1`
  `MirProgramRoutineIndex`, and its expression-graph arena own the artifact and
  identities. `direct_mir_backend_projection_owner.pgy` is a consumer and
  projection boundary; it must not become a second semantic owner.
- Last legitimate consumers: the backend-neutral direct projection boundary
  and its C/LLVM textual emitters. `CompilerTargetProjectionFact` owns target
  selection; the native compiler remains runtime oracle evidence only.
- Forbidden fallback: backend-specific MIR JSON readers, MIR-to-AST-to-semantic
  reconstruction, native source re-lowering, guessed local/type/call facts, a
  second MIR per backend, `new ? old` reads, raising the 3,072 MB cap, or moving
  ordinary `pgy` before shared backend admission is complete.
- Focused falsifier: run `let_log.pgy` through the Pergyra producer once and
  mutate its local result identity, graph use edge, arithmetic node, and
  `ToString` call target independently. Each missing/invalid fact must reject
  both targets before an output artifact exists.
- Acceptance gate: extend the direct dual-backend gate so the unchanged
  `let_log` MIR produces compiling C and LLVM with oracle-equal output, while
  its local/use/graph/call negatives and the existing bridge ratchet pass. This
  advances direct graph consumption; it still does not authorize normal
  `pgy` default selection.

## Current measured evidence

| Slice | Exit/time | Peak private / working set | Result |
| --- | ---: | ---: | --- |
| v63 observed current-driver build | 0 / 54,476 ms | 2,593.7 / 2,582.8 MB | Current parser/interpolation owners compiled below the cap. |
| C-oracle full MIR producer | 0 / 767,407 ms | 844.3 / 762.8 MB | 54,205,046-byte verified MIR emitted. |
| Pergyra gen2 full MIR producer | 0 / 1,210,574 ms | 1,091.0 / 963.4 MB | Byte-identical to the C-oracle MIR; no partial output. |
| full MIR consumer to gen2 C | 0 / 1,774,216 ms | 1,714.8 / 1,590.9 MB | Complete 3,378,704-byte C emitted. |
| gen2 host compile | 0 / 4,721 ms | 302.1 / 316.4 MB | `driver_gen2_v63.exe` created. |
| gen2 to gen3 C | 0 / 800,248 ms | 2,033.2 / 1,867.9 MB | Same MIR consumed; gen3 C byte-equal to gen2 C. |
| gen3 host compile | 0 / 4,942 ms | 337.0 / 351.6 MB | `driver_gen3_v63.exe` created. |
| fresh v64 codegen/parser seed refresh | 0 / 412,649 ms | 1,107.9 / 1,123.6 MB | Isolated current gen2 codegen and parser seeds created. |
| rewired full-bootstrap runner | 0 / 3,770,822 ms | 2,658.0 / 2,667.1 MB | Pergyra/C MIR parity, gen2 compile/bounded preflight, and gen2/gen3 C equality all passed. |
| v65 bounded integrated-driver rebuild | 0 / not separately timed | not separately sampled | Pergyra-built seed includes the backend-neutral direct MIR projection owner. |
| one-MIR direct C/LLVM gate | 0 / 12,596 ms | not separately sampled | One MIR SHA remained stable; both artifacts compiled, ran, and matched the native C oracle; graph/kind/target negatives passed. |

## Current gates and artifacts

Green:

- focused parser interpolation graph contract and 188-row parser manifest;
- native/self-host/fixture AST byte parity for `pipe_and_try`;
- DRV-2 C build and executable `let_log` readiness;
- `tests/self_host_preparation_smoke.sh`;
- `tests/self_hosted_component_contract_smoke.sh`;
- `bash -n tests/self_hosted/parity/driver_bootstrap.sh`;
- `bash -n tests/self_hosted/parity/one_mir_dual_backend_projection.sh`;
- `tests/self_hosted/parity/one_mir_dual_backend_projection.sh` using the
  Pergyra-built v65 seed;
- `tests/build_pressure_contract_smoke.sh`;
- `tests/self_host_ci_profile_smoke.sh`;
- `PGY_DOC_QUALITY_FULL_UTF8=1 tests/documentation_quality_smoke.sh`;
- `git diff --check`;
- gen2/gen3 complete C byte equality and bounded gen2/gen3 parity.
- the rewired `tests/self_hosted/parity/driver_bootstrap.sh` full-fixpoint body
  with fresh isolated seeds under the 3,072 MB pressure owner.

Environment omission:

- `mingw32-make` is not installed on this host, so the Make wrapper target was
  not executable. Its full-fixpoint runner body was invoked directly with the
  same environment and pressure contract; do not claim the wrapper passed.

Known unrelated RED, unchanged and not weakened:

- `tests/self_host_hard_contract_smoke.sh` stops only because
  `driver_rung2_owner.pgy` lacks the pre-existing literal
  `"tests/cases/backend_compare/device_slot_machine_layer/main.pgy"`.
- `tests/self_host_compiler_world_contract_smoke.sh` still expects the retired
  `CompileSourceToMirJsonVerified(` spelling while the current entrypoint owns
  the pressure-observed/verified file variants. This mismatch predates v65 and
  was not weakened or folded into the active direct-backend rung.

Current ignored evidence:

- `.tmp/instruction_writer_pressure/driver_source_v63_interpolation_graph.mir.json`;
- `.tmp/instruction_writer_pressure/driver_source_v63_gen2_owned.mir.json`;
- `.tmp/self_hosted/driver_bootstrap/v63_full.c`;
- `.tmp/self_hosted/driver_bootstrap/v63_gen3.c`;
- `.tmp/self_hosted/driver_bootstrap/driver_gen2_v63.exe`;
- `.tmp/self_hosted/driver_bootstrap/driver_gen3_v63.exe`.
- `.tmp/self_hosted/codegen/bootstrap_v64_formal/`;
- `.tmp/self_hosted/driver/bootstrap_v64_formal_r3/`;
- `.tmp/build-pressure/self-host-codegen-seed-v64-formal.summary.json`;
- `.tmp/build-pressure/self-host-driver-fixpoint-v64-formal-r3.summary.json`.
- `.tmp/self_hosted/driver/bootstrap_v65_one_mir/`;
- `.tmp/self_hosted/driver/one_mir_v65_formal/`.

## Historical execution directive: gen2 takeover before global SoT closure

Effective 2026-07-26, freeze broad SoT expansion and new fixture breadth until
the integrated gen2 driver exists and takes over the compiler-source build.
This is a scheduling boundary, not permission to bypass an owner or weaken a
fail-closed check. The planning estimate for attempting to close the remaining
SoT globally is approximately one year because the unresolved ownership seams
are individually difficult; treating that global closure as a prerequisite
would prevent the executable bootstrap from reaching a terminus.

Count the active bootstrap in this order:

1. the existing C-owned seed consumes the complete compiler source and emits
   the full `driver_gen2.c`;
2. the native C compiler builds that artifact into the integrated gen2 driver;
3. gen2 consumes the same complete compiler source and emits `driver_gen3.c`;
4. only then compare gen2/gen3 artifacts and behavior for the fixed point.

The first hard self-host threshold is step 3: gen2 must take over the complete
compiler-source build currently performed by the C-owned seed. A bounded
component fixed point, additional owner document, registry closure, or fixture
count does not satisfy that threshold.

Apply SoT work only when the current executable rung exposes a concrete missing
fact. Name that fact, its owner, its last legitimate consumer, the forbidden
fallback, and the falsifying case; close only that blocking seam, then resume
the same gen2 run. Do not sweep unrelated `BRIDGE` rows or pursue global
registry closure. Do not add breadth fixtures. A new fixture is allowed only
as the smallest reproducer for the blocker observed on the active complete
gen2 path, and it must not become a substitute for rerunning that path.

Reassess the remaining SoT and fixture backlog only after gen2 has consumed the
same complete source successfully. Until then, executable artifacts and their
observed gates outrank SoT percentage, document volume, fixture count, and
bounded-only parity as progress evidence.

## Post-gen2 Coq gap audit (queued; not the active executable rung)

Do not start a broad proof expansion before the gen2 takeover above. Commits
`ae638458` and `58b3830d` establish the first vertical spine: 41 registered
`.v` files now include shared root `PergyraCore.v`, importers
`PergyraCoreComposition.v`, `UnifiedCore.v`, and
`PergyraCoreZoneBridge.v`, plus foundation-first/load-path wiring in the kernel
gate. The source audit found no `Admitted` or Coq `Axiom`, and only the two
declared `SlotCalculus` interface parameters (`MaxSlotId` and `verify_token`).
`tests/formal_semantics_smoke.sh` now registers all 41 files and compiles them
from the same sibling-module load path. No local Coq/Rocq binary was available,
so both new proofs and the migrated capstone remain pending the dedicated Rocq
9 kernel CI; the local structural run was an explicit prover skip, not proof
success.

The important proof gaps are refinement gaps, not unfinished `Qed` blocks:

1. the new shared core is not yet comprehensively bound to the live
   parser/semantic/AIR/MIR owner facts used by the integrated compiler;
2. the parser-to-AST boundary is still outside the machine-readable pass/loss
   manifest;
3. `IntentStepSoundness.v` proves a linear authority-guarded fragment, not the
   composed types/generics/world/zone/effect/slot/async language core;
4. exceptional and cancellation exits are not covered by the pin/resource
   cleanup proof;
5. the transitive world/zone/projection frontier scheduler and its termination
   are not closed;
6. cross-axis generic carriage and full call-site evidence attribution remain
   outside the current mechanized bindings.

The first post-gen2 Coq unit must therefore bind the exact gen2-accepted
compiler path to live owner facts and a negative adequacy gate. Do not add
another independent abstract law before that refinement bridge exists, and do
not turn whole-language soundness into the next global-closure project.

## Historical v60 resume checkpoint

- Implementation checkpoint: `3418b0f3` (v60 structured expression occurrence
  identity) on `main`. Structured MIR-to-AST emission carries
  `(global instruction row, AST lane, derived ordinal)` occurrences into one
  final graph arena. Repeated CFG visits repeat the producer key and receive a
  fresh range; source text is only an assertion. Required MIR producer coverage
  fails closed, the intermediate persisted sequence view is deleted, and the
  native range branch now projects its stop expression while loop-init retains
  the start. The complete run passed the v59 positional mismatch, completed
  graph construction and semantic analysis, and reached assignment body typing
  below 1,131 MB private. It advances the executable rung but is not gen2 or
  hard substitution.
- The v60 predecessor is `a4738c25`, following `7eef684b` (v59 prefix
  readiness) and `19ecce41` (linear expression arena assembly). v59 removed
  cumulative `place_kinds` rebuilding, per-append whole-arena readiness, and
  program/routine-index reconstruction, then exposed the positional identity
  mismatch at `ParsePrimaryFact` instead of crossing the 3 GiB cap.
- The accepted predecessor is `195d9b64` (v58 single-consumption loop branch
  projection) on `main`. It removes the second per-block branch
  selection and second per-branch scalar read from loop-summary readiness,
  preserves exact routine/block/span identity and FOR range/foreach semantics,
  and materially improves the adjacent v57 normalized markers through routine
  1,728. The accepted v57 predecessor is `ab3f9066` (direct match-local
  routine-index consumption). The preceding v56 implementation is
  `6f5c373d`, reverted by `c9e8011a`; its separate instruction-alignment pass
  remained slower than the adjacent v48 control after MIR-start normalization.
  v57 removes that redundant pass, retains one routine-index owner and one
  instruction loop, and materially improves the shared normalized markers.
  Do not add a third match-local read shape. The rejected v55 implementation
  is `2eeeec13`, reverted by `1f77b0bc`; focused gates and disassembly proved
  the local transformation, but the fixed full run regressed materially. The
  rejected v52 implementation
  is `8c49f74f`, reverted by `40037e52`.
  The successor-pair seam is abandoned after its first measured shape; do not
  re-express it as another pair struct, wrapper, or carrier. The rejected v51
  implementation is `e6abdeaa`; the rejected v50 carrier is `530682af`,
  reverted by `c5ee6e62`. Accepted compiler source
  retains `5e12cf43`'s isolated stray runtime-row fail-closed correction. Its
  accepted performance baseline remains `8074d6c8` branch selection plus that
  correction. The resource ABI performance seam is now abandoned after both
  carrier and local-scan shapes regressed materially. The earlier rejected v49
  implementation is
  `80a54268`, reverted by `85cee4ff`. Its phi-prefix
  admission predecessor is
  `a05aaf06` (`admit MIR phi prefixes once per routine`). Its phi-prefix carrier
  predecessor is `99e76e76` (`carry
  MIR phi prefixes in routine facts`). Its branch-row predecessor is `4ee29ce2` (`carry MIR
  branch rows in routine facts`). Its CFG negative predecessor is `ec4b9eef`
  (`cover invalid CFG backedge batch results`), with CFG owner implementation
  `73133678` (`batch MIR CFG backedge facts per routine`). Its scalar-key
  predecessor is `dfc8e406`, its optional ABI scalar predecessor is
  `bf8a56b8`, its
  exact ABI witness predecessor is `0da9c5c2`, its ABI
  row-capture predecessor is `a5d56f42`, its
  routine-scalar predecessor is `dd68d6f3`, its
  instruction-view predecessor is `06f6994d`, its
  evidence predecessor is `84f68161`, its
  admitted-structure predecessor is `190d0dbf`, its document-index predecessor
  is `67502f50`, its
  routine-consumer predecessor is `d62553ee`, its
  exact-span predecessor is `157c340b`, its
  machine-admission predecessor is `0857899e`, and the complete artifact
  predecessor is `6329356f` (`bound-mir-json-string-leaf-lifetime`).
- The verified driver now proves semantic readiness once and enters
  `SelfMirProgramFactsFromReadyArtifact`; the independently callable checked
  entrypoint still owns the complete validation contract.
- Direct local assignments still require local/target type equality. Member
  and indexed assignments validate the root local separately and no longer
  compare that root type with the final selected member/index type.
- Production `--emit-mir-json-verified` writes through
  `SelfMirProgramJsonWriteFile` instead of materializing one whole-program
  `String`. Program/routine/block and instruction-local unbounded graph/list
  rows are streamed. Escaped/quoted string leaves use a call-local allocator
  pool released immediately after synchronous `FileWrite`; numeric and fixed
  bounded projections remain unchanged.
- Initializer local visibility now advances through
  `SemanticAstInitializerEnvironmentCursor`. Function-base rows are seeded
  once, lexical locals are appended/popped in source order, destructure rows
  publish atomically, and the two per-row full-function local scans are absent
  from the production loop.
- Pergyra semantic and canonical MIR facts remain the SoT. C and LLVM remain
  peer native compiler projections with their existing execution/reference
  roles; self-hosted artifacts must be compared against the declared C/LLVM
  oracle class. The Pergyra-built DRV-2 is still a bounded self-host replacement
  lane; this checkpoint does not claim a fully self-hosted driver or a
  Pergyra-owned LLVM emitter. It does establish the first complete current
  full-driver MIR artifact below 3072 MB.
- The MIR consumer now creates one typed machine admission and carries the
  exact declaration and routine index used by that proof. Exact-bound JSON
  readers accept only structure-owner spans; declaration phases and the first
  AST reconstruction reuse their inventories instead of rebuilding root facts.
- Routine headers, match/destructure arrays, render/ABI facts, and phi result
  identity now consume one exact routine/instruction owner. CFG structural
  merge is a pure `mir_cfg_graph_owner.pgy` query with branch-local blocked
  reachability; the routine index no longer runs candidate-local BFS.
- The hard MIR input builds one `MirDocumentFactIndex` and carries its root and
  top-level array bounds through schema, capture, routine, and machine
  admission. Exact-bound string materialization no longer calls
  `Substring(json, ...)`, and null tokens use `SubEqualsWithLen`.
- The admitted `MirProgramRoutineIndex` captures the program-order
  routine/block/instruction structure, instruction kind/source type, and raw
  machine spans. Machine admission and `MirRoutineFactIndex` consume this
  derived `pgy.mir.v1` view. Whole-program readiness is proved once at
  admission; per-routine construction uses an O(1) row guard.
- Routine reconstruction now consumes a typed instruction view and a canonical
  CFG block-id projection from that admitted structure. Common no-layout and
  no-resource instructions are decided from exact bounds without repeatedly
  validating the same instruction object and rediscovering its field bounds.
- CFG successor identity is decoded once into `Array<Int>` rows. Missing edges
  alone use the internal negative sentinel; an explicit negative wire target
  fails closed at `cfg_successor` and is exercised through both C and LLVM.
- MIR phi `uses` is treated as the producer-owned incoming-value inventory, not
  a predecessor-indexed native phi table. Its accepted arity is
  `2 <= use_count <= predecessor_count`, and a self-result input requires a
  CFG-proven incoming backedge.
- Each `MirRoutineInstructionFactBundle` construction now captures `result`,
  `expr0`, `expr1`, `arg0`, `arg1`, `slot_anchor`, `abi_type_name`, and
  `match_variant` plus raw ABI value spans in one pass over a routine's
  program-owned spans. It remains
  routine-local rather than turning the program index into a second
  global/local aggregate. Render,
  match, graph, assignment, and phi consumers use that bundle. A malformed
  count cannot cross into the next routine, and duplicate or non-string scalar
  fields fail closed.
- Required ABI rows no longer rebuild a generic object table for every field
  and then repeat the same work during identity hashing. The ABI owner captures
  one nested row and its field rows, applies canonical hash order to that
  capture, and owns both producer and final-consumer identity. The old
  instruction-span validator and repeated-scan hash path are absent.
- One MIR-to-AST execution retains only successful exact ABI validation
  witnesses. A required hit needs the raw type value, canonical decimal ID,
  required state, and complete raw layout payload. ID-only and cross-run reuse
  are forbidden; a changed payload is revalidated and fails closed.
- The routine scalar pass carries whether the ABI type value was one valid
  string or exact optional `null`. The ABI owner remains the semantic owner and
  uses that observation only with exact optional `id=0`/`layout=null` tokens.
  Required tuples still take the complete raw witness path; wrong-kind or
  noncanonical values are not repaired or guessed.
- The same scalar owner scans each key for an escape and dispatches plain keys
  to their raw-length comparison group. Escaped keys retain full semantic
  comparison and duplicate detection. No scalar carrier, helper, cache, or ABI
  semantic owner was added.
- The existing CFG graph owner computes backedge headers once per routine from
  one entry-reachability result and one avoiding traversal per reachable
  distinct incoming target. The fact index consumes that result; the old
  per-edge function is deleted. Invalid batch input is an empty typed result
  and a nonempty consumer reports `cfg_backedge`. Structural merge and phi are
  unchanged.
- The routine-local instruction fact bundle now carries each block's unique
  branch global row from its existing scalar pass. Condition, loop-transfer,
  and match-binding consumers select that row through the admitted routine fact
  index instead of searching the block or repeating full bundle admission.
  Routine/block identity, local/global range, scalar span, and final branch kind
  are checked. Duplicate, out-of-block, scalar-span-mismatched, or non-branch
  rows fail closed; the old bundle accessor and routine-lowering search cannot
  return as fallbacks. The program index remains structure/identity-only rather
  than becoming a second global/local scalar aggregate.
- `BuildMirMatchBindingLocalFacts` now consumes the already-admitted
  `MirProgramRoutineIndex` row directly. One row-readiness proof and bounded
  block/instruction ownership checks precede one instruction loop; only
  canonical `AST_MATCH_CASE` branch rows contribute match local names/types.
  Invalid owners, zero-block parallel-array gaps, wrong-kind match rows, and
  name/type count mismatches fail closed, while forged non-match local arrays
  are ignored. No second graph, carrier, cache, backend split, or old-read
  fallback was introduced.
- `LoopFlowSummaryProjectionReady` consumes each block's owned branch global
  row once. Positive rows receive one exact branch selection and one scalar
  capture; no-branch rows use exact `-1`. Routine identity, block spans,
  instruction offsets/counts, malformed sentinels, FOR fields, and foreach
  iteration facts fail closed before projection. `BlockHasLoopTransfer` and
  rendered `BlockCond`/`"for "` classification are absent, and no second graph,
  cache, carrier, helper, backend split, or fallback was added.
- The same routine-local bundle carries each block's leading phi count. A phi
  after the first non-phi is an invalid sentinel. The phi semantic owner scans
  only that prefix while retaining predecessor, arity, result, incoming-use,
  and backedge validation. It admits routine identity, exact block counts, and
  bundle shape once at entry, then directly reads the prefix array. The one-use
  per-block accessor is deleted. Missing/invalid prefix facts cannot fall back
  to a whole-block scan or JSON kind recovery.
- A direct `EmitBlockStmts` block-slice experiment passed its fail-closed gates
  but regressed the fixed run by 8,169 ms at routine 1,920 and lost routine
  1,984. It is explicitly reverted. Current source retains the accepted v48
  block-accessor shape; the failed v49 shape is evidence, not an active
  fallback.
- A later resource-runtime experiment captured four top-level fact families in
  every instruction scalar and expanded the routine bundle. It removed about
  145.6 MB of repeated resource top-span reading by static estimate but built
  in 62,385 ms and reached only routine 1,728 at 296,959 ms. `c5ee6e62`
  reverts it. The review-discovered stray wrong-kind runtime row fail-open is
  retained alone in `5e12cf43`; a non-resource instruction can no longer treat
  an explicit runtime row as absence.

## Historical v60 dirty state

The semantic implementation checkpoint is `3418b0f3`; its handoff-only
successor carries no semantic change. After that checkpoint is pushed,
`main` and `origin/main` are synchronized and no task-owned implementation or
documentation change is dirty. These unstaged files are concurrent user work
and must remain unmodified and excluded from task commits:

- `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`.

## Historical v60 executable objective card

- Objective: finish MIR-to-AST lowering for the completed admitted full-driver
  MIR artifact, emit and compile the integrated gen2 driver, and immediately
  make gen2 consume the same complete compiler source to emit gen3.
- Priority: preserve the exact `pgy.mir.v1` artifact identity, keep the MIR
  consumer and semantic owners fail closed, stay below the fixed pressure cap,
  complete the gen2 takeover, then establish the fixed point. Do not widen SoT
  or fixtures before that takeover.
- Fact owner: the verified `SelfMirProgramFacts` producer and its completed
  `pgy.mir.v1` artifact. At the current boundary,
  `SemanticAstAssignmentTypeFactsFromArtifact` owns assignment body-type
  derivation and `SemanticAstBodyTypeBundle` is its receiving boundary. The
  structured occurrence order and final expression arena are already admitted
  inputs; they must not be rebuilt inside assignment typing.
- Last legitimate consumer: current `driver_oracle.exe --mir-json` emitting
  `driver_gen2.c`, followed by the native C compiler only as the bootstrap
  object-code boundary.
- Forbidden fallback: regenerating a native oracle MIR per generation,
  backend-specific JSON reads, source-text or graph-text recovery, a second
  expression graph/order/cache, per-assignment whole-program reconstruction,
  `new ? old` authority, or raising the 3,072 MB / 1,800-second bounds.
- Focused falsifier: on the same 51,807,108-byte MIR artifact, progress from
  `semantic-body-type-stage assignment:start` to `assignment:done` under the
  fixed limits, or expose the exact assignment row and repeated owned read that
  prevents completion. Do not reopen graph identity or broaden fixtures.
- Acceptance gate: pressure-owned full MIR consumption emits `driver_gen2.c`,
  that artifact builds, and the resulting gen2 consumes the same complete
  compiler source to emit `driver_gen3.c`. The bounded preflight remains a
  focused diagnostic, not a prerequisite track that may delay this takeover;
  compare gen2/gen3 only after both complete artifacts exist.

## Historical measured evidence through v60

The original 20+ GiB observation was dominated by repeated graph/readiness
validation. Closing those repeated validations brought the current driver into
the fixed 3 GiB pressure window. Sequential instruction projection plus
call-local string-leaf lifetime now completes the full artifact in that same
window. The latest fixed-cap observations are:

| Slice | Peak private | Peak working set | Last observed state |
| --- | ---: | ---: | --- |
| `mir-fact-ready` | 2865.8 MB | 2359.0 MB | Reached MIR lowering; exposed the composite-assignment invariant at syntax node 5290. |
| `assignment-composite-ready` | 3233.9 MB | 2716.4 MB | MIR facts completed; crossed the cap after `json:start`. |
| `json-builder-ready` | 3195.6 MB | 2680.9 MB | MIR facts completed; whole-program JSON still crossed the cap. |
| `json-file-ready` | 3290.1 MB | 2775.6 MB | Wrote 20,013,056 bytes before routine-string materialization crossed the cap. |
| `json-block-file-ready` | 3197.3 MB | 2678.8 MB | Wrote 20,901,888 bytes; per-instruction/field strings still accumulated. |
| `initializer-cursor-ready` | 3117.9 MB | 2601.7 MB | All 8,229 initializer rows and MIR facts completed; crossed after `json-write:start` with 13,709,312 bytes. |
| `instruction-stream-ready` | 3092.7 MB | 2574.5 MB | Unbounded instruction/graph rows streamed; crossed with a 40,263,680-byte partial artifact because leaf strings remained result-lived. |
| `instruction-string-pool-ready` | 3064.3 MB | 2544.9 MB | Exit 0; complete 51,807,108-byte artifact and `json-write:done`. |
| `full-mir-consumer-admitted` | 53.0 MB | 66.1 MB | Input schema/capture completed; timed out at machine admission. |
| `full-mir-consumer-bounded-cursor` | 54.8 MB | 67.8 MB | Timed out while building the routine index; cursor-only `strlen` debt remained in field reads. |
| `full-mir-consumer-exact-bound` | 59.3 MB | 72.0 MB | Reached `routine-index:done`; timed out after `instruction-scan:start`. |
| `full-mir-consumer-machine-twofield` | 63.6 MB | 76.0 MB | One-pass two-field instruction read; still timed out after `instruction-scan:start`. |
| `full-mir-consumer-key-compare` | 57.1 MB | 69.9 MB | Machine/input admission completed; timed out after `mir-to-ast:start`. |
| `full-mir-consumer-exact-span` | 58.0 MB | 70.7 MB | Declaration fields and routine ends consume carried spans; reached `declarations:done`. |
| `full-mir-consumer-routine-fact-exact` | 58.0 MB | 70.8 MB | Routine fact bundle consumes exact spans; reached `first-top-level-routine-fact-index:done`. |
| `full-mir-consumer-routine-indexed` | 58.0 MB | 70.7 MB | Result/match facts consume one routine index; first top-level routine completed, no gen2 output. |
| `full-mir-consumer-cfg-owner` | 57.8 MB | 68.7 MB | Structural merge uses branch-local blocked reachability; first top-level routine completed, no 16 marker or gen2 output. |
| `mir-document-index-driver-build-v2` | 2319.9 MB | 2322.4 MB | Integrated C driver compiled in 57,528 ms below the fixed cap. |
| `full-mir-consumer-document-index` | 63.4 MB | 74.0 MB | Timed out at 300,554 ms after the 16-routine marker; no gen2 output. |
| `mir-program-instruction-index-driver-build-v3` | 2405.9 MB | 2409.3 MB | Integrated C driver compiled in 50,974 ms below the fixed cap. |
| `full-mir-consumer-program-instruction-index-v3` | 85.2 MB | 93.6 MB | Timed out at 300,606 ms after the 16-routine marker; no gen2 output or cap crossing. |
| `full-mir-consumer-borrowed-fact-v9` | 82.6 MB | 92.8 MB | `ref` accessors alone did not help; routine 16 completed at 133,593 ms. |
| `full-mir-consumer-bounds-fast-v10` | 82.7 MB | 91.1 MB | Exact-bound common paths cut routine 16 to 69,919 ms, then exposed `FindTopLevelComma` phi inventory drift. |
| `full-mir-consumer-phi-inventory-v11` | 88.5 MB | 96.7 MB | Passed the phi counterexample and reached routine 64 at 99,411 ms; timed out with no gen2. |
| `full-mir-consumer-direct-block-v12` | 88.5 MB | 96.5 MB | Direct canonical block rows preserved behavior; routine 64 at 99,803 ms. |
| `full-mir-consumer-int-cfg-v13` | 88.6 MB | 96.6 MB | Timed out at 180,056 ms; routine 64 at 99,447 ms and routine 128 at 164,457 ms; no gen2. |
| `mir-int-cfg-negative-ratchet-driver-build-v14` | 2442.7 MB | 2430.8 MB | Final-source integrated C driver compiled in 48,451 ms below the cap. |
| `full-mir-consumer-int-cfg-v14-300s` | 94.3 MB | 102.1 MB | Timed out at 300,324 ms; routine 192 at 235,898 ms; no gen2. |
| `mir-routine-scalar-bundle-driver-build-v23` | 2509.8 MB | 2498.5 MB | Current-source integrated C driver compiled in 47,746 ms below the cap. |
| `full-mir-consumer-routine-scalar-bundle-v23` | 87.0 MB | 95.3 MB | Timed out at 180,343 ms; routine 64 at 96,607 ms and routine 128 at 160,331 ms; no gen2. |
| `full-mir-consumer-routine-instruction-detail-v37-300s` | 92.2 MB | 100.1 MB | Timed out at 300,186 ms; required ABI rows dominated and routine 248 completed at 290,268 ms. |
| `full-mir-consumer-abi-bounds-v38-300s` | 92.1 MB | 100.0 MB | Outer-bound capture alone was a negative result; routine 248 regressed to 293,877 ms. |
| `full-mir-consumer-abi-row-capture-v39-300s` | 134.7 MB | 140.8 MB | Timed out at 300,560 ms; routine 192 at 102,775 ms, routine 448 at 231,271 ms, and routine 640 at 298,374 ms; no gen2. |
| `full-mir-consumer-abi-owner-v40-build` | 2565.3 MB | 2554.5 MB | Exact final-source integrated C driver compiled in 55,007 ms below the fixed cap. |
| `full-mir-consumer-abi-exact-reuse-v41-build` | 2346.8 MB | 2336.6 MB | Exact-source integrated C driver compiled in 52,722 ms below the fixed cap. |
| `full-mir-consumer-abi-exact-reuse-v41-300s` | 157.2 MB | 162.3 MB | Timed out at 300,227 ms after routine 640 at 228,455 ms, routine 704 at 238,884 ms, and routine 896 at 288,574 ms; no gen2. |
| `full-mir-consumer-abi-optional-fast-v42-build` | 2515.0 MB | 2503.6 MB | Exact-source integrated C driver compiled in 53,265 ms below the fixed cap. |
| `full-mir-consumer-abi-optional-fast-v42-300s` | 214.4 MB | 216.6 MB | Timed out at 300,115 ms after routine 704 at 162,849 ms, routine 896 at 192,157 ms, routine 1,600 at 241,729 ms, and routine 1,920 at 293,147 ms; no gen2. |
| `full-mir-consumer-key-dispatch-v43-build` | 2523.0 MB | 2511.6 MB | Exact-source integrated C driver compiled in 52,451 ms below the fixed cap. |
| `full-mir-consumer-key-dispatch-v43-300s` | 215.1 MB | 217.1 MB | Timed out at 300,268 ms after routine 704 at 162,255 ms, routine 896 at 190,875 ms, routine 1,600 at 239,277 ms, and routine 1,920 at 290,054 ms; no routine 1,984 or gen2. |
| `full-mir-consumer-cfg-backedge-batch-v44-build` | 2433.5 MB | 2427.0 MB | Exact-source integrated C driver compiled in 52,316 ms below the fixed cap. |
| `full-mir-consumer-cfg-backedge-batch-v44-300s` | 202.7 MB | 205.0 MB | Timed out at 300,682 ms after routine 704 at 162,403 ms, routine 896 at 191,236 ms, routine 1,600 at 240,535 ms, and routine 1,920 at 291,308 ms; CPU negative/noise versus v43, no routine 1,984 or gen2. |
| `full-mir-consumer-branch-row-bundle-v45-build` | 2534.1 MB | 2522.6 MB | Exact-source integrated C driver compiled in 52,025 ms below the fixed cap. |
| `full-mir-consumer-branch-row-bundle-v45-300s` | 204.8 MB | 206.9 MB | Timed out at 300,345 ms after routine 704 at 161,510 ms, routine 896 at 189,756 ms, routine 1,600 at 238,576 ms, routine 1,920 at 288,324 ms, and the first routine 1,984 marker at 298,381 ms; no routine 2,048 or gen2. |
| `full-mir-consumer-phi-prefix-bundle-v46-build` | 2556.9 MB | 2546.0 MB | Exact-source integrated C driver compiled in 52,507 ms below the fixed cap. |
| `full-mir-consumer-phi-prefix-bundle-v46-300s` | 202.1 MB | 204.3 MB | Timed out at 300,163 ms after routine 704 at 163,937 ms, routine 896 at 193,024 ms, routine 1,600 at 242,500 ms, and routine 1,920 at 293,716 ms; CPU negative/noise versus v45, no routine 1,984/2,048 or gen2. |
| `full-mir-consumer-phi-prefix-admission-v47-build` | 2535.7 MB | 2524.3 MB | Exact-source integrated C driver compiled in 51,436 ms below the fixed cap. |
| `full-mir-consumer-phi-prefix-admission-v47-300s` | 207.7 MB | 209.7 MB | Timed out at 300,384 ms after routine 704 at 158,438 ms, routine 896 at 186,805 ms, routine 1,600 at 234,127 ms, routine 1,920 at 283,594 ms, and routine 1,984 at 293,201 ms; recovered v46 and improved on v45, no routine 2,048 or gen2. |
| `full-mir-consumer-branch-index-admission-v48-build` | 2567.8 MB | 2557.0 MB | Exact-source integrated C driver compiled in 51,479 ms below the fixed cap. |
| `full-mir-consumer-branch-index-admission-v48-300s` | 206.3 MB | 208.3 MB | Timed out at 300,615 ms after routine 704 at 158,817 ms, routine 896 at 187,672 ms, routine 1,600 at 235,166 ms, routine 1,920 at 285,333 ms, and routine 1,984 at 295,075 ms; CPU negative/noise versus v47, no routine 2,048 or gen2. |
| `full-mir-consumer-block-slice-admission-v49-build` | 2587.7 MB | 2578.1 MB | Rejected exact-source experiment compiled in 60,860 ms below the cap but materially slower than v48. |
| `full-mir-consumer-block-slice-admission-v49-300s` | 202.3 MB | 205.0 MB | Rejected experiment timed out at 300,269 ms after routine 704 at 166,252 ms, routine 896 at 194,769 ms, routine 1,600 at 243,264 ms, and routine 1,920 at 293,502 ms; 8,169 ms later than v48 and no routine 1,984/gen2. Reverted by `85cee4ff`. |
| `full-mir-consumer-resource-raw-capture-v50-build` | 2445.2 MB | 2438.9 MB | Rejected exact-source experiment compiled in 62,385 ms below the cap but 10,906 ms slower than v48. |
| `full-mir-consumer-resource-raw-capture-v50-300s` | 178.2 MB | 182.3 MB | Rejected experiment timed out at 300,680 ms after routine 704 at 189,951 ms, routine 896 at 222,884 ms, routine 1,600 at 279,085 ms, and routine 1,728 at 296,959 ms; no routine 1,792/2,048 or gen2. Reverted by `c5ee6e62`. |
| `full-mir-consumer-resource-local-scan-v51-build` | 2576.8 MB | 2565.8 MB | Rejected exact-source experiment compiled in 56,417 ms below the cap but 4,938 ms slower than v48. |
| `full-mir-consumer-resource-local-scan-v51-300s` | 192.6 MB | 195.6 MB | Rejected experiment timed out at 300,614 ms after routine 704 at 173,196 ms, routine 896 at 204,052 ms, routine 1,600 at 255,976 ms, routine 1,728 at 272,517 ms, and routine 1,792 at 287,519 ms; it lost v48's routine-1,984 marker and produced no gen2. Reverted by `6879f0c0`. |
| `full-mir-consumer-block-successor-pair-v52-build` | 2591.5 MB | 2580.9 MB | Rejected exact-source experiment compiled in 67,265 ms below the cap, 15,786 ms slower than v48. |
| `full-mir-consumer-block-successor-pair-v52-300s-observed` | 172.9 MB | 176.6 MB | Rejected experiment timed out at 300,560 ms after machine routine-index completion at 83,531 ms and routines 704/896/1,600/1,664 at 198,093/233,293/291,565/298,472 ms; no routine 1,728/2,048 or gen2. Reverted by `40037e52`. |
| `full-mir-consumer-llvm-performance-v53-build` | 2399.0 MB | 2389.0 MB | Accepted-source LLVM projection compiled successfully in 139,295 ms below the cap and preserved focused C/LLVM semantics. |
| `full-mir-consumer-llvm-performance-v53-300s-observed` | 214.0 MB | 210.8 MB | LLVM projection timed out at 300,518 ms after machine routine-index completion at 73,014 ms and routines 704/896/1,600/1,856 at 172,586/202,127/250,313/295,125 ms; it was slower than C v48 and produced no gen2. |
| `full-mir-consumer-c-clang-v54-build` | 2557.6 MB | 2546.5 MB | Accepted-source C projection compiled with the explicit Windows clang host toolchain in 42,649 ms, 8,830 ms faster than GCC v48, with byte/failure parity preserved. |
| `full-mir-consumer-c-clang-v54-300s-observed` | 206.0 MB | 208.0 MB | clang-built C projection timed out at 300,665 ms after routines 704/896/1,600/1,920/1,984 at 160,553/188,638/237,074/286,528/296,279 ms; build-time win but runtime negative/noise versus GCC v48, no gen2. |
| `full-mir-consumer-json-ascii-constants-v55-build` | 2516.9 MB | 2505.4 MB | Rejected exact-source experiment compiled in 51,536 ms; focused C/LLVM, bounded SHA, and wrong-ABI behavior remained exact. |
| `full-mir-consumer-json-ascii-constants-v55-300s-observed` | 202.9 MB | 205.3 MB | Rejected experiment timed out at 300,480 ms after routines 704/896/1,600/1,920 at 162,958/191,199/240,394/291,112 ms; 5,779 ms later than v48 at routine 1,920, no routine 1,984/gen2. Reverted by `1f77b0bc`. |
| `full-mir-consumer-match-owner-filter-v56-build` | 2587.0 MB | 2576.3 MB | Rejected exact-source experiment compiled in 69,158 ms; focused C/LLVM, component, bounded SHA, and wrong-ABI behavior remained exact. |
| `full-mir-consumer-match-owner-filter-v56-300s-observed` | 166.2 MB | 170.5 MB | Timed out at 300,772 ms after routine 1,408 at 296,916 ms. After adjacent-v48 MIR-start normalization it was 2,420/2,929/5,767 ms slower at routines 256/704/896; reverted by `c9e8011a`. |
| `full-mir-consumer-v48-current-control-300s-observed` | 174.2 MB | 177.9 MB | Adjacent unchanged-source control under the current load: MIR-to-AST start at 83,190 ms, routines 704/896/1,600/1,664 at 198,926/233,149/290,131/296,995 ms; no gen2. |
| `full-mir-consumer-match-routine-owner-v57-build` | 2588.3 MB | 2577.6 MB | Accepted exact-source C driver compiled in 56,640 ms; focused C/LLVM, component, bounded SHA, and wrong-ABI behavior passed. |
| `full-mir-consumer-match-routine-owner-v57-300s-observed` | 197.5 MB | 200.4 MB | Timed out at 300,609 ms after routines 704/896/1,600/1,664/1,728/1,792/1,856 at 172,807/202,276/251,736/258,128/267,628/281,858/296,651 ms. Normalized gains over adjacent v48 are 17,102/21,856/29,378/29,850 ms at 704/896/1,600/1,664; accepted, no gen2. |
| `full-mir-consumer-match-routine-owner-v57-adjacent-v58-control-300s-observed` | 177.5 MB | 181.1 MB | Adjacent accepted v57 control timed out at 300,250 ms; MIR-to-AST started at 80,208 ms and routines 256/704/896/1,600/1,664/1,728 completed at 104,993/191,418/224,809/280,783/287,747/298,614 ms; no gen2. |
| `full-mir-consumer-loop-branch-owner-v58-build` | 2587.9 MB | 2577.0 MB | Accepted exact-source C driver compiled in 60,952 ms below the fixed cap. |
| `full-mir-consumer-loop-branch-owner-v58-bounded` | 0.0 MB sampled | 0.0 MB sampled | Exit 0 in 1,688 ms; the process finished between 100 ms samples, output remained 414 bytes with the established SHA. |
| `full-mir-consumer-loop-branch-owner-v58-wrong-abi` | 0.0 MB sampled | 0.0 MB sampled | Exit 1 in 1,672 ms with the owned ABI diagnostic and no output; the process finished between samples. |
| `full-mir-consumer-loop-branch-owner-v58-300s-observed` | 197.3 MB | 200.0 MB | Timed out at 300,470 ms after routines 704/896/1,600/1,664/1,728/1,792/1,856 at 173,630/202,723/252,244/258,345/267,970/282,271/297,340 ms. Normalized gains over adjacent v57 are 13,115/17,413/23,866/24,729/25,971 ms through 1,728; accepted, no gen2. |
| `mir-lower-loop-branch-owner-v58-llvm-build` | 315.5 MB | 318.3 MB | Focused LLVM `mir_lower` compiled in 4,104 ms; C/LLVM valid output and invalid-ABI failure were byte-equal. |
| `full-mir-consumer-loop-branch-owner-v58-integration-completion` | 3072.1 MB | 2459.3 MB | Reached MIR-to-AST completion at 387,029 ms, then stopped on the unchanged memory limit at 1,059,616 ms inside expression graph construction; no output. |
| `full-mir-consumer-expression-arena-linear-v59-ready-proof-build` | 2590.1 MB | 2579.1 MB | Exact-source v59 driver compiled in 66,274 ms below the fixed cap. |
| `full-mir-consumer-expression-arena-linear-v59-ready-proof-bounded` | 0.0 MB sampled | 0.0 MB sampled | Exit 0 in 1,336 ms; 414 bytes with the established SHA. |
| `full-mir-consumer-expression-arena-linear-v59-ready-proof-wrong-abi` | 0.0 MB sampled | 0.0 MB sampled | Exit 1 in 486 ms with the owned ABI diagnostic and no output. |
| `full-mir-consumer-expression-arena-linear-v59-integration-completion` | 801.8 MB | 749.4 MB | Reached MIR-to-AST completion at 429,211 ms and failed closed at 1,645,538 ms on the positional graph/surface identity mismatch; no output and no memory-limit crossing. |
| `v59-expression-surface-count-probe-full` | 230.4 MB | 233.2 MB | Completed in 498,952 ms: 41,299 surfaces, 35,638 persisted-required lanes, and 1,758 parser-only lanes. Flat MIR contains only 34,962 roots. |
| `full-mir-consumer-structured-occurrence-v60-build` | 2480.3 MB | 2473.7 MB | Exact-source v60 C driver compiled in 69,368 ms below the fixed cap. |
| `full-mir-consumer-structured-occurrence-v60-observed-build` | 2575.8 MB | 2564.5 MB | Observed bootstrap driver compiled in 65,293 ms below the fixed cap. |
| `full-mir-consumer-structured-occurrence-v60-integration` | 1130.3 MB | 1041.1 MB | Expression graph done at 1,673,958 ms, semantic analysis done at 1,674,754 ms, then timed out at 1,800,768 ms during assignment body typing; no graph error, cap crossing, or gen2 output. |

The cursor run completed in 869,913 ms before the pressure owner stopped it
inside routine `SemanticExpressionGraphNodeKind`. `e5587bee` then removed the
complete production instruction and graph Strings. Its first fixed-cap run
completed all current 8,266 initializer rows and MIR facts, started JSON near
2,956 MB, and advanced to 40,263,680 bytes before escaped/quoted leaf results
crossed the cap at 810,472 ms.

`6329356f` moves only those file-boundary leaves into a call-local pool and
destroys it after synchronous `FileWrite`. The successor run exited 0 in
675,355 ms. Peak private was 3,064.3 MB, with `driver_oracle.exe` at
3,063.1 MB; two processes and no compiler/link subprocess were observed. The
artifact is valid `pgy.mir.v1` with 2,345 routines, 142 declarations, and
SHA-256
`1621adf4070bc778dd90493e29db857c22f13722d951bea8a94d1241e9ee884e`.
The full JSON parse and closing `]}` were observed. The production gate is
green, but its 7.7 MB sampled margin is narrow and does not close the broader
semantic/MIR live-state debt.

The consumer measurements are CPU failures, not memory failures. The first
cursor implementation called generated `strlen(json)` at least three times per
routine/block/instruction row, implying about 8.8 TB of avoidable length
walking before field reads. Exact-bound readers removed that debt and reached
`routine-index:done` for the first time. Allocation-free normal-key comparison
then completed the instruction scan, machine admission, and input boundary.
`157c340b` next removed about 2.45 TB of logical declaration-field walking and
at least 118.9 TB from the routine fact prefix. `d62553ee` captures routine
headers, instruction results, and instruction-local arrays once, then moves
structural-merge selection from worst-case O(B^3) candidate-local BFS to
O(B^2) branch-local BFS. The full artifact contains 20,022 blocks, 34,091
instructions, 3,532 phi rows, and 214,151 expression-graph nodes. Its first
top-level routine is only 2,063 bytes with one block/instruction, so the fixed
window is dominated by the admitted machine path and accumulated routine
work, not by that routine or memory.

`67502f50` closes another observed hidden length path. The 34,091 null
machine-layer tokens performed about 1.766 TB of whole-document length walking,
and the minimum kind/name routine decode added about 243 GB, because bounded
reads still materialized through native `Substring(json, ...)`. The common
JSON owner now uses the caller limit while materializing strings, and machine
null reads use `SubEqualsWithLen`. The unchanged 300-second run advanced from
the first routine to 16 routines at only 63.4 MB peak private. This remains
RED: no run opened a partial gen2 C artifact.

`190d0dbf` closes the next structural duplication. The admitted program view
captures 2,345 routine, 20,022 block, and 34,091 instruction spans once and
carries kind/source type plus machine contact/layer spans. Machine admission
and per-routine fact construction no longer rescan nested structure. Review
also found and removed a whole-program `StructureReady` call from every routine
builder; the component contract rejects its return. The v3 fixed-window run
still ended at the 16-routine marker, so the removed work was real but not the
dominant remaining cost. Routines 1-64 contain only 274,581 of 51,741,503
routine-object bytes (0.531%); neither marker is completion. Peak private was
85.2 MB, `limit_exceeded=false`, and no gen2 file was opened.

`06f6994d` closes the instruction-local repeat-scan seam reached by that run.
Merely changing fact-table accessors to `ref` did not improve the v9 timing.
Generated-C inspection corrected the earlier diagnosis: `String` is passed as
a `char *`, and `JsonObjectFactTable` stores that source pointer plus bounds;
it does not deep-copy 51.8 MB into every table. The real cost was repeatedly
revalidating the same instruction object and rediscovering fields/bounds from
the same 51.8 MB-backed source view. Exact-bound ABI/resource common paths
avoid those repeated object/table reads: the observed
instruction ABI step fell from 492 ms to 9 ms, the resource step from 646 ms to
0 ms, and routine 16 from 133,593 ms to 69,919 ms. The next real producer-wire
counterexample was `FindTopLevelComma`, whose loop header has seven CFG
predecessors but two incoming inventory values. The phi owner now preserves
that wire meaning and v11 passed it.

The v13 full-artifact run kept `output_capture_complete=true`,
`limit_exceeded=false`, and only 88.6 MB peak private while reaching routine 64
at 99,447 ms and routine 128 at 164,457 ms. This is a CPU bottleneck, not a
return of the 3 GiB memory defect. The final v14 driver build stayed below the
cap and its bounded output remained exactly 414 bytes with SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
No run reached `consumer:mir-to-ast:done` or opened a complete
`driver_gen2.c`.

`dd68d6f3` closes the next measured routine-local seam. Each routine fact-index
construction now captures the render/result fields in one strict scalar pass,
while the admitted program index remains structure/identity-only. The active
MIR-to-AST reconstruction reuses that bundle, but the later expression-graph
and assignment post-passes still reconstruct a routine index and remain an
open re-entry seam. Phi context is computed lazily
only for blocks that actually contain a phi, and its incoming-backedge fact is
read from the canonical routine index instead of recomputing dominators. The
current v23 build completed in 47,746 ms below 3 GiB and preserved the exact
414-byte bounded SHA. Its 180-second run used 87.0 MB peak private / 95.3 MB
working set and moved routine 128 from the v14 300-second run's 165,019 ms to
160,331 ms. The improvement is real but modest; repeated scalar reads were not the
dominant remaining cost. `output_capture_complete=true`,
`limit_exceeded=false`, and no gen2 output was opened.

`a5d56f42` closes the required ABI-layout repeated-scan seam exposed by the
v29-v37 observation ladder. The v38 outer-bound-only experiment did not improve
the required row cost, proving the nested object/field validation and second
identity walk were dominant. The ABI owner now captures the nested row once,
validates at most eight fields, and hashes the captured values in canonical
semantic order. Raw instruction value spans remain location evidence, not a
second ABI authority. The producer compatibility entrypoint delegates to the
same captured identity owner, and component/ABI gates reject the deleted path.

The v39 300-second run used 134.7 MB peak private / 140.8 MB working set and
moved routine 192 from v38's 233,517 ms to 102,775 ms. It reached routine 640 at
298,374 ms, versus v38 ending near routine 248. The exact final-source v40
driver built in 55,007 ms below 3 GiB and preserved the exact 414-byte bounded
SHA. A bounded wrong-ID tuple exits 1 with the owned ABI diagnostic. This is
material executable progress but remains RED for bootstrap completion: no
`consumer:mir-to-ast:done` marker and no gen2 file exist.

`0da9c5c2` closes the identical-required-row revalidation seam without making
the 28-bit layout ID a cache authority. Before routine 640, 580 required rows
reduce to five complete tuples. The ABI owner remembers a tuple only after the
full order-independent capture and canonical hash succeed. Reordered JSON is a
safe miss and full revalidation; the same ID with a changed nested offset is a
miss and rejection. The focused C/LLVM fixture locks down both cases.

The exact-source v41 driver built in 52,722 ms at 2,346.8 MB peak private /
2,336.6 MB working set. Its 1,251 ms bounded result remains exactly 414 bytes,
and the wrong-ID input exits 1 without opening output. The full fixed-window
run moved routine 640 earlier by 69,919 ms (23.4%) relative to v39, passed the
old routine-704 falsifier, and reached routine 896 at 288,574 ms. It timed out
at 300,227 ms with 157.2/162.3 MB peak private/working set. This remains RED:
there is still no `consumer:mir-to-ast:done` marker or gen2 file.

`bf8a56b8` closes the duplicate optional ABI wire-read seam. The existing
routine scalar scan now carries type-value readiness, while the ABI owner keeps
the sole semantic decision and accepts the common optional case only with exact
raw `0`/`null` tokens. The v42 driver built in 53,265 ms below 3 GiB, preserved
the exact 414-byte bounded SHA, and rejected the wrong-ABI input in 551 ms with
no output. Its fixed-window run reached routine 704 at 162,849 ms, routine 896
at 192,157 ms, and routine 1,920 at 293,147 ms before timing out at 300,115 ms.
Peak private/working set was 214.4/216.6 MB. This is 76,035 ms and 96,417 ms
earlier at the shared 704/896 markers and 1,024 routines farther than v41, but
still RED for bootstrap completion: no `consumer:mir-to-ast:done` or gen2 file.

The v42 interval census covers all 29 completed 64-routine intervals. Interval
time versus instruction count has R-squared 97.43%; the remaining 425 routines
contain 7,873 instructions. The measured linear projection places
`top-level-routines:done` near process timestamp 355.9 seconds, before the still
unmeasured string join and AST inventory cost. That is a projection, not green
evidence and not permission to enlarge the 300-second diagnostic window. The
next measured CPU owner is `BuildMirRoutineFactIndex`: focused samples spend
1,051 of 1,464 ms (71.8%) in fact-index construction. Inside its scalar scan,
34,091 instruction objects expose 852,275 keys and currently trigger eleven
semantic key comparisons per key (9,375,025 calls). Dispatching plain keys by
their already-owned raw length reduces that to about 1,159,094 calls while an
escaped-key fallback preserves JSON equivalence and duplicate rejection. This
is the first minimal executable seam because it changes no fact owner, bundle,
or ABI decision. If linear cost remains after that, the broader candidate is
the second full instruction-object scan from
`BuildMirRoutineInstructionFactBundle` into
`MirRoutineInstructionScalarCaptureWithin`, after the admitted program index
already scanned every instruction for identity. A separate CFG census found
15,940 tail BFS calls but could not distinguish them from the strongly
collinear instruction/block volume; do not introduce a CFG cache or move phi
ownership on correlation alone.

`dfc8e406` executed the smaller falsifier first. Plain scalar keys now run only
their matching raw-length comparison group, while escaped keys retain the full
semantic fallback. The exact-source v43 driver built in 52,451 ms below 3 GiB,
preserved the 414-byte bounded SHA, and rejected the wrong-ABI input with no
output. The fixed-window run reached routine 1,920 at 290,054 ms, 3,093 ms
(1.06%) earlier than v42, then timed out at 300,268 ms without routine 1,984.
Peak private/working set was 215.1/217.1 MB. The comparison-count reduction is
real but not dominant. The next owner-directed move is inside the existing CFG
graph owner: compute the routine backedge result once, migrate the fact-index
consumer, and ratchet the per-edge dominator call. Keep structural merge and
phi unchanged for this slice.

`73133678` performs that owner migration and deletes the old edge-local
function; `ec4b9eef` proves the malformed result reaches an explicit consumer
failure. The static remaining-tail model reduces backedge BFS calls from 9,144
to 4,128, but the fixed-window v44 result is a CPU negative/noise observation.
The exact-source driver built in 52,316 ms below 3 GiB and preserved the bounded
SHA and wrong-ABI rejection. It reached routine 1,920 at 291,308 ms, 1,254 ms
(0.43%) later than v43, before timing out at 300,682 ms. Peak private/working
set was 202.7/205.0 MB. No routine 1,984, `mir-to-ast:done`, or gen2 file exists.
The single CFG owner and negative ratchet remain useful, but this evidence does
not authorize structural-merge or phi caching as the next CPU track.

`4ee29ce2` closes the next measured routine-lowering seam in the existing
routine-local fact bundle. Its scalar pass records one unique branch global row
per block, and condition, loop-transfer, and match-binding consumers no longer
reconstruct typed instruction views to search each block. The complete input
contains 20,022 blocks, 34,091 instructions, and 8,387 branch blocks; the three
mandatory searches removed at least 77,112 repeated view reconstructions.
Duplicate branches and forged row identity fail closed, and a component ratchet
forbids the old call in `routine_lower.pgy`.

The exact-source v45 driver built in 52,025 ms below 3 GiB, preserved the
414-byte bounded SHA, and rejected the wrong-ABI input without opening output.
The fixed-window run reached routine 1,920 at 288,324 ms and the first routine
1,984 marker at 298,381 ms before timing out at 300,345 ms. Peak
private/working set was 204.8/206.9 MB. That shared 1,920 marker is 2,984 ms
(1.02%) earlier than v44. This remains RED for bootstrap completion: no routine
2,048, `consumer:mir-to-ast:done`, or gen2 file exists.

`99e76e76` closes the remaining explicit whole-instruction phi scan. The
existing routine-local bundle records the leading phi count per block and a
late-phi invalid sentinel. The phi semantic owner reconstructs only those rows;
program-owned kind, predecessor, arity, result, incoming-use, and backedge
checks remain intact. The full artifact view count falls from 34,091 rows to
3,532, and the active 1,984-through-2,048 interval falls from 1,161 rows to 104.
The old all-instruction loop and fallback are statically rejected.

The exact-source v46 driver built in 52,507 ms below 3 GiB, preserved the
414-byte bounded SHA, and rejected the wrong-ABI input without opening output.
The fixed-window run reached routine 1,920 at 293,716 ms before timing out at
300,163 ms with 202.1/204.3 MB peak private/working set. That marker is 5,392
ms (1.87%) later than v45, and v46 did not recover v45's routine-1,984 marker.
This is a CPU negative/noise result rather than a speedup. The owner closure
remains, but the same revision must not be rerun for a favorable sample and the
window/cap must not be enlarged.

`a05aaf06` removes the v46 read-path regression at its exact boundary. The phi
owner admits program-row identity, block counts, and the routine-local bundle
once, reads block prefix counts directly, and rejects invalid counts. The
one-use accessor is deleted. This cuts full-artifact admission from 20,022
block calls to 2,345 routine calls, removing 17,677 admissions and at least
406,571 shape checks without adding a cache or global/local aggregate.

The exact-source v47 driver built in 51,436 ms below 3 GiB, preserved the
414-byte bounded SHA, and rejected the wrong-ABI input without opening output.
The fixed run reached routine 1,920 at 283,594 ms and routine 1,984 at 293,201
ms before timing out at 300,384 ms with 207.7/209.7 MB peak private/working
set. Routine 1,920 is 10,122 ms earlier than v46 and 4,730 ms earlier than v45;
routine 1,984 is 5,180 ms earlier than v45. This is measured CPU progress, but
routine 2,048, `consumer:mir-to-ast:done`, and gen2 output remain absent.

`8074d6c8` moves branch selection from the bundle accessor to the admitted
routine fact index. The branch row stays in the existing bundle, while the new
boundary checks routine/block identity, local/global range, scalar span, and
final program-owned kind. The old accessor is deleted and all three consumers
use the index owner. The full validation-loop lower bound removes 21,910 full
admissions and at least 503,930 shape checks without adding a cache or
aggregate.

The exact-source v48 driver built in 51,479 ms below 3 GiB, preserved the
414-byte bounded SHA, and rejected the wrong-ABI input without opening output.
The fixed run reached routine 1,920 at 285,333 ms and routine 1,984 at 295,075
ms before timing out at 300,615 ms with 206.3/208.3 MB peak private/working
set. Those markers are 1,739 and 1,874 ms later than v47. This is an
owner/fallback closure and CPU negative/noise result, not a speedup. Routine
2,048, `consumer:mir-to-ast:done`, and gen2 output remain absent.

`80a54268` tested the next larger static candidate by replacing
`EmitBlockStmts`' three checked accessors with one block-boundary guard and
direct instruction/scalar construction. Its C/LLVM cross-block negative and
component ratchet passed, but generated-code cost dominated the eliminated
shape checks. The driver build regressed from v48's 51,479 ms to 60,860 ms.
The full run reached routine 1,920 at 293,502 ms, 8,169 ms later than v48, and
lost routine 1,984 before timing out at 300,269 ms. Peak private/working set was
only 202.3/205.0 MB, so this was a CPU/code-shape regression, not memory.

`85cee4ff` reverts that experiment. `git diff 7dd78069..85cee4ff` is empty, so
the v49 revert restored byte-for-byte v48 source while the failed attempt
remains auditable in history. Do not reintroduce the same direct block
aggregate construction or equate lower static check count with lower
generated-program cost.

`530682af` then moved resource runtime ABI top-field capture into every routine
instruction scalar and bundle row. The focused C/LLVM and bounded gates were
green, but the driver build regressed to 62,385 ms. The full run reached only
routine 1,728 at 296,959 ms and timed out at 300,680 ms with 178.2/182.3 MB peak
private/working set. Even the machine routine-index marker moved from v48's
67,567 ms to 80,353 ms, so the regression is broader generated-program cost,
not resource-row validation alone or memory pressure. `c5ee6e62` reverts the
carrier experiment. `5e12cf43` keeps only the independently found correctness
ratchet: a non-resource instruction carrying a stray runtime ABI value now
fails closed, with current-source C/LLVM negatives and the component contract
green.

The focused instruction-writer gate now compares raw, unnormalized
String/file bytes for five small, graph-heavy, match, destructure, and
ABI/optional fixtures through both C and LLVM, then compares C/LLVM file bytes.
It also corrupts instruction row count and proves the sentinel output is not
opened or truncated. The earlier 11,262-byte small fixture SHA remains
`007d5dacdd8157a0d5dd0f87975f82c7abe2fa4987983afb3945bd61b29efc09`.
`FileOpen` failure is observable and fails closed; the current runtime does not
return a `FileWrite` status, so the writer must not claim write-error detection
that the runtime cannot provide.

Broad runs remain explicit RED evidence. `mir_machine_layer_smoke.sh` reaches
the MIR consumer and then fails at the existing `local declaration is missing
its MIR ABI type fact`. `mir_json_parity.sh` expects an enum variant substring
without the current `param_types:[]` field. A filtered `dir_walk` /
`break_after_stmt` attempt stops earlier because reconstructed C lacks current
`PGY_RUNTIME_PANIC` declarations. Update those owners only when their
executable slice is active; none is a green CFG/runtime verdict.
The current focused DRV-2 body attempt also stopped while compiling
`valid_array_builtins` because emitted C omitted `<string.h>` and runtime panic
declarations. The separately isolated `nested_if_in_loop` current-driver run
is green, and a forged one-predecessor header phi is rejected with
`MIR phi facts are missing or inconsistent`; this does not relabel the broad
body gate green.

## Historical observed gates through v60

Green on implementation checkpoint `3418b0f3` plus the retained predecessor
measurements:

- `tests/self_hosted_component_contract_smoke.sh`;
- `tests/self_hosted/parity/driver_rung2_structured_expression_order_owner.sh`;
- `tests/self_host_program_graph_unification_smoke.sh` with
  `phase=unified structural_owners=1`;
- focused native/self `forloop` `mir_json_parity.sh`: range loop-init graph is
  start `0`, range branch graph is stop `3`, and a start-graph regression is
  rejected;
- v60 exact-source and observed driver builds: exit 0 in 69,368/65,293 ms at
  2,480.3/2,575.8 MB peak private;
- v60 bounded consumer: exit 0, 414 LF-normalized bytes, established SHA;
- v60 wrong-ABI and missing/invalid graph mutations: exit 1 with owned
  diagnostics and no output;
- v60 full integration: graph and semantic completion observed before the
  1,800-second timeout in assignment body typing, with 1,130.3/1,041.1 MB peak
  private/working set and no memory-limit crossing;
- `tests/self_hosted/parity/json_bounded_string_owner_smoke.sh` (C/LLVM,
  plain, escaped, empty, and truncated exact-bound strings);
- `tests/self_hosted/parity/mir_program_routine_index_owner_smoke.sh` (C/LLVM,
  partitions, direct-field spans, malformed scalar tails, missing structure,
  corrupted counts, invalid row guards, explicit negative CFG successor
  rejection, missing/unique/duplicate/forged/out-of-block branch-row facts, and
  leading/late/truncated phi-prefix facts, plus invalid match owners,
  zero-block parallel-array misalignment, wrong-kind match rows, match
  name/type count mismatch, forged non-match local arrays, malformed FOR
  scalar rows, invalid branch sentinel, same-endpoint scalar range, and
  no-branch block-span mutation);
- `tests/self_hosted/parity/mir_cfg_graph_query_owner_smoke.sh` (C/LLVM,
  diamond, re-entry, unrestricted-ranking, self-loop, tie, fallback, and
  detached-component witnesses);
- `tests/self_hosted/parity/driver_rung2_mir_abi_layout_negative_owner.sh`;
- `tests/abi_ownership_shape_smoke.sh`;
- `tests/protocol_registry_smoke.sh`;
- `tests/gate_sot_single_owner_smoke.sh`;
- integrated `driver_bootstrap_main.pgy` C build under the 3072 MB pressure
  owner (`full-mir-consumer-loop-branch-owner-v58-build`): exit 0, 60,952 ms,
  2,587.9 MB peak private / 2,577.0 MB peak working set;
- v59 readiness-proof integrated C build: exit 0, 66,274 ms, 2,590.1 MB peak
  private / 2,579.1 MB peak working set;
- v59 bounded MIR consumer: exit 0 in 1,336 ms, 414 bytes, established SHA;
- v59 wrong-ABI mutation: exit 1 in 486 ms with the owned diagnostic and no
  output;
- v59 full completion attempt: MIR-to-AST done at 429,211 ms, fail-closed at
  1,645,538 ms, 801.8/749.4 MB peak private/working set, no gen2;
- v59 surface-count probe: 41,299 surfaces, 35,638 persisted-required lanes,
  1,758 parser-only lanes, proving the flat-root count mismatch;
- bounded MIR consumer byte check: 414 bytes, SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
- bounded wrong-ABI mutation: exit 1 with the owned ABI diagnostic and no
  output file;
- focused current-source resource runtime ABI negatives through C- and
  LLVM-built drivers, including missing/identity/payload/aux rows and a stray
  wrong-kind row on a non-resource instruction;
- `tests/build_pressure_contract_smoke.sh`;
- focused current-driver `nested_if_in_loop` MIR production/consumption plus a
  forged one-predecessor header-phi rejection;
- `tests/self_hosted/parity/module_manifest_resolver_parity.sh` (C/LLVM,
  clean plus malformed/missing manifest negatives);
- `tests/self_hosted/parity/air_graph_json_validator_parity.sh` (C/LLVM,
  clean, missing-key, and live-drift negatives);
- `tests/self_hosted/parity/mir_json_instruction_writer_byte_parity.sh`
  (C/LLVM, five raw String/file and cross-backend fixtures, plus invalid
  pre-open sentinel rejection);
- `instruction-string-pool-ready` pressure shard: exit 0, complete JSON below
  3072 MB;
- `tests/self_hosted/parity/semantic_initializer_environment_cursor_owner_smoke.sh`;
- `tests/self_hosted/parity/semantic_expression_environment_owned_lifetime_smoke.sh`;
- `tests/self_hosted/parity/initializer_projection_probe_parity.sh` (C/LLVM,
  including shadow/exit/destructure positives and self/sibling negatives);
- `tests/self_hosted/parity/driver_rung2_iteration_graph_use_owner.sh`;
- `python scripts/protocol_registry_gate.py`:
  `7 protocol rows valid; no authority duplicated`;
- `python scripts/sot_registry_gate.py`:
  `49 authorities, 41 derived fact carriers; CLOSED=29 BRIDGE=20 ACTIVE=0`;
- `git diff --check` and `git diff --cached --check`.

`tests/self_host_hard_contract_smoke.sh` remains RED at the unrelated existing
manifest assertion that `driver_rung2_owner.pgy` contain
`tests/cases/backend_compare/device_slot_machine_layer/main.pgy`. This was not
weakened or relabeled as success.

The shell gates must use `C:\Program Files\Git\bin\bash.exe` in the current
Windows environment. `C:\Windows\System32\bash.exe` resolves to WSL and fails
because `/bin/bash` is unavailable; that is an execution-environment failure,
not a project gate result.

## Historical temporary artifacts through v60

The current full artifact and driver oracle remain under
`.tmp/instruction_writer_pressure/` because the next executable rung consumes
that exact artifact. The relevant file is
`driver_source_pool.mir.json` (51,807,108 bytes, SHA above). The 40,263,680-byte
`driver_source.mir.json` is the preceding RED partial and must never be used as
input. Pressure evidence remains under
`.tmp/build-pressure/instruction-stream-ready.*` and
`.tmp/build-pressure/instruction-string-pool-ready.*`. Consumer progression is
captured by `full-mir-consumer-admitted.*`,
`full-mir-consumer-exact-bound.*`,
`full-mir-consumer-machine-twofield.*`,
`full-mir-consumer-key-compare.*`, `full-mir-consumer-exact-span.*`, and
`full-mir-consumer-routine-fact-exact.*`,
`full-mir-consumer-routine-indexed.*`, and
`full-mir-consumer-cfg-owner.*`, and
`full-mir-consumer-document-index.*`, and
`full-mir-consumer-program-instruction-index-v3.*`,
`full-mir-consumer-int-cfg-v14-300s.*`, and
`full-mir-consumer-routine-scalar-bundle-v23.*`,
`full-mir-consumer-abi-bounds-v38-300s.*`, and
`full-mir-consumer-abi-row-capture-v39-300s.*`, and
`full-mir-consumer-abi-exact-reuse-v41-300s.*`,
`full-mir-consumer-abi-optional-fast-v42-300s.*`, and
`full-mir-consumer-key-dispatch-v43-300s.*`, and
`full-mir-consumer-cfg-backedge-batch-v44-300s.*`, and
`full-mir-consumer-branch-row-bundle-v45-300s.*`, and
`full-mir-consumer-phi-prefix-bundle-v46-300s.*`, and
`full-mir-consumer-phi-prefix-admission-v47-300s.*`, and
`full-mir-consumer-branch-index-admission-v48-300s.*`. The rejected/reverted
v49 evidence remains under
`full-mir-consumer-block-slice-admission-v49-300s.*`. The rejected/reverted v50
evidence remains under
`full-mir-consumer-resource-raw-capture-v50-300s.*`. The rejected/reverted v51
evidence remains under
`full-mir-consumer-resource-local-scan-v51-300s.*`. The rejected/reverted v52
successor-pair evidence remains under
`full-mir-consumer-block-successor-pair-v52-{build,bounded,wrong-abi,300s,300s-observed}.*`;
only the `300s-observed` run has valid routine-marker evidence. The v53 LLVM
projection evidence remains under
`full-mir-consumer-llvm-performance-v53-{build,bounded,wrong-abi,300s-observed}.*`.
The v54 explicit clang-via-C evidence remains under
`full-mir-consumer-c-clang-v54-{build,bounded,wrong-abi,300s-observed}.*`.
The rejected v55 local-call evidence remains under
`full-mir-consumer-json-ascii-constants-v55-{build,bounded,wrong-abi,300s-observed}.*`.
The rejected v56 evidence remains under
`full-mir-consumer-match-owner-filter-v56-{build,bounded,wrong-abi,300s-observed}.*`;
its adjacent unchanged-source control is
`full-mir-consumer-v48-current-control-300s-observed.*`. The accepted v57
evidence remains under
`full-mir-consumer-match-routine-owner-v57-{build,bounded,wrong-abi,300s-observed}.*`.
The adjacent v57 control for v58 is
`full-mir-consumer-match-routine-owner-v57-adjacent-v58-control-300s-observed.*`.
The accepted v58 evidence is
`full-mir-consumer-loop-branch-owner-v58-{build,bounded,wrong-abi,300s-observed}.*`;
its focused LLVM build is
`mir-lower-loop-branch-owner-v58-llvm-build.*`.
The first completion continuation is
`full-mir-consumer-loop-branch-owner-v58-integration-completion.*`; it reached
expression graph construction and stopped at the 3,072 MB cap. v59 evidence is
`full-mir-consumer-expression-arena-linear-v59-{integration-completion}.*` and
`full-mir-consumer-expression-arena-linear-v59-ready-proof-{build,bounded,wrong-abi}.*`.
v60 evidence is
`full-mir-consumer-structured-occurrence-v60-{build,observed-build,integration}.*`.
The current diagnostic executables are
`.tmp/self_hosted/driver_bootstrap/driver_rung2_v60_structured_occurrence.exe`
and
`.tmp/self_hosted/driver_bootstrap/driver_bootstrap_v60_structured_occurrence.exe`.
The temporary count
probe source/executable were deleted after their result was recorded; its
pressure evidence remains under `v59-expression-surface-count-probe-full.*`.
The latest full consumer evidence passes the former 35,638-vs-34,962
positional mismatch, completes graph construction and semantic analysis below
1,131 MB private, and times out at assignment body typing. The requested
`v60_full.c` does not exist because output is committed only after verified
completion. The rejected v50
executable is
`.tmp/self_hosted/driver_bootstrap/driver_rung2_v50_resource_raw_capture.exe`;
its 414-byte bounded result is
`.tmp/self_hosted/driver_bootstrap/v50_bounded.c`. These files are diagnostic
evidence only, not semantic authority or commit content.

## Historical v60 next executable work

1. The resource ABI and block-successor pair read seams are abandoned. Their
   focused correctness gates passed, but their carrier/local-scan/pair shapes
   materially regressed generated-driver CPU. Do not try another representation
   of either read consolidation.
2. The accepted-source LLVM v53 projection is connected and semantically
   byte-equal, but it is slower than C v48 and reaches only routine 1,856 in the
   fixed window. Keep LLVM's general performance-primary direction, but do not
   use the current LLVM-built DRV-2 as the active bootstrap executable and do
   not change semantics to make that positioning claim pass.
3. The explicit clang-via-C v54 projection improves integrated driver build
   time but is runtime negative/noise against GCC v48 and produces no gen2.
   Keep the existing Windows GCC-first default and do not confuse host compile
   speed with generated compiler progress.
4. The v55 JSON ASCII experiment removed the expected generated calls, but
   routine 1,920 regressed by 5,779 ms and routine 1,984 was lost. It is
   reverted. Do not retry literal constants, a shared ASCII helper, backend
   intrinsics, or unchecked character access; the static call-count hypothesis
   did not identify an integrated dominant cost.
5. The v56 match-local filter is reverted because its extra alignment pass
   regressed adjacent-v48 normalized markers. Accepted v57 directly consumes
   the routine-index owner; accepted v58 then consumes each loop-projection
   branch row once and improves every adjacent-v57 normalized marker through
   routine 1,728. Keep both closed shapes; do not add a third match-local read,
   a second branch pass, or rendered-condition fallback. v58 still produces no
   gen2, so count it as owner closure and generated-driver CPU improvement, not
   hard substitution progress or completion.
6. v60 closes the structured graph occurrence seam. Keep its repeated-key
   semantics, one final arena, producer coverage, deleted sequence view, and
   native range-stop producer ratchet. Do not reopen raw positional pairing,
   text lookup, deduplication, or a second graph/order.
7. The active seam is `SemanticAstAssignmentTypeFactsFromArtifact`, entered at
   `semantic-body-type-stage assignment:start` after graph and semantic
   completion. Add narrow stage/row evidence only as needed to locate repeated
   owned work; do not start a broad assignment fixture campaign.
8. Rerun the same complete artifact under the unchanged 1,800-second / 3,072 MB
   pressure gate. Acceptance for this slice is `assignment:done`, or one exact
   assignment row, owner read, and falsifying case if it still cannot finish.
9. Continue the same run through statement/body verification. If it emits a
   complete `driver_gen2.c`, compile that C as the bootstrap object-code
   boundary; do not regenerate another oracle MIR.
10. Make the generated gen2 driver consume the same complete compiler source
   and emit `driver_gen3.c`. Do not divert into global SoT closure or fixture
   expansion; close only a concrete owner seam that blocks this exact run.
11. Compare complete gen2/gen3 artifacts and behavior. Use the existing bounded
   MIR fixture only as a focused falsifier when diagnosing a failure on this
   path, not as an independent breadth campaign.
12. Keep the separate foreach assignment-binding, ABI-type, stale enum-parity,
   and reconstructed-runtime-header failures out of this active CPU seam. Do
   not raise the fixed integration time or memory limits as a substitute for
   closing the owner path.

## Historical v60 resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, `src/self_hosted/OWNERS.md`,
   `docs/180_compiler_logical_spine_handles_gates.md`, and
   `docs/semantics/sot_owner_spine_registry.md`.
2. Verify HEAD/origin, `git status --short --branch`, and the three protected
   dirty files above.
3. Re-run the component, structured-expression-order, program-graph, and
   focused native `forloop` MIR parity gates through Git Bash before a broad
   build.
4. Confirm no unrelated `pgy`, `genN`, `driver_oracle`, `gcc`, `cc1`, or
   `clang` process is active before the pressure gate; concurrent broad builds
   invalidate attribution.
5. Continue the v60 executable on the same frozen MIR and fixed
   1,800-second/3,072 MB pressure gate; the first required marker is
   `semantic-body-type-stage assignment:done`.
6. Treat current source, registries, and executable gates as authoritative if
   this snapshot disagrees with them.

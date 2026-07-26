# Current Work Handoff

Updated: 2026-07-26 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. Verify it against the
current source, `git status --short --branch`, the SoT registries, the named
owner, and the named executable gate.

## Execution directive: gen2 takeover before global SoT closure

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

## Deferred Coq gap audit (after gen2 takeover)

Do not start a broad proof expansion before the gen2 takeover above. The
2026-07-26 source audit found 38 registered `.v` files, no `Admitted` or Coq
`Axiom`, and only the two declared `SlotCalculus` interface parameters
(`MaxSlotId` and `verify_token`). The dedicated CI kernel gate is designed to
compile all 38 files and reject any larger axiom budget. No local Coq/Rocq
binary was available during this audit, so the kernel gate was not rerun here.

The important proof gaps are refinement gaps, not unfinished `Qed` blocks:

1. existing small models are not comprehensively bound to the live
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

## Resume checkpoint

- Implementation checkpoint: `6879f0c0` (revert rejected v51 resource local
  scan) on `main`. The rejected v51 implementation is `e6abdeaa`; the rejected
  v50 carrier is `530682af`, reverted by `c5ee6e62`. Accepted compiler source
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
- Pergyra semantic and canonical MIR facts remain the SoT. C is the primary
  execution/bootstrap projection; LLVM is the independent differential and
  optional performance projection over the same facts. The Pergyra-built
  DRV-2 is still a bounded self-host replacement lane; this checkpoint does
  not claim a fully self-hosted driver or a Pergyra-owned LLVM emitter. It does
  establish the first complete current full-driver MIR artifact below 3072 MB.
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
- The same routine-local bundle carries each block's leading phi count. A phi
  after the first non-phi is an invalid sentinel. The phi semantic owner scans
  only that prefix while retaining predecessor, arity, result, incoming-use,
  and backedge validation. It admits routine identity, exact block counts, and
  bundle shape once at entry, then directly reads the prefix array. The one-use
  per-block accessor is deleted. Missing/invalid prefix facts cannot fall back
  to a whole-block scan or JSON kind recovery.
- A direct `EmitBlockStmts` block-slice experiment passed its fail-closed gates
  but regressed the fixed run by 8,169 ms at routine 1,920 and lost routine
  1,984. It is explicitly reverted. Current source uses the accepted v48
  accessor path; the failed v49 shape is evidence, not an active fallback.
- A later resource-runtime experiment captured four top-level fact families in
  every instruction scalar and expanded the routine bundle. It removed about
  145.6 MB of repeated resource top-span reading by static estimate but built
  in 62,385 ms and reached only routine 1,728 at 296,959 ms. `c5ee6e62`
  reverts it. The review-discovered stray wrong-kind runtime row fail-open is
  retained alone in `5e12cf43`; a non-resource instruction can no longer treat
  an explicit runtime row as absence.

## Exact dirty state

At the handoff checkpoint represented by this file, `main` and `origin/main`
are synchronized and no task-owned implementation or documentation change is
dirty. These unstaged files are concurrent user work and must remain
unmodified and excluded from task commits:

- `docs/117_backend_strategy_positioning.md`;
- `docs/20_compiler_pipeline_guide.md`;
- `docs/51_c_backend_reference_policy.md`;
- `docs/52_llvm_native_first_roadmap.md`;
- `docs/self_hosted/13_compiler_substrate_architecture.md`;
- `docs/semantics/16_language_contract_golden_spine.md`;
- `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`.

## Active executable objective card

- Objective: finish MIR-to-AST lowering for the completed admitted full-driver
  MIR artifact, emit and compile the integrated gen2 driver, and immediately
  make gen2 consume the same complete compiler source to emit gen3.
- Priority: preserve the exact `pgy.mir.v1` artifact identity, keep the MIR
  consumer and semantic owners fail closed, stay below the fixed pressure cap,
  complete the gen2 takeover, then establish the fixed point. Do not widen SoT
  or fixtures before that takeover.
- Fact owner: the verified `SelfMirProgramFacts` producer and its completed
  `pgy.mir.v1` artifact. `MirMachineLayerAdmittedJsonInput` carries the one
  machine proof, declaration, and routine span inventory; consumers may not
  reconstruct a second producer or machine authority. `MirProgramRoutineIndex`
  remains a derived structural view, not another semantic owner.
- Last legitimate consumer: current `driver_oracle.exe --mir-json` emitting
  `driver_gen2.c`, followed by the native C compiler only as the bootstrap
  object-code boundary.
- Forbidden fallback: regenerating a native oracle MIR per generation,
  backend-specific JSON reads, source-text fact recovery, process-sharded fact
  stores, per-routine whole-program structure revalidation, `new ? old`
  authority, or raising the 3072 MB cap.
- Focused falsifier: continue the current source beyond its observed
  `top-level-routines:1984` marker and reach routine 2,048, or expose one named
  routine/owner failure. Do not open the later expression-graph or assignment
  post-pass until `consumer:mir-to-ast:done` is observed. Even 2,048 of 2,345
  top-level routines would be only a sentinel, not gen2.
- Acceptance gate: pressure-owned full MIR consumption emits `driver_gen2.c`,
  that artifact builds, and the resulting gen2 consumes the same complete
  compiler source to emit `driver_gen3.c`. The bounded preflight remains a
  focused diagnostic, not a prerequisite track that may delay this takeover;
  compare gen2/gen3 only after both complete artifacts exist.

## Latest measured evidence

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

## Last observed gates

Green on current checkpoint `5e12cf43` plus the accepted v48 measurements:

- `tests/self_hosted_component_contract_smoke.sh`;
- `tests/self_hosted/parity/json_bounded_string_owner_smoke.sh` (C/LLVM,
  plain, escaped, empty, and truncated exact-bound strings);
- `tests/self_hosted/parity/mir_program_routine_index_owner_smoke.sh` (C/LLVM,
  partitions, direct-field spans, malformed scalar tails, missing structure,
  corrupted counts, invalid row guards, explicit negative CFG successor
  rejection, missing/unique/duplicate/forged/out-of-block branch-row facts, and
  leading/late/truncated phi-prefix facts);
- `tests/self_hosted/parity/mir_cfg_graph_query_owner_smoke.sh` (C/LLVM,
  diamond, re-entry, unrestricted-ranking, self-loop, tie, fallback, and
  detached-component witnesses);
- `tests/self_hosted/parity/driver_rung2_mir_abi_layout_negative_owner.sh`;
- `tests/abi_ownership_shape_smoke.sh`;
- `tests/protocol_registry_smoke.sh`;
- `tests/gate_sot_single_owner_smoke.sh`;
- integrated `driver_bootstrap_main.pgy` C build under the 3072 MB pressure
  owner (`full-mir-consumer-branch-index-admission-v48-build`): exit 0, 51,479 ms,
  2,567.8 MB peak private / 2,557.0 MB peak working set;
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

## Temporary artifacts

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
`full-mir-consumer-resource-local-scan-v51-300s.*`. The current accepted
executable is
`.tmp/self_hosted/driver_bootstrap/driver_rung2_v48_branch_index_admission.exe`; its
414-byte bounded result is
`.tmp/self_hosted/driver_bootstrap/v48_bounded.c`. The latest accepted-source
full consumer evidence reaches routine 1,984 at 295,075 ms. The rejected v50
executable is
`.tmp/self_hosted/driver_bootstrap/driver_rung2_v50_resource_raw_capture.exe`;
its 414-byte bounded result is
`.tmp/self_hosted/driver_bootstrap/v50_bounded.c`. These files are diagnostic
evidence only, not semantic authority or commit content.

## Next executable work

1. The resource ABI read seam is abandoned. Both v50's expanded carrier and
   v51's ephemeral local scan passed correctness gates but materially regressed
   fixed-window CPU progress. Do not add a third resource carrier, guard, or
   scan shape.
2. Combine the two direct block-object reads for `succ_true` and `succ_false`
   into one exact, order-independent pair capture in the existing JSON fact
   transport owner. `MirRoutineFactIndex.block_succ_true/block_succ_false`
   remain the semantic owner and last consumers remain graph/backedge/merge/
   phi. Do not add a program-global carrier, second graph, backend branch,
   field-order dependency, missing-to-zero guess, or old two-read fallback.
3. Preserve missing-edge `-1` only for absence. Duplicate, malformed, explicit
   negative, and out-of-range successor values must fail at `cfg_successor`.
   Focused current-source C/LLVM gates must cover reordered fields, missing one
   or both fields, string-valued numbers, duplicate keys, explicit negative,
   and out-of-range targets. A static ratchet must reject the old two lookups.
   The quantified full input has 20,022 blocks: one pass removes 20,022 object
   scans and about 49.5 million character visits overall, including about 1.8
   million visits in routines 1,984 through 2,048.
4. Run the same bounded/wrong-ABI/full ladder once. Accept only if semantics
   remain byte-equal and shared markers do not materially regress from v48;
   otherwise revert before choosing another measured owner seam.
5. Continue the same admitted artifact until `consumer:mir-to-ast:done` is
   observed. Post-loop markers for top-level completion, string join, and AST
   inventory must distinguish projected completion from an observed result.
6. Continue that same run to emit `driver_gen2.c`, then compile that C as the
   bootstrap object-code boundary; do not regenerate another oracle MIR.
7. Make the generated gen2 driver consume the same complete compiler source
   and emit `driver_gen3.c`. Do not divert into global SoT closure or fixture
   expansion; close only a concrete owner seam that blocks this exact run.
8. Compare complete gen2/gen3 artifacts and behavior. Use the existing bounded
   MIR fixture only as a focused falsifier when diagnosing a failure on this
   path, not as an independent breadth campaign.
9. Keep the ABI-type, stale enum-parity, and reconstructed-runtime-header
   failures separate from this active CPU seam; do not raise either the
   300-second diagnostic window or 3072 MB memory cap as a substitute for
   closing the owner path.

## Resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, `src/self_hosted/OWNERS.md`,
   `docs/180_compiler_logical_spine_handles_gates.md`, and
   `docs/semantics/sot_owner_spine_registry.md`.
2. Verify HEAD/origin, `git status --short --branch`, and the three protected
   dirty files above.
3. Re-run the component and raw instruction-writer byte gates through Git Bash
   before a broad build.
4. Confirm no unrelated `pgy`, `genN`, `driver_oracle`, `gcc`, `cc1`, or
   `clang` process is active before the pressure gate; concurrent broad builds
   invalidate attribution.
5. Treat current source, registries, and executable gates as authoritative if
   this snapshot disagrees with them.

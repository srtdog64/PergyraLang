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

## Resume checkpoint

- Implementation checkpoint: `a5d56f42` (`optimize self-hosted MIR ABI
  validation`) on `main`. Its routine-scalar predecessor is `dd68d6f3`, its
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
- C and LLVM remain peer native compiler backends. The Pergyra-built DRV-2 is
  still a bounded self-host replacement lane; this checkpoint does not claim a
  fully self-hosted driver or a Pergyra-owned LLVM emitter. It does establish
  the first complete current full-driver MIR artifact below 3072 MB.
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

## Exact dirty state

At the handoff checkpoint represented by this file, `main` and `origin/main`
are synchronized and no task-owned implementation or documentation change is
dirty. These three unstaged files are concurrent user work and must remain
unmodified and excluded from task commits:

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
- Focused falsifier: continue the same admitted artifact beyond the observed
  `top-level-routines:128` marker and either reach the next 64-routine marker or
  expose one named routine/owner failure. Do not open the later expression-
  graph or assignment post-pass until `consumer:mir-to-ast:done` is observed.
  Even 128 is only a progress sentinel, not gen2.
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

Green on `a5d56f42` plus the documented working measurements:

- `tests/self_hosted_component_contract_smoke.sh`;
- `tests/self_hosted/parity/json_bounded_string_owner_smoke.sh` (C/LLVM,
  plain, escaped, empty, and truncated exact-bound strings);
- `tests/self_hosted/parity/mir_program_routine_index_owner_smoke.sh` (C/LLVM,
  partitions, direct-field spans, malformed scalar tails, missing structure,
  corrupted counts, invalid row guards, and explicit negative CFG successor
  rejection);
- `tests/self_hosted/parity/mir_cfg_graph_query_owner_smoke.sh` (C/LLVM,
  diamond, re-entry, unrestricted-ranking, self-loop, tie, fallback, and
  detached-component witnesses);
- `tests/abi_ownership_shape_smoke.sh`;
- `tests/protocol_registry_smoke.sh`;
- `tests/gate_sot_single_owner_smoke.sh`;
- integrated `driver_bootstrap_main.pgy` C build under the 3072 MB pressure
  owner (`full-mir-consumer-abi-owner-v40-build`): exit 0, 55,007 ms,
  2,565.3 MB peak private / 2,554.5 MB peak working set;
- bounded MIR consumer byte check: 414 bytes, SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
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
`full-mir-consumer-abi-row-capture-v39-300s.*`. The current executable is
`.tmp/self_hosted/driver_bootstrap/driver_rung2_v40_abi_owner.exe`; its
414-byte bounded result is
`.tmp/self_hosted/driver_bootstrap/v40_bounded.c`. The latest full consumer
evidence includes complete stage capture through the 640-routine marker. These
files are diagnostic evidence only, not semantic authority or commit content.

## Next executable work

1. Profile the first range after marker 640 using the current ABI,
   instruction, and CFG owners. The next falsifier is marker 704. If exact ABI
   tuple reuse is introduced, compare the complete raw/canonical tuple; never
   authorize a cache hit by the 28-bit layout ID alone.
2. Re-run the current artifact until marker 704 and ultimately
   `consumer:mir-to-ast:done` are observed under the fixed pressure owner.
3. Continue that same run to emit `driver_gen2.c`, then compile that C as the
   bootstrap object-code boundary; do not regenerate another oracle MIR.
4. Make the generated gen2 driver consume the same complete compiler source
   and emit `driver_gen3.c`. Do not divert into global SoT closure or fixture
   expansion; close only a concrete owner seam that blocks this exact run.
5. Compare complete gen2/gen3 artifacts and behavior. Use the existing bounded
   MIR fixture only as a focused falsifier when diagnosing a failure on this
   path, not as an independent breadth campaign.
6. Keep the ABI-type, stale enum-parity, and reconstructed-runtime-header
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

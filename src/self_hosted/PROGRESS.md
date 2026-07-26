# Self-Host Progress

2026-07-26 measured the accepted C projection with the available Windows clang
host compiler as v54 (`PGY_CC=clang --target=x86_64-w64-mingw32`). No Pergyra
source, backend meaning, runtime contract, or default toolchain selection
changed. The integrated driver built successfully in 42,649 ms at 2,557.6 MB
peak private / 2,546.5 MB working set, 8,830 ms faster than the GCC-built v48.
Its 1,470 ms bounded result remained 414 bytes with the established SHA;
wrong ABI exited 1 in 1,486 ms with the owned diagnostic and no output.

The observed full run did not improve executable progress. Machine routine-
index admission completed at 68,635 ms and routines 704/896/1,600/1,920/1,984
completed at 160,553/188,638/237,074/286,528/296,279 ms. It timed out at
300,665 ms with 206.0 MB peak private / 208.0 MB working set and no routine
2,048, `consumer:mir-to-ast:done`, or gen2 output. Routine 1,984 was 1,204 ms
later than v48, so record v54 as a host build-time win and runtime negative/
noise result. Keep GCC v48 as the accepted executable baseline and retain the
documented Windows default compiler choice.

The backend-neutral JSON ASCII hypothesis was implemented as `2eeeec13` and
reverted by `1f77b0bc`. Focused C/LLVM and component gates passed. Disassembly
confirmed that `JsonSkipWhitespaceWithin` went from up to five `CharCode` calls
per examined byte to one checked input call plus a constant membership test,
and `JsonIsDigitCode` became a call-free `48..57` range check.

The local code-generation win did not improve the complete artifact. v55 built
in 51,536 ms at 2,516.9 MB peak private / 2,505.4 MB working set. Its bounded
result completed in 1,545 ms and preserved the established 414-byte SHA;
wrong ABI exited 1 in 1,529 ms with the owned diagnostic and no output. The
observed full run reached routines 704/896/1,600/1,728/1,792/1,856/1,920 at
162,958/191,199/240,394/256,094/270,606/285,090/291,112 ms, then timed out at
300,480 ms with 202.9/205.3 MB private/working set and no routine 1,984 or
gen2. Routine 1,920 was 5,779 ms later than v48. Keep the accepted scanner and
select the next executable seam from integrated profiling or owned stage
evidence rather than static helper call counts.

2026-07-26 measured the accepted source through the declared LLVM performance
projection as v53. No source or semantic owner changed. The integrated LLVM
driver built successfully in 139,295 ms at 2,399.0 MB peak private / 2,389.0
MB working set. Its bounded MIR result completed in 2,625 ms, remained 414
bytes, and preserved SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
Wrong ABI exited 1 with the same owned diagnostic and no output. This proves
the current LLVM projection is connected to the integrated DRV-2 path and
preserves the focused contract.

The observed full run was not a performance replacement. Machine routine-index
admission completed at 73,014 ms and routines 704/896/1,600/1,728/1,792/1,856
completed at 172,586/202,127/250,313/267,008/280,841/295,125 ms. It timed out
at 300,518 ms with 214.0 MB peak private / 210.8 MB working set and no routine
1,920/1,984/2,048, `consumer:mir-to-ast:done`, or gen2 output. The same accepted
source therefore runs this workload more slowly through the current LLVM
projection than through C v48. This does not change the general LLVM
performance-primary strategy, but it forbids claiming the present self-host
driver as its realized performance path. Keep C v48 as the accepted executable
baseline and do not change semantic owners to make the LLVM sample win.

2026-07-26 rejected block-successor pair capture (`8c49f74f`, reverted by
`40037e52`). The experiment replaced the two direct `succ_true`/`succ_false`
block reads with one order-independent capture in the existing MIR JSON fact
transport owner. `MirRoutineFactIndex` remained the fact owner; no second graph,
global carrier, cache, backend branch, or old-read fallback was added. Focused
current-source C/LLVM and component gates passed for reordered fields, one or
both missing fields, string-valued numbers, plain and escaped semantic
duplicates, explicit negative values, and out-of-range targets. Missing alone
kept the internal negative sentinel; all present invalid rows failed at
`cfg_successor`.

The v52 driver built in 67,265 ms at 2,591.5 MB peak private / 2,580.9 MB
working set, 15,786 ms slower than v48. Its 1,704 ms bounded result remained
414 bytes with SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
wrong ABI exited 1 with the owned diagnostic and no output. The correctly
observed fixed run completed machine routine-index admission at 83,531 ms,
reached routine 704 at 198,093 ms, 896 at 233,293 ms, 1,600 at 291,565 ms,
and 1,664 at 298,472 ms, then timed out at 300,560 ms with only 172.9 MB peak
private / 176.6 MB working set. Against v48, machine admission was 15,964 ms
later and routine 704 was 39,276 ms later. No routine 1,728/2,048,
`consumer:mir-to-ast:done`, or gen2 output exists. This is another generated-
code CPU regression, not memory pressure. The successor pair seam is abandoned;
do not try a second pair carrier/parser shape. Accepted performance evidence
returns to v48 plus the isolated stray-row fail-closed correction.

2026-07-26 rejected the second and final resource-runtime read-shape
experiment (`e6abdeaa`, reverted by `6879f0c0`).
`MirResourceRuntimeRowFactReady` replaced four independent top-level object
lookups with one ephemeral local scan while keeping ABI meaning in the same
owner. No carrier, array, cache, helper file, program-global aggregate, or
backend branch was added. Expanded current-source C/LLVM gates covered
markerless and explicit-`true` rows, escaped keys, duplicate semantic keys,
wrong-kind/`false` required markers, null/non-string/duplicate names, stray
rows, and auxiliary-table failures. Focused gates, the component ratchet,
bounded output, and the wrong-ABI negative all passed.

The v51 driver built in 56,417 ms at 2,576.8 MB peak private / 2,565.8 MB
working set. Its 1,408 ms bounded result remained 414 bytes with SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
wrong ABI exited 1 with the owned diagnostic and no output. The fixed full run
reached routine 704 at 173,196 ms, 896 at 204,052 ms, 1,600 at 255,976 ms,
1,728 at 272,517 ms, and 1,792 at 287,519 ms. It timed out at 300,614 ms with
192.6 MB peak private / 195.6 MB working set and did not reach routine 1,856,
1,984, 2,048, `consumer:mir-to-ast:done`, or gen2 output. This again materially
regressed v48's CPU markers without memory pressure. The resource read seam is
therefore abandoned after its carrier and local-scan shapes; do not attempt a
third variant. Accepted performance evidence remains v48 plus the isolated
stray-row fail-closed correction. The next executable seam is the two block
successor reads already owned by `MirRoutineFactIndex`.

2026-07-26 rejected resource-runtime raw scalar carriage (`530682af`, reverted
by `c5ee6e62`). The experiment captured instruction `name` plus exact unique
`runtime_call_abi_required`, `runtime_call_abi`, and `runtime_call_abi_aux`
bounds in the existing scalar pass and carried them through the routine-local
bundle. Resource ABI meaning remained in its existing owner, and the old top
instruction-span reads were removed. Focused routine-index C/LLVM, resource
negative C/LLVM, component, bounded, and wrong-ABI gates passed. Independent
review also found an existing fail-open for a stray wrong-kind runtime row on a
non-resource instruction; that smaller correctness fix survives separately as
`5e12cf43` with a C/LLVM negative ratchet.

The exact-source v50 driver built in 62,385 ms at 2,445.2 MB peak private /
2,438.9 MB working set, slower than v48's 51,479 ms build. Bounded output was
still 414 bytes with SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`
and completed in 609 ms; wrong ABI exited 1 with the owned diagnostic and no
output. The fixed full run reached routine 704 at 189,951 ms, routine 896 at
222,884 ms, routine 1,600 at 279,085 ms, and routine 1,728 at 296,959 ms. It
timed out at 300,680 ms with only 178.2 MB peak private / 182.3 MB working set.
This is a material CPU/generated-code-shape regression, not memory pressure.
No routine 1,792/2,048, `consumer:mir-to-ast:done`, or gen2 output exists.
Current accepted performance evidence therefore remains v48 plus the isolated
stray-row fail-closed correction; do not restore the expanded per-instruction
runtime carrier merely from its repeated-byte estimate.

2026-07-26 rejected block-local direct traversal (`80a54268`, reverted by
`85cee4ff`). The experiment made `EmitBlockStmts` admit a complete block slice
once and construct instruction/scalar views directly, with a focused C/LLVM
cross-block negative and no fallback. Although this removed an estimated
1,202,928 repeated shape checks, it expanded the block-boundary guard and
direct generated-code construction enough to regress both build and full-run
wall time. The exact-source v49 driver built in 60,860 ms at 2,587.7 MB peak
private / 2,578.1 MB working set. Bounded output remained 414 bytes with the
established SHA; wrong ABI still exited 1 with no output.

The fixed full run reached routine 704 at 166,252 ms, routine 896 at 194,769
ms, routine 1,600 at 243,264 ms, and routine 1,920 at 293,502 ms. It timed out
at 300,269 ms with 202.3 MB peak private / 205.0 MB working set, no routine
1,984 or 2,048, `consumer:mir-to-ast:done`, or gen2 output. Routine 1,920 was
8,169 ms (2.86%) later than v48 and the current 1,984 marker was lost. This is
larger than fixed-window noise, so `85cee4ff` explicitly restores the v48
source instead of retaining a structurally plausible performance regression.
Do not repeat this direct-construction shape or count static `ArrayLength`
removal as a speedup. Current accepted evidence remains v48 and routine 2,048
remains the next falsifier.

2026-07-26 admitted routine-index branch selection (`8074d6c8`). Branch global
row ownership remains in `MirRoutineInstructionFactBundle`, while selection now
validates through `MirRoutineFactIndexBranchAtBlock`. The accessor requires an
admitted index, exact routine/block identity, local/global instruction range,
carried scalar span equality, and final program-owned `kind=branch`. The old
bundle accessor and all three consumers are removed rather than retained as a
fallback. A missing sentinel remains valid/not-found; other negative,
out-of-block, forged-kind, or inconsistent rows fail closed. No instruction
scan, JSON kind read, new cache/global aggregate, or backend split was added.

The focused C/LLVM routine-index and component gates pass, including the new
out-of-block carrier negative; the index owner remains at its 600-line cap. The
exact-source v48 driver built in 51,479 ms at 2,567.8 MB peak private / 2,557.0
MB working set. Its bounded run finished in 1,513 ms with the established
414-byte SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
the wrong-ABI input exited 1 with the owned diagnostic and no output. The fixed
full run reached routine 704 at 158,817 ms, routine 896 at 187,672 ms, routine
1,600 at 235,166 ms, routine 1,920 at 285,333 ms, and routine 1,984 at 295,075
ms. It timed out at 300,615 ms with 206.3 MB peak private / 208.3 MB working
set, no routine 2,048, `consumer:mir-to-ast:done`, or gen2 output. Routine
1,920 and 1,984 are 1,739 and 1,874 ms later than v47. Record the exact owner
and fallback closure, but treat the fixed-window result as CPU negative/noise,
not a speedup. Routine 2,048 remains the next falsifier.

2026-07-26 routine-boundary phi-prefix admission (`a05aaf06`). The v46 prefix
consumer called a shape-validating accessor once per block, repeating routine
row and bundle admission 20,022 times across 2,345 routines. The phi owner now
admits row identity, exact block counts, and the routine-local bundle once at
entry, then reads `block_phi_prefix_counts` directly. The one-use accessor is
deleted rather than retained as C-style helper fragmentation. Negative,
truncated, or oversized prefix facts fail closed; the old instruction scan,
per-block re-admission, JSON kind recovery, and backend split remain forbidden.

The focused C/LLVM routine-index and component gates pass, including a
truncated prefix carrier negative. The exact-source v47 driver built in 51,436
ms at 2,535.7 MB peak private / 2,524.3 MB working set. Its bounded run finished
in 1,410 ms with the established 414-byte SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
the wrong-ABI input exited 1 with the owned diagnostic and no output. The fixed
full run reached routine 704 at 158,438 ms, routine 896 at 186,805 ms, routine
1,600 at 234,127 ms, routine 1,920 at 283,594 ms, and routine 1,984 at 293,201
ms. It timed out at 300,384 ms with 207.7 MB peak private / 209.7 MB working
set, no routine 2,048, `consumer:mir-to-ast:done`, or gen2 output. Routine
1,920 is 10,122 ms earlier than v46 and 4,730 ms earlier than v45; routine
1,984 is 5,180 ms earlier than v45. This is measured executable CPU progress.
The next fixed-window falsifier remains routine 2,048.

2026-07-26 routine-local phi-prefix carriage (`99e76e76`). The existing
`MirRoutineInstructionFactBundle` scalar pass now records the leading phi count
for every block and records a negative sentinel when a phi appears after the
first non-phi instruction. `MirRoutinePhiFactsReady` consumes only that prefix;
the old whole-block instruction-count loop and its late-phi rescan state are
statically rejected. Phi predecessor, arity, result, incoming-use, and backedge
semantics remain in `phi_fact_owner.pgy`. No separate cache, program-global
scalar aggregate, JSON kind fallback, or backend split was added.

The focused routine-index C/LLVM parity and component contract gates pass. The
exact-source v46 driver built in 52,507 ms at 2,556.9 MB peak private / 2,546.0
MB working set. Its bounded run finished in 1,442 ms and preserved the 414-byte
SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
the wrong-ABI input exited 1 with the owned diagnostic and no output. The fixed
300-second full consumer reached routine 704 at 163,937 ms, routine 896 at
193,024 ms, routine 1,600 at 242,500 ms, and routine 1,920 at 293,716 ms. It
timed out at 300,163 ms with 202.1 MB peak private / 204.3 MB working set, no
routine 1,984 or 2,048, `consumer:mir-to-ast:done`, or gen2 output. Although the
full artifact reduces phi view reconstruction from 34,091 rows to 3,532, the
shared routine-1,920 marker is 5,392 ms (1.87%) later than v45. Record this as
an owner/fallback closure and CPU negative/noise result, not a speedup. Keep the
next fixed-window falsifier at routine 2,048 without rerunning v46 for a more
favorable sample.

2026-07-26 routine-local branch row carriage (`4ee29ce2`). The existing
`MirRoutineInstructionFactBundle` scalar pass now records the unique branch
global row for each block. `BlockCond`, `BlockHasLoopTransfer`, and
`BlockMatchBindingLine` consume that fact instead of searching every block for
`kind=branch`. A missing branch remains an explicit valid/not-found result;
duplicate branches, a row outside the block, a scalar-span mismatch, or a row
whose program-owned kind is not `branch` fail closed. The old routine-lowering
search calls are statically rejected. No program-global scalar aggregate, new
file/cache, JSON fallback, or backend split was added.

The exact-source v45 driver built in 52,025 ms at 2,534.1 MB peak private /
2,522.6 MB working set. Its bounded run finished in 1,487 ms and preserved the
414-byte SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
the wrong-ABI input exited 1 with the owned diagnostic and no output. The fixed
300-second full consumer reached routine 704 at 161,510 ms, routine 896 at
189,756 ms, routine 1,600 at 238,576 ms, routine 1,920 at 288,324 ms, and the
new routine-1,984 falsifier at 298,381 ms. It timed out at 300,345 ms with
204.8 MB peak private / 206.9 MB working set, no routine 2,048,
`consumer:mir-to-ast:done`, or gen2 output. Routine 1,920 is 2,984 ms (1.02%)
earlier than v44 and 1,730 ms earlier than v43. The next fixed-window
falsifier is routine 2,048.

2026-07-26 routine-level CFG backedge batch (`73133678`, negative fixture
`ec4b9eef`). The existing CFG graph owner now computes entry reachability once
and one avoiding traversal per reachable distinct incoming target, then marks
backedge headers with target-major source checks. `BuildMirRoutineFactIndex`
consumes that single routine result. The old per-edge
`MirRoutineEdgeTargetsDominator` definition is deleted and statically rejected;
structural merge and phi ownership are unchanged. Malformed array/target facts
return an empty typed result and the consumer exposes `cfg_backedge` instead of
guessing an all-zero header view. C/LLVM gates cover disconnected cycles,
self-loops, ordinary loops, duplicate incoming targets, numeric earlier merges,
and invalid inputs.

The exact-source v44 driver built in 52,316 ms at 2,433.5 MB peak private /
2,427.0 MB working set. Its bounded run finished in 1,425 ms and preserved the
414-byte SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
the wrong-ABI input exited 1 with the owned diagnostic and no output. The fixed
300-second full consumer reached routine 704 at 162,403 ms, routine 896 at
191,236 ms, routine 1,600 at 240,535 ms, and routine 1,920 at 291,308 ms. It
timed out at 300,682 ms with 202.7 MB peak private / 205.0 MB working set and no
routine 1,984, `consumer:mir-to-ast:done`, or gen2 output. Routine 1,920 is
1,254 ms (0.43%) later than v43, so the theoretical tail backedge-BFS reduction
is a measured CPU negative/noise result, not a speedup claim. Do not widen into
structural-merge or phi caching without a new hot-owner observation.

2026-07-26 MIR scalar key length dispatch (`dfc8e406`). The routine-local
scalar owner now scans a key once for an escape and, for a plain key, runs
semantic equality only inside the matching raw-length group. Escaped spelling
still takes the complete semantic comparison path, so a plain/escaped duplicate
remains invalid. Same-length non-target keys are ignored. This adds no helper,
fact carrier, cache, ABI decision, or backend-specific path.

The exact-source v43 driver built in 52,451 ms at 2,523.0 MB peak private /
2,511.6 MB working set. Its bounded run finished in 1,454 ms and preserved the
414-byte SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
the wrong-ABI input exited 1 with the owned diagnostic and no output. The fixed
300-second full consumer reached routine 704 at 162,255 ms, routine 896 at
190,875 ms, routine 1,600 at 239,277 ms, and routine 1,920 at 290,054 ms. It
timed out at 300,268 ms with 215.1 MB peak private / 217.1 MB working set,
without routine 1,984, `consumer:mir-to-ast:done`, or gen2 output. The shared
1,920 marker is 3,093 ms (1.06%) earlier than v42: safe progress, but evidence
that key comparison dispatch is not the dominant remaining fact-index cost.
The next executable seam is the existing CFG owner's routine-level backedge
batch, replacing per-edge dominator BFS without adding a second graph/cache.

2026-07-26 captured optional ABI scalar reuse (`bf8a56b8`). The existing
routine-local scalar pass now carries whether `abi_type_name` was decoded as a
valid string or observed as the exact optional `null`. The ABI owner still owns
the type/layout relationship and all diagnostics; the scalar carrier supplies
only the already-observed wire value and readiness bit. Optional rows prove the
exact `id=0` and `layout=null` tokens without reparsing the same instruction
object. Missing, duplicate, wrong-kind, noncanonical ID, and required-layout
paths remain fail closed, and no backend-specific path or second ABI cache was
introduced.

The exact-source v42 integrated driver built in 53,265 ms at 2,515.0 MB peak
private / 2,503.6 MB working set, below the fixed 3,072 MB cap. Its bounded run
finished in 1,433 ms and preserved the 414-byte SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
The wrong-ABI negative exited 1 in 551 ms with the owned diagnostic and no
output. The fixed 300-second full consumer reached routine 704 at 162,849 ms,
routine 896 at 192,157 ms, routine 1,600 at 241,729 ms, and routine 1,920 at
293,147 ms before timing out at 300,115 ms. Peak private/working-set memory was
214.4/216.6 MB. Relative to v41, routines 704 and 896 moved earlier by 76,035
and 96,417 ms, and the same window advanced 1,024 routines farther. It still
did not reach `consumer:mir-to-ast:done` and did not create gen2 output. The
next executable falsifier is routine 1,984 under the unchanged window and cap.

2026-07-26 program-lifetime exact ABI validation reuse (`0da9c5c2`). The v39
full-input census found 10,635 instructions but only 40 exact ABI tuples before
routine 640. Required rows were more concentrated: 580 rows reduced to five
complete tuples. The ABI owner now retains only successful validation witnesses
for one MIR-to-AST execution. A hit requires the exact raw type value, canonical
decimal ID, required state, and complete raw layout payload; the 28-bit ID alone
cannot authorize reuse. Optional rows still prove `id=0` plus `layout=null`, and
the type-key witness keeps the decoded type name in a parallel owned array.

The C/LLVM fixture first admits `Array<Int>`, then proves that a semantically
equal row with different JSON key order takes the full validation path and that
the same ID with a changed nested offset is rejected. Missing, duplicate,
wrong-kind, and wrong-ID paths retain the owned ABI diagnostic. The validation
session is backend-neutral, program-lifetime only, and never becomes a semantic
layout table or a cross-run cache.

The exact-source v41 integrated driver built in 52,722 ms at 2,346.8 MB peak
private / 2,336.6 MB working set, below the fixed 3,072 MB cap. Its bounded run
finished in 1,251 ms and preserved the 414-byte SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
The 300-second full consumer reached routine 640 at 228,455 ms, routine 704 at
238,884 ms, and routine 896 at 288,574 ms before timing out at 300,227 ms.
Peak private/working-set memory was 157.2/162.3 MB. It still did not reach
`consumer:mir-to-ast:done` and did not create gen2 output. The next executable
falsifier is routine 960 under the same 300-second/3,072 MB gate.

2026-07-26 ABI-layout row capture progression (`a5d56f42`). Focused v29-v37
instrumentation isolated the full-consumer stall inside required ABI-layout
validation, not memory growth or the routine fact-index builder. The v38
outer-bound migration deliberately preserved the old nested validator and was
a negative result: routine 248 moved from 290,268 ms to 293,877 ms, while the
required Array/Option rows still cost roughly 1.35/1.09 seconds each.

`MirRoutineInstructionScalarCapture` now carries the four raw ABI value spans
from its single instruction-object walk. The renderer passes those bounds to
the ABI owner, which captures the nested layout row and its field rows once,
then computes the canonical identity from that capture. The old
`MirAbiLayoutHashRow` repeated-scan implementation is deleted, and the producer
compatibility entrypoint delegates to the same captured identity owner.
Missing, duplicate, wrong-kind, mismatched-identity, and truncated parallel
bounds fail closed; C and LLVM execute a known required `Array<Int>` row.

The v39 300-second run stayed below the fixed cap at 134.7 MB peak private /
140.8 MB working set. Routine 192 moved from v38's 233,517 ms to 102,775 ms,
routine 248 from 293,877 ms to 115,450 ms, and the run reached routine 640 at
298,374 ms. This is a material CPU closure, but it still timed out before
`consumer:mir-to-ast:done` and emitted no gen2 artifact. The exact final-source
v40 driver built in 55,007 ms at 2,565.3 MB peak private / 2,554.5 MB working
set and preserved the 414-byte bounded SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
The next executable falsifier is the routine-704 marker, then
`consumer:mir-to-ast:done`, under the unchanged 300-second/3072 MB gate.

2026-07-26 routine-local instruction scalar progression (`dd68d6f3`).
`MirRoutineInstructionFactBundle` now captures result/render/ABI/match scalars
in one pass per routine fact-index construction from the admitted program
instruction spans. Program-global
structure and routine-local facts remain separate. Render, match, expression
graph, assignment, and phi consumers use the bundle; duplicate or non-string
scalar rows and a corrupted count that would cross into the next routine fail
closed. Phi context is computed lazily only for blocks containing a phi, and
the canonical incoming-backedge fact replaces a duplicate dominator proof. The
active MIR-to-AST reconstruction reuses this bundle; the later expression-graph
and assignment post-passes still rebuild routine indexes and remain an open
re-entry seam.

Generated-C inspection corrected the prior diagnosis: an instruction fact
table did not deep-copy the 51.8 MB source payload. Pergyra `String` is passed
as `char *`, and the table stores the pointer plus bounds. The real cost was
repeated instruction-object validation and repeated field/bound discovery.

The current v23 integrated C driver built in 47,746 ms at 2,509.8 MB peak
private / 2,498.5 MB working set. Its bounded result remains 414 bytes with
SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
The 180-second production run stayed at 87.0 MB peak private / 95.3 MB working
set and reached routine 64 at 96,607 ms and routine 128 at 160,331 ms. That is
4,688 ms earlier than the v14 300-second run's routine-128 marker but still
timed out without `mir-to-ast:done` or a gen2 artifact. A current-driver `nested_if_in_loop` MIR
round trip passed, and a forged one-predecessor header phi failed with the
owned diagnostic. The broader body gate remains RED at the unrelated
`valid_array_builtins` emitted-runtime-header failure.

2026-07-26 admitted instruction-view CPU progression (`06f6994d`).
Routine lowering now reuses one typed instruction view and canonical block-id
projection from the admitted `MirProgramRoutineIndex`. Common no-layout and
no-resource instructions are validated from exact bounds without repeatedly
revalidating the same instruction table and rediscovering its field bounds.
Changing table accessors to `ref` alone was falsified by v9; the table and
field reads, not a deep copy of the source payload, owned the repeated work.

The observed instruction slice improved from 492 ms to 9 ms for ABI facts and
from 646 ms to 0 ms for resource facts. Routine 16 moved from 133,593 ms to
69,919 ms. The next counterexample showed that MIR phi `uses` is an incoming
value inventory rather than a predecessor-indexed machine phi table:
`FindTopLevelComma` has seven predecessors and two inventory values. The owner
now requires `2 <= use_count <= predecessor_count` and permits a self-result
only with a CFG-proven incoming backedge.

CFG successors are decoded once into integer identities. Missing facts use an
internal sentinel, while explicit negative wire successors fail closed in the
C/LLVM structure gate. The final v14 integrated C driver built in 48,451 ms at
2,442.7 MB peak private / 2,430.8 MB working set. The bounded result remains
414 bytes, SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.

The full v13 artifact run is still RED: timeout at 180,056 ms, 88.6 MB peak
private / 96.6 MB working set, routine 64 at 99,447 ms, and routine 128 at
164,457 ms. It did not reach `consumer:mir-to-ast:done` and emitted no complete
gen2 artifact. `mir.execution_graph` remains `BRIDGE`; self-hosting and the
bootstrap fixed point are not complete.

2026-07-26 admitted MIR program-instruction view progression (`190d0dbf`).
`MirProgramRoutineIndex` now captures one ordered routine/block/instruction
structure plus instruction kind/source type and raw machine contact/layer
spans. Machine admission consumes those spans directly, and
`MirRoutineFactIndex` consumes the same block/instruction partitions instead of
reopening nested arrays. Instruction result identity remains in the routine
fact owner; the program view is a derived `pgy.mir.v1` carrier, not a second
MIR authority.

Review found that the first version called the whole-program
`MirProgramRoutineIndexStructureReady` validator from every routine builder.
That recreated the repeated whole-graph validation defect. Readiness is now
proved once at admission, per-routine construction uses an O(1) row guard, and
the component contract rejects the full validator inside
`BuildMirRoutineFactIndex`. The focused structure fixture passes C and LLVM for
valid partitions and machine spans and rejects scalar array tails, missing
required structure, corrupted counts, and invalid rows.

The integrated v3 C driver built in 50,974 ms at 2,405.9 MB peak private /
2,409.3 MB working set. Its bounded result remains exactly 414 bytes, SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
The unchanged full-artifact window is still RED: timeout at 300,606 ms,
85.2 MB peak private / 93.6 MB working set, `limit_exceeded=false`, last marker
`top-level-routines:16`, and no gen2 file. Routines 1-64 contain only 274,581
of 51,741,503 routine-object bytes (0.531%), so 64 would be a progress sentinel,
not completion. The next executable seam is the routine emitter's remaining
raw instruction kind/source-type/machine-span consumers; registry status stays
`BRIDGE` and released/default substitution is unchanged.

2026-07-26 MIR document-index and bounded-string progression (`67502f50`).
The hard MIR input path now scans the 51,807,108-byte document root once and
carries the root, declaration, routine, and parallel-capture array bounds into
schema, capture, and machine admission. It no longer rebuilds three independent
top-level fact tables. Compatibility entrypoints still perform their own one
admission; the production path statically rejects those fallback calls.

The audit also found a hidden whole-document cost inside the exact-bound API.
Every one of the 34,091 `machine_layer:null` rows used `Substring(json, ..., 4)`,
whose native implementation first calls `strlen(json)`: about 1.766 TB of
logical walking. At least 4,690 routine identity decodes reached the same
`ReadJsonStringBounded -> Substring(json)` path, another 243 GB before nonempty
owners. Null validation now uses `SubEqualsWithLen`, and bounded string
materialization copies with the caller-owned limit rather than rediscovering
the 51.8 MB document length. A focused executable gate proves plain, escaped,
empty, and truncated reads through both C and LLVM.

The integrated C driver build `mir-document-index-driver-build-v2` is green in
57,528 ms at 2,319.9 MB peak private / 2,322.4 MB peak working set. Its bounded
MIR result is still exactly 414 bytes with SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
The fixed 300-second full-artifact gate remains CPU-red, but it advanced from
one top-level routine to the `top-level-routines:16` marker at only 63.4 MB
peak private / 74.0 MB peak working set. No gen2 C file was opened. The next
executable seam is the instruction structure that machine admission scans and
then discards before `MirRoutineFactIndex` scans it again; it must reuse the
existing program/routine identity rather than add a second JSON authority.

2026-07-26 MIR routine-consumer single-owner progression (`d62553ee`). The
admitted routine span now feeds one `MirRoutineHeaderFacts` capture for
generics, parameters, ABI carriage/resource/pass shape, and return type.
Instruction-local match/destructure arrays and scalar render/ABI reads consume
exact owner bounds; they no longer count and restart the same JSON array.
`MirRoutineFactIndex` captures every instruction result once, and phi
validation consumes that index instead of rescanning every instruction JSON
for every phi use. Missing or misaligned result facts fail closed.

CFG structural-merge selection now belongs entirely to
`mir_cfg_graph_owner.pgy`. Each conditional branch computes its two
branch-blocked reachability arrays once, while the original unrestricted
distance sum, ascending candidate order, strict tie break, terminal fallback,
and disconnected-component behavior remain unchanged. This reduces the
structural-merge analysis from candidate-local BFS, worst-case O(B^3), to
branch-local BFS, O(B^2), without creating another compiler fact owner or
splitting the Pergyra implementation into C-style fragments. The focused
production-function gate exercises diamond, re-entry-only, ranking, self-loop,
tie, fallback, and detached-component cases through C and LLVM.

The integrated C driver build `mir-cfg-owner-driver-build` exited 0 in 58,512
ms at 2,422.7 MB peak private / 2,411.3 MB peak working set. Its bounded MIR
consumer output remains byte-identical at 414 bytes, SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
The unchanged 300-second full-artifact gate
`full-mir-consumer-cfg-owner` remained CPU-red: 57.8 MB peak private / 68.7 MB
peak working set, last marker
`consumer:mir-to-ast:first-top-level-routine:done`, no 16-routine marker, and
no gen2 C output opened. The first top-level routine itself is only 2,063
bytes, one block, and one instruction; the fixed window is dominated by the
admission and accumulated consumer work, not that routine or memory pressure.
This is executable substitution progress but not a completed gen2 or
self-hosted driver.

The filtered broad `mir_json_parity.sh` attempt for `dir_walk` and
`break_after_stmt` stopped before CFG/runtime parity because reconstructed C
omitted the current runtime panic declarations. That is explicit adjacent RED
evidence, not a CFG-owner regression or a green result.

2026-07-25 sequential MIR instruction JSON artifact closure (`e5587bee`,
`6329356f`). Production block emission no longer calls
`SelfMirJsonInstruction(...) -> String`. The responsibility-named instruction
artifact writer emits expression-graph nodes, match/destructure rows, uses,
and runtime-call ABI auxiliary rows sequentially from the one verified
`SelfMirProgramFacts` owner. The String projection remains only as a focused
wire oracle. A new raw-byte gate compiles the probe through C and LLVM and
compares String/file bytes for small, graph-heavy, match, destructure, and
ABI/optional fixtures; it also proves invalid instruction rows are rejected
before an existing output file is opened or truncated.

The first fixed-cap run after aggregate instruction/graph removal remained
red: `instruction-stream-ready`, elapsed 810,472 ms, peak private 3,092.7 MB,
peak working set 2,574.5 MB, and a 40,263,680-byte partial artifact. It began
JSON near 2,956 MB and then accumulated escaped/quoted leaf strings allocated
through `AllocatorResult()`. `JsonStringLiteralWriteFile` now gives those
transient bytes one call-local pool and destroys it only after synchronous
`FileWrite` returns. This preserves the wire while bounding leaf lifetime.

The exact successor run is green at the unchanged 3072 MB ceiling:
`instruction-string-pool-ready`, exit 0, elapsed 675,355 ms, peak private
3,064.3 MB, peak working set 2,544.9 MB, top oracle private 3,063.1 MB, and no
compiler/link subprocess. It completed a 51,807,108-byte `pgy.mir.v1` artifact
with SHA-256
`1621adf4070bc778dd90493e29db857c22f13722d951bea8a94d1241e9ee884e`,
2,345 routines, and 142 declarations; the pressure log reached
`json-write:done` and a full JSON parse succeeded. This closes production MIR
artifact creation under the cap, but the margin is only 7.7 MB and the
generated-driver consumer/fixed-point gate has not yet run. The next active
executable rung consumes this exact artifact to emit and compile gen2, then
checks the bounded gen2/gen1 parity fixture without reopening a native fact
owner.

2026-07-25 admitted MIR consumer scan closure (`e9592a6a`). The path-based
consumer now validates machine-layer facts once and carries the admitted JSON,
the exact machine declaration, and one `MirProgramRoutineIndex` through the
lowering boundary. `EmitMirProgramTreeFromRoutineIndex` reuses that index;
raw-text compatibility entrypoints perform their own one admission and then
enter the same core. The old second machine validation is statically rejected.

The first 51,807,108-byte consumer attempts exposed CPU rescan debt rather
than memory pressure. `JsonArrayNextObjectBounds`, whitespace, object-end, and
routine identity reads repeatedly rediscovered the full JSON length; generated
C lowered each `StringLength(json)` to `strlen`. The structural cursor alone
performed at least 56,458 rows and roughly 8.8 TB of avoidable length walking.
Exact-bound JSON fact reads now consume only structure-owner `[start,end)`
spans. Routine and declaration inventories are built once, canonical
declaration phases reuse the inventory, and expression-graph node arrays use
sequential bounds instead of indexed restart scans.

The improvement is executable but gen2 is not complete. Under the unchanged
3072 MB cap, `full-mir-consumer-exact-bound` first reached
`machine-layer:routine-index:done`, while the two-field pass still stopped at
`instruction-scan:start`. Checkpoint `0857899e` compares normal bounded JSON
keys without decoding them into temporary Strings and retains the old decoder
only for an escaped key. `full-mir-consumer-key-compare` then reached
`instruction-scan:done`, `machine-layer:done`, and `input:done` before stopping
at `consumer:mir-to-ast:start` after 300,437 ms. Peak private was 57.1 MB and no
partial gen2 C was opened. The machine admission CPU seam is closed; the next
active executable seam is the MIR-to-AST lowering pass reached by that same
admitted artifact.

2026-07-25 MIR-to-AST exact-span executable closure (`157c340b`). The admitted
declaration and routine inventories now keep their exact object ends through
declaration emission, method lookup, top-level routine lowering, and the
per-routine fact bundle. `EmitDeclFields` walks the owned field array with one
forward cursor instead of restarting at row zero, and routine lowering can no
longer rediscover an object end or fall back to the document end. Iteration,
resource-flow, loop-flow, block, instruction, and local fact reads consume the
same validated `[start,end)` spans through bounded accessors. Static gates
reject the retired generic reads in these owners.

The first fixed-window run, `full-mir-consumer-exact-span`, reached
`consumer:mir-to-ast:declarations:done`; the field-array change removed a
measured lower bound of 47,290 whole-document `strlen` calls, about 2.45 TB of
logical byte walking. The successor `full-mir-consumer-routine-fact-exact`
also reached `consumer:mir-to-ast:first-top-level-routine-fact-index:done`.
It removed at least another 118.9 TB of logical whole-document walking from
block successor/instruction-array and instruction-kind reads. Both runs timed
out at 300 seconds with 58.0 MB peak private, not memory pressure, and neither
opened a partial gen2 C output. The bounded MIR consumer still emits the same
414 bytes as the prior driver. The active CPU seam is now after the first valid
routine fact index and before all top-level routines return.

2026-07-25 initializer environment cursor executable rung (`ffe31ce8`).
`ast_initializer_environment_cursor_owner.pgy` now keeps one function base
environment and one active lexical-local suffix while initializer rows advance
in source order. `SemanticAstLocalBindingFacts` and the typed AST arena remain
the identity/order/scope authorities. The cursor owns only transient traversal
state, publishes a destructure node's rows atomically after their shared
initializer verdict, restores outer bindings on scope exit, and fails closed on
function/node order drift. The initializer production loop no longer calls
`SemanticAstExpressionSeedVisibleLocals` or
`SemanticAstExpressionSeedVisibleLocalModes` per row. Its standalone wrapper
also releases the callable table it creates after the last consumer.

The cursor owner smoke, borrowed-lifetime smoke, component contract, protocol
and SoT registries, and the expanded initializer projection parity passed.
Both C and LLVM prove outer-shadow visibility, nested-scope restoration, and
atomic destructure publication; self-reference and sibling-scope leakage fail
with `undefined_symbol`.

The exclusive full-driver pressure run on `ffe31ce8` remains red at the
unchanged 3072 MB ceiling: label `initializer-cursor-ready`, elapsed 869,913 ms,
peak private 3,117.9 MB, peak working set 2,601.7 MB, and oracle private
3,116.7 MB. It completed every 8,229-row base initializer, the full semantic
body, verification, and MIR facts, then crossed after `json-write:start` with a
13,709,312-byte partial artifact inside routine
`SemanticExpressionGraphNodeKind`. Compared with `json-block-file-ready`, the
cap was reached about 129,685 ms earlier and the sampled overshoot was 79.4 MB
smaller, but the pre-JSON baseline remained approximately 2,937 MB; this is not
a memory-gate closure. The next executable owner is instruction JSON file
emission: remove `SelfMirJsonInstruction(...) -> String` from the production
writer while preserving the same `pgy.mir.v1` field order and byte parity.

2026-07-25 MIR readiness, composite assignment, and JSON artifact lifetime
delta. The verified driver now calls
`SelfMirProgramFactsFromReadyArtifact` after the body bundle has already owned
the whole-semantic proof; the checked standalone entrypoint retains that proof
and delegates. This moved the full source through `mir-facts:done` below the
3 GiB cap and exposed the first real MIR invariant failure instead of another
readiness rescan.

Node 5290 was `ApplyPostfixFact`'s `base.text = ...`: the root local `base` has
type `ParserExpressionFact`, while the selected member has type `String`.
`routine_assignment_owner.pgy` now enforces root-local/final-target type
equality only for direct assignments (`target_text == target`); member and
indexed targets keep root existence plus semantic target-graph/type facts.
The focused lifetime/component gates and initializer C/LLVM parity passed.

The next full measurements all completed MIR fact construction and isolated a
separate JSON lifetime defect. `assignment-composite-ready` reached
`json:start` and crossed at 3,233.9 MB. Replacing the shared JSON emitter's
per-character `Substring` and nested `StringJoin`/`Concat` assembly with
`TextBuilder` reduced that result to 3,195.6 MB but did not close the cap.
The production `--emit-mir-json-verified` path now writes the same
`pgy.mir.v1` order through
`program_json_artifact_writer_owner.pgy`; program, routine, and block strings
are no longer materialized as one aggregate. Small-fixture output remains
byte-identical to the prior self-host path (11,262 bytes, SHA-256
`007d5dacdd8157a0d5dd0f87975f82c7abe2fa4987983afb3945bd61b29efc09`), and
the shared JSON escape/object probe is byte-identical through C and LLVM.

The end-to-end cap is still red. Whole-routine file emission stopped at
3,290.1 MB with a 20,013,056-byte partial artifact; routine/block streaming
advanced to 20,901,888 bytes and reduced the peak to 3,197.3 MB, but retained
small instruction/field strings still accumulate above a roughly 2,933 MB
pre-JSON baseline. No complete full-driver MIR artifact exists yet. The next
active optimization owner is the initializer expression-environment cursor:
the current per-local environment reconstruction is statically O(sum of
squared function-local counts) and must become one scope-aware sequential
owner before adding more JSON fragments or any backend-local cache.

2026-07-25 initializer readiness amortization. `589b6638` first restored the
expression environment to a borrowed-row contract: producers use ordinary
`ArrayPush`, row reset pops logical entries while retaining reusable backing,
and the final clear drops only the empty backing arrays. The earlier
`ca35a157` owned-string entry below is historical and is superseded by this
committed contract.

The active Pergyra semantic slice then split match-binding seeding into a
checked standalone entrypoint and
`SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact`. The
initializer outer owner already proves artifact and expression-surface
readiness, so its 8,149-row hot loop now calls the borrowed ready-artifact core
instead of reconstructing whole expression/match graph `seen` and `stack`
arrays for every local. Static lifetime and component gates reject the old
checked call or a readiness rescan in the core; focused initializer C/LLVM
parity passed.

The official full-driver gate confirms this seam but remains red overall. All
initializer rows completed through `row:done:8148`, then the pressure owner
stopped the later `call-targets:start` stage after `7,992,190 ms` at 3,074.3 MB
peak private / 2,521.4 MB peak working set; `driver_oracle.exe` owned 3,063.3
MB private and the target returned `Error 88`. No full MIR artifact was
produced. The next executable SoT boundary is therefore call-target resolution,
not another initializer readiness workaround and not a backend-specific C or
LLVM patch.


2026-07-25 call-target readiness consumer closure. The body resolver already
proved the expression surface once but called the checked match-environment
entrypoint per surface. It now borrows the same ready-artifact core as the
initializer, and the lifetime gate rejects restoration of the checked call.
Focused lifetime/component gates and initializer/call-target C/LLVM parity
passed.

The focused 3 GiB shard completed `call-targets:done` and
`initializer-refine:done`, then crossed at `expression-places:start` after
328,425 ms (`peak_private_mb=3072.8`, oracle private 3,071.5 MB). This counts as
an executable consumer migration; the full pressure gate remains red and the

2026-07-25 remaining semantic-body readiness closure. Expression-place,
statement, and generic-specialization consumers now borrow the ready-artifact
match environment after their outer owner has proved expression-surface
readiness. The lifetime gate rejects a checked match-environment call returning
inside any of these hot loops. Focused lifetime/component gates and initializer
C/LLVM parity passed after every slice.

The expression-place shard completed expression-place and assignment stages
before stopping at `statement:start` after 266,437 ms (3,076.9 MB peak
private). The statement shard completed that stage before stopping at
`generic:start` after 274,579 ms (3,074.7 MB peak private). The generic shard
completed `generic:done`, `verdict:done`, `body-types:ready`, and `verify:done`
before stopping at `mir-facts:start` after 264,914 ms (3,073.5 MB peak private,
3,072.3 MB owned by the oracle). This is executable self-host progress through
the complete semantic-body bundle, not a full-pressure green result. No full
MIR artifact was produced; MIR-fact materialization is the next executable SoT
boundary. C and LLVM remain peer backends over the same Pergyra fact spine.
next active consumer is expression-place resolution.
2026-07-25 semantic expression scratch lifetime closure (`ca35a157`). The
focused initializer/member-call C probe exposed Windows heap corruption
(`0xC0000374`), not an acceptable self-hosting cost. Generated-code tracing
identified a borrowed owner-field name inserted with ordinary `ArrayPush` and
later freed by the owned environment cleanup. Owner-field, match-binding, and
iteration seeders now use the same owned-string insertion contract as the core
environment owner.

The last production consumers now release expression scratch rows after
assignment, call-target, place, generic-specialization, initializer, iteration,
and statement projection. Strings retained in result facts are copied before
that boundary. The lifetime smoke gate names each producer and consumer and
rejects ordinary pushes, missing cleanup, and missing result copies. The
program graph remains a separate single structural owner and still reports
`phase=unified structural_owners=1`; neither C nor LLVM owns a private lifetime
policy or a copied semantic graph. Focused assignment C/LLVM parity passed.
The full-driver 3 GiB verdict remains open until the pressure owner measures
this exact committed revision.

2026-07-25 semantic lifetime and program-graph line consolidation. The
previously isolated `codex/semantic-environment-owned-lifetime` line is merged
with the executable foreach program-graph closure on `main`; this preserves one
Git history instead of treating physical worktrees as competing source trees.
The compiler semantic graph remains independently enforced as one structural
owner by `tests/self_host_program_graph_unification_smoke.sh`.

The lifetime slice is Pergyra-owned rather than a pair of backend patches.
`ArrayPushOwnedString` and `ArrayDropOwnedStrings` are one typed collection ABI
registered through semantic checking and self-hosted runtime-call ownership,
then projected by both C and LLVM. Semantic expression scratch rows are
released at their last consumer, and the artifact callable-table owner releases
its owned rows only after body analysis has materialized every derived fact;
the released fact is invalidated so a stale read fails closed. The initializer
type fact no longer duplicates parser-owned expression text, and the assignment
type fact no longer duplicates parser/assignment-owned target text.

These are bounded reclamation and SoT substitutions, not a claimed memory
closure. The last attributable source observation is still red at 3 GiB
(`3080.9 MB` peak private for the lifetime line); the graph-only observation is
also red (`3091.3 MB`). The combined revision must pass the same exclusive
full-driver pressure owner before any improvement is claimed. Raising the cap,
adding an ambient allocator fallback, or splitting lifetime policy between C
and LLVM remains forbidden.

2026-07-25 non-identifier foreach program-graph closure. The executable
falsifier was `src/self_hosted/mir_lower/fixture/for_each_call.pgy`: the
Pergyra-built MIR producer attached the source call graph to the hoist
definition, then constructed a separate one-node graph for the synthetic
collection local used by loop-init and branch. The instruction graph owner
requires every attached root to belong to the same program graph, so the C
producer failed with `MIR expression graph attachment failed`. The same failure
reproduced at the clean parent checkpoint, so it was not caused by the current
callable-table migration.

The replacement keeps physical components but one graph authority. HIR
`program_graph_owner.pgy` is the only structural extension API;
`ast_iteration_graph_root_owner.pgy` asks that owner to append compiler-created
leaf nodes, attaches semantic call-target/place overlays, and records stable
synthetic names plus root handles in iteration facts. MIR consumes those facts.
`SelfMirSyntheticLocalExpressionGraph` and MIR-side
`SelfMirForEachSyntheticOrdinal` reconstruction are deleted. The structural
gate still reports `phase=unified structural_owners=1`.

The static iteration gate rejects the retired sibling-graph and MIR ordinal
paths and forbids semantic code from mutating topology arrays directly. The
`for_each_call` runtime gate requires each of the three synthetic handles to
project as the same leaf graph at loop-init and branch. Focused Pergyra-built C
and LLVM drivers each passed all 20 body fixtures plus the selected MIR
fixture, including canonical MIR, emitted C, and execution comparison. This is
an executable self-host replacement, not evidence that the full compiler is
self-hosted. No full-driver pressure claim changed: the pre-MIR semantic
lifetime falsifier and the 3 GiB cap remain active.

2026-07-25 Pergyra collection-mutation program-graph use closure. Objective:
make `ArrayPop`, `ArrayPush`, and `ArraySet` receiver/value/index SSA uses consume
the one semantic expression graph without restoring a source-text scan or a
third MIR graph slot. Priority was program-graph identity, lane ownership,
missing-receiver failure, fallback deletion, executable negative evidence, then
file layout. A copied receiver graph, `new ? old` reads, identifier-text recovery,
or C/LLVM-local receiver inference is forbidden.

`fd2e0597` is the executable replacement: parser lanes own the receiver/value/
index roots; MIR derives uses from graph views; Push/Set retain the two existing
wire slots; Pop is receiver-use-only; and the retired text-use functions are
negative-gated. `4ee38b73` is the bounded ownership follow-up: persisted
`expr0`/`expr1` requirements live in a 98-line MIR instruction policy while the
schema decoder/NodeId binder returns to 222 lines. The policy owns no graph
storage, so `hir/program_graph_owner.pgy` remains the only structural owner.

In a clean detached `4ee38b73` worktree, the component contract passed. Fresh
C-built and LLVM-built DRV-2 parity each passed 20 body fixtures and four focused
collection MIR fixtures. SoT authority adequacy and live negative mutations
passed with Coq/Rocq explicitly skipped because no prover is installed; program
graph unification reported `phase=unified structural_owners=1`; the build-pressure
contract passed. The main worktree concurrently contains an unfinished semantic
function-table/lifetime slice, so its unresolved-name compile failure is neither
green evidence nor a regression attributed to this graph closure. The next
executable memory rung remains owner-proved whole-program semantic lifetime
reduction followed by a pressure-owned full-driver run below 3 GiB.

2026-07-24 Pergyra generic wrapper materialization executable closure.
Objective: materialize only concrete Option/Result value types while preserving
the exact generic specialization symbol carried by semantic and MIR facts.
Priority was specialization identity, structured formal-type exclusion,
concrete return/parameter restoration, missing-fact failure, and negative
ratchets. Treating `Option<T>` as a C value declaration, trimming the terminal
separator from a constructed generic actual, reparsing source call text, and
guessing a concrete wrapper in codegen are forbidden.

`value_wrapper_usage_owner.pgy` now excludes a generic signature's formal
return/parameter surfaces through typed AST parent identity plus the structured
signature type-expression facts. It then adds the resolved concrete return and
parameter types from `CodegenGenericSpecializationFacts`; the declaration
scheduler remains the last consumer. `symbol_table_owner.pgy` keeps the
existing runtime type suffix policy but preserves the terminal constructed-type
separator for specialization actuals, so the self symbol is exactly
`Wrapper_Echo_Option_Int_`.

A fresh Pergyra-built DRV-2 passed its bounded build smoke. Hard producer-first
parity then passed for `generic_member_inferred_flow`,
`generic_vessel_member_inferred_flow`, `generic_member_constructed_return_flow`,
`generic_member_array_return_flow`, and
`generic_member_record_array_return_flow` with `backends=1 body_fixtures=20
mir_fixtures=5`. The full 280-row matrix and LLVM lane remain to be resumed
after this focused closure.

2026-07-24 Pergyra canonical declaration-order SoT executable closure.
Objective: make MIR JSON canonicalization independent of producer declaration
family order while preserving one Pergyra-owned projection order. Priority was
semantic identity, one declaration-order owner, fallback removal, an
order-adversary gate, then patch size. Reusing input family order as canonical
authority, reparsing source, backend guessing, and `new ? old` compatibility
reads are forbidden.

The first unfiltered 280-row DRV-2 run reached `role_operator_dispatch` and
exposed a real dual-authority seam: native raw MIR listed ability before role,
while self-host raw MIR listed role before ability. Both canonical outputs
grouped declarations alike, but reconstructed temporary AST node order changed
the role method `source_syntax_id` from `12` to `6`.
`decl_lower.pgy` now owns the explicit canonical family order
`nominal -> role -> ability -> enum`, preserves input order only within a
family, and fails closed on unsupported declaration kinds. The focused
order-adversary gate asserts the opposite raw orders, the shared canonical
order, and the nonzero `IntMath.Add` source identity.

In a clean detached verification tree, both canonical MIR artifacts were
byte-equal with `IntMath.Add source_syntax_id=6`, and fresh Pergyra-built DRV-2
hard parity passed with `body_fixtures=20 mir_fixtures=1`. The main-tree
component smoke reached an unrelated concurrent native file-split assertion,
so that broader gate is not claimed. The remaining unfiltered matrix rows and
the LLVM matrix were not run yet.

2026-07-24 Pergyra Set-literal SoT executable closure.
Objective: carry `{...}` and `{}` through a distinct parser Set-literal spine,
declared `Set<T>` semantic compatibility, and the existing Set runtime ABI
owner into C emission. Priority was parser identity, contextual empty-literal
typing, homogeneous element evidence, runtime symbol ownership, fail-closed
negatives, then patch size. Treating Set literals as arrays/structs, guessing
element ABI, reparsing source in codegen, and accepting an untyped empty Set
are forbidden.

`AstExpressionNodeSetLiteral`/`SetElement` are now carried by the parser graph;
`ast_expression_graph_set_literal_owner.pgy` owns ordered element projection,
homogeneous inference, and declared-type matching. Composite emission consumes
`CollectionSetRuntimeFact` for `Set<Int>`/`Set<String>` construction and add
symbols. `set_literal_basic` hard C emits both Set families and runs as
`3, true, false, 0, 2, true`; duplicate insertion remains delegated to the
runtime Set semantics. Missing Set ABI, mismatched element, and untyped empty
literal negatives fail closed. The focused Set-literal gate passed; full 280-row
DRV-2 and LLVM matrices were not run.

2026-07-24 Pergyra Set runtime/call SoT executable closure.
Objective: carry `SetNew/Add/Has/Remove/Size` from graph-owned target,
receiver, and element facts through one Set runtime ABI owner into C emission.
Priority was direct target identity, element/return typing, contextual
constructor promotion, runtime symbol ownership, fail-closed negatives, then
patch size. Receiver/element guessing, source Set spelling as ABI, Queue/List
substitution, and missing Set runtime fact success are forbidden.

`ast_expression_graph_set_call_owner.pgy` owns the call verdict;
`set_runtime_owner.pgy` owns the supported `Set<Int>`/`Set<String>` C value and
operation symbols. Native/self MIR agree on `Set<Int>` and all five direct Set
targets. Self-host C emits `PgySet_int` and `pgy_set_*_int`, runs with `true`,
`false`, `1`, and rejects both a missing declaration ABI fact and a String
passed to `Set<Int>`.

Fresh Pergyra-built DRV-2 hard parity passed with `body_fixtures=20
mir_fixtures=1`; component, hard-contract, authority-edge, and
authority-adequacy gates passed. Authority adequacy used the declared
`PGY_ALLOW_MISSING_COQ=1` skip because no Coq/Rocq prover is installed. Commit
`f743db5b` is local, and the manifest contains 278 rows. Full unfiltered DRV-2
and LLVM matrices were not run.

2026-07-24 Pergyra indexed Set argument-value SoT executable closure.
Objective: project an `Array<T>` receiver and `Int` index into the shared scalar
argument type fact consumed by Set calls. Set-local source reparse, element
guessing, backend index recovery, and source spelling as ABI are forbidden.

`ast_expression_graph_scalar_shape_owner.pgy` now owns the shared index
projection. Native/self MIR carry `Array<String>`/`Array<Int>` indexed values
into Set calls; self-host C emits `pgy_as_get`/`pgy_ai_get`. Hard execution of
`loop_collect_distinct_set` prints `4`, `true`, `true`, `true`, `false`, and
the wrong-index negative fails closed at `owner: collection_value_type`.
The supplemental `set_member_pipeline` hard run prints `15`, `40`, `3`.

Fresh Pergyra-built DRV-2 producer-first parity passed with
`body_fixtures=20 mir_fixtures=1`; authority-edge and hard-contract gates also
passed. The component smoke remains unclaimed because a concurrent native C
file-split change lacks `mir_json_emit_decl_generic_params(out, header);`.
Commit `23c2f0cb` is pushed. Full unfiltered 279-row DRV-2 and LLVM matrices
were not run.

The next active surface is `set_literal_basic`: Set literal syntax remains a
separate parser/graph surface and must not be conflated with indexed-value
projection.

2026-07-24 Pergyra Queue runtime/call SoT executable closure.
Objective: carry contextual `Queue<T>` literals and Queue operation calls from
graph-owned target/element facts through MIR ABI rows into one Queue runtime ABI
owner and C emission. Priority was target identity, element/return typing,
contextual literal promotion, runtime symbol ownership, fail-closed negatives,
then patch size. Queue receiver/element guessing, source spelling as runtime
ABI, Queue facts recovered by List owners, and missing Queue ABI success are
forbidden.

`ast_expression_graph_queue_call_owner.pgy` now owns Queue call verdicts;
`queue_runtime_owner.pgy` owns Queue C value and operation symbols; Queue
call/type and composite emitters consume those facts. Native/current DRV-2 MIR
agrees on `Queue<Int>`, `Queue<String>`, Queue literal facts, and direct
QueuePop/QueueSize targets. Emitted C compiles and runs with
`3, 3, 5, 2, beta, 2, 7, 8, 0`. Removing the Queue ABI fact and injecting a
String into `Queue<Int>` both fail closed.

The Queue-specific gate and hard producer-first parity passed with
`body_fixtures=20 mir_fixtures=1`; staged component, hard-contract, and
authority-edge gates passed. Authority adequacy passed with the declared
`PGY_ALLOW_MISSING_COQ=1` skip because no Coq/Rocq prover is installed.
Commits `5bc2e996` and `0d5e186b` are pushed; the DRV-2 manifest contains 277
rows. Full unfiltered DRV-2 and LLVM lanes were not run.

The clean generated-C `None` bootstrap blocker is closed by `e725ea26`.
Contextual `Option<T>` initialization, assignment, return, and typed call
arguments share `RewriteSemanticExpectedValue` plus the dedicated
`expr_semantic_option_value_owner.pgy`. Clean gen1/gen2 seed generation and a
Pergyra-built DRV-2 compiler completed without a compile define or
`Option<Int>` fallback. `0d5e186b` is the first SoT-only follow-up after the
Queue executable delta and centralizes collection binding kind lookup.

The next observed executable failure is
`tests/cases/backend_compare/set_ops/main.pgy`: the Pergyra-built driver fails
closed with `undefined_function`, `func: SetNew`. Five additional Set call
fixtures reproduce the missing builtin boundary; passing Queue probes are not
promoted into inferred work. The next active rung must establish one Set
semantic/type/runtime ABI spine, with a missing runtime row or mismatched Set
element as the first falsifier.

2026-07-24 Pergyra List literal contextual typing executable closure.
Objective: type `[ ... ]` from a declared `List<T>` initializer through the
parser-owned sequence shape and emit the declared List runtime ABI. Priority
was declared element compatibility, initializer fact promotion, MIR ABI
carriage, List constructor/push emission, fail-closed negatives, then patch
size. Array-to-List name guessing, backend element recovery, source reparse,
and Queue-as-List runtime substitution are forbidden.

`SemanticSequenceElementType` now owns Array/Slice/List/Queue shape projection;
the array-literal graph owner validates element compatibility and the
initializer owner promotes the verified graph to the declared List type. The
composite emission owner consumes that carried type and emits element-specific
List construction. Native/self MIR agree on `List<Int>`, `List<String>`, and
empty `List<String>`; emitted C runs with `3`, `3`, `5`, `2`, `beta`, `0`.
Removing the List ABI fact and injecting a String into `List<Int>` both fail
closed. Focused producer-first parity passed with `body_fixtures=20
mir_fixtures=1`; the component and hard contracts passed against the clean
staged snapshot. Commit `431c2416` is pushed and the DRV-2 manifest contains
276 rows. Full unfiltered DRV-2 and LLVM lanes were not run.

The clean bootstrap blocker noted at this List checkpoint was later closed by
`e725ea26`; Queue now closes the remaining path in
`sequence_literal_list_queue`.

2026-07-24 Pergyra lexical List shadow identity executable closure.
Objective: preserve parser/semantic local-binding identity through MIR SSA
construction and restore the active typed environment at lexical block exit.
Priority was binding identity, MIR declaration ABI carriage, scope restoration,
codegen typed-environment restoration, fail-closed negative evidence, then
patch size. Source-name-only SSA lookup, function-wide nested-block local
reuse, and backend List ABI recovery are forbidden.

`ast_local_binding_fact_owner.pgy` now owns a per-name binding ordinal;
`SelfMirRoutineDeclareLocal` carries that identity into `items.1`/`items.2`,
and block lowering restores the prior local inventory at branch exit. Codegen
threads copied typed environments into nested blocks, so the inner
`List<String>` binding cannot overwrite the outer `List<Int>` ABI. Native/self
MIR agree, emitted C runs with `shadow`, `10`, `20`, and removing the inner
`abi_type_name` fails closed with `local declaration is missing its MIR ABI type
fact`.

Focused hard producer-first parity passed with `body_fixtures=20
mir_fixtures=1`; component and hard-contract gates passed. The clean bootstrap
seed gate remains blocked by the pre-existing generated-C `None` identifier
error; an isolated compile define was used only to build the focused driver.
Executable commit `564de5be` is pushed and the DRV-2 manifest contains 275
rows. Full unfiltered 275-row DRV-2 and LLVM lanes were not run.

The List portion of `tests/cases/backend_compare/sequence_literal_list_queue/main.pgy`
is now closed; the self-host driver reaches `undefined_function QueueSize`.
The next owner must establish the Queue runtime ABI/call boundary without
making List consumers recover Queue facts.

2026-07-24 Pergyra ListPush scalar graph value executable closure.
Objective: carry the arithmetic value in `ListPush(xs, i * i)` through one
parser-owned expression graph operator policy rather than teaching the List
call owner a second scalar type system. Priority was shared scalar policy,
List call validation, target/ABI carriage, fail-closed negative evidence, then
patch size. Source-text arithmetic inference, fixture/name special cases, a
List-local operator taxonomy, and backend type guessing are forbidden.

`ast_expression_graph_scalar_shape_owner.pgy` now owns the common unary/binary
scalar result policy used by the general scalar type owner and List call
argument projection. `ast_expression_graph_resolved_call_type_owner.pgy`
consumes that projection for receiver, index, and value validation. Native and
self-host MIR agree on direct `ListPush`, multiply `i * i`, and `i: Int`;
self-host C emits the element-specific List push/size/get ABI and both programs
print `5` then `55`. Removing the multiply right edge and changing the value to
`i * "bad"` both fail closed. Executable commit `ec719baa` is pushed, and the
DRV-2 manifest contains 274 rows.

Focused C and hard producer-first parity passed with
`body_fixtures=20 mir_fixtures=1`. Full C codegen parity passed 85 fixtures plus
the tagged-enum, event, temporary-ref, cyclic value/Result/nested
Option<Result>, and role-operator legs. Component, hard-contract, authority
edge, authority adequacy, documentation quality, shell syntax, and
`git diff --check` passed. The edge gate reports 45 authorities, 40 derived
carriers, `CLOSED=25 BRIDGE=20 ACTIVE=0`; Coq/Rocq was an explicit declared
skip because no prover is installed. The full unfiltered 274-row DRV-2 matrix
and LLVM lane were not run.

The next observed executable seam is now recorded at the top of this file:
`sequence_literal_list_queue` contextual typing.

2026-07-24 Pergyra List foreach executable closure.
Objective: carry the semantic iteration row for `List<Int>` through self-host
MIR reconstruction and project one canonical List size/get ABI in C codegen.
Priority was the `selfhost.iteration_type_verdict` SoT, fail-closed MIR row
admission, shared Array/List foreach shape, runtime ABI projection, then patch
size. Source-local type guessing, MIR-side collection guessing, and a separate
List statement emitter are forbidden.

`SemanticAstIterationTypeFacts` remains the authority. The routine-local MIR
index validates `iteration_type_facts`; `iteration_type_fact_owner.pgy`
admits foreach only when iterable, element, and binding types agree; and
`foreach_collection_runtime_owner.pgy` is the final Array/List ABI projection.
`for_in_list_int` now has native/self MIR parity, emits
`pgy_list_size_int`/`pgy_list_get_int`, and runs with output `12`. Removing the
MIR iteration row fails closed. Executable commit `67033cad` is pushed and the
DRV-2 manifest contains 273 rows.

Focused C and hard producer-first parity passed with
`body_fixtures=20 mir_fixtures=1`. The full C codegen matrix passed all 85
fixtures plus tagged-enum, event, temporary-ref, cyclic value/Result/nested
Option<Result>, and role-operator regressions. Component, hard-contract, SoT
authority edge, authority adequacy, shell syntax, and `git diff --check`
passed. The edge gate reports 45 authorities, 40 derived carriers,
`CLOSED=25 BRIDGE=20 ACTIVE=0`; Coq/Rocq was an explicit declared skip because
no prover is installed. The full unfiltered 273-row DRV-2 matrix and LLVM lane
were not run.

The next observed executable seam is
`tests/cases/backend_compare/list_push_get_loop/main.pgy`. Native C runs with
`5` then `55`; the current self-host driver fails closed as
`ast_artifact_invalid` with owner `collection_value_type` for
`ListPush(xs, i * i)`. The next owner must route that argument through the
existing graph scalar-type verdict instead of the List call owner's local
literal/leaf-only classifier. A changed arithmetic operand type and a missing
carried expression edge are the first falsifiers.

2026-07-24 Pergyra ListGet compound return-type executable closure.
`list_call_type_owner.pgy` carries the resolved `ListGet` element type into the
compound-expression consumer, so `list_int_loop` emits and runs instead of
failing at addition typing. The focused hard producer-first gate passed with
one MIR fixture. Executable commit `7fdef5aa` is pushed; this is the direct
predecessor of the List foreach closure above.

2026-07-24 Pergyra List operation call ABI executable closure.
Objective: carry direct ListPush/ListGet/ListSet/ListRemove/ListSize target,
receiver, arity, value, and return facts through one semantic-to-runtime ABI
path. Priority was one semantic List-call owner, element-specific runtime
symbols, addressable receiver enforcement, missing-target negative ratchet,
then patch size.

`ast_expression_graph_resolved_call_type_owner.pgy` owns the List-call
protocol, `list_runtime_owner.pgy` owns element-specific operation symbols, and
`list_call_emit_owner.pgy` is the last codegen consumer. Native and self-host
MIR agree; self-host C emits `pgy_list_*_int` and runs with
`3, 10, 30, 99, 2, 99`. `List<String>` independently emits and runs with
`world`. Removing the carried `ListPush` target fails closed with
`MIR instruction expression graph is missing or invalid`. Commit `0c19a8c5`
is pushed.

The focused producer-first, component, hard-contract, shell syntax,
`git diff --check`, authority adequacy, and authority edge gates passed; Coq
was explicitly skipped because `rocq`/`coqc` is not installed. The committed
DRV-2 manifest is now 271 rows. The next executable seam is
`list_int_loop`: `ListGet`'s resolved `Int` return fact is missing at the
semantic addition operand consumer. `list_get_string` already passes its
String ABI path.

2026-07-24 Pergyra nested generic List ABI executable closure.
Objective: carry contextual `ListNew` typing and the canonical
`List<HashMap<String, Int>>` runtime ABI from parser-owned call/type facts
through self-host semantic validation, C layout, and emitted runtime symbols
under one owner. Priority was one contextual type owner, canonical list ABI,
fail-closed missing/unsupported facts, negative ratchets, then patch size.

`SemanticBuiltinSignatureRows` owns `ListNew^List<Unknown>^none`,
`ast_contextual_builtin_type_owner.pgy` joins the call spine with the declared
binding type, and `list_runtime_owner.pgy` owns the supported element ABI and
specialization macro. Native/self MIR agree on the nested type; self-host C
emits `PGY_LIST_DEFINE(HashMap_String_Int, PgyHashMap_Int)`, compiles, and runs
the fixture. Missing contextual type fails with
`initializer_type_unresolved`; unsupported `HashMap<String, Float>` fails
closed with the List runtime ABI diagnostic. Commit `474e6e76` is pushed.

The focused producer-first gate passed with
`backends=1 body_fixtures=20 mir_fixtures=1`. Component contract, hard
contract, shell syntax, `git diff --check`, authority adequacy, and authority
edge gates passed. The edge gate reports 45 authorities, 39 derived carriers,
`CLOSED=25 BRIDGE=20 ACTIVE=0`; Coq was explicitly skipped because no
`rocq`/`coqc` is installed. The next observed seam is ListGet return-type
carriage on `list_int_loop`; the current driver reaches codegen but fails at
the addition operand type consumer.

2026-07-24 Pergyra ability generic multi-bound/default executable closure.
Objective: carry declaration-site ability generic `where` bounds and defaults
from parser/HIR facts through `SemanticAstRoleFacts`, native/self MIR, and
emitted C dispatch under one SoT. Priority was one owner, owner-directed fact
carriage, fail-closed malformed/missing bounds, the negative ratchet, then
patch size. `SFAbilityGenericBounds` and the
`selfhost.ability_generic_bounds` registry row now identify
`SemanticAstRoleFacts` as the owner; the bound verdict consumes only those
canonical rows.

Native and self-host MIR agree on `Packable<T>` with
`constraint: Comparable + Cloneable` and `default_type: Item`. The
`MissingAbility` mutation fails closed with owner
`SemanticAstAbilityGenericBoundVerdict`; emitted C vtable/adapter facts are
checked by the focused producer-first gate. That gate passed with
`backends=1 body_fixtures=20 mir_fixtures=1`; the current and previous ability
rungs passed together with `mir_fixtures=2`. Full C codegen parity passed 85
fixtures, including role-operator runtime parity. Component, hard-contract,
shell syntax, `git diff --check`, authority adequacy, and the authority edge
gate passed. The edge gate reports 45 authorities, 39 derived carriers, and
`CLOSED=25 BRIDGE=20 ACTIVE=0`; the Coq model was explicitly skipped because
no `rocq`/`coqc` is installed.

The isolated current native compiler built and linked with `LLVM_ENABLED=0`
and Windows `D:/...` build paths. A separate `/d/...` response-file invocation
needed path normalization. The full unfiltered 269-row matrix and LLVM lane
were not run. Implementation commit `c5903680` and handoff refresh `ae041e06`
are pushed.

The nested generic List ABI seam described in the current handoff is now
closed by `474e6e76`; the next executable work is the `collection_call_target`
artifact boundary on list mutation calls.

2026-07-24 Pergyra dynamic ability-bind dispatch executable closure. Objective:
carry party role-slot, role implementation, ability method, and bind identity
from semantic facts through MIR JSON into one C vtable/bind ABI. Priority was
one semantic bind owner, declaration/MIR carriage, fail-closed missing ABI
facts, direct-call fallback removal, then patch size. The new bind verdict,
declaration rows, `slot_anchor`, dynamic-call owner, bind owner, nominal layout
owner, and role-dispatch owner now form one consumer path. Commit `e09d680e`
is pushed; the following handoff refresh is `15ae286f`.

Native and self-host MIR both carry party role slots, ability defaults, role
implementations, `AST_BIND_STMT` with `slot_anchor`, and the resolved
`Bufferable_Put` member target. Self-host C emits the dynamic vtable field,
bind helper, role adapter, and vtable call. The bad argument mutation fails
closed as `call_arg_type_mismatch`; direct ability-call fallback is rejected.
The focused producer-first gate passed with
`backends=1 body_fixtures=20 mir_fixtures=1`; regenerated driver C built with
GCC and the fixture ran with output `12`. Full C codegen parity covered 85
fixtures, and the role-operator regression ran with output `123`. Component
contracts, shell syntax, and `git diff --check` passed. The authority edge gate
reports 44 authorities, 39 derived carriers, and
`CLOSED=24 BRIDGE=20 ACTIVE=0`; live owner/consumer negative mutations pass.
The Coq model was explicitly skipped because no `rocq`/`coqc` is installed.
The full unfiltered 268-row matrix and LLVM lane were not run.

The next observed executable seam is
`generic_multi_bound_defaults`, where parser admission stops at
`where T: Comparable + Cloneable`. The next owner must carry ordered generic
constraint facts or reject with a stable owner diagnostic; consumers must not
split source text again. `nested_generic_containers` remains a later
`ListNew` undefined-function failure. Do not add fixture-name,
party/class-name, or compatibility fallback paths.

The executable checkpoint is `e09d680e`, followed by handoff refresh
`15ae286f`. At this refresh another active task owns uncommitted parser/HIR/
semantic/MIR declaration edits, so the worktree is intentionally not recorded
as clean. Resume from `git status --short --branch` and preserve those edits.

2026-07-23 Pergyra generic Future spawn executable rungs 263-266. Objective:
carry generic specialization identity through `spawn`, then materialize Int and
String payloads through one tagged runtime invocation descriptor. Priority was
generic call-node identity, one `Future<T>` ABI owner, real worker-pool
execution, payload/arity fail-closed behavior, and negative ratchets. The
generic specialization graph fact and wrapper payload owner are the semantic
owners; `spawn_runtime_owner.pgy` is the last runtime consumer. Payload- or
arity-specific C helpers, source-name branches, synchronous lowering, and
detached local capture are forbidden.

Before closure, the Int fixture failed as `ast_artifact_invalid`, the
two-argument fixture failed behind a one-argument codegen assumption, and the
String fixture failed because `Future<String>` had no owned ABI row. Generic
specialization is now carried by call-node identity through the parent spawn.
The runtime uses one closed descriptor (`signature`, function union, value
union, and two bounded arguments) and one worker/dispatch function for Int
unary, Int binary, and String unary calls. No `new ? old` path or payload-
specific helper remains.

Counted fixtures are 263 `generic_future_spawn_int`, 264
`generic_future_spawn_multi_arg`, 265 `generic_future_spawn_string`, and 266
`generic_future_spawn_mixed`. The 261-265 focused C gate passed with
`backends=1 body_fixtures=20 mir_fixtures=5`; the mixed capstone passed as a
separate one-fixture gate with runtime output `42`, `77`, and `hi`. String
mutation fails as `let_type_mismatch` (`expected: String`, `actual: Int`), and
the Int fixtures retain their symmetric payload mismatch gates. Component,
shell, hard-contract, substitution-velocity, SoT authority, single-Gate-SoT,
and protocol-registry gates passed. Commits: `0169b856`, `3d74c9dd`,
`e6f321f2`, `793b93e5`, and capstone ratchet `366fc46b`. The full unfiltered
266-row matrix, LLVM async lane, and Coq model were not run.

The declaration-site generic-default seam described above was the next
executable seam from this point and is now closed by `ce712b8e`. The following
ability-bind falsifier is recorded in the current handoff; do not turn nearby
generic fixture enrollment into a substitute for that executable replacement.

2026-07-23 Pergyra named Future spawn/await executable rung 262. Objective:
materialize a named `Future<Int>` spawn result as the owner-directed
`PgyTaskHandle` ABI value and consume it through `await task`. Priority is handle
identity, ABI type ownership, worker-pool lifetime, fail-closed payloads, then
patch size. `FuturePayloadTypeOpt`, `spawn_runtime_owner.pgy`, ABI layout, and
the semantic graph await branch are the owners/last consumers. Treating Future
as a scalar, lowering synchronously, fixture-name branching, and detached
local-storage capture are forbidden.

The previous driver rejected `let task: Future<Int> = spawn Inc(4)` with
`unsupported let type ... Future<Int>`. `EmitLet` now registers the typed
`PgyTaskHandle`, and named await validates the owned payload before emitting
`pgy_await_take(task, long long)`. Filtered C producer-first parity passed
(`backends=1 body_fixtures=20 mir_fixtures=1`) with native output `5`;
component contracts, shell syntax, and `git diff --check` passed. Implementation
commit: `11367f33`; negative contract ratchet: `3f2ba459`; both are pushed with
`HEAD=origin/main=3f2ba459`. The full unfiltered 262-row matrix and LLVM async
lane were not run.

The next observed executable failure is
`tests/cases/backend_compare/generic_future_spawn_int/main.pgy`: the driver
returns `ast_artifact_invalid`, owner `SemanticAstInitializerTypeFacts`, with
`node_count: 17`. Repair the parser-owned generic Future artifact or fail
closed; do not add a generic-name exception or synchronous fallback.

2026-07-23 Pergyra inline spawn/await executable rung 261. Objective: carry
inline `spawn` identity, its `Future<T>` result type, and the owned worker-pool
await boundary from the expression graph through MIR and C. Priority is async
graph identity, carried type, real runtime lifetime, fail-closed unsupported
shapes, then patch size. `AstExpressionNodeSpawn` and the semantic scalar-type
owner provide the graph/type facts; `spawn_runtime_owner.pgy` owns the C ABI and
is the last runtime consumer. Forbidden fallbacks are sequential call lowering,
source-text spawn detection, fixture-name branching, and detached local-storage
capture.

Before this change the self-host driver rejected `await_inline_spawn` with
`initializer_type_unresolved` because `spawn` was a leaf without an owned async
fact. The parser now preserves `spawn` as a unary graph node, semantic typing
derives `Future<Int>` for the bounded direct `Int -> Int` shape, and codegen
dispatches through the then-current unary spawn owner plus the owned
`pgy_await_take` boundary. Pool startup is explicit; unsupported spawn shapes
fail closed. That unary runtime shape was superseded by the tagged descriptor
in rungs 264-266 above.

Filtered C producer-first parity passed (`backends=1 body_fixtures=20
mir_fixtures=1`) with runtime output `5` and `10`. Component contracts and all
modified shell syntax passed, and `git diff --check` passed. Implementation
commit: `e98ba4ac`, pushed with `HEAD=origin/main=e98ba4ac`. The full unfiltered
261-row matrix and LLVM async lane were not run; Coq/Rocq remains unavailable.

The former `async_spawn_await` failure was closed by the named-Future rung
above. Preserve the same owner-directed handle and await boundary in the next
generic Future artifact rung.

2026-07-23 Pergyra graph-owned foreach iterable executable rungs 251-252.
Objective: derive a non-identifier foreach iterable type from the parser-owned
expression graph and carry that type through iteration facts, the synthetic
hoist, MIR, and codegen. Priority is graph identity, homogeneous array-literal
typing, nominal member typing, fail-closed mismatch, then patch size. The
array-literal graph owner owns recursive literal element/type evidence;
`SemanticAstIterationTypeFacts` is the last semantic consumer. Forbidden
fallbacks are reparsing the loop payload, assuming `Array<Int>`, special-casing
fixture/member names, or letting codegen rediscover the iterable type.

The previous driver rejected both `for_in_array_literal_iterable` and
`for_in_member_iterable` with `statement_type_unresolved` and `actual:
Unknown`. Iteration verdicts now consume the carried value/auxiliary graph
roots. A non-empty homogeneous literal derives `Array<T>` recursively, while
`b.items` resolves through the nominal member owner. Both produce an explicit
`Array<Int>` iteration fact and one `__pgy_forin_0` hoist. The DRV-2 MIR
manifest now contains 260 rows.

Filtered producer-first parity passed both fixtures for C-built and LLVM-built
Pergyra drivers (`backends=1 body_fixtures=20 mir_fixtures=2` in each lane).
Runtime matches the native oracle: the literal loop prints `60` and the member
loop prints `15`. A heterogeneous literal fails closed as `actual: Unknown`;
replacing `b.items` with `b` fails closed as `actual: Bag`; the native compiler
rejects both. Component and shell contracts pass. Implementation commit:
`8cc9ad68`; graph-only contract ratchet: `54bed08e`. The full unfiltered
260-row matrix was not run.

The former `await_inline_spawn` failure was closed by the subsequent inline
spawn/await rung above. Preserve real concurrency, an owned result/wait
boundary, and runtime lifetime in the named-Future rung above.

2026-07-23 Pergyra stable fieldless nominal owner executable rung 250. Objective:
carry a valid empty `fields` inventory for `Calc` through the nominal/type
environment owner into C codegen. Priority is owner-presence identity,
fieldless ABI materialization, runtime parity, then missing-fact failure. The
type environment presence owner is `LookupKindTypeRowPresent`; method binding
and semantic struct-call emission are its last consumers. Forbidden fallbacks
are treating a missing row as an empty row, faking a field, or adding a
class-name exception.

The previous Pergyra driver reached codegen but failed with `method owner field
inventory is missing` for `fieldless_class_method`. The owner now carries
`Calc=fields:` as a present empty row, emits a reserved C storage byte and a
zero initializer, and rejects `Calc(1)` with `call_arity_mismatch`. The rebuilt
driver emitted and ran standard C with output `r=7`. Filtered producer-first
source/MIR parity passed the fieldless fixture (`backends=1 body_fixtures=20
mir_fixtures=1`), and component contracts plus shell syntax passed. Commit:
`8afd9160`.

The former broader failure was `await_inline_spawn`; it was closed by the
subsequent graph-owned spawn/await rung. Do not erase concurrency or add a
sequential fallback in later async work.

2026-07-23 Pergyra stable StringConcat alias executable rung 249. Objective:
project the stable `StringConcat(String, String) -> String` source name through
the same semantic signature and generated concat runtime ABI already owned by
`Concat`. Priority is stable builtin row identity, one runtime symbol owner,
fail-closed argument types, then patch size. `SemanticBuiltinSignatureRows` is
the source signature owner and `RuntimeCallCName` is the last runtime-symbol
consumer. Forbidden fallbacks are a `StringConcat` branch in semantic call
emission, a second C helper, source-text substitution, or shifting existing
builtin row identities.

The previous Pergyra driver rejected both `string_utility_aliases` and
`nested_array_string` with `undefined_function: StringConcat`. The new
signature is append-only row 101 (the 102nd row), so every existing builtin
index stays stable. `Concat` and `StringConcat` both project to
`StringRuntimeCConcatFn`; the emitted program contains `pgy_concat` and no
`StringConcat` C symbol. Manifest fixture 249 is `string_utility_aliases`, and
the DRV-2 MIR manifest now contains 257 rows.

Filtered producer-first source/MIR parity passes fixture 249 for both the
C-built and LLVM-built Pergyra drivers (`backends=1 body_fixtures=20
mir_fixtures=1` in each lane). Runtime output matches the native oracle:
`HELLO`, `world`, `ok:Hello World`. Mutating the second alias argument from the
owned `String` local to `Int` fails closed with `call_arg_type_mismatch`; the
native compiler rejects the same mutation. Component contracts and shell
syntax pass. The full unfiltered 257-row matrix was not run.

The next smaller observed synchronous failure is
`fieldless_class_method`: after the alias closure, the driver reaches codegen
and fails with `method owner field inventory is missing`. Audit the nominal
declaration/environment owner for a valid zero-field inventory before changing
emission; do not fake a field or add a class-name exception. `await_inline_spawn`
remains the broader async failure and still must not gain a sequential fallback.

2026-07-23 Pergyra contextual Result-field executable rung 248. Objective:
allow an `Ok(T)` or `Err(E)` graph value to enter a nominal constructor field
whose declared type is `Result<T, E>` without inventing the missing wrapper
parameter or adding constructor-name policy. Priority is the declared
field/parameter type, canonical wrapper assignability, fail-closed mismatch,
then patch size. `ResultTypeAssignableTo` is the assignability fact owner;
`SemanticExpressionGraphFieldValueAssignableTo` and `CompareCallArgs` are the
last consumers reached by this rung. Forbidden fallbacks are `Wallet`/`Cell`
branches, parsing `Ok` as a special constructor at the consumer, accepting all
Result pairs, and guessing an error type into the produced value.

The previous Pergyra-built driver rejected both `Wallet(Ok(100))` and
`Cell(10, Ok(5))` as `Result<Int>` versus `Result<Int, E>`. The field graph
consumer and the remaining non-generic call consumer now delegate to the same
canonical assignability owner instead of maintaining `Result<Unknown>` and
strict-string-equality rules. Manifest fixture 248 is
`result_as_class_field`; the active manifest therefore contains 256 DRV-2 MIR
rows and the counted executable frontier advances from 245 to 248. No class,
variant, or error-enum name appears in the semantic implementation.

Filtered producer-first source/MIR parity passes the new fixture for both the
C-built and LLVM-built Pergyra drivers (`backends=1 body_fixtures=20
mir_fixtures=1` in each lane). Both drivers preserve the declared
`Result<Int, CardErr>` field, the carried `Wallet`/`Ok`/`Err` call graph, the
typed Result C ABI, and native-oracle runtime output. A source mutation from
`Ok(100)` to `Ok("bad")` fails closed with
`call_arg_type_mismatch`; the native compiler also rejects that program. The
wrapper policy contract independently keeps incompatible payload and explicit
error types negative. Component contracts and shell syntax pass. The full
unfiltered 256-row matrix was not run, and Coq/Rocq remains unavailable.

The next known real failure is `await_inline_spawn`, where the Pergyra driver
reports `initializer_type_unresolved` because `spawn` is still a leaf rather
than an owned async expression/type/codegen fact. That is a multi-owner async
rung, not a safe one-line continuation; the next session must either frame its
objective card and falsifying graph or select a smaller observed synchronous
failure. Do not add a sequential fallback for `spawn`.

2026-07-23 LLVM runtime-call argument ABI closure. Objective: compile
Pergyra-owned wrapper/assignment code when a dependent-return expression is a
runtime call argument, without adding another C call-name policy. Priority is
one ABI owner, removal of the AST re-scan fallback, fail-closed emitted-value
validation, then patch size. The registered `LLVMFuncEntry::fn_type` parameter
is the fact owner; `llvm_emit_function_call_args` is the last legitimate
consumer. Forbidden fallbacks are `UnwrapOption` name branches, re-walking an
AST call through `mir_source_local_call_expr_type_name`, source-text recovery,
and unchecked `LLVMBuildCall2` arguments.

`llvm_emit_function_call_args` now scopes the registered parameter's LLVM type
as `expected_abi_type`, restores the prior context after inference, and rejects
argument count or emitted LLVM value type drift from the same function ABI.
Call inference can consume only that scoped boundary fact; it does not learn
Option/Result helper names. The interim native MIR call re-scan, its
`UnwrapOption` branch, and the Option type-text helper from `e3cc1375` were
removed after an isolated HEAD-plus-ABI build proved them unnecessary.

The wrapper policy LLVM probe and assignment projection pass on both C and
LLVM, including missing expected type, target type, call target, C binding, and
collection `cref`-only fail-closed negatives. The 8-fixture C/LLVM codegen
shard also passes (`rung-0..21`) for
`array_sum,array_push,array_pop,array_param,for_sum,for_each,option_try,result_try`.
The negative ratchet rejects restoration of the AST/MIR call re-scan or a
native `UnwrapOption` type guess and requires ABI context restoration plus
emitted-value validation.

The next executable falsifier is not selected from the passing focused gates;
the full unfiltered matrix remains an explicit budget omission. The MIR
declaration inventory smoke gate still has a pre-existing unrelated baseline
failure in `src/codegen/transpiler.c`
(`emit_class_decl_from_mir_header(header, ctx)`), so it is not evidence against
this closure.

2026-07-23 Pergyra function-binding consumer SoT follow-up: collection
mutation targets (`ArraySet`, `ArrayPush`, `ArrayPop`) now read their C binding
through `CodegenCollectionTargetCBindingOrDie`, and `let`, `try-let`, range-loop,
and foreach bindings use `CodegenFunctionValueBindingFactFor`. Statement and
try-let emitters no longer sanitize binding names locally; a missing collection
binding fails closed with its owned diagnostic.

The focused assignment projection C leg, C codegen parity for
`array_sum,array_push,array_pop,array_param,for_sum,for_each,option_try,result_try`,
filtered producer-first DRV-2 for `class_with_array_param`, component contract,
hard-substitution contract, substitution-velocity contract, authority gates,
and shell syntax all pass. The DRV-2 witness reports `backends=1
body_fixtures=20 mir_fixtures=1` and proves a reference collection parameter
keeps the owner-projected dereferenced `cbind`. Coq/Rocq was explicitly skipped
because no prover is installed.

The next executable falsifier recorded at this checkpoint was the LLVM
assignment projection `UnwrapOption` return-type failure. It is closed by the
runtime-call argument ABI entry above; the temporary native MIR call-name path
is not part of the closed design.

2026-07-23 Pergyra function-value binding SoT closure: the focused assignment
projection gate first reproduced `assignment target C binding fact is missing`
after its TestHarness path manifest grew from 15 to 21 rows. The compiler
emitter was already correctly fail-closed; the probe had supplied an empty
`cbind` environment. `function_binding_env_owner.pgy` now owns one
`CodegenFunctionValueBindingFact` carrying source identity, semantic type,
runtime kind, C binding name, and the complete type-environment row. Function
definitions, prototypes, generic prototypes, locals, and the focused probe
consume that fact. `EmitAssign` no longer imports the symbol owner and cannot
use target text or locally remangle it as a C binding.

The positive assignment probe still emits the five pinned scalar/Option/indexed
rows. Missing expected type, indexed target type, direct call target, and C
binding each fail closed with distinct diagnostics. The default focused gate
passes; its LLVM leg is an explicit unavailable-backend skip. Focused C codegen
parity passes `func_call,option_int_core` through rung `0..21`, and filtered
producer-first DRV-2 passes `owner_field_assignment` with `body_fixtures=20
mir_fixtures=1`. Component contract, shell syntax, authority-edge (`43
authorities`, `38 derived`, `CLOSED=23 BRIDGE=20 ACTIVE=0`), and live
owner/negative-mutation adequacy pass. The hard-substitution and
substitution-velocity contracts also pass after their stale scalar call-target
and payload-free-only enum assertions were aligned with the ordered member
array and common enum ABI path. Coq/Rocq remains unavailable and was explicitly
skipped, not claimed checked.

No next executable failure was observed by these focused gates. Select the next
rung from a failing probe against one of the ledger's five direct substitution
blockers; do not convert a passing coverage fixture into an inferred blocker.

2026-07-23 Pergyra enum ABI value SoT closure: the new
`codegen/abi_layout/enum_abi_value_fact_owner.pgy` owns one explicit projection from
semantic enum layout to C value type and bare-return default. Payload-free and
tagged layouts differ only inside that owner. General ABI value/field/default
consumers and `Option<T>` materialization now consume `EnumAbiValueFact`; they
no longer repeat layout switches or maintain an Option-private enum mapping.
Unknown or missing enum layout returns an invalid fact, and the ABI readiness
contract fails closed rather than guessing storage.

The executable fixture `option_payload_free_enum_field_declaration` covers an
`Option<Failure>` nominal field plus `Some(Timeout)`, `Some(Reset)`, contextual
`None`, outer Option match, and inner payload-free enum match. Native C and the
Pergyra-origin codegen tool both run as `7`, `9`, `0`. Focused C codegen parity
reports rung `0..21` green; filtered producer-first DRV-2 reports `backends=1
body_fixtures=20 mir_fixtures=1`; shell syntax and component contract pass.
The active manifests are 85 codegen fixtures, 255 DRV-2 MIR rows, and 21
TestHarness codegen paths. LLVM was unavailable for the focused DRV-2 gate,
and the full unfiltered matrix was not run.

The next observed executable failure at that checkpoint was the
assignment-projection C leg:
after consuming the 21-path TestHarness manifest it fails closed with
`assignment target C binding fact is missing`. That seam requires a fresh
owner/last-consumer audit; it is not silently attributed to the enum ABI work.

2026-07-23 Pergyra canonical value-wrapper declaration SoT closure: semantic
type identity now comes from `SemanticCanonicalTypeName`, while
`codegen/input/value_wrapper_usage_owner.pgy` recursively inventories concrete
by-value `Option` and explicit `Result` nodes from one semantic type surface.
`type_declaration_emit_owner.pgy` schedules that inventory together with
nominal and enum declarations. It no longer creates an Option wrapper as a
side effect of every nominal/enum declaration, and there is no Result-only
inventory or post-hoc wrapper scan. The graph for the active witness is
`Payload/Failure -> Result<Payload, Failure> -> Option<Result<Payload,
Failure>> -> Envelope`.

The same canonical identity feeds runtime facts and C symbols. Result facts
canonicalize at their boundary; Option facts carry both semantic inner type and
owned C value type, recursively consume an already scheduled Result/Option,
and derive helper/type names through `CompilerSymbolCMangledTypeName`. The C
symbol owner trims trailing separators. The bootstrap C mirror uses the same
generic `sanitize_c_suffix` projection for Option C types and contextual
`None`, so it no longer emits `PgyOption_Result<Payload,Failure>`.

The formal positive fixture
`nested_option_result_field_declaration` compiles and runs as `0` through both
native C and the Pergyra-origin codegen tool. A new
`Loop -> Option<Result<Loop, Failure>> -> Loop` negative fixture fails closed
in both paths with `cyclic by-value type declaration dependency`; it reaches
neither an incomplete C type nor GCC. The existing direct and Result-mediated
cycle negatives remain green. Component ratchets require the recursive
canonical inventory and sanitized native Option projection, and reject the
retired Result-only inventory and nominal-triggered Option path.

Fresh evidence: focused C codegen parity reports rung `0..21` green with three
fixtures, equal runtime output, three native cycle rejects, and the matching
self-host rejects. Filtered producer-first DRV-2 reports `backends=1
body_fixtures=20 mir_fixtures=3`; component contract passes. The active
manifests are 84 codegen fixtures, 254 DRV-2 MIR rows, and 21 TestHarness
codegen paths. The full unfiltered and LLVM matrices were not run for this
focused rung.

At that checkpoint, the next falsifier was observed rather than inferred.
`Result<Option<Payload>,
Failure>` and `Option<Option<Payload>>` both compile and run as `0` on native
and Pergyra paths, so they are coverage-only. A nominal field
`Option<Failure>` where `Failure` is payload-free compiles on native C but the
Pergyra codegen fails closed with `constructed Option declaration fact is
missing`. The next objective is one enum ABI value fact that lets Option
materialization consume both payload-free and tagged enum layouts without an
enum-name switch or a second wrapper scheduler.

2026-07-23 Pergyra generic enum-payload declaration/match SoT closure: the
semantic enum owner carries ordered `(variant, payload_index, type)` rows
through native MIR declaration metadata and the JSON `param_types` array. The
canonical HIR match-pattern fact owns an ordered binding array; the semantic
environment validates binding/payload cardinality and type, MIR carries every
binding/type row, and selfhost `mir_lower` emits one contiguous tagged-enum
projection graph per payload field. Codegen consumes those same owner facts and
maps bindings to `_0.._N`; it does not branch on a fixture name, variant name,
or fixed arity. The former `payload enum variants are not supported` rejection,
singular match binding, and singular `param_type` wire are retired. Tagged enum
equality remains an explicit negative gate because it is not yet an owned
semantic operation, not because payload enums are generically rejected.

Fresh executable evidence: filtered MIR JSON parity passed
`enum_option_payload,enum_multi_payload` as `2 fixtures, 0 clean rejects`, with
missing/unknown payload rows rejected. Filtered source/codegen parity passed the
multi-payload fixture and direct C execution produced `0`, `75`, `28`, `120`,
`81`; the committed full C codegen checkpoint passed all 78 fixtures. The
producer-first DRV-2 source/MIR gate now passes `enum_multi_payload` with
`body_fixtures=20` and `mir_fixtures=1`, and its removed second `Rect` binding
type mutation fails closed. Component, match-binding carrier, compiler-world,
single-Gate-SoT, and registry authority-edge gates are green; authority-edge
reports `CLOSED=23 BRIDGE=20 ACTIVE=0`. Coq/Rocq remains unavailable on this
Windows runner, so the formal model is an explicit declared skip while the live
owner/consumer and negative-mutation portion passes.

The stale direct DRV-2 `enum_multi_payload` blocker is closed. The next
falsifying case should challenge type generality rather than add another arity
branch: an owner-carded heterogeneous or nested aggregate enum payload must
flow through direct source codegen and DRV-2 using the same ordered facts. The
exact fixture is not selected yet; tagged equality must stay negative unless a
separate semantic-operation owner and diagnostic contract are deliberately
defined.

2026-07-23 Pergyra whole-language match-binding type carrier delta: the native
semantic owner now records every payload binding type with stable
`(function_syntax_id, match_case_syntax_id, binding_index)` identity for
`Option`, `Result`, and user enum patterns. HIR and MIR copy that owned fact;
MIR match instructions and JSON expose only the copied execution fact, and a
missing row fails lowering instead of reopening the source AST or deriving a
type from a variant name. The Pergyra `mir_lower` local-fact and render owners
require exact binding/type cardinality plus concrete types and always emit a
typed binding. The pre-delta witness reconstructed `Let: v =
UnwrapOption(val)`; the closed path reconstructs `Let: v : Int =
UnwrapOption(val)`. Deleting the row or replacing it with `Unknown` fails with
a `match_binding_type` diagnostic.

This is an executable integration rung, not manifest fixture 248. The numbered
DRV-2 fixture frontier remains 245 because rows 246 and 247 are coverage-only
ratchets. A freshly Pergyra-built `mir_lower` consumed native `option_match`
MIR, its typed re-AST was compiled by the Pergyra codegen path, and the result
matched the native oracle at runtime output `42`. Native `Option`, `Result`,
and multi-arity enum MIR probes carry the expected distinct binding types.
`match-binding-type-fact-test-smoke`, the complete native MIR unit slice
(`152 passed, 0 failed`), C and LLVM compiler builds, component, substitution-
velocity, AST-to-MIR loss, and SoT authority gates pass; the registry now
reports `CLOSED=23 BRIDGE=20 ACTIVE=0`.

The carrier is language-wide, but this does not falsely declare all match
execution closed. The current Pergyra reconstruction owner still has bounded
`Some`/`Ok`/`Err` unwrap rendering; generic user-enum payload extraction and
multi-binding execution remain a later executable rung. An earlier unfiltered
MIR parity attempt stopped before `option_match` while compiling concurrent
Pergyra codegen edits. After those edits advanced, a fresh current-tree
`option_match`-filtered run passed the complete native-MIR -> Pergyra
`mir_lower` -> Pergyra codegen -> C oracle lane (`1 fixtures, 0 clean rejects`).
The full unfiltered matrix was not rerun, so it remains an explicit integration
omission rather than a claimed green gate.

2026-07-23 Pergyra nested-coalesce-chain coverage ratchet: DRV-2 MIR manifest
row 247 is `nested_coalesce_chain`. Two `HalvedIfPositive` calls carry
`Option<Int>` through `?? fallback` into the local `first -> second` value
chain. The existing coalesce semantic owner remains the sole payload-
compatibility owner; the new parity owner checks both graph nodes and direct
target cardinality, then rejects a missing nested target or mutated coalesce
kind. The pre-existing 245-hard driver accepts the unmodified row, so this is
not an executable replacement delta and the counted frontier remains 245. Its
first coalesce-kind mutation fails at the owned initializer boundary with
`initializer_type_unresolved`; the parity owner pins that actual diagnostic.
No source reparse, fixture-name branch, native-MIR injection, backend
reconstruction, or fallback inference was added. Focused C, LLVM, and the
freshly Pergyra-built 245-hard producer-first parity pass with
`body_fixtures=20` and `mir_fixtures=1`; hard native/self canonical MIR is
byte-equal at SHA-256
`3C518BBC3E89A82FFA538F99F6E205F8F60A8A4E16DF18E6BA20283A0ACDF7CF`, hard
oracle/self/source emitted C is byte-equal at SHA-256
`B5E682F33D9CED51C492C5C4ED6BDC5AC12A47CE21519457DFB7543BE8F50F6E`, and
runtime output is `10`, `5`, `2`, `49`, `0`. Component and shell-syntax gates
pass. This coverage ratchet does not advance the executable substitution
frontier; fixture 248 is not selected.

2026-07-23 Pergyra Option-Bool coalesce-condition coverage ratchet: DRV-2 MIR
manifest row 246 is `coalesce_in_if_condition`. The typed
`MaybeFlag(arr[i]): Option<Bool>` fact flows through `?? false` into an `if`
inside the `count`/`i` array loop. The dedicated parity owner checks the direct
target, indexed element, coalesce graph, and loop phis, then rejects missing
target, index identity, operator kind, and phi input mutations.

This row does not advance the executable substitution frontier. The existing
245-hard driver already accepted it, and the 246 slice changes the manifest and
parity ownership without changing a Pergyra semantic implementation owner.
Accordingly, the counted executable frontier remains fixture 245. Treat row 246
as useful whole-language breadth and a negative regression ratchet, not as a
replacement delta; no source reparse, fixture-name branch, native-MIR injection,
backend reconstruction, or fallback inference was added.

Focused C, LLVM, and the freshly Pergyra-built 245-hard driver pass producer-
first parity with `body_fixtures=20` and `mir_fixtures=1`. Hard native/self
canonical MIR is byte-equal at SHA-256
`CB6F19B233F03FC3C16551F9DEA57A80801020B509FB658E2601AE4B9CF79138`; hard
oracle/self/source emitted C is byte-equal at SHA-256
`CDFFD8220B8FD9943D0DB116E55D1B687BB046BDB8E79EFB486A5DE6A0BF767B`; runtime
output is `3`, `0`, `1`. The focused component and shell-syntax gates pass.
The full 246-row manifest matrix remains omitted under the 30-minute budget;
the last complete unfiltered matrix is 230/230, and the next executable
replacement fixture is not selected.

2026-07-23 Pergyra collection-option-coalesce-loop delta: DRV-2 MIR fixture
245 is `coalesce_accumulate_loop`. The executable seam keeps the typed
`Array<Int>` element identity through `ParityVal(arr[i]): Option<Int>`, the
`??` coalesce node, and the `total`/`i` loop-carried state. The semantic owner
`SemanticExpressionGraphConcreteScalarValueOwned` now admits a coalesce node
only when `OptionCoalescePayloadTypeOpt` proves the wrapped and fallback scalar
types compatible; MIR and the parity owner consume that fact without source
reparse, fixture-name branching, native-MIR injection, backend reconstruction,
or fallback inference. The previous 244-hard and its native-oracle bridge are
the rejection witnesses: both classified the enclosing addition as `Int +
Option<Int>` and failed with `binop_type_mismatch` instead of silently lowering
the expression.

Focused C, LLVM, and freshly Pergyra-built 245-hard producer-first parity pass
with `body_fixtures=20` and `mir_fixtures=1`. The hard driver is
`.tmp/bin_coalesce_accumulate_loop_245_hard/pgy-self-driver.exe`, SHA-256
`30D5204624512EEBDF39827F271E611AAC0C4AC73CAAA616CC4BC5729ED79ED3`, with a
245-row manifest. Hard oracle/self canonical MIR is byte-equal at SHA-256
`779AC39186B42C828EB671016B4A3D9B02FBAED97550F2BE899FF37A63E2B84D`; hard
oracle/self/source emitted C is byte-equal at SHA-256
`2D044764A426ACF6AB5BFFE44D7E639668197CD44D8A74CD298817DD0ED0D549`;
runtime output is `117`, `18`, `-1`, `0`. Removing `ParityVal`, changing the
index node to a leaf, changing `coalesce` to `logical_or`, or deleting a loop-
phi input fails closed. The focused component and shell-syntax gates pass.
The eight-fixture current-hard Option/coalesce/array impact shard, loop-flow,
diff, SoT authority, single-gate-owner, seven-row protocol registry, and
substitution-velocity gates also pass. The full 245-row matrix remains omitted
under the 30-minute budget; the last complete unfiltered matrix is 230/230 and
the next executable replacement rung is not selected. Manifest row 246 below is
coverage only.

2026-07-23 Pergyra collection-enum-match-loop delta: DRV-2 MIR fixture 244 is
`array_match_action_sim`. The executable seam is intentionally whole-language:
`prices[i]: Int` flows through `DecideOf -> Action`, an exhaustive `match`, and
the `cash`/`shares`/`i` loop-carried state. The 243-hard baseline exposed the
real missing fact: its Pergyra MIR-to-structured-AST path could reach a candidate
merge only after re-entering the same branch through the loop backedge, so it
reconstructed the increment and `Continue` twice and then failed canonical MIR
consumption. `MirRoutineGraphIsSameIterationMerge` now owns the current-
iteration CFG condition: both arms must reach the candidate without re-entering
the branch. `routine_fact_index_owner.pgy` consumes that fact, while the CFG
query remains in `mir_cfg_graph_owner.pgy`; no C fragment, source reparse,
fixture-name branch, native-MIR injection, backend reconstruction, or fallback
was added.

Focused C, LLVM, and freshly Pergyra-built 244-hard producer-first parity pass
with `body_fixtures=20` and `mir_fixtures=1`; the corrected structural probe has
exactly one increment and one `Continue`. The new driver is
`.tmp/bin_array_match_action_sim_244_hard/pgy-self-driver.exe`, SHA-256
`F742594D3F60704CA5FA24153E7CB9364C3679974FD3308543B8E3CFFDE6DE9A`,
with 244 manifest rows. Native/self canonical MIR is byte-equal at SHA-256
`751E8420182B99A7BEF93D45FF5B0D811F2D588D15A8FDAE6068CDD5CEF86EBD`;
hard oracle/self/source emitted C is byte-equal at SHA-256
`DC42840CD11F49A56512158289E66B35A84372713160224F5C4BACF5F2810773`;
runtime output is `1060`, `1000`, `1000`. Removing a required call target,
changing the index graph kind, removing `Hold`, deleting a loop-phi input, or
changing the collection to `Array<String>` fails closed. The eight-fixture
array/enum/match impact shard passes.

The focused component, loop-flow, structural, shell, diff, SoT, protocol, and
substitution-velocity gates pass. A broader unfiltered `mir_json_parity.sh` run
stopped at the pre-existing `option_match` carrier gap: native MIR carries
`match_bindings` but not `match_binding_types`, and the Pergyra renderer
correctly refuses to infer the missing type. This is recorded as an open broader
gate, not fixture-244 success. Full 244 remains omitted under the 30-minute
budget; the last complete matrix is 230/230 and fixture 245 was not selected at
that checkpoint.

2026-07-23 Pergyra indexed-array-to-method composition delta: DRV-2 MIR
fixture 243 is `class_param_method_arr`. `rates[i]` keeps its `Int` identity
from the `Array<Int>` parameter through the index node into the
`Bag2.Worth(rate: Int)` member call, and the result joins the `total` loop phi.
The existing class/array composition owner was generalized for both fixture
242 and 243; no C-shaped second owner was added. No source reparse,
method-name/C-type guess, backend array-element reconstruction, native-MIR
injection, fallback, or runtime fragment was added.

Focused C/LLVM/current-242-hard/new-243-hard producer-first parity passed in
separate lanes. The new driver is
`.tmp/bin_class_param_method_arr_243_hard/pgy-self-driver.exe`, SHA-256
`4A60C32EDA22778441FB3A309C88F0CF3378006AA6807407EAB82B6DF85F8697`,
with 243 manifest rows. All four lanes produced canonical MIR SHA-256
`854D22B250D3FA04F067050079FA7D10581316EDA0258C5769C2F4FF53D7848F`.
Hard oracle/self/source emitted-C SHA-256 is
`848F3290CF90348203718BF88B7B2E05FA88B64D9685837CBDFE9D15E61EB882`;
runtime output is `1800`, `100`, `0`, `0`. Removing `Bag2`, `Bag2_Worth`, or
`TotalWorth`, changing the index node to a leaf, or changing the array element
type to `String` fails closed; the last case reports
`call_arg_type_mismatch`. The eight-fixture class/array hard shard and all
component, shell, diff, SoT, protocol, and substitution-velocity gates pass.

A diagnostic combined LLVM/previous-hard run stopped on the existing
`valid_compound_local` body fixture because the two driver runtimes frame an
otherwise identical C artifact with two terminal LF bytes versus one. The
separate official-style lanes are green and fixture 243 canonical semantics
are identical; this stdout-framing observation is not recorded as semantic
parity. Full 243 remains omitted under the 30-minute budget; the last complete
matrix is 230/230 and fixture 244 was not selected at that checkpoint.

2026-07-23 Pergyra class/array composition delta: DRV-2 MIR fixture 242 is
`class_with_array_param`. `FillArr` creates, mutates, loops over, and returns
one typed `Array<Int>` value; `SumWith` consumes that same array together with
the nominal `Slot2` value through indexed reads and loop-carried totals. One
class/array composition owner checks the function signatures, array and scalar
phi chains, nominal member graphs, indexed-assignment graph, and exact target
cardinality. No C pointer or array-shape guess, source reparse, fixture compiler
branch, native-MIR injection, backend fallback, or runtime fragment was added.

Focused C/LLVM/current-241-hard/new-242-hard parity passed. The new driver is
`.tmp/bin_class_with_array_param_242_hard/pgy-self-driver.exe`, SHA-256
`73499B3EAE8688A7DB9E2E8FD72467E6F3628E5CF61BB9FC446CC9B24C4BADDC`,
with 242 manifest rows. Hard canonical MIR SHA-256 is
`1DBD9F2297163F4C725FCE3C90ADD59DF71E9BDA3741F06669C455CF7AE9CB65`;
emitted-C SHA-256 is
`08A649224DB7521A377B401DF2450A14F065AAD5DABFB38CB95A392E2C6F27A6`;
runtime output is `93`, `146`, `138`, `225`. Removing a required target or the
indexed-assignment graph fails expression-graph admission; replacing the array
parameter type with `Unknown` fails with `assignment_type_unresolved`. The
seven-fixture class/array hard shard and all component, shell, diff, SoT,
protocol, and substitution-velocity gates pass. Full 242 remains omitted under
the 30-minute budget; the last complete matrix is 230/230 and fixture 243 is
not selected.

2026-07-23 Pergyra nominal/builtin identity delta: DRV-2 MIR fixture 241 is
`class_user_box`. A user-declared `class Box` remains a nominal class even
though its name collides with the generic builtin `Box<T>`. The typed graph
carries `New -> Box_WithWeight -> Box_Heavy` through fluent temporaries and
then projects `weight` and `label`. A named nominal-collision owner checks the
class declaration, methods, return identity, target cardinality, and negative
mutations. No spelling-based builtin precedence, generic-shape guess,
method-chain source reparse, fixture compiler branch, C type guess,
native-MIR injection, or backend fallback was added.

Focused C/LLVM/current-240-hard/new-241-hard parity passed. The new driver is
`.tmp/bin_class_user_box_241_hard/pgy-self-driver.exe`, SHA-256
`61DDEF412F281EFBF3DE8D72220C2D590256D08EDD634615E38B68E2AF5CD3FF`,
with 241 manifest rows. Hard canonical MIR SHA-256 is
`B0FD14CCFE846A752E345D4AA2DE8F6976B13AA80BAFA3B48682AFD509205AB1`;
emitted-C SHA-256 is
`AFFB7BE44EB3404D306C6AC986A5723E187BAB6799727E4FF96BFBE540B5EBFB`;
runtime output is `false`, `true`, `0`, `5`. Removing `Box`, `New`,
`Box_WithWeight`, or `Box_Heavy` fails expression-graph admission; removing
the `WithWeight` routine identity fails with `class method is missing routine
fact: Box.WithWeight`. The six-fixture nominal/class hard shard and all
component, shell, diff, SoT, protocol, and substitution-velocity gates pass.
Full 241 remains omitted under the 30-minute budget; the last complete matrix
is 230/230 and fixture 242 is not selected.

2026-07-23 Pergyra class-predicate-to-enum composition delta: DRV-2 MIR fixture
240 is `class_method_enum_classify`. `Counter.IsZero`, `IsBig`, and `IsPos`
produce typed `Bool` decisions for `Classify -> Verdict`; `Process` consumes
that enum through an exhaustive match and then reads the same `Counter.value`.
The prior enum-to-class test owner was migrated to one class/enum composition
owner covering both directions. The old owner path is deleted and statically
rejected. No method/variant name compiler branch, condition or match source
reparse, C enum/struct guess, native-MIR injection, backend fallback, or
runtime fragment was added.

Focused C/LLVM/current-239-hard/new-240-hard parity passed. The new driver is
`.tmp/bin_class_method_enum_classify_240_hard/pgy-self-driver.exe`, SHA-256
`D8FD169659A41883253ABCBBF636624E82E80DD8814FE6B84B57308C3EAA61EF`,
with 240 manifest rows. Hard canonical MIR SHA-256 is
`9D36D5AD76893D408F236D4A855E8DBB67C5C457E6E4108E9F6FA948ACE07D52`;
emitted-C SHA-256 is
`CD0E1060F1F0EABF650B8EEB1451B05060E5A5128E3275AB776512B87298DE1E`;
runtime output is `0`, `5`, `1500`, `3`, `99`, `1010`. Removing any predicate
member target or `Classify` fails expression-graph admission; removing `Zero`
fails the match-variant owner. The seven-fixture class/enum hard shard passes.
The fixture component contract passed after the owner migration. A later
whole-tree rerun briefly exposed the concurrent MIR-region slice's missing
`hir->region_escape_facts` term in `driver_app.c`; the current tree has since
closed that in-progress connection and the component gate is green again.
Shell, diff, SoT, protocol, and substitution-velocity gates pass. Full 240
remains omitted under the 30-minute budget; the last complete matrix is
230/230 and fixture 241 was not yet selected at that checkpoint.

2026-07-23 Pergyra enum-to-class composition delta: DRV-2 MIR fixture 239 is
`enum_to_class_match`. An exhaustive `Class` match returns the nominal `Stat`
value through three constructor arms; `Power` then carries that exact type
through the `StatOf` call into `s.val * s.scale`. One cohesive parity owner
checks the variant, constructor, direct-call, local-type, and member-consumer
spine. No arm source reparse, fixture/name compiler branch, C struct guess,
native-MIR injection, backend fallback, or runtime fragment was added.

Focused C/LLVM/current-238-hard/new-239-hard parity passed. The new driver is
`.tmp/bin_enum_to_class_match_239_hard/pgy-self-driver.exe`, SHA-256
`7DFAC543959457B623423BF72451EC3D7273E99B4E648B6D5DD92D33CAAA3109`,
with 239 manifest rows. Hard canonical MIR SHA-256 is
`C56176FBC7A957839E6564C97762D9E5E38EBB4A2D35E8ABE4CBACF8271A1C12`;
emitted-C SHA-256 is
`E528C2F62DD94ACE037EA1EA78A850AEC3DAD78A8EF55B2B3F36FA6E2667F4A0`;
runtime output is `100`, `100`, `90`. Removing the `Stat` or `StatOf` direct
target fails expression-graph admission, and removing `Tank` from the enum
declaration fails the match-variant owner. The eight-fixture enum/class/
wrapper/recursive hard shard and all static/SoT gates pass. Full 239 remains
omitted under the 30-minute budget; the last complete matrix is 230/230 and
fixture 240 is not selected.

2026-07-23 Pergyra recursive class-state delta: DRV-2 MIR fixture 238 is
`class_recursive_factory`. `Train` recursively composes the class-valued
`LevelUp(Charge(...))` result and the final `State.Power()` member call. The
typed MIR graph owns the direct targets `Train`, `LevelUp`, and `Charge`, plus
the member target `State_Power`; no source-call reparse, name reconstruction,
fixture compiler branch, native-MIR injection, C fallback, backend-local
policy, or runtime fragment was added. Removing any one target fails graph
admission.

Focused C/LLVM/current-237-hard/new-238-hard parity passed. The driver is
`.tmp/bin_class_recursive_factory_238_hard/pgy-self-driver.exe`, SHA-256
`2124BAFB7A32A02315DE68653588DDA9E29740B83965F446DA550081E1FCEFF1`,
with 238 manifest rows. Hard canonical MIR SHA-256 is
`B913B640CAC865090F25904D98A8BC6E775C5EDB221151AF595A51425851B8DC`;
emitted-C SHA-256 is
`8659DBEC896573DE8D1D465ADD332935CEBF7039BB9A9EA6AB105426F7C3A712`;
runtime output is `10`, `20`, `40`, `60`. The six-fixture recursion/class
hard shard and all static/SoT gates pass. Full 238 remains omitted under the
30-minute budget; the last complete matrix is 230/230 and fixture 239 is not
selected.

2026-07-23 Pergyra Bool short-circuit composition delta: DRV-2 MIR fixture 237
is `class_method_short_circuit`. `Counter_IsAtLeast` and `Counter_IsBetween`
return typed `Bool` values consumed by explicit `logical_or`, `logical_and`,
and `logical_not` graph nodes. No truthy C conversion, source-condition reparse,
method-name reconstruction, fixture compiler branch, or backend fallback was
added. Removing either member target fails graph admission; mutating
`logical_or` to arithmetic `add` fails at the semantic artifact boundary with
`statement_type_unresolved`.

Focused C/LLVM/current-236-hard/new-237-hard parity passed. The driver is
`.tmp/bin_class_method_short_circuit_237_hard/pgy-self-driver.exe`, SHA-256
`D63ACF6742DC35657474E4F598E3462DBBE5EEC108F7CEEA5F78BE37BD121C02`,
with 237 manifest rows. Hard canonical MIR SHA-256 is
`12237342D552943CD977FE6BBA3BA1CB365092C9C89B17559A04CE2AB151710E`;
emitted-C SHA-256 is
`28BB101C11D4D78EAE7650AD2C82DA84E017FE4B743CD6FD30B26ED1E27AA429`;
runtime output is `1`, `2`, `0`, `1`. The seven-fixture Bool/class shard and
all static/SoT gates pass. Full 237 remains omitted under the 30-minute budget;
the last complete matrix is 230/230 and fixture 238 is not selected.

2026-07-23 Pergyra nested-class composition delta: DRV-2 MIR fixture 236 is
`class_within_class_chain`. It preserves nested `Inner` identity through an
`Outer` value return and the temporary chain
`MakeOuter(...).WithNewTag(...).InnerId()`. The typed graph owns both
`Outer_WithNewTag` and `Outer_InnerId`; the call-target parity owner consumes
an explicit target list instead of a one-target special case. No dotted-text
reparse, temporary C-type guess, fixture/name compiler branch, native-MIR
injection, C fallback, backend-local policy, or runtime fragment was added.

Focused C/LLVM/current-235-hard/new-236-hard parity passed. The new driver is
`.tmp/bin_class_within_class_chain_236_hard/pgy-self-driver.exe`, SHA-256
`48BCCE98B059CAE485420EFCF769262B9F4039073DE507AD5B28AAA07543D4BC`,
with 236 manifest rows. Hard canonical MIR SHA-256 is
`C9E25AF4ED67BD39AA2637F32454EAFE11EAD993381D9BA540FB08B807D0E02B`;
emitted-C SHA-256 is
`219F6C813B42C6148406D95539A41BDF31DA307CE349838E19E90A48AE7D9D1E`;
runtime output is `42`, `1`, `42`, `99`, `100`, `10`. Removing either member
target fails graph admission. The seven-fixture nested-class shard and all
static/SoT gates pass. Full 236 remains omitted under the 30-minute budget;
the last complete unfiltered matrix is 230/230 and fixture 237 is not selected.

2026-07-23 Region callee identity SoT closure: the region escape producer now
consumes the semantic-owned `BuiltinKind` fact on each call through the AST
accessor contract. It no longer certifies a direct `Print` concat by recovering
callee spelling from source-shaped AST text; a missing semantic fact stays
HEAP. The semantic call checker records the fact, and the C/LLVM region backend
parity gate passes for certified and non-certified cases.

The focused region unit gate passes, including the missing-fact negative. The
self-hosted region-plan owner gate passes with 11 required projections and 6
producer-rejection terms; region-plan and arena smoke gates also pass. Compiler
builds pass with `LLVM_ENABLED=0` and `LLVM_ENABLED=1`, with only the existing
warning set. This closes the callee-identity fallback seam only; the full
`resource.region_allocation_plan` registry row remains `BRIDGE` until semantic
HIR/MIR retention and the remaining producer migration are complete.

2026-07-23 Pergyra Option/class loop replacement delta: DRV-2 MIR fixture 235
is `class_bump_option_match`. It proves wrapper ownership is not Result-only:
`Counter.Bump -> Option<Counter> -> Some(next) -> c -> while backedge`.
The self-hosted producer carries `Some(next): Counter`, the member target
`Counter_Bump`, and the exact state chain `c.1 -> c.3 -> c.7/c.3 -> c.10 ->
c.3`. The prior Result-named loop-phi test owner was migrated to one
`wrapper_match_loop_phi` owner shared by Result and Option; the old path is
deleted and statically rejected. Compiler semantics gained no fixture/name
branch, source re-scan, wrapper representation guess, native-MIR injection,
C fallback, backend-local policy, or runtime fragment.

Focused C-built, LLVM-built, current-234-hard, and freshly Pergyra-built
235-hard parity each passed with 20 body fixtures and one MIR fixture. The new
driver is `.tmp/bin_class_bump_option_match_235_hard/pgy-self-driver.exe`,
SHA-256
`AE573D90C1266DE447E9CC63EA71466E9F62ACFA3D348894DCB865B8C5798904`,
and its manifest has 235 rows ending in `class_bump_option_match`. On the new
focused hard lane, native/self canonical MIR SHA-256 is
`4EA70E1B407EADDE4B21F0F928CC82A2B6DDBBC39B9D3A3A9EEC0004500A7B7B`;
oracle-MIR/self-MIR/source emitted C SHA-256 is
`BFA6F1EA4FA410122B51808BE04CCF4F953CAF191F058F201B8144C932098506`;
and runtime output is `5`, `10`, `10`, `0`. Removing the Option payload type or
`Counter_Bump` fails graph admission; removing `c.7` fails the `Steps` phi
verifier. A twelve-fixture Result/Option/enum/match-phi/frontier hard shard and
component, shell syntax, diff, SoT authority, gate-owner, protocol-registry,
and substitution-velocity gates pass. The full 235-row matrix was not run
under the 30-minute integration budget; the last complete unfiltered
current-hard matrix remains 230/230. Released/default-driver replacement
remains open, and fixture 236 was not yet selected at that checkpoint.

2026-07-23 Pergyra class-method Result loop replacement delta: DRV-2 MIR
fixture 234 is `class_method_result_loop`. It closes the method-owned variant
of the same high-level state transition:
`Calc.DivBy -> Result<Int,DivErr> -> match -> acc -> while backedge`.
The self-hosted producer carries the member call target `Calc_DivBy`, `Ok(v):
Int`, `Err(e): DivErr`, and the explicit accumulator chain
`acc.1 -> acc.4 -> acc.8/acc.12 -> acc.13 -> acc.4`. The Result-match owner,
member-call owner, MIR phi owner, and existing Result/class emission owners
consume those typed facts. No fixture/name branch was added to compiler
semantics, no source re-scan or pattern inference was introduced, and no
native-MIR, C fallback, backend-local representation, or runtime fragment was
added.

Focused C-built, LLVM-built, current-233-hard, and freshly Pergyra-built
234-hard producer-first parity each passed with 20 body fixtures and one MIR
fixture.
The fresh hard driver is
`.tmp/bin_class_method_result_loop_234_hard/pgy-self-driver.exe`, SHA-256
`AFE689EDCB93AEAE7FE9CC9FDFCAD16E4F03C6AE244053EAA59A01DA27FDCE2E`.
Its hard runtime output is `104`, `-2`, `0`; native/self canonical MIR
SHA-256 is
`B11981A79C4A892A20ADC489254E896A4B01262119845DB972F92120584C1CDA`; and
hard source-MIR-C/self-MIR-C SHA-256 is
`1C0F1419F35B8F0F7AE43E47C8772A71516C11254C19C79C54F2439072495D0F`.
Removing `match_binding_types` or the `Calc_DivBy` call target fails graph
admission. Removing the `acc.8` match-success merge input fails closed with
`MIR phi facts are missing or inconsistent: RunSeries`. Component, shell
syntax, hard contract, diff, SoT authority, gate-owner, protocol-registry, and
substitution-velocity gates pass. An eleven-fixture
Result/Option/enum/match-phi/frontier hard shard also passes. The full 234-row
matrix was not run because the observed runtime exceeds the 30-minute
integration budget; the last complete unfiltered current-hard matrix remains
230/230. Released/default-driver replacement remains open, and fixture 235 was
not yet selected at that checkpoint.

2026-07-23 Pergyra loop-state replacement delta: DRV-2 MIR fixture 233 is
`class_result_chain_loop`. It closes one high-level state transition:
`Wizard -> Result<Wizard,DraftErr> -> match -> next Wizard -> while backedge`,
with explicit early returns from both error variants. The self-hosted producer
carries `Ok(after): Wizard`, `Err(e): DraftErr`, the direct `CastSpell` target,
and the exact SSA state chain `w.1 -> w.3 -> w.7 -> w.13 -> w.3`. Existing
match, SSA def/phi, Result runtime, class, array-index, and emission owners
consume those facts. No `w`-name or fixture branch was added to compiler
semantics, and there is no source re-scan, struct-copy guess, native-MIR
injection, C fallback, backend-local representation, or new runtime fragment.

Focused C-built and LLVM-built parity, current-232-hard parity, and freshly
Pergyra-built 233-hard parity each passed with 20 body fixtures and one MIR
fixture. The new driver is
`.tmp/bin_class_result_chain_loop_233_hard/pgy-self-driver.exe`, SHA-256
`19CC79B10900099F60FFF64D81B9CE13BC527E6BF831CCE7108A69BE73D91E6A`,
and its manifest has 233 rows ending in `class_result_chain_loop`. On the new
focused hard lane, native/self canonical MIR SHA-256 is
`4130D3F3B898DD0FC917A64E58483517C3CAB528645125CC5FF7243B6410BBDE`;
oracle-MIR/self-MIR/source emitted C SHA-256 is
`DDF21027CE91D240312391538980571B531EF5CDDF6AFDCD702D3FBA1FE42BA1`;
and runtime output is `100`, `0`, `20`, `0`. Removing the `Wizard` match
binding type fails graph admission; removing loop-carried input `w.7` fails the
`ManaPoints` phi verifier. A ten-fixture Result/Option/enum/match-phi/frontier
hard shard passed, as did component, shell syntax, diff, SoT authority,
gate-owner, protocol-registry, and substitution-velocity gates. The full
233-row matrix was not run because the observed runtime exceeds the 30-minute
integration budget; the last complete unfiltered current-hard matrix remains
230/230. Released/default-driver replacement remains open, and fixture 234 is
not selected.

2026-07-23 Pergyra-composition replacement delta: DRV-2 MIR fixture 232 is
`class_factory_result_wrap`. It extends the executable frontier through one
language-level composition rather than a C-shaped feature shard:
`MakeTax -> Result<Tax,TaxErr> -> match -> t.Compute() -> Int return`.
The existing semantic typed-expression environment owns the `Tax`/`TaxErr`
binding facts, MIR carries them in `match_binding_types`, and the typed member
call graph carries `Tax_Compute` to the existing Result/class emission owners.
No compiler fixture branch, source re-scan, pattern-string inference,
backend-local representation, C oracle fallback, or new runtime fragment was
added. The executable delta is the 232-row manifest plus generalized existing
Result-binding and member-call negative gates.

Focused C-built and LLVM-built parity, current-231-hard parity, and freshly
Pergyra-built 232-hard parity each passed with 20 body fixtures and the one
selected MIR fixture. The new driver is
`.tmp/bin_class_factory_result_wrap_232_hard/pgy-self-driver.exe`, SHA-256
`D565A28EF6B5C5750AE5EE45D77D0BE46A323FE22B77DCB780B62C0CCFE54F53`,
and its manifest has 232 rows ending in `class_factory_result_wrap`. On the new
focused hard lane, native/self canonical MIR SHA-256 is
`038B15579E570FC780A7CD891EDABEC7DE8863CDB2753378A637DB8A1658909B`;
oracle-MIR/self-MIR/source emitted C SHA-256 is
`91C40FC856A00A4E1944380D892BE553D4D5807A1B1103FAAEEA4A2739CDAD0F`;
and runtime output is `10`, `25`, `-1`, `-2`, `0`. Removing either the `Tax`
match binding type or `Tax_Compute` call target fails closed with
`MIR instruction expression graph is missing or invalid`. An eight-fixture
Result/Option/enum/class/frontier hard shard passed. Component, shell syntax,
diff, SoT authority, gate-owner, protocol-registry, and substitution-velocity
gates pass. The full 232-row matrix was not run because the prior 231 attempt
projected beyond the 30-minute integration budget; the last complete
unfiltered current-hard matrix remains 230/230. Released/default-driver
replacement remains open, and fixture 233 was not yet selected at that
checkpoint.

2026-07-23 executable `Result<class, enum>` delta: DRV-2 MIR fixture 231 is
`dish_result_collect`. Its active seam is one Pergyra-owned flow:
`Result<Dish,CookErr>` shape and contextual `Ok`/`Err` types -> semantic
statement result type -> MIR `match_binding_types` -> explicit Result runtime
fact -> C emission. `Ok(d)` carries `Dish`, `Err(e)` carries `CookErr`, and the
nested enum match consumes that carried binding type without a source scan,
pattern-spelling inference, fixture branch, or hidden native-C fallback. The
MIR lowerer accepts a missing binding type only while reconstructing the
explicitly named legacy oracle bridge; ordinary canonicalization and hard MIR
consumption still reject the missing fact through expression-graph admission.
Lexical version zero is now an inactive arm-local state, so a declaration in
one `if` or `match` arm cannot manufacture an outer phi or leak into a later
same-spelling binding. The Result runtime remains one generated tagged
specialization selected from semantic type usage, not a C-shaped shard per
fixture.

Focused C-built and LLVM-built producer-first parity passed with 20 body
fixtures and the selected MIR fixture. A freshly Pergyra-built hard driver at
`.tmp/bin_dish_result_231_hard/pgy-self-driver.exe` has SHA-256
`DD4A4CD6913A0EF3F329487DDAF9C34E0B4C858DC625810910374448A308DC97`,
emits a 231-row manifest ending in `dish_result_collect`, and passed both the
focused hard lane and a seven-fixture Result/Option/match/enum/frontier hard
shard. On the focused hard lane, native/self canonical MIR SHA-256 is
`57D74F8EC14255E63C1A0AC05650FF3DBF9460618F78FCB088878D2B5905AA9A`,
source/self-MIR C SHA-256 is
`49971A30E6FF299EEE6122670BDF0013DC3F5E44491868857957131268F2D123`,
and native/self runtime output is `175`, `-1`, `-2`. Removing the first
`match_binding_types` row fails closed with `MIR instruction expression graph
is missing or invalid`. The self-host component contract and `git diff
--check` pass. An attempted unfiltered 231 hard matrix was stopped after 21
runtime rows when its projected duration exceeded the repository's integration
shard budget; it is not recorded as green. The last complete unfiltered hard
matrix remains 230/230, released/default-driver replacement remains open, and
fixture 232 was not yet selected at that checkpoint.

2026-07-23 executable class-field/string-return breadth delta: DRV-2 MIR
fixture 230 is `class_suit_score`. Its `Card` fields, `String`/`Int` helper
returns, field comparisons, early returns, direct calls, and scalar/string Log
calls all consume the existing class declaration, owner-field environment,
typed expression graph, statement type, MIR, and C-emission owners. No new
semantic rule, fixture helper, source rescan, native-MIR injection, C fallback,
or backend-local policy was added. Focused C-built, LLVM-built, and freshly
Pergyra-built hard producer-first parity each passed with 20 body fixtures and
one MIR fixture. On the hard lane, native/self canonical MIR SHA-256 is
`047E19BC06678B64D5D0843FC869CB979CEF34B9EDD55EC8813AD4DCD6476547`,
source/self-MIR C SHA-256 is
`B7EC186FB9B6F66D368AD3FCB88F777ACEB644F7DB69301385F1A44468A0F3AE`,
and runtime output is `Heart`, `14`, `Spade`, `7`, `Club`, `13`. Generic
missing/invalid expression-graph mutations remain fail closed. The same new
Pergyra-built hard artifact then passed the unfiltered integration gate with
`body_fixtures=20` and `mir_fixtures=230`; one existing worker-pool-inactive
message made the documented serial execution choice visible without failing
the gate. The complete current-hard 230/230 matrix is closed;
released/default-driver replacement remains open, and fixture 231 is not yet
selected.

2026-07-23 current-hard integration closure: the 229-fixture DRV-2 frontier is
green as one sharded producer-first matrix. The Pergyra-built hard driver
matched the native C oracle's canonical MIR, emitted compilable C from both
source and MIR consumption, and produced byte-equal runtime output for every
manifest row; each shard also reran the 20 body fixtures. The integration run
found a real shared semantic omission rather than a fixture-specific gap:
native `type_is_assignable` permits `Float -> Double`, while the self-hosted
`ExpressionAssignableTo` did not. The self-hosted type-compatibility owner now
matches that widening direction, the existing 113-fixture semantic corpus
proves `Float -> Double` return/initializer acceptance and `Double -> Float`
return rejection on both C- and LLVM-built binaries, and
`array_double_aggregate_core` proves the array-element consumer. Its static
`Array<Double>` ABI row is owned by that fixture's own missing-row/wrong-ID
negative gate instead of being required from the Long/Float/Bool aggregate
fixture. `EmitLog` now consumes an explicit `Double` type fact through the
formatted-scalar path; only an explicit `String` uses the string logger, and an
unsupported type fails closed instead of falling through. Focused hard parity
for `array_scalar_aggregate_core`, `array_double_aggregate_core`, and
`owner_field_assignment` passed after the full run exposed two stale gate
expectations. The owner-field gate now follows the MIR contract: only defined
SSA versions appear in `uses`, while the version-zero parameter remains an
expression-graph leaf. The complete current-hard 229/229 matrix is closed;
released/default-driver replacement remains 0%, and no fixture 230 is admitted
yet.

2026-07-22 runtime-header/TextBuilder SoT delta: DRV-2 MIR fixture 229 is
`text_builder_lifecycle`. The hand-written generated-C allocator/TextBuilder
layout and implementation block is deleted. `runtime_header_owner.pgy` now
selects canonical runtime inline owners while the existing TextBuilder ABI
owner retains only stable call spellings; it does not duplicate runtime C.
The emitted-C runtime-header classifier is also one shared test owner consumed
by both producer-first parity compilation and the hard-driver installer, so a
narrow canonical header cannot silently fall back to headerless compilation.
The current self-codegen bootstrap rebuilt gen0, gen1, and gen2 with an empty
gen1 compile log. Focused C, LLVM, and a newly Pergyra-built hard driver each
passed producer-first parity with `body_fixtures=20` and `mir_fixtures=1`;
native/self canonical MIR is byte-identical in every lane with SHA-256
`CBA1C55B664BEC216DF874043186E3FC0FC40DEFE4BC61EC7096667163168779`,
source/self-MIR emitted C is byte-identical within every lane, and runtime
output is `PergyraLang`. Static gates reject the removed block and require the
canonical allocator/TextBuilder headers. The complete 229-fixture matrix and
released/default-driver replacement remain open.

2026-07-22 executable allocator-lane breadth delta: DRV-2 MIR fixture 228 is
`allocator_lane_boxarray`. `AllocatorScratch`, `AllocatorResult`, and
`AllocatorPersistent` all flow through the existing builtin-signature,
semantic local/type graph, MIR graph, and `BoxArray` runtime-ABI owners. The
fixture required no allocator-name switch, fixture-specific helper, C-shaped
runtime shard, source re-read, or backend-local representation. Focused C,
LLVM, and a newly Pergyra-built hard driver each passed producer-first parity
with `body_fixtures=20` and `mir_fixtures=1`; native/self canonical MIR is
byte-identical in every lane with SHA-256
`E64FCE30FAED2191271F28ECFD880EF9F382E192BCA25F7F1B662E467BA50F74`,
source/self-MIR emitted C is byte-identical within every lane, and runtime
output is `401`, `402`, `403`. Missing and invalid expression graphs remain
fail-closed through the shared negative owner. The complete 228-fixture matrix
and released/default-driver replacement remain open.

2026-07-22 executable allocator/defer SoT delta: DRV-2 MIR fixture 227 is
`allocator_defer_cleanup`. Allocator constructors, `Box<Array<T>>`
materialization, and `AllocatorDestroy` cleanup remain one Pergyra-level typed
flow: the initializer consumes the expected-type semantic graph, and `defer`
carries an ordered call-expression graph plus the direct target fact instead of
a `defer_body` source string. The consumer reconstructs the direct call only
from that graph and rejects a missing graph, an invalid graph, or disagreement
between the graph and target fact. The allocator-backed `Box<Array<T>>` boundary
includes the canonical runtime owner; the older TextBuilder materialization is
retained only as a bounded bootstrap bridge for programs that do not cross that
type boundary. No fixture-specific emitter, C-shaped allocator shard, source
re-read, dual runtime read, or backend-local fallback was added. Focused C,
LLVM, and freshly Pergyra-built hard producer-first parity each passed with
`body_fixtures=20` and `mir_fixtures=1`; source/self-MIR C SHA-256 is
`D4AFF2F749AB8A4F7FF7E5FC4AEE37A0131E321A4E9F03688C57C759820CB35A`,
canonical native/self MIR SHA-256 is
`52C9F41BEAA6C8060E0594273EDF50B715A29357CC8A3BFE8CFE81DB419CD8AC`,
and runtime output is `1201`, `1202`, `1203`, `1204`. The self-host component
and runtime-bitcode contracts pass. The complete 227-fixture matrix and
released/default-driver replacement remain open.

2026-07-22 executable breadth/parser-SoT delta: DRV-2 MIR fixture 226 is
`class_method_coalesce_call`. Parenthesized expressions now re-enter the
existing `ParseExprFact` precedence owner, so grouped `??` expressions carry
the coalesce, member-call target, Option type, and ABI facts through
semantic/MIR production. `OptionCoalescePayloadTypeOpt` is the shared payload
type owner, and C emission consumes the existing Option runtime ABI symbols
rather than spelling a second representation. The MIR consumer now rejects a
`coalesce -> logical_or` kind mutation before C emission. No fixture-specific
owner/helper, C-shaped split, source re-read, or backend-local fallback was
added. Current-tree focused C, LLVM, and Pergyra-built hard producer-first
parity passed with `body_fixtures=20`, `mir_fixtures=1`, missing/invalid graph
negatives, the operator-kind mutation, and runtime output `10`, `0`, `6`,
`100`, `0`. The LLVM runtime-bitcode merge now leaves externally linked
stateful globals as declarations, so the runtime object remains their sole
definition owner; the runtime-bitcode and self-host component contracts pass.
The complete 226-fixture matrix and released/default-driver replacement remain
open.

2026-07-22 executable breadth delta: DRV-2 MIR fixture 225 is
`class_method_chain_slot`. The existing semantic nominal/method-call and
`ClaimSlot`/`Read`/`Write` runtime-call ABI owners were sufficient; no
fixture-specific owner/helper, source reconstruction, or backend-local lookup
was added. Focused C/LLVM producer-first parity and the Pergyra-built hard
driver passed with runtime output `3`. The complete 225-fixture matrix was not
run before fixture 226 became the active executable surface.

2026-07-21 builtin-call scalar SoT delta: DRV-2 MIR fixture 224 is
`class_field_init_order`. The canonical builtin signature row already owns
`ToString -> String` with an `Unknown` argument wildcard; graph scalar
validation now consumes that row when the actual argument graph is typed,
instead of falling back to text-level binary checking for a larger string
concatenation. The focused C/LLVM gate is the acceptance boundary; the
complete 224-fixture matrix and released/default replacement remain open.

2026-07-21 executable MIR/ABI-first aggregate delta: the current DRV-2
manifest is 222 rows and includes the new `array_scalar_aggregate_core`
aggregate fixture. The self-host collection-runtime owner now
routes `Array<Float>` to a typed `pgy_af` value/runtime family; `Array<Long>`
shares the owned integer runtime and `Array<Bool>` uses the existing bool
family. The self MIR producer carries native `Array<Long|Float|Bool>` layout
rows and focused C/LLVM parity covers canonical MIR, missing/wrong ABI-row
mutations, emitted C, host compilation, and runtime output. Generic,
target-specific, and `Array<Double>` aggregate rows remain outside this rung.

2026-07-21 executable breadth delta: DRV-2 MIR fixture 222 is
`class_steps_loop_simple`. The Pergyra producer already carried the class
method, owner-field, while-CFG, reassignment, and loop-phi facts required by
this program, so the admission adds no source re-read or compatibility fact.
The focused C/LLVM gate is the acceptance boundary; the complete 222-fixture
matrix and released/default replacement remain open.

2026-07-21 owner-field match SoT delta: DRV-2 MIR fixture 221 is
`class_holds_enum_field`. MIR match subject validation now consumes the
semantic method owner-field type query after ordinary local lookup, without
seeding fields as fake SSA locals or rescanning source text. Focused C/LLVM
producer-first parity plus missing field and enum-variant mutations are the
acceptance gate. The complete 221-fixture matrix and released/default
replacement remain open.

2026-07-21 executable breadth delta: DRV-2 MIR fixtures 218 and 219 are
`class_loop_method_total` and `class_method_branch_nest`. The existing class
receiver, method-call target, field, branch, loop, return, and expression-graph
facts are sufficient; no semantic fallback or new fact family is introduced.
Focused C-built and LLVM-built self-driver parity is the acceptance gate. The
complete 219-fixture matrix and released/default replacement remain open.

2026-07-21 executable breadth delta: DRV-2 MIR fixture 217 is
`class_helper_method_chain`. The existing class receiver, method-call target,
field, return, and expression-graph facts were sufficient; no semantic
fallback or new fact family was added. Focused C-built and LLVM-built self
drivers match the C oracle after canonical MIR normalization, emit identical C
from the source and MIR consumer paths, compile that C, and produce identical
runtime output. The complete 217-fixture matrix and released/default
replacement remain open.

2026-07-21 executable Slot ABI SoT delta: DRV-2 MIR fixture 216 is
`bool_helper_while_slot`. The semantic owner recognizes the plain `Slot<T>`
Claim/Read/Write/Release surface, and the MIR producer projects each required
runtime-call ABI row from carried expression-graph call identities and typed
local bindings. A primary row plus instruction-keyed auxiliary rows survive
MIR JSON; the MIR-lower consumer validates owner, concrete type, operation,
symbol, call shape, and stable identity. Self-produced rows carry an explicit
`runtime_call_abi_required` marker, so deleting or corrupting a row fails before
emission while native MIR's separate resource-op rows remain compatible. The
runtime header spelling also moved behind the runtime-ABI owner. Focused
C-built and LLVM-built self drivers match the C oracle on canonical MIR,
emitted C, host compilation, and runtime output, including missing, identity,
payload, and auxiliary-row negative mutations. Final emission still projects a
validated row through the canonical runtime ABI table; direct row-payload
transport into the emission artifact remains open. The complete 216-fixture
matrix and released/default replacement remain open.

2026-07-21 MIR-only ABI-first layout delta: the self-host MIR JSON producer now
materializes the fixed `Slot<T>`, `DeviceSlot<T>`, and `SecureSlot<T>` rows for
`T` in `{Int, Long, Float, Double, Bool, String}`, plus explicit-tag
`Option<Int|Long|Float|Double|Bool|String>` and
`Result<Int|Bool|String>` rows, plus `Array<Int>` and `Array<String>` aggregate
rows. `option_string_core`, `array_sum_filtered`, and `str_array` are now
focused DRV-2 ABI fixtures. The producer declares native-shaped
`Array<Long>`/`Array<Float>`/`Array<Bool>` rows, but their current self-host
semantic array-usage consumer fails closed before executable promotion.
native/self C and LLVM producers agree on
`Option<String>` `LayoutId=589228278`, and missing-row/wrong-ID mutations fail
at the MIR consumer. Unknown and target-dependent aggregate rows remain an
explicit dynamic bridge.

2026-07-21 executable breadth delta: DRV-2 MIR fixtures 205 through 214 are
`class_dual_method_loop`, `class_dual_predicate`,
`class_factory_aggregate_loop`, `class_factory_filter_field`,
`class_factory_in_loop`, `class_field_method_chain_inline`,
`class_helper_in_match`, `class_helper_module`,
`class_immutable_step_until`, and `class_in_loop_field_use`. Clean
committed-source C-built and LLVM-built self drivers match the C oracle after
canonical MIR normalization, emit identical C from the MIR consumer and source
paths, compile that C, and produce byte-identical runtime output for all ten
programs. No semantic fallback or new fact family was required: this batch
demonstrates that the admitted class/call/aggregate/loop facts generalize beyond
the fixtures that introduced them. Only the focused ten-fixture matrix ran;
the complete 215-fixture matrix and released/default replacement remain open.

2026-07-21 executable breadth delta: DRV-2 MIR fixture 204 is
`class_arg_helper_loop`. Clean committed-source C-built and LLVM-built self
drivers match the C oracle byte-for-byte after canonical MIR normalization,
emit identical C from both the MIR consumer and source path, compile that C,
and produce the same `60`, `0` runtime output. This fixture needed no new
semantic fact or fallback; it extends the executable substitution surface
already admitted by the existing class, array, call-target, loop-phi, and
assignment owners. The
reached 550-line gate also moved command-line mode routing out of the compiler
stage owner into `driver_rung2_cli_owner.pgy`; no semantic decision moved. The
complete 204-fixture matrix was not run, and released/default replacement
remains 0%.

2026-07-21 executable assignment-mode SoT delta: DRV-2 MIR fixture 203 is
bubble_sort_basic. Its indexed inout parameter target exposed a valid
version-zero case: parameter input is not an SSA definition, so no synthetic
arr.0 use is invented. The semantic assignment owner now carries its existing
inout_param mode into MIR, the verifier permits an omitted projected-base use
only for canonical parameter modes, and canonical self-MIR input must match the
semantic mode fact instead of silently rebuilding over a bad input row. The
C-oracle compatibility bridge remains separately named. Clean committed-source
C-built and LLVM-built self drivers agree with the C oracle on canonical MIR,
MIR-to-C, host compilation, and runtime output; changing inout_param to local
fails with the assignment binding-mode diagnostic. The complete 203-fixture
matrix was not run, and released/default replacement remains 0%.

2026-07-21 executable breadth/test-cost delta: DRV-2 MIR fixtures 193 through
202 are `bubble_sort_small`, `bucket_count_array`, `buffer_full_check`,
`build_phase_advance`, `buyer_shopping_chain`, `caesar_shift_decode`,
`cell_grid_total`, `chain_match_factory`, `check_all_positive`, and
`class_alive_while_loop`. A clean C-built self driver from committed source and
an independent LLVM-built self driver passed source-to-MIR, canonical-MIR,
MIR-to-C, host-compile, and runtime parity for all ten against the C oracle.
The frontier probe rejected `bubble_sort_basic` because its indexed assignment
target graph lacks the `arr` base use, and rejected
`break_continue_while_slot` and `class_bump_option_match` at the semantic
subset boundary; none was admitted through fallback. The focused fixture
selector now derives path basenames with Bash parameter expansion instead of
spawning `dirname` and `basename` for every wanted-fixture/manifest pair. This
removes the observed Windows O(filter x manifest) process storm without
weakening the manifest or parity work. The complete 202-fixture matrix was not
run, and released/default replacement remains 0%.

2026-07-21 executable defer-transport delta: DRV-2 MIR fixtures 191 and 192
are `branch_defer_scope` and `branch_defer_skipped`. The reached MIR facts
already determine the bounded beta behavior: dynamic-control defer is rejected,
and only a reachable `AST_DEFER_STMT` with a typed body graph registers cleanup.
The self-host C emitter now carries that registration in a routine-level LIFO
state transformer instead of emitting a lexical child block's cleanup
immediately. Missing typed defer graphs still fail closed, and no C/source
fallback or text reconstruction was added. Independent C-built and LLVM-built
self drivers passed focused canonical-MIR, emitted-C, host-compile, and runtime
parity for both fixtures (`2,1` for the taken branch and `2` for the skipped
branch). Native C/LLVM backend defer registration still consumes an AST body
pointer attached to the MIR instruction and remains a separate backend-SoT
bridge; this rung does not claim that native seam is closed. The complete
192-fixture matrix was not run, and released/default replacement remains 0%.

2026-07-21 executable breadth/blocker delta: DRV-2 MIR fixtures 181 through
190 are `bool_ladder_chain`, `bool_logic_helpers`, `bool_negate_branch`,
`bool_short_circuit_calls`, `bool_short_circuit_chain`,
`bool_short_circuit_method`, `bool_state_toggle`, `bool_to_string_concat`,
`break_continue`, and `bubble_sort_inline`. The Pergyra-built hard driver and
independent C-built and LLVM-built drivers passed focused canonical-MIR,
emitted-C, host-compile, and runtime parity for all ten fixtures. The producer
probe also found the deferred-cleanup transport blocker later closed by the
191-192 delta above. No source fallback or immediate-execution compatibility
path was admitted. The complete 190-fixture matrix was not run, and
released/default replacement remains 0%.

2026-07-21 executable breadth/graph-SoT delta: DRV-2 MIR fixtures 171 through
180 are `bank_interest_recursive`, `basic`, `bid_max_score`, `bin_push_chain`,
`binary_search_int`, `binary_to_int`, `bitwise_via_division`,
`bool_compound_predicates`, `bool_cursor_equivalence`, and `bool_expr_chain`.
The first focused run exposed an `Exit(1)` instruction whose text existed but
whose typed expression graph was absent. The statement producer now attaches
the atom-lane graph, the MIR JSON graph consumer classifies `Exit` as required,
and instruction validation rejects a missing graph. No C/source-text fallback
was added. The semantic fixture inventory is now the separate
`driver_rung2_semantic_fixture_manifest_owner.pgy` owner; the executable driver
retains source/MIR convergence and is 520 lines. The Pergyra-built hard driver
and independent C-built and LLVM-built drivers passed focused canonical-MIR,
emitted-C, host-compile, and runtime parity for all ten fixtures. The complete
180-fixture matrix was not run, and released/default replacement remains 0%.

2026-07-21 executable breadth delta: DRV-2 MIR fixtures 161 through 170 are
`array_set_in_place_memo`, `array_skip_pattern`, `array_sliding_diff`,
`array_squeeze_zeros`, `array_sum_filtered`, `array_swap_pairs`,
`array_swap_pos_neg`, `array_zero_out_evens`, `atom_charged_match`, and
`bank_fluent_chain`. This extends the replacement path across additional
in-place array mutation, filtering, enum-match dispatch, and fluent class
calls without admitting allocator, async, or extern surfaces whose producer
facts still fail closed. The Pergyra-built hard driver and independent
C-built and LLVM-built self drivers passed focused canonical-MIR, emitted-C,
host-compile, and runtime parity for all ten fixtures. The complete
170-fixture matrix was not run, and released/default replacement remains 0%.

2026-07-21 executable breadth/owner delta: DRV-2 MIR fixtures 151 through 160
are `array_prefix_sum`, `array_remove_value`, `array_reverse_in_place`,
`array_rotate_left`, `array_running_avg_int`, `array_running_avg_window`,
`array_running_distinct_count`, `array_running_max`, `array_running_xor`, and
`array_selection_sort`. The executable driver had reached 593 lines before
this expansion. Its readiness checklist is now the separate
`driver_rung2_readiness_owner.pgy` fact contract (68 lines), while the driver
retains source/MIR convergence, verification, emission, and argument routing.
This is an owner split, not a generic helper bucket. The resulting driver is
540 lines. The Pergyra-built hard driver and independent C-built and
LLVM-built self drivers passed focused canonical-MIR, emitted-C, host-compile,
and runtime parity for all ten fixtures. The complete 160-fixture matrix was
not run, and released/default replacement remains 0%.

2026-07-21 executable breadth delta: DRV-2 MIR fixtures 141 through 150 are
`array_inline_class_weighted`, `array_insertion_sort`,
`array_kadane_max_subarray`, `array_min_max_combined`, `array_min_max_loop`,
`array_minmax_pair`, `array_minmax_range`, `array_of_strings_loop`,
`array_pair_concat_sort`, and `array_partition_pivot`. These programs extend
the hard replacement path across class-valued array calls, in-place sorting,
range and pair aggregates, String arrays, nested loops, and branch/loop phi
rows without adding a source-text recovery path or a C-owned fact fallback.
The Pergyra-built hard driver and independent C-built and LLVM-built self
drivers passed focused canonical-MIR, emitted-C, host-compile, and runtime
parity for all ten fixtures. The complete 150-fixture matrix was not run, and
released/default replacement remains 0%.

2026-07-21 executable breadth/SoT delta: DRV-2 MIR fixtures 131 through 140
are `array_dedup_inplace`, `array_element_assign`, `array_enum`,
`array_filter_count_sum`, `array_filter_into_new`,
`array_filter_predicate_class`, `array_first_missing_positive`,
`array_fold_minmax_sum`, `array_index_loop_sum`, and `array_inline_access`.
`array_enum` exposed a consumer gap: the MIR declaration and codegen type
environment already owned `Color` as a payload-free enum, but local emission
accepted only structs on the nominal path. `EmitLet` now consumes the existing
enum-kind row, and renaming only that declaration is rejected with
`let_type_mismatch` instead of recovering a type from source text.
`array_fold_minmax_sum` exposed a separate spelling seam. The typed C AST
printer used `%g` for every numeric node and the self parser compensated by
rewriting large decimal integers to exponent text. Integer nodes now retain
decimal spelling while Float nodes retain `%g`; the compatibility rewrite was
deleted and ratcheted. The self-host codegen reached `gen2 == gen3` at 35,277
lines, and the Pergyra-built hard driver plus independent C-built and
LLVM-built self drivers passed focused canonical-MIR, emitted-C, host-compile,
negative-mutation, and runtime parity for all ten fixtures. The complete
140-fixture matrix was not run, and released/default replacement remains 0%.

2026-07-21 executable call-target/SoT delta: DRV-2 MIR fixtures 129 and 130
are `array_elem_class_literal` and `array_elem_class_method`. Their loop bodies
call `p.V()` where `p: P` is introduced by `for p in b`. Call-target fixpoint
resolution previously ran before iteration-type facts existed, so it could not
resolve the member target without reopening source text. The iteration-type
fact owner now runs first, and the call-target resolver consumes its visible
loop binding directly. Both fixtures carry the canonical member target `P_V`.
Removing that target from the MIR expression graph is rejected by the MIR
consumer instead of falling back to source syntax. The Pergyra-built hard
driver and independent C-built and LLVM-built self drivers passed focused
canonical-MIR, emitted-C, host-compile, negative-mutation, and runtime parity
for both fixtures. The complete 130-fixture matrix was not run, and
released/default replacement remains 0%.

2026-07-21 executable breadth delta: DRV-2 MIR fixtures 119 through 128 are
`array_avg_dev_chain`, `array_balanced_split`, `array_binary_search`,
`array_cond_compound`, `array_count_above_avg`, `array_count_inversions`,
`array_count_occurrences`, `array_count_ones_bits`, `array_count_pairs_sum`,
and `array_count_sorted_pairs`. These programs exercise array parameters and
indexing, nested loops, loop/branch phi rows, compound conditions, and several
counting algorithms without adding source-text recovery or a C-owned fact
fallback. The Pergyra-built hard driver and independent C-built and LLVM-built
self drivers passed focused canonical-MIR, emitted-C, host-compile, and runtime
parity for all ten fixtures. The complete 128-fixture matrix was not run, and
released/default replacement remains 0%.

2026-07-21 executable breadth/SoT delta: DRV-2 MIR fixtures 114 through 118
are `aggregate_param_loop_phi`, `and_or_mix_chain_branches`,
`arith_grand_total`, `arithmetic_overflow_check`, and `array_avg_class`.
The aggregate loop exposed two owner defects before it could join the rung.
Routine entry had seeded parameters at SSA version one even though the MIR
entry contract and canonical verifier require parameter definitions at version
zero; loop phi construction now consumes that same entry version. Separately,
the semantic expression graph already carried the `Bool` argument type for
`ToString(state.ok)`, but direct-call emission discarded it and selected the
integer runtime ABI row. The graph call owner now selects the Bool runtime ABI
symbol from that carried type, without parsing expression text. The
Pergyra-built hard driver and independent C-built and LLVM-built self drivers
passed focused canonical-MIR, emitted-C, host-compile, and runtime parity for
all five fixtures; the aggregate output is `true|preserved`. The complete
118-fixture matrix was not run, and released/default replacement remains 0%.

2026-07-21 executable breadth delta:
`action_outcome_dispatch` is DRV-2 MIR fixture 113. This adds an enum match
dispatch with nested branches, aggregate construction/return, member reads,
and direct calls without adding a compatibility parser or backend-owned fact.
The Pergyra-built hard driver and an independent C-built self driver passed
focused canonical-MIR, emitted-C, host compile, and runtime parity
(`200`, `-1`, `50`, `50`, `-1`). A previously built independent LLVM self
driver passed the same focused lane. A fresh LLVM self-driver rebuild was not
counted because unrelated in-flight LLVM ABI-row work in the local workspace
failed before fixture execution. The complete 113-fixture matrix was not run,
and released/default replacement remains 0%.

2026-07-21 executable member-call/SoT delta:
`class_method_self_access` is DRV-2 MIR fixture 112. The parser expression
graph now owns the distinction between a standalone member call and a plain
member value, and the HIR artifact binds that owner kind through exact parser
provenance instead of inspecting dot syntax. The oracle-MIR bridge separately
preserves `AST_CALL` as the internal `Call:` tree projection, so neither the
source path nor canonicalization has to recover a call from expression text.
The Pergyra-built hard driver and independent C-built/LLVM-built self drivers
passed focused canonical-MIR, emitted-C, host compile, and runtime parity
(`101:50`, then `50`). Removing the `Account_Deposit` member-target fact is
rejected by the MIR consumer instead of falling back to AST text. The complete
112-fixture matrix was not run, the next non-manifest fixture has not yet been
selected, and released/default replacement remains 0%.

2026-07-21 executable assignment-binding/SoT delta:
`owner_field_assignment` is DRV-2 MIR fixture 111. Semantic assignment typing
now carries the winning binding mode together with the verified target type;
MIR input admits that row explicitly and seeds SSA version zero only when the
semantic owner classified the target as an implicit owner field. The first
`balance = balance + amount` therefore preserves `balance.0 -> balance.1`
without rewriting the source to `self.balance` or asking a backend to infer a
field from its spelling. C emission consumes the existing function-scoped
`cbind` row and emits `self.balance`. Removing only the `balance` declaration
row while retaining the expression graph fails with `undefined_symbol`
instead of recovering from text. The Pergyra-built hard driver and independent
C-built/LLVM-built self drivers passed focused canonical-MIR, emitted-C, host
compile, negative mutation, and runtime parity (`75`) for this fixture. The
complete 111-fixture matrix was not run; standalone member-call statements in
the broader `class_method_self_access` probe remain the next bounded gap, and
released/default replacement remains 0%.

2026-07-21 executable CFG/SoT delta: `class_node_field_access` is DRV-2 MIR
fixture 110. In a loop-local `if` whose true arm returns, cyclic reachability
made that terminal true block look like its own structural merge. The MIR
routine fact index now owns the terminal-block verdict from successor facts,
and CFG-to-tree lowering consumes it before selecting the false successor as
the continuation. It does not inspect return text or reopen the source AST.
This preserves `return total` inside the true arm and keeps the carried
expression-graph sequence aligned with reconstructed `SyntaxNodeId` lanes.
The Pergyra-built hard driver passed focused canonical-MIR, emitted-C, host
compile, and runtime parity for a six-fixture frontier including this case;
C-built and LLVM-built self drivers independently passed the new fixture. The
complete 110-fixture matrix was not run, and released/default replacement
remains 0%.

2026-07-21 hard executable delta: the installed Pergyra-built DRV-2 is now a
direct producer in focused parity instead of merely being smoke-run after its
build. The compiler-build cache hashes the parser owner's freshly composed AST
rather than a separately generated source-set fingerprint; this removed a
stale-cache split where the installed hard driver exposed 76 MIR fixtures
while the live owner exposed 109. The hard lane produced and consumed
`class_compare_return`, `class_as_strategy`, `class_compose_factory`,
`device_slot_routine`, and `defer_scope`, then matched the native oracle's
canonical MIR, generated C, host compilation, and runtime result. This path
also found and closed a real materialization seam: machine-runtime programs now
consume the runtime header's `pgy_log_bool` ABI, while standalone programs
materialize the self-host owner's local implementation. The focused hard lane
is now required by self-host preparation CI. The complete 109-fixture hard
matrix was not run, stage-0 seeds and the final host compilation remain
C-owned bridges, and released/default replacement remains 0%.

2026-07-21 hard substitution delta: `make self-host-compiler` no longer asks
the native `pgy --backend=c` path to compile `driver_rung2_main.pgy` directly.
The stage-0 parser seed now owns the composed source graph, the Pergyra-built
gen2 codegen consumes that AST, and the host C compiler only compiles the
resulting C artifact. The parser also binds imported expression rows once as
`composed_rows` before executable/match partitioning; repeated extraction of
the aggregate had made the full DRV-2 import graph fail while every individual
owner parsed cleanly. Parser C parity remains 188/188 byte-equal. A hard-built
DRV-2 compiled the bounded `valid_call_int` source successfully, and its
source-set fingerprint reuses the installed driver on the next invocation.
The native C compiler still builds stage-0 seeds and the final C artifact, and
the default `pgy` driver remains C-owned, so released/default replacement is
still 0%.

2026-07-21 executable SoT delta: `class_compare_return` is DRV-2 MIR fixture
109. Statement-type evidence already classified each direct `Log` argument as
`Bool`; the self-host emitter now consumes that carried type and the string
runtime ABI owner supplies the sole `pgy_log_bool` spelling and implementation.
The previous catch-all string-log fallback is no longer used for Bool values.
Current C-built and LLVM-built self drivers matched canonical MIR,
source/MIR C, native compilation, and runtime output for the fixture. The
complete 109-fixture matrix was not rerun, and released/default replacement
remains 0%.

2026-07-21 executable breadth delta: fifteen more class strategy, factory,
aggregate, and composition programs are DRV-2 MIR fixtures 94 through 108.
Current C-built and LLVM-built self drivers each matched the native oracle's
canonical MIR, source/MIR C artifact, native C compile result, and runtime
output for all fifteen. The fixture identity owner now derives a
`backend_compare/*/main.pgy` identity from its parent directory instead of
growing a per-fixture alias table. At that slice, three nearby probes remained
fail-closed: implicit owner-field assignment needed an assignment target
binding fact, and the looped node member fixture lost its expression-graph
order during canonical consumption. The latter is now fixture 110 under the
CFG/SoT delta above. The complete 108-fixture
matrix was not rerun, and the
released/default driver replacement remains 0%.

2026-07-21 executable breadth delta: five additional class composition and
method flows are DRV-2 MIR fixtures 89 through 93: `class_chain_methods`,
`class_compose_factory`, `class_two_step_no_loop`,
`class_three_params_clamp`, and `class_field_method_chain`. The Pergyra MIR
producer accepted all five without a C fact fallback. Focused C-built and
LLVM-built DRV-2 drivers matched the native oracle's canonical MIR,
MIR-consumed C, and runtime output (`body_fixtures=20`, `mir_fixtures=5`). The
complete 93-fixture matrix was not rerun in this slice, and the
released/default driver replacement remains 0%.

2026-07-21 executable breadth delta: six class call/value flows are DRV-2 MIR
fixtures 83 through 88: `class_self_factory_chain`,
`class_self_field_method`, `class_chained_factory_call`,
`class_param_return_chain`, `class_returning_class`, and
`class_nested_field_chain`. The Pergyra producer already carried sufficient
nominal constructor, receiver, return, and nested field facts for all six; no
C-owned fact fallback or new semantic reconstruction was added. Focused
C-built and LLVM-built DRV-2 drivers matched the native oracle's canonical
MIR, MIR-consumed C, and runtime output for all six fixtures. The complete
88-fixture matrix was not rerun in this slice, and the released/default driver
replacement remains 0%.

2026-07-20 executable delta: verified wrapper calls now preserve their
argument-dependent receiver type after call-target carriage. The receiver-type
owner consumes the carried `UnwrapOption`/`Unwrap` target before consulting a
general callable return row, so `UnwrapOption(owner).name` derives the nominal
payload and field type instead of collapsing to `Unknown`. The existing
`option_struct_value_flow` fixture now includes the direct
`UnwrapOption(built).left` chain. Focused C-built and LLVM-built DRV-2 drivers
match canonical MIR, emitted C, negative mutations, and runtime output
`7 / 11 / 5`. This closes the wrapper-payload receiver seam reached by the
active rung; it does not change the released/default driver replacement from
0% or claim the complete 82-fixture matrix.

2026-07-20 executable delta: `class_method_self_chain` is DRV-2 MIR fixture
82. Bare `val` and `limit` references inside `Builder` methods are resolved
from the function-owner row plus the nominal declaration's ordered field rows.
The semantic environment and final C binding environment consume the same
owner identity; parameters and locals are appended later and therefore retain
lexical shadowing precedence. Removing only the `val` declaration row leaves
the expression graph unchanged and now fails with the structured
`undefined_symbol` diagnostic instead of reopening source text or AST state.
Focused C-built and LLVM-built self drivers match native canonical MIR,
emitted C, diagnostics, and runtime output for this fixture. The complete
82-fixture matrix was not rerun in this slice, and the released/default driver
replacement remains 0%.

2026-07-20 executable delta: `class_method_self_return` is DRV-2 MIR fixture
81. A member call whose receiver is another call now resolves that receiver
from the carried direct-call target plus the callable return-type row; it does
not infer the type from expression text. The MIR JSON consumer requires every
call node to retain its canonical call-target kind and name before semantic
body analysis. Removing only `Stat_PromoteIf` from the carried graph now fails
at the MIR expression-graph boundary instead of being repaired by the semantic
fixpoint. Focused C-built and LLVM-built self drivers match native canonical
MIR, emitted C, diagnostics, and runtime output for the fixture. This closes
the bounded chained-member call-target carriage seam, not the default driver
or the remaining declaration/runtime consumers.

2026-07-20 executable MIR/ABI-first delta: runtime-call ABI rows now carry a
stable `runtime_call_abi_id` derived from the canonical
`domain|abi_type|operation` key. Native MIR, the self-host MIR producer, the
self-host MIR consumer, C, and LLVM validate the same identity; removed or
mutated IDs fail before emission. A statement containing several resource
operations no longer collapses them into one consumer row. Each nested
`Write`/`Read` consumes the exact MIR resource instruction selected by its
source-stable identity, and a missing exact row fails instead of reopening a
backend table lookup. The focused native C/LLVM comparison passes
`while_loop_slot_read` and `three_slots_cross_update`, and the MIR suite passes
149/149. RIR now owns the enclosing source-statement identity and MIR carries
it to resource operations and statement consumers; source-line and call-argument
recovery are negative-gated. Static layout IDs use the disjoint `0x2...`
namespace and runtime-call IDs use `0x4...`. This closes the multi-operation
carriage sub-rung, not all legacy
non-MIR compatibility paths.

The ID remains a stable logical key, not a content hash. Native MIR, C, LLVM,
and the self-host MIR consumer separately compare the carried symbol, target
kind, materialization, and call shape with the canonical owner. A valid ID with
a mutated payload is covered by native and DRV-2 negative tests.

The same DRV-2 rung now carries an explicit `CompilerTargetProjectionFact`
from the target-capability owner into the final Pergyra C emitter. The emitted
artifact records the capability schema and selected `cpu-c` projection, and a
missing projection fact is a blocking negative fixture. C-built and
LLVM-built Pergyra drivers both pass the focused `device_slot_routine`
canonical-MIR, emitted-C, diagnostic, and runtime comparison. This is a hard
projection-carriage result only. Native target fingerprint, concrete
size/alignment/endian values, AIR evidence binding, and non-C projections are
still bridge work, so `target.capability_profile` is not closed.

2026-07-20 executable MIR/ABI-first delta: DRV-2 fixtures 78 and 79 are the
`device_slot_machine_layer` and `device_slot_remote` programs. The Pergyra
producer requires the target-owned declaration fixture, imports its physical
grant as an explicit ABI input, owns the contact rows, and fails when that
declaration is absent. The C oracle does not generate this comparison input.
Focused C-built and LLVM-built self drivers matched native/self canonical MIR, MIR-consumed C,
producer-first C, and runtime output for both fixtures. The oracle normalizer
now erases MIR `resource-op` evidence rows from executable statement recovery
instead of turning their `expr0` payload into an AST statement. This is a
two-fixture machine/ABI replacement result, not a completed 79-fixture matrix
or a default-driver replacement. `make self-host-mir-abi-first-test-smoke`
owns the focused comparison bundle.
The same turn's seed-only compiler-scale bootstrap attempt did not emit
`gen1.c` and was stopped after about 15 minutes at 8.98 GB working set / 12.5
GB private bytes. The focused lane is therefore executable evidence, while the
codegen fixed point remains blocked by the existing string-amplification debt.

2026-07-20 exhaustive-parity SoT closure: the assignment projection probe no
longer passes parser-shaped call graphs directly to codegen. Its positive
`Some`/`None` expressions now consume builtin callable identity from
`SemanticExpressionGraphCallTargetsFromSignatures`, matching the compiler
pipeline owner. A dedicated missing-call-target mode bypasses that owner and
must fail in the final semantic call emitter. This closes the probe's direct-
call identity seam; the broader expression-surface registry row remains a
bridge.

2026-07-20 bootstrap corpus gate: chunk policy, lane policy, and compiler
reachability are now explicit integrated-driver corpus rows. The current
C-built integrated owner rejects all three with controlled `CODEGEN ERROR`, so
their initial status is `out_of_subset`; the Pergyra-built driver gate rejects
both regression and unrecorded promotion. A stale cross-platform seed must be
runnable on the current host before the driver consumes it. Full local seed
regeneration was stopped when the existing codegen path reached 7.2 GB RSS
(about 10 GB private bytes), so self-built corpus execution remains CI/manual
integration evidence rather than a focused-gate claim.
The first isolated retry exposed a second path owner: the executable used the
override directory while AST, component, tool, MIR, and fuzz paths still named
the shared cache. `B_REL` now derives every such child path from the selected
build directory; the policy corpus has its own output-directory override; and
the component contract rejects the old hardcoded AST path. That retry was
stopped at 8.8 GB RSS / 12.2 GB private bytes; isolation is closed, but codegen
string amplification remains measured performance debt.

2026-07-20 executable delta: `else_if_chain` is DRV-2 MIR fixture 77.
The Pergyra producer carries all three nested branch conditions as distinct
typed expression graphs, and the hard consumer emits the chain only from those
graphs. Native/self canonical MIR, emitted C, and runtime output are equal for
the focused C leg. The shared graph mutation owner removes or corrupts the
required graph and is rejected before emission. This closes only the bounded
else-if condition transport seam; it is not whole-control-flow closure.

2026-07-19 executable delta: `for_continue` is DRV-2 MIR fixture 76.
The Pergyra producer now publicly carries a `for` loop whose nested branch
reaches the loop header through an explicit continue CFG edge. MIR owns both
that edge and the associated LoopFlowSummary row; the hard consumer derives
structured `Continue` from the successor graph rather than the branch label.
Changing only the declared loop-summary count fails closed before emission.
Focused C parity proves canonical MIR and runtime output (`25`) against the
native oracle. This is bounded continue-flow evidence, not a claim that all
loop state rows or nested break/continue combinations are closed.

2026-07-19 executable delta: `result_try` is DRV-2 MIR fixture 75.
The existing typed postfix-try graph owner now has blocking Result evidence in
addition to Option evidence. `?Validate(doubled)` retains the `try` node and
its ordered call-argument spine through self MIR and both canonical artifacts;
the hard consumer emits Result success extraction and Err early return from
that graph. Focused C parity covers both the successful `Process(10)` path and
the failing `Process(60)` path, plus the surrounding pipe expressions and
runtime output. The graph-negative owner remains the missing-fact ratchet; no
operand-text recovery or Result-specific parser was added.

2026-07-19 executable delta: `enum_match` is DRV-2 MIR fixture 74.
The parser preserves each bare case identifier as a pattern fact without
guessing whether it is a scalar or enum value. Semantic analysis proves that
the match subject has the declared enum type and that the selected variant has
zero payload fields. MIR keeps the native-compatible
`match_variant = null` row, while the hard MIR consumer resolves the variant
through carried enum declaration rows and constructs `Owner.Variant`
equality graphs. Removing only the `North` declaration row now fails closed
with the enum-declaration diagnostic. The focused C parity gate proves
source/MIR canonical equivalence and runtime output for all three variants.
Payload-bearing enum cases remain outside this bounded rung. Native block
ordering still requires the explicitly named oracle canonicalization bridge;
the self-produced MIR path does not use that bridge.

2026-07-19 executable delta: `defer_scope` is DRV-2 MIR fixture 73.
The bounded single-`Log` cleanup body now crosses native and self MIR as an
explicit `AST_DEFER_STMT` instruction with `arg0=Log` and an `expr0_graph` for
the deferred expression. The hard MIR consumer reconstructs the cleanup body
from that graph and no longer reads `defer_body` statement strings. Native
MIR retains the old array only as compatibility provenance; deleting the typed
graph while keeping that text fails closed. The fixture proves LIFO block exit
and early-return cleanup against the C oracle. Multi-statement and non-`Log`
defer bodies remain outside this bounded rung rather than falling back to text.

2026-07-19 executable breadth delta: five already-supported compiler paths are
now blocking DRV-2 MIR fixtures 68 through 72 instead of codegen-only evidence.
`ref_param` and `inout_return_forward` carry readonly-ref and value-result
parameters through self MIR; `option_int_core`, `array_param`, and `bool_logic`
cover wrapper mutation, collection parameter/return flow, and recursive
logical control flow. For each path, the Pergyra producer, native MIR oracle,
strict canonicalizer, MIR-to-C consumer, and runtime result agree. The parity
runner accepts a comma-separated fixture filter so one bounded change can
validate its exact impact set without rebuilding the driver once per fixture.
This is executable replacement breadth, not a new semantic-owner closure.
At that checkpoint, `defer_scope` remained excluded because native MIR still
serialized its deferred body as statement strings; the subsequent delta above
lands the first typed cleanup-body fact instead of admitting that fallback.

2026-07-19 executable delta: direct and member generic calls now share one
self-MIR specialization owner. `generic_member_inferred_flow`,
`generic_vessel_member_inferred_flow`, and
`generic_member_constructed_return_flow` plus
`generic_member_array_return_flow` and
`generic_member_record_array_return_flow` are DRV-2 fixtures 63 through 67, and
the existing inferred `Identity<T>` fixtures use the same top-level row. The stable
identity is `(owner SyntaxNodeId, expression lane, local call
ordinal)`; an expression-graph index is never serialized as identity. The hard
MIR-to-C entrypoint passes only the decoded MIR specialization view into body
codegen instead of first projecting semantic rows and overwriting the result.
Recomputed semantic rows remain verifier evidence. Missing direct/member rows,
identity drift, and symbol drift fail closed. The nested member fixture carries
ordinals 0 and 1 and infers `T=Int` without source `<Int>` actuals. Native MIR
captures the child specialization before its parent so the exact generic
return binding reaches the outer call; both paths emit one deduplicated
`Box_Echo_Int` body. The adjacent vessel fixture proves the same path for the
Pergyra-specific passive state host and emits `Cell_Echo_Int`. The constructed
return fixture infers the inner `Wrapper_Wrap_Int`, substitutes its declared
`Option<T>` return as `Option<Int>`, then uses that exact type to specialize the
outer call as `Wrapper_Echo_Option_Int_`; both hard source/MIR consumers run as
`43`. Native MIR now renders substitutions through its structured return-type
AST, and its verifier rejects any method formal retained as an identifier token
inside an actual type. Option codegen identifies `Some`/`None` only from the
semantic direct-call target fact; deleting or changing that fact fails before
emission. This remains `BRIDGE`, not global closure: native and self identities
are not yet one representation, and multi-formal or deeper constructed return
shapes remain outside this bounded rung.

The adjacent `Array<T>` fixture proves that the structured substitution is not
Option-specific. `ArrayWrapper.Wrap<T> -> Array<T>` resolves to `Array<Int>`,
the outer specialization becomes `ArrayWrapper_Echo_Array_Int_`, and hard
source/MIR consumers both run as `44`. Runtime usage no longer treats the
declared `Array<T>` surface as an unknown nominal-record array. It recognizes
declared formals through signature facts and separately scans concrete generic
specialization actuals for materialized array element types. Unknown non-formal
elements still fail closed.

The record-array falsification fixture instantiates the same return as
`Array<Point>`. Its specialization actual retains the `Point` runtime array
definition, the inner and outer symbols become `RecordArrayWrapper_Wrap_Point`
and `RecordArrayWrapper_Echo_Array_Point_`, and native/self source/self MIR all
run as `45`. The emitted-C gate requires the concrete `pgy_Point_array` row, so
the specialization-usage consumer cannot become decorative.

The class, vessel, Option-return, and Array-return nested inferred member programs are
now part of the public bounded replacement gate. `pgy --self-driver` must
produce the same canonical MIR,
MIR-consumed C artifact, and runtime output as the direct DRV-2 binary and C
oracle. The hard self side remains graph-owned; only the legacy native oracle
artifact enters the explicitly named `--canonicalize-oracle-mir-json` bridge.
A missing self driver remains an error; this does not change the released/default
compiler path or claim whole-compiler replacement.

2026-07-19 executable delta: `array_destructure` is DRV-2 MIR fixture 62.
The parser owns the destructure pattern and initializer graphs; semantic owns
the ordered local-binding and element-type rows; MIR carries one typed
destructure instruction with exact binding names, source locals, and initializer
uses. The final consumer derives canonical temp/index expression graphs from
those MIR facts and no longer infers `Split` or element type from source text.
The canonical temp identity is binding-derived rather than JSON-offset-derived.
C-built and LLVM-built focused drivers matched native canonical MIR, emitted C,
and runtime output. Removing only `destructure_element_type` fails closed, and
the static contract forbids the retired `Split(` recovery path. This closes the
last ratcheted-out executable counterexample from the 59-row breadth expansion;
released/default compiler replacement remains 0%.

2026-07-19 executable delta: `nested_if_in_loop` is DRV-2 MIR fixture 61.
`loop_reachability_fact_owner.pgy` derives separate backedge and fallthrough
facts from the typed control-flow tree before loop-header SSA construction.
The producer compares that plan with the completed CFG and emits a header phi
only when normal fallthrough or explicit `continue` reaches the header. It no
longer scans instruction result spellings to guess the live version. C-built
and LLVM-built official parity matched native canonical MIR, emitted C, and
runtime output. A negative injects the retired one-predecessor header phi and
both final consumers reject it. At that checkpoint, `array_destructure` was the
only ratcheted-out executable counterexample; the subsequent delta above closes
it.

2026-07-19 executable delta: `break_after_stmt` is DRV-2 MIR fixture 60.
The MIR-to-tree consumer now uses the then-arm successor fact when a cyclic CFG
would otherwise select a later iteration's then block as the structural merge.
It emits the loop-exit arm as `break` and the fallthrough arm as the loop
continuation without reopening source text. The focused official harness passed
20 body fixtures plus this MIR fixture under C-built and LLVM-built self
drivers, including canonical MIR and runtime parity. At that checkpoint,
`nested_if_in_loop` and `array_destructure` remained excluded; the subsequent
deltas above close both.

2026-07-19 executable breadth delta: DRV-2 now commits 59 MIR fixtures. The
previous 41-row frontier is joined by 18 already-supported declaration,
arithmetic, call, branch, reassignment, and loop programs. Each promoted row
matched the native oracle's canonical MIR and runtime output under both the
C-built and LLVM-built self drivers. This slice did not rerun the complete
59-row matrix. At that checkpoint, three executable counterexamples remained
excluded. The subsequent delta above closed `break_after_stmt`; the other two
must enter through their missing owners, not by source-text recovery or C
fallback.

2026-07-19 executable delta: index emission now derives the receiver collection
type from `CodegenExpressionTypeFromGraph` and the collection ABI owner. It no
longer reads receiver node text or invokes `ExprMemberFieldType`. The new
`member_array_index` DRV-2 fixture carries `holder.values[1]` through a nested
member/index graph. C-built and LLVM-built self drivers matched canonical MIR,
emitted C, and runtime output. A negative changes only member-node provenance
from `holder.values` to `stale.provenance`; both drivers still emit the child-
graph-owned `pgy_ai_get(holder.values, 1)`, proving the old text path is not the
last consumer.

2026-07-19 executable delta: payload-free enum expected values no longer recover
their value from root expression text. `RewriteSemanticExpectedValue` delegates
to the parser-owned leaf/member graph and the canonical enum row in
`TypeEnvZone`. Focused DRV-2 parity for `enum_return` passed with C-built and
LLVM-built self drivers across 20 body fixtures plus the selected MIR fixture.
The negative leg preserved the root `Choice.B` provenance, changed only the
member child from `B` to `Missing`, and both consumers rejected it. Legacy enum
text projection remains only in separately named call/match compatibility
owners; the mixed-expression blocker therefore stays open.

2026-07-19 executable delta: `await` is no longer recovered from an `"await "`
leaf prefix. The self parser emits an arity-one `await` expression node, semantic
and codegen type owners derive its `Future<T>` / `RemoteFuture<T>` result, MIR
JSON preserves the node kind, and the machine-layer C emitter selects the
remote-await ABI row from the typed child binding. The component contract
forbids the retired string slicer, while the focused machine-layer gate mutates
only the MIR node kind from `await` to `leaf` and requires fail-closed rejection.
The current self-host emission slice remains bounded to a named
`RemoteFuture<T>` binding; arbitrary await operands and local executor lowering
are not claimed.

2026-07-19 executable delta: scalar `match` occupies DRV-2 MIR fixtures 38-39. The
Pergyra producer lowers semantic statement-owned subject/pattern facts into an
eight-block case/default CFG, carries each case through
`SelfMirMatchFactRows`, and projects `match_patterns` without reopening source
or AST text. The MIR consumer derives reconstructed `x == pattern` expression
graphs from the carried subject graph plus pattern fact. C-built and LLVM-built
focused drivers matched canonical MIR, emitted C, and native runtime output;
deleting the first pattern is rejected by the final MIR consumer and by the
instruction verifier contract. The merge block is emitted last, and the fixture
executes case 1/2/3 plus default before requiring one post-match continuation
on every path. This closes bounded integer single-pattern match with a final
default. `match_case_assign` also proves N-way SSA merge ownership: all four
live arm versions enter one phi and C/LLVM-built producers return 10/20/30/40.
The final MIR consumer also compares phi arity with indexed CFG predecessors;
deleting one arm input now fails before structured C can hide the loss.
Fixture 40 adds bounded `Option<T>` variant/binding carriage. Parser-owned
pattern graphs project through one HIR fact; `Some(v)` seeds a case-local
payload binding for a leaf `Option<T>` subject, and MIR carries exact `Some` /
`None` variant plus binding rows. The final consumer derives `IsSome`, logical
not, and `UnwrapOption` graphs from those facts. Focused C/LLVM canonical MIR,
emitted C, and runtime parity passed, while deleting the `Some` binding fails
closed. Enum variants, complex scrutinees, and multiple bindings remain out of
the bounded rung; released/default compiler replacement remains 0%.

2026-07-19 executable delta: initializer scalar typing no longer reprojects
member/index/call text after the parser graph verdict. A negative contract
keeps `box.value` as the initializer spelling while changing only its internal
member-name node to `missing`; C- and LLVM-built semantic checkers reject it.
Focused C/LLVM DRV-2 parity passed 20 source fixtures plus
`nested_member_access`. With initializer, assignment, and statement consumers
migrated, `projection_type_owner.pgy` had zero references and was deleted; the
component gate now rejects its return. This closes that legacy semantic
projection owner, not all expression bridges or whole-compiler substitution.

2026-07-19 executable delta: `match` scrutinees now enter the parser-owned
Atom graph lane. Statement typing consumes that root and rejects an unresolved
scrutinee instead of calling `SemanticProjectionExpressionType`. A negative
contract preserves `box.value` at the source/root spelling but changes the
member-name child to `missing`; both C- and LLVM-built semantic checkers reject
the row. Parser C/LLVM output remained byte-equal for 188 sources and semantic
C/LLVM verdict parity passed 111 fixtures. This semantic-only step did not by
itself claim MIR substitution; the later fixture-38 entry above records the
separate executable producer/consumer rung.

2026-07-19 executable delta: assignment target and RHS scalar typing now
consume parser-owned expression graph handles. The target owner derives the
binding root and, for indexed writes, the collection base and index roots from
the `Atom` graph; it no longer reprojects `box.value` or `values[i]` from
source text. The RHS verdict no longer falls back to
`SemanticProjectionExpressionType` after graph typing. A negative contract
keeps the source spelling `box.value` intact while changing only the graph's
member-name child to `missing`; both C- and LLVM-built semantic checkers reject
the assignment instead of recovering the old field from text. Focused DRV-2
C/LLVM parity passed 20 source fixtures plus `indexed_assignment`, including
the existing missing-target-graph failure. This closes the assignment type
projection seam, not whole-compiler or released/default substitution.

2026-07-19 executable delta: collection statement typing now consumes the
parser-owned expression graph for `ArrayPush` values and both `ArraySet`
index/value arguments. The parser carries the `ArraySet` index in the value
lane and its replacement value in the auxiliary lane; MIR preserves those as
`expr1_graph` and `expr0_graph`, respectively. The statement type owner no
longer calls the legacy projection for either argument. A kind-only mutation
keeps the source spelling `0` while changing its graph identity from
`integer_literal` to `leaf`; the hard semantic contract rejects it instead of
recovering `Int` from text. Focused C-built and LLVM-built DRV-2 parity passed
20 source fixtures plus `ast_node_array_set`, and deleting the secondary MIR
graph fails closed. This is one hard substitution rung, not whole-compiler
self-host completion.

2026-07-19 executable delta: statement-return array-literal typing now consumes
the parser-owned expression graph. The final semantic consumer of
`SemanticProjectionArrayLiteralMatchesDeclaredType` moved to
`SemanticExpressionGraphArrayLiteralMatchesDeclaredType`, and the dead
text-reparsing owner was deleted instead of retained as a compatibility alias.
`array_return_literal` is DRV-2 MIR fixture 37. Focused C-built and LLVM-built
self-host drivers produced matching canonical MIR, matching emitted C, and
run-equal output; the return instruction retains its array graph and the
existing missing-graph mutation remains fail-closed. This closes array-literal
typing for initializer, assignment, and statement-return lanes. It does not
close all expression typing or released/default compiler substitution.

2026-07-19 executable delta: assignment array-literal typing now consumes the
parser-owned expression graph through
`SemanticExpressionGraphArrayLiteralMatchesDeclaredType`; the assignment type
owner may no longer reparse the value text through the legacy projection
helper. The self-host assignment emitter also consumes the existing semantic
expected-type row for `Array<T>` instead of dropping the composite literal into
the untyped expression path. `array_literal_assignment` is DRV-2 MIR fixture 36
and passes the focused source-to-MIR-to-C producer/consumer comparison with
both C-built and LLVM-built self-host drivers. This closed the assignment lane;
the later statement-return delta above closes the remaining semantic consumer
of the text array-literal projection. Neither delta closes whole-driver
substitution.

2026-07-17 executable delta: scalar expression leaves now emit from the
parser-owned graph plus canonical binding/enum rows. Bool, String, Int, Long,
bound identifiers, and callable references bypass the legacy `RewriteTokens`
scanner, and unsupported leaves fail closed. Literal-only and higher-order call
arguments therefore consume the same graph path. Fine-grained scalar literal
and callable-reference node kinds remain future work, so this narrows but does
not close the mixed expression bridge.

**This is the canonical progress measurement for Pergyra self-hosting.**
The number that matters is *how much of the C/LLVM compiler has been
substituted by Pergyra-written equivalents* -- not how many peripheral
audit tools exist.

Last updated: 2026-07-19

Evidence currency: this file is the canonical progress ledger, but individual
green claims remain dated to the gate runs named in each section. Updating this
ledger or touching an isolated SoT owner does not imply a fresh
`self-host-preparation-test-smoke` run. New validation should follow
`docs/152_validation_isolation_policy.md`: run the owner-scoped self-host rung
gate first, and escalate to the heavy preparation/parity bundle only when a
broader compiler-world artifact changed or broad parity is explicitly requested.
The latest broad parity refresh was `make self-host-preparation-test-smoke`
on 2026-07-09: it completed green with 203 real sources accepted by both
selfcheck backends, codegen bootstrap `gen2 == gen3` at 9816 generated-C lines,
DRV-0/DRV-1 driver parity, LSP parity, backend tri-compare, and MIR JSON rung-0b
parity over 86 fixtures. Later focused refreshes on 2026-07-10
raised the M2 ledger to 219/219 after the incremental fact graph owner and
completeness impact owner split landed; the changed-source impact run proved
the incremental graph, completeness ledger, impact owner, and TestHarness owner
sources through lexer/parser/semantic/codegen;
the MIR JSON fact-only frontier then moved to 100 fixtures. It first added Long
scalar flow, array index assignment, `Option` `?` propagation, and string
equality-plus-concat surfaces, then closed the remaining committed codegen
fixture surfaces: C-reserved binding spelling, payload-free enum match
comparison projection, Float signatures, seeded random flow, and string-array
index return flow.
The next focused slice raised the source inventory to 221 after splitting the
shared driver pipeline owner from DRV parity policy. That bootstrap gate proved
the standalone codegen fixed point and the first integrated parser/codegen
driver fixed point. The older integrated proof covered the bounded
source-to-AST-to-C pipeline, not the later MIR-producing driver and not the
whole compiler. Semantic analysis owns
executable `Main` cardinality, signatures, local declaration/function/scope,
initializer, iteration, assignment, expression-use, return, condition, and
ordered body-verdict rows. The latest standalone codegen bootstrap fixes
`gen2 == gen3` at 14,673 generated-C lines. DRV-2 now integrates bounded MIR
production and consumption. Its current full-source gen2 artifact builds and a
34.5-second `mir_lower` owner preflight is byte-identical between seed and gen2;
the blocking bootstrap gate now compares the Pergyra-built integrated seed
with the native-built same driver on a real source. Both builds also produce
byte-identical verified MIR for a bounded TestHarness-owned sample and consume
the common fact to byte-identical C. The full-input stage2 plus `gen2 == gen3` consumer
comparison is retained in the explicit
`self-host-driver-bootstrap-full-test-smoke` target. Bounded DRV-2 fixtures
retain self-host MIR-producer parity, and the real-source sample comparison
retains end-to-end evidence. This is an integrated seed and bounded consumer
proof, not a released compiler substitution claim. A local full-source
self-host producer probe historically reached about 68 GB private allocation
before completion. That number is neither normal nor the current result. On
2026-07-19, the bounded LLVM self-source routine-lowering probe completed all
1,816 routines with zero errors in 62.085 seconds at 794.4 MB peak private
memory and 728.5 MB peak working set, below its 3 GB fail-closed cap. Parameter
SSA aliases and block live-in scope now consume MIR facts instead of sibling-
block state, and the codegen statement dispatcher no longer represents its
top-level kind selection as a deep `else if` AST chain. The same current parser
completed its own 323,644-byte source set in 12.501 seconds at 72.1 MB peak
private memory. This closes the historical amplification claim for the measured
routine-lowering core, not whole-driver substitution. The released replacement
claim remains open until the integrated producer and consumer complete inside
their integration budget and retain C/LLVM parity.
The driver job now requests only the codegen `gen2` and parser-producer seed;
the standalone codegen fixed point and breadth retain a separate blocking
Linux job instead of being repeated before the driver boundary.
The landing Windows run observed about 1.35 GB peak working set in the first
integrated driver seed generation. A later full-stage2 consumer stayed below
101 MB private memory but exceeded the 30-minute integration budget. The
integrated seed proof is blocking; full stage2/stage3 remains explicit until
self-host codegen makes this path production-cheap.

The current codegen-only owner slice is materially cheaper: single-pass
runtime-call rewriting plus `TextBuilder` measures 956.1-956.5 MB and
15.792-15.877 seconds on the pinned 1,289,598-byte artifact with byte-identical
output in the historical 2026-07-12 series. A fresh 2026-07-16 semantic-bundle
series measures 665.9-670.2 MB and 12.185-12.338 seconds on a distinct
1,367,585-byte artifact, with byte-identical output across both runs and a
26,227-line gen2/gen3 byte-identical fixed point. These are
separate input series, not a direct A/B comparison. A separate integrated-driver
probe moved complete typed-arena row-shape
validation back to the artifact boundary and made readers validate only their
consumed rows. That reduced peak private memory from 223.4 MB to
159.4-161.0 MB with identical generated output, but did not improve the
34-36-second runtime. Parser/semantic/MIR character and substring scans remain
the current CPU boundary; these measurements do not close the expensive
full-source gen3 fixed point.

A later allocation-free comparison slice repointed five remaining hot
`Substring(...) == token` checks in codegen literal/expression scans to the
existing `SubEqualsWithLen` fact. On that same pinned artifact, two control runs
measured 948.4-985.5 MB peak private memory and two candidate runs measured
947.6-951.6 MB; all four emitted the same 1,191,490-byte artifact and canonical
SHA, and an LLVM-built candidate emitted that same artifact. Candidate elapsed
time was 16.286-17.031 seconds versus 15.798-16.942 for
the controls, so this is not recorded as a speed win. Function-level String
reclamation and integrated parser/semantic/MIR stage lifetime measurement
remain open.

Generic type canonicalization now consumes the same nested-comma range owner as
call and parameter facts. The call-only producer name was removed instead of
kept as an alias. Top-level label and closing-delimiter checks use byte facts;
integrated `CharAt` calls fell again from 424,152 to 337,974 (88.1 percent
cumulative from the original profile). Runtime remained neutral at
36.432-36.743 seconds against a 36.528-second same-window control, and C/LLVM
drivers retained the byte-identical artifact.

The first shared source-scan slice now consumes `CharCode` and
`SubEqualsWithLen` facts rather than allocating one-character and keyword
`String` values in trivia, parser-cursor, and semantic-text hot loops. On the
same integrated-driver `mir_lower` input, the measured runtime moved from
37.915-38.071 seconds to 36.891-37.131 seconds with byte-identical output.
Parser 188/188 and semantic 111/111 manifests remain expected-artifact equal
under C and LLVM, and the C/LLVM integrated drivers emit the same 151,762-byte
artifact. This is one source-scan owner closure, not whole parser/semantic text
lifetime closure.

The next semantic slices give top-level operators one shared fact owner,
validate qualified callable names through byte ranges, and capture call
argument/signature partitions as `SemanticDelimitedRangeFacts`. Arity,
builtin, receiver, and type checks now share those ranges by `ref`. Integrated
`CharAt` calls fell from 2,851,682 to 424,152 (85.1 percent), while same-window
runtime remained neutral at 36.854-36.927 seconds against a 37.029-second
control. C/LLVM semantic parity remains 111/111 and both integrated drivers
emit the same 151,762-byte artifact. This closes duplicated semantic text
ownership; it does not close remaining type-name/expression scans or the
full-source gen3 fixed point.

The latest DRV-2 slice carries normalized expression graphs from the parser/HIR
artifact into MIR branch, definition, value-return, Log, and bare-call
instructions as
`expr0_graph`. The precedence/postfix parser emits logical, equality,
relational, additive, multiplicative, index, logical-not, numeric-negate,
member-access, direct-call, and call-argument node kinds and child edges in the
same walk that produces the compact parity projection. Unary and call-root
nodes carry one edge; member-access nodes carry receiver/member edges, and each
call-argument node carries the prior call spine plus one argument. Malformed
arity, a non-leaf member name, or a call-argument whose left edge is not a call
spine is rejected. Array literals carry one zero-arity literal root plus an
ordered element chain; each element remains its full recursive expression
graph. Struct literals carry a braced literal root plus ordered field-name,
field-binding, and field-spine nodes; each field value remains its full
recursive expression graph. An element or field whose left edge is not its
declared spine is rejected. Typed
source compilation binds roots by
`(owner kind, lane)` for `if`, `while`, `let`, assignment, and value return; a
text-created artifact without a required graph fails closed. Direct
`--mir-json` compilation likewise requires the carried graph and does not
reparse `expr0`. C-built and LLVM-built drivers emit byte-identical MIR JSON
and C across the bounded source/MIR intersection. The strengthened
mutable-local fixture covers arithmetic precedence in initializer, assignment,
condition, and return positions, and its generated program exits successfully.
This closes parser production, MIR carriage, and hard consumption for those
migrated operators and statement lanes. Index reads select the collection
runtime ABI from their receiver fact without calling the legacy index scanner.
Logical-not and numeric-negate emit directly from their operand handle rather
than reparsing the unary root text. Direct identifier calls now emit arguments,
parameter modes, runtime ABI aliases, and struct constructors from the carried
call spine without the legacy argument-list scanner. The `class_method` MIR
fixture also emits simple `self.field` access and `v.Method(arg)` dispatch from
the receiver/member handles, method signature rows, and explicit receiver
argument; those consumers cannot call the legacy member, qualified-call,
field-access, or parenthesis scanners. The `nested_member_access` fixture
projects `line.end.x` and `line.start.x` recursively from receiver/member edges
and field/type rows rather than scanning a dotted path. The
`nested_member_call` fixture follows the same recursive type facts for
`line.end.LengthPlus(2)`, then consumes the method signature row and emits the
explicit receiver without a dotted-path or member-call scanner. The named
compact C-oracle canonicalization bridge reuses the Pergyra expression parser
to upgrade legacy native MIR text; it cannot feed the hard consumer, which
still requires `expr0_graph`. Namespace-qualified calls now carry a canonical
callable target in each call-node row; direct hard-MIR consumption validates
that target against semantic signature ownership before codegen. Object-init
internals, borrow/receive/spawn/await unary forms, and expression result-type
classification beyond graph-owned struct literals remain `BRIDGE` work.

The focused codegen rung requires expression graphs for both collection
mutators: `ArrayPush` consumes the value lane and `ArraySet` consumes the
auxiliary lane. The parser extracts the constructor argument from each call
spine, the typed artifact carries that root, self MIR attaches it to the call
instruction, and the MIR JSON importer requires the same graph before codegen.
A collection element is emitted through one graph-element owner and the
expected collection element type; indexed assignment, `ArrayPush`, and
`ArraySet` cannot select scalar, String, or struct emission by reparsing the
value text. Focused C-built and LLVM-built DRV-2 legs matched canonical MIR,
emitted C, and native execution for Int, String, and `CodegenAstTextNode`
collection values, and missing or invalid graphs failed closed.
Array-literal element emission now consumes the parser-owned array root and
ordered element graph handles. The old local-binding body rows and dedicated
codegen view are deleted, and the static gate rejects sequence splitting in
the hard `Let` consumer. `ast_node_array_literal.pgy` is the twenty-seventh
DRV-2 MIR fixture. C-built and LLVM-built drivers produced byte-identical
canonical MIR and C for that fixture, and both rejected missing or invalid
graphs. `str_array.pgy` is the twenty-eighth DRV-2 MIR fixture. Indexed
assignment now carries the parser-owned target/index tree in the assignment
auxiliary lane and projects it as instruction-owned `expr1_graph`. The hard
emitter recursively consumes the receiver and index handles; the old
target-index text accessor and `IntEval(idx_expr, env)` recovery are deleted.
C-built and LLVM-built focused drivers emitted byte-identical self MIR, matched
the native canonical MIR, generated programs that printed `2`, and rejected a
missing target graph with the same fail-closed diagnostic. This focused slice
did not rerun the complete 28-case matrix.

The Log statement lane now extracts its single argument subtree from the
parser-owned call spine, requires that atom-lane root in semantic and MIR
verification, and emits the value through `RewriteExprFromSemanticGraph`.
`EmitLog` cannot reopen the old `StartsWith("ToString(")`, `Substring`, or
semantic-shape paths. A richer self MIR call graph and the approved compact
C-oracle graph initially selected different ToString optimizations; the parity
gate falsified that topology-dependent output, so both now project the same
runtime-alias C form. Hard `Log` formatting now consumes the verified
`SemanticAstStatementTypeFacts` row carried by `DriverRung2VerifiedFacts`;
`EmitLog` no longer calls `ExprKind` or reclassifies the source payload. The
node-handle query rejects wrong-kind, unverified, `Unknown`, and missing rows.
Hard `match` emission consumes the same row for its subject type, so enum case
projection no longer calls `ExprKind(match_subject, env)`. The owner contract
contains a real Match row and verifies its inferred `Int` type under both C and
LLVM. This closes the statement-result-type owner row. Other expression
result-type classification beyond the graph-owned nominal struct-literal row
belongs to the separate `selfhost.expression_surface` bridge.

The bare-call statement lane now classifies direct calls through the canonical
`TypedAstCallStatementKindForCallee` owner, carries the complete parser call
spine at `(TypedAstKindBareCallStmtTag, AstExpressionLaneAtom)`, and emits it
through `RewriteExprFromSemanticGraph`. The retired text payload accessor and
`RewriteExpr(call_expr, env)` fallback are gate-forbidden. On the pinned
`param_carriage` fixture, C-built and LLVM-built DRV-2 drivers emitted
byte-identical MIR JSON and generated C, both projected `Mutate(&value);`, and
the generated program matched the native oracle output `2 / 2 / 42`. Removing
only that instruction's graph produced the same fail-closed diagnostic under
both driver builds. The component contract is green. The broader 20-source /
13-MIR corpus was started but exceeded the five-minute focused-gate budget, so
this entry claims only the pinned falsifying fixture and does not present a new
full-corpus refresh or a released/default replacement increase.

Pipe syntax now canonicalizes in `ParsePipeFact` to the existing direct-call
and call-argument graph instead of collapsing the rendered call to a leaf. The
new `pipe_carriage` fixture proves `5 |> Double |> Add(3)` as a nine-node nested
call spine with root text `Add(Double(5), 3)`. C-built and LLVM-built DRV-2
drivers emitted byte-identical MIR JSON and generated C; the native-oracle and
self-produced canonical MIR JSON were byte-identical, and both executables
printed `13`. Removing the initializer graph failed closed with the same
diagnostic under both driver builds. The DRV-2 manifest therefore contains 14
canonical MIR producer/consumer fixtures. This closes pipe graph production,
carriage, and consumption; `?`/object-init/special-unary and structured leaf
bridges remain open, and the full 20-source/14-MIR matrix is not claimed
refreshed by this focused run.

The 2026-07-11 owner-isolated closure raised the M2 source and stage minima to
250. The preceding unfiltered ledger exposed six codegen gaps; focused reruns
closed those six at 6/6, and the two newly split executable contract owners
passed lexer/parser/semantic/codegen at 2/2. The readonly `ref` parameter row is
now emitted as `const T *`, retained as a readonly binding fact, and consumed by
member/call rewriting without recovering parameter mode from text. `Action:`
rows under `Subject` now project to the canonical function kind and carry the
subject owner/self type through the same signature inventory as ordinary
functions. This is a 250-source completeness ratchet, not a released compiler
replacement claim; released/default replacement remains 0%.

The typed `AstArena` shape now lives at
`src/self_hosted/hir/typed_ast_arena_owner.pgy`, not under codegen. The old
codegen-owned file is rejected by the component contract. This is an owner
closure, not a substitution-percentage increase: parser output is still a
compact AST-text artifact internally, but parser now returns one
`AstTreeArtifact` carrying text provenance, the shared arena, and node count.
The temporary `CodegenAstTextNode` inventory is consumed while constructing the
arena and does not cross the artifact boundary. The integrated driver passes
that same artifact through one `SemanticAstArtifactAnalysis` and into codegen
without rebuilding the arena. Entrypoint cardinality and function owner/name,
parameter name/type/mode, and return-type rows are derived once by semantic and
consumed by function emission, prototype emission, role-operator lookup, and the
codegen type environment. The deleted codegen-owned signature scanner cannot
return. Local declaration name/type/scope/initializer-payload facts now follow
the same path and codegen no longer recovers them from the arena.
The isolated DRV-2 `--emit-c-verified` path joins semantic initializer,
iteration, assignment, expression-use, return, condition, and body facts and
fails closed before codegen. DRV-0/DRV-1 remain the lightweight breadth path.
Wiring the current source scanner into the driver would create a second parser
and does not count.

**Velocity correction (2026-07-12):** the expansion ledger currently has nine
ACTIVE blockers: five direct executable-substitution blockers and four
process/evidence blockers. Despite substantial bounded owner and gate work over
roughly fifteen days, released/default replacement remains 0%. SoT is therefore
enforced as a condition of one active hard-substitution rung, not pursued as an
independent globally complete project. The track uses a 70/20/10 effort split
for executable replacement, build/test feedback, and SoT/process maintenance,
and permits at most two consecutive SoT-only commits before an executable
delta or an explicit blocked record. The accepted process is
`docs/self_hosted/16_hard_substitution_velocity_process.md`.

The first post-correction executable delta moved array-literal body ownership
into `SemanticAstLocalBindingFacts`. The codegen view consumes the typed row,
the old AST-text array-literal owner is deleted, and the component gate forbids
`StringTrim` / `CharAt` structure recovery in the replacement view. The focused
`array_index_assign` fixture produced byte-identical generated C from C-built
and LLVM-built codegen tools (SHA-256
`DD203935F1F28983577975D65F4C3C0E8E679DF3FB45115F5AF9446A9A138756`) and the
generated program matched the committed output. This is one mixed-tree
consumer closure; released/default replacement remains 0%.

The next executable delta first moved try-expression shape into a semantic
local-binding row. That intermediate owner is now superseded: postfix `?` is a
parser-owned `AstExpressionNodeTry` with one operand edge, and hard codegen
consumes only the semantic expression-graph view. The parallel local-binding
operand string and its dedicated codegen view are deleted. A named compact
bridge preserves the same Try graph only while canonicalizing legacy/native
MIR input; it is not a hard-codegen fallback. Focused `option_try` evidence
made C-built and LLVM-built DRV-2 raw MIR byte-identical, made native/self
canonical MIR byte-identical, and made every source/MIR route emit the same C
and committed runtime output. Removing `expr0_graph` fails closed on both
drivers. The full 20-source/15-MIR matrix was not refreshed in this slice, and
released/default replacement remains 0%.

The next executable delta completed recursively nested field reads. The parser
already emitted `MemberAccess(MemberAccess(line, end), x)`; hard codegen now
derives each receiver type recursively from that graph and `LookupFieldType`
rows. It cannot call the text-backed `ExprMemberFieldType` dotted-path scanner.
The `nested_member_access` fixture made C-built and LLVM-built drivers emit
byte-identical MIR and C, executed equal to the native oracle (`3`), and rejected
both a missing graph and an invalid root. The full producer-first gate is green
at 20 source fixtures and 17 MIR fixtures across both driver backends.
Nested-receiver instance method calls are closed in this bounded hard path.

The next executable delta closes namespace-qualified call classification.
`SemanticExpressionCallTargetFact` resolves `Math.Add` through the canonical
callable index, carries `Math_Add` through self MIR JSON, and requires the same
target during direct `--mir-json` consumption. Hard codegen consumes that fact
before method dispatch and no longer rebuilds a namespace symbol from receiver
text. Replacing the carried target with `none` fails closed under C-built and
LLVM-built drivers. The full producer-first gate is green at 20 source fixtures
and 18 MIR fixtures; both drivers emit byte-identical MIR and C, and the new
fixture runs equal to the native oracle (`7`). Widening the hot semantic graph
arena to six growable-array values exposed an LLVM aggregate ABI crash, so the
semantic representation keeps one optional canonical target-name row while the
MIR boundary remains explicitly tagged by `call_target_kind` plus
`call_target_name`. Released/default replacement remains 0%.

The following executable delta closes `for` value/auxiliary graph carriage.
`ParseForStmt` now captures the lower/collection expression in the value lane
and the range upper expression in the auxiliary lane during the canonical
precedence walk. MIR attaches the value graph to `loop-init` and the range-stop
graph to `branch`; direct MIR consumption requires them in that order. Hard
codegen emits range bounds and identifier foreach collections from those node
handles and no longer calls `IntEval(start/end)`, `ExprKind(collection)`, or
`RewriteExpr(collection)` on the statement text. C-built and LLVM-built
drivers emitted byte-identical C for `forloop` and `for_each` (SHA-256
`D39BE785...B57F7D3` and `C17441A...356DD`) and matched outputs `0/1/2` and
`60/abbccc`. Removing only the range-stop or foreach-value graph failed closed
under both drivers. The full 20-source/18-MIR matrix exceeded the five-minute
focused budget and was terminated, so that historical slice claimed only those
two falsifying fixtures. Non-identifier foreach classification was closed by a
later 20-MIR producer run; released/default replacement remains 0%.

The next executable delta deletes the payload-free enum call-argument text
bridge from hard graph emission. A qualified enum argument such as
`IsEast(Direction.East)` is already a parser-owned
`member_access(Direction, East)` subtree, and the type environment owns its
enum projection row. `RewriteSemanticCallArgument` now delegates directly to
that graph instead of reclassifying the source token from the expected
parameter type. The nineteenth DRV-2 MIR fixture emitted byte-identical C from
C-built and LLVM-built drivers (SHA-256
`E4E901D03F43C7429A2E9E033FCC12651D58718897453F0875AE4285D82409A3`) and
both executables printed `east`. Removing only the call graph failed closed on
both drivers while the expected enum parameter and expression text remained.
Array and struct literal arguments remain bounded text bridges; released/
default replacement remains 0%.

The twentieth DRV-2 MIR fixture closes non-identifier foreach normalization.
`SemanticAstIterationTypeFacts` owns the full iterable type and whether the
collection requires a single-evaluation hoist. Self MIR consumes those rows to
emit the same reserved synthetic local and post-order ordinal as native
`forin_desugar`, while direct MIR consumption sees only the normalized local.
The nested/sibling fixture makes native/self canonical MIR byte-identical under
both C-built and LLVM-built drivers and run-equal at `30`; removing the
synthetic source-local type is rejected. The temporary consumer-side callable
return lookup was deleted and is forbidden by the component contract. The
synthetic ordinal lookup reports absence as `Option<Int>`; the likeness gate
forbids reintroducing its former `-1` sentinel.

The twenty-first DRV-2 MIR fixture closes array-literal call-argument
reparsing. `ParsePrimaryFact` now keeps the empty literal root and every element
subgraph instead of collapsing the literal to one leaf. The semantic/MIR graph
and JSON projection preserve that ordered spine, and
`RewriteSemanticCallArgument` emits each element through its node handle; it no
longer recognizes `[` or calls `EmitArrayLiteralValue(source, ...)`. A nested
arithmetic/direct-call fixture made all four C-built/LLVM-built native/self
canonical MIR artifacts byte-identical (SHA-256
`56A2A77CBDCE635ECE29084E378B801C56E5359F31DBDA758DBD391D45A98A13`) and
printed `11`. Reclassifying the literal root as a leaf while retaining the
element chain is rejected as an invalid MIR expression graph. Struct literal
arguments and expression result-type classification remain open; released/
default replacement remains 0%.

The twenty-second DRV-2 MIR fixture closes named struct-literal call-argument
reparsing. The native AST records whether an `AST_CALL` came from braced
initializer syntax, so AST/MIR round trips preserve `Line { ... }` instead of
aliasing it to `Line(...)`. The self-host parser/HIR graph carries explicit
struct-literal, field-name, field-binding, and field-spine nodes. Semantic
identifier checking traverses field values while treating the type and field
labels as declarations, and expected-type codegen recursively emits nested
struct values from those graph edges. The old call-argument struct text
classifier and rewrite fallback are deleted. C-built and LLVM-built DRV-2
drivers are green across 20 source and 22 MIR fixtures; native/self canonical
MIR, emitted C, and execution agree, the nested fixture prints `6`, and
reclassifying a struct-literal spine node as a leaf fails closed. Top-level
struct values outside the migrated call-argument lane and initial compact
bridge graph construction remain open; released/default replacement remains
0%.

The twenty-third DRV-2 MIR fixture closes general named struct-literal value
reparsing for local initialization, assignment, and value return. The semantic
expression graph owns the braced literal and ordered field spines; its nominal
type owner validates the literal type against the canonical constructor row.
Initializer, assignment, and return verdicts consume that graph fact, and
codegen emits through the expected-type graph boundary instead of
`EmitStructValue`. A borrowed expression-surface view yields only the scalar
root handle, so initializer, assignment, statement, and iteration rows no
longer return a graph-bearing view per lookup. C-built and LLVM-built DRV-2
drivers are green across 20
source and 23 MIR fixtures; the value-flow fixture prints `11`, and changing
the carried struct-literal root to a leaf is rejected as invalid MIR. The
legacy text struct emitter remains only on explicit unclosed lanes such as
`Option<struct>` payloads and `CodegenAstTextNode` collection elements; initial
compact graph construction and non-struct result-type classification also
remain open. Released/default replacement remains 0%.

The twenty-fourth DRV-2 MIR fixture closes `Option<struct>` `Some` constructor and
payload reparsing for local initialization, assignment, and value return. It
also carries contextual `None` through local initialization, reassignment, and
value return: the native C assignment consumer obtains the expected option type
from the MIR source-local expression-type fact, the LLVM consumer obtains it
from the MIR local expected-type fact, and both fail closed instead of selecting
an `Option<Int>` fallback. A
shared semantic call-spine view owns the ordered `Some` argument handle, while
the expected `Option<T>` type selects the MIR-owned scalar or struct runtime ABI
row. Codegen emits the payload through the expected-type semantic graph
boundary; it cannot recognize `Some(`, slice payload text, or call the legacy
struct text emitter. C-built and LLVM-built DRV-2 drivers are green across 20
source and 24 MIR fixtures; the fixture prints `7` and `11`, and reclassifying
its `Some(Pair { ... })` call-argument spine as a leaf is rejected as invalid
MIR. The same graph view joins nominal constructor field rows before type
inference: assigning `String` to `Pair.left: Int` is rejected by both the
self-host driver and native oracle. The emitted-C ratchet requires the
`pgy_option_none_Pair()` ABI constructor, and C-built and LLVM-built DRV-2
drivers remain green across 20 source and 24 MIR fixtures.
Initial compact graph construction, array-literal collection elements, and
non-struct result-type classification remain open.
Released/default replacement remains 0%.

The next assignment-consumer delta removes the remaining scalar
`Option<T>` reassignment fallback from self-host codegen. A single verified
body-type view carries canonical assignment and statement type rows; the
assignment emitter now selects Option ABI emission from the semantic expected
type and expression graph rather than an environment-kind lookup or `None`
text inspection. Direct synchronous recursion may reborrow the same readonly
fact parameter into the same parameter position, while return escape remains
fail-closed. The semantic suite is green at 2799/2799 and the compiler-scale
HIR probe has zero diagnostics. A focused assignment projection probe is
run-equal under C-built and LLVM-built binaries for `Option<Int>` and
`Option<String>` `Some`/`None` reassignment; both binaries also reject a missing
expected type. This delta does not raise the released/default replacement
percentage or close the active expression-surface row because the full
71-fixture codegen matrix was not rerun. The broader codegen parity harness
reuses its manifest-producing C tool for the C parity leg only when the full
self-host source-set, tool-source, backend, and compiler fingerprints match.

The initializer type-fact rung projects semantic initializer rows into MIR
routine input. `let seed: Int = 1; let x = seed + 2;` now lowers `x` as an `Int`
local without requiring a source annotation, source-text reclassification, or
a backend default. The initializer owner may rebuild its own facts for its
contract, but iteration, assignment, statement, and MIR consumers only check
and consume the projection. Focused C and LLVM probes are output-equal; both
reject a missing initializer row and a present row with no inferred type. This
closes `selfhost.initializer_type_verdict`. For fully graph-owned scalar
operator trees, semantic result typing now consumes graph nodes instead of
calling `ExprType` on the preserved source text. A graph-leaf-only corruption
is rejected as `undefined_symbol` under both backends, while changing the same
leaf to a String produces `binop_type_mismatch` from graph child types. The
unchanged source/root text cannot recover either result. The mixed expression
bridge remains ACTIVE for call/member/composite trees outside the declared
scalar capability subset. Direct named calls with concrete scalar returns now
consume the graph callee and canonical callable return table. Keeping
`ToIntValue(2)` and its root text unchanged while changing only the graph
callee to `ToTextValue(Int) -> String` fails as `let_type_mismatch` under both
backends; the positive call projects `x: Int` into MIR identically. Call
arguments, member/namespace calls, generic/aggregate returns, and composite
typing remain the explicit bridge. Direct calls whose parameter rows and
argument nodes are graph-owned scalar trees now also validate operand
diagnostics, arity, and argument types from graph handles. Keeping
`ToIntValue(2)` unchanged while replacing only its graph argument leaf with a
String fails as `call_arg_type_mismatch`; `ToIntValue(1 + (2 * 3))` projects
`x: Int` under C and LLVM, while changing only the nested graph leaf to a
String fails as `binop_type_mismatch`. Parser-canonical root spelling is
verified by the same compact parser owner rather than accepted as an alternate
text authority. Concrete nested direct calls recurse over graph call-spine
handles. `ToIntValue(ToIntValue(2))` projects `x: Int` under C and LLVM;
changing only the inner graph callee to a String-returning function fails at
the outer call as `call_arg_type_mismatch` under both backends. The same single
concrete-scalar capability now composes operator and call nodes:
`1 + ToIntValue(2)` projects `x: Int` under C and LLVM, while changing only
the graph callee to a String-returning function fails as
`binop_type_mismatch`. The former direct-call-only verdict owner and names were
deleted rather than retained as aliases.
Namespace-qualified static calls now consume the canonical target already
carried on the semantic call node. `Math.Add(2)` resolves through `Math_Add`
under C/LLVM parity; changing only that carried target to the String-returning
`ToTextValue` fails as `let_type_mismatch`. The direct-leaf-only type owner and
names were deleted rather than retained as aliases.
Receiver-bound scalar member calls now resolve through one semantic target
fact. The callable table preserves method ownership as `Owner_Method`, the
member graph supplies receiver/member handles, and the lexical environment
supplies the receiver type. `box.Get()` consumes the implicit `self: Box`
parameter through target offset one; changing only the graph member handle to
the String-returning `Box_Text` fails as `let_type_mismatch` under C and LLVM.
That same bounded target is now stored as `(member, Box_Get)` on the semantic
graph, copied into self MIR, and consumed by hard codegen as `Box_Get(box)`.
Codegen no longer joins receiver type and member spelling. Removing only the
carried target row fails at MIR production under C and LLVM. Compact graph
construction threads row arrays instead of passing the enlarged graph arena as
an `inout` aggregate; this fixes the LLVM-only crash and is gate-ratcheted.
Generic receiver locals now consume the typed canonical type-name fact:
`Box<Int>.Count()` carries `member/Box_Count` through self MIR and emits
`Box_Count(box)` under C/LLVM parity. Removing only that generic target row
fails at MIR production. Chained field receivers now consume nominal field
facts from graph handles: `holder.box.Count()` carries the same target and
emits `Box_Count(holder.box)` under C/LLVM parity. Direct calls now carry a
mandatory `direct` target through semantic analysis and self MIR; hard codegen
consumes that canonical name and cannot recover identity from the callee leaf.
Removing the direct row fails before emission under C and LLVM. The bounded
call-target identity row is closed, while generic substitution, collection,
Option/Result, and composite aggregate validation remain bridged.
Released/default replacement is still 0%.

Nominal aggregate call returns now consume that closed target identity and the
canonical signature return row. `MakeBox() -> Box` projects a `Box` local and
hard codegen emits `MakeBox()` under C/LLVM parity. A source-preserving target
mutation to `ToTextValue() -> String` fails as `let_type_mismatch`; the scalar
graph consumer no longer indexes the return array independently. Generic
substitution for exact-formal direct calls is also executable: the typed HIR
generic-parameter row feeds ordered signature facts, and
`Identity<T>(value: T) -> T` projects `Identity(2)` as `Int` under C/LLVM.
Changing only the carried target to `ToTextValue(Int) -> String` fails as
`let_type_mismatch`. Signature capture now owns one flat parameter/return
type-expression arena. The same structural owner projects
`Wrap<T>(value: T) -> Option<T>` as `Option<Int>` and
`First<T>(values: Array<T>) -> T` as `Int` for an `Array<Int>` argument. An
`Int` argument to the structured `Array<T>` parameter is rejected. No
call-site type text is reparsed or replaced. The postfix parser now preserves
ordered explicit actuals as graph nodes. The typed-artifact path projects
`PickSecond<Int, String>(2, "value")` as `String`, while changing only the
first actual to `String` rejects the `Int` argument as
`call_arg_type_mismatch` under C/LLVM parity. The compact text bridge is not an
accepted path because it erases the actual rows. Collection/Option/Result
policy and composite aggregate validation remain bridged. The next executable
slice closes scalar Option/Result builtin policy on the typed graph path:
initial target capture now includes builtin signatures, and
`SemanticExpressionGraphWrapperValueFact` consumes that target plus graph
arguments without `ExprType` or `CheckCall`. Native C-oracle, C-built, and
LLVM-built probes agree for `Some`/`UnwrapOption`/`Ok`/`UnwrapOr` and reject
non-concrete Option, non-wrapper arguments, and carried-target drift. Collection
result/element typing and uncovered aggregate wrapper payloads remain bridged.

The next executable slice closes caller-visible collection mutation admission
without claiming collection typing as a whole. One canonical policy owner is
consumed by specialized array statements and by general graph calls. The graph
path requires a carried direct target and receiver node, and the source call
checker no longer replays that mutation decision for graph-owned calls. Native
C-oracle, C-built, and LLVM-built probes accept local/`inout` mutation, reject
default-parameter mutation, and reject carried-target drift. Collection
result/element typing and aggregate payload validation remain bridged.

Aggregate field validation is now graph-owned for the active scalar/direct
nominal capability. A dedicated field type owner projects scalar operators,
direct returns, nested struct values, `Some(struct)`, wrapper unknowns, and
structural integer-literal widening. The struct verdict no longer calls
`ExprType` or source-text `ExpressionAssignableTo`. Native C-oracle, C-built,
and LLVM-built probes reject a bad field, a source-preserving leaf type drift,
and a missing child fact. Generic/member aggregate field values remain bridged.
The actual C-built and LLVM-built rung-2 drivers also pass all 20 body fixtures
and the selected `option_struct_value_flow` MIR fixture. The remaining MIR and
integration matrix is not implied by this bounded result.

The Pergyra-owned gate dashboard exposes twelve active proof gates with their
Make targets, tiers, budgets, declared states, and owner facts. Run state comes
only from a separately validated result artifact: absent results stay
`NOT_RUN`, unknown/duplicate IDs fail closed, and the process bridge enforces
the manifest budget through the portable timeout owner. The dashboard is
operational evidence, not substitution progress.

The third executable delta deleted
`codegen/input/ast_text_collection_stmt_owner.pgy`. The parser-owned artifact
was already captured by `SemanticAstStatementFacts`; `ArrayPush` target/value
and `ArraySet` target/index/value now flow through the fail-closed semantic
statement codegen view. The focused `array_push`, `array_sum`,
`str_array_push`, and `str_array` fixtures were run-equal under C-built and
LLVM-built codegen tools, and all four emitted C artifacts were byte-identical.

The fourth executable delta added `SemanticAstEnumFacts` to the integrated
artifact analysis and deleted `codegen/input/ast_text_enum_variant_owner.pgy`.
`CollectEnums` now consumes semantic enum names, ordered variants, and payload
arity through a fail-closed codegen view; enum aux text is no longer parsed in
codegen. Native and self-host AST printers now preserve variant parameter
types; parser parity is 188/188 on C and LLVM with live drift enabled.
`enum_match` remains run-equal and byte-identical across codegen tool backends,
while `codegen_parity.sh` requires both tools to reject the TestHarness-owned
two-parameter payload-enum artifact with the same committed fail-closed
diagnostic. Semantic enum capture uses nested comma ranges, so
`Rect(Int, Int)` is one variant with arity two rather than two rows.

The fifth executable delta reused the already integrated
`SemanticAstNominalConstructorFacts` owner for nominal names and ordered field
name/type rows. `CollectStructs` no longer walks nominal/field AST rows; the old
mixed declaration owner was deleted and the remaining role bridge was renamed
to its exact responsibility. Four struct fixtures are run-equal under C-built
and LLVM-built codegen tools.

The sixth executable delta added `SemanticAstRoleFacts` for role name, target
type, and owned method `NodeId` rows. Operator binding and role receiver ABI now
consume those rows; the role AST bridge and descendant scan are deleted. The
TestHarness-owned role operator prints `123` under C-built and LLVM-built
codegen tools, matching the native C oracle.

The seventh executable delta moved runtime/header expression usage to
`SemanticAstExpressionSurfaceFacts`. Codegen keeps builtin-group policy but no
longer reads arena atom/value/auxiliary rows or parses calls locally. Nine
runtime-family fixtures plus the role/enum hard legs are run-equal under
C-built and LLVM-built tools.

The eighth executable delta moved canonical runtime type usage to
`SemanticAstTypeSurfaceFacts`. Codegen no longer scans arena type-name rows.
The LLVM leg exposed and then closed one missing concrete `String` unwrap fact;
the same nine runtime-family fixtures now pass C/LLVM parity.

The ninth executable delta moved runtime statement-kind usage to
`SemanticAstKindSurfaceFacts`. Codegen no longer scans arena kind rows, and the
old local tag named `ArrayLiteral` was removed because canonical tag 16 is
`ArrayPopStmt`. Five kind-driven fixtures plus the enum/role fail-closed legs
are run-equal under C-built and LLVM-built tools. Runtime usage projection now
accepts only semantic expression, type, and kind facts; its dead arena/count
parameters are gone.

The tenth executable delta moved `Main` cardinality and selected function-node
identity behind `SemanticAstFunctionSignatureFacts`. The semantic verdict now
counts signature names, and codegen consumes an `Option<Int>` projection rather
than rescanning arena function/name rows or using `-1` as hidden control flow.
The helper-before-Main `func_call` fixture and `hello` pass C/LLVM parity.

The eleventh executable delta moved statement dispatch to three semantic
authorities: local-binding identity for `Let`, assignment identity for
`Assign`, and statement kind rows for all remaining emitted statements.
`Defer`, `Break`, `Continue`, and `MatchDefault` were added to the statement
inventory; twenty codegen arena predicates were deleted. Twelve representative
fixtures pass C/LLVM parity while `Else`/`Block`/`Then` remain syntax-structure
traversal rather than semantic fallback.

The twelfth executable delta moved top-level declaration dispatch to semantic
node identity. Function signatures own function nodes; nominal, role, and enum
rows own their declaration nodes. `program_emit.pgy` no longer classifies those
four declarations through codegen arena predicates. Seven focused declaration
fixtures plus payload-enum rejection and role-operator parity pass under both
C-built and LLVM-built self-host codegen tools.

The thirteenth executable delta completed top-level declaration classification:
ability and event nodes now consume canonical `SemanticAstKindSurfaceFacts`.
The earlier runtime-consumer-specific owner label was generalized to node-kind
surface ownership rather than duplicated. Event rejection is now a committed
TestHarness negative leg, and no codegen arena declaration predicate remains.

The fourteenth executable delta moved top-level expression operator positions
into `SemanticAstExpressionSurfaceFacts`. The owner stores normalized
atom/value/auxiliary payloads and compact operator rows. `Log` emission now
looks up its atom row by node identity, and the role-operator path consumes the
stored additive index and operator kind instead of calling `FindTopLevelPlus`.
The fixture proves `+` dispatch without misrouting `-`. The migrated
function is ratcheted against that fallback, and C/LLVM-built tools remain
oracle-equal. Value/auxiliary consumers and recursive child expressions remain
the active mixed-expression bridge.

The fifteenth executable delta extended the same authority without adding a
new expression owner. Scalar/String returns consume atom-lane shape rows;
ordinary scalar/String local initializers and assignments consume value-lane
rows. Five focused fixtures plus the role and negative declaration legs pass
under C-built and LLVM-built tools. Indexed collection values,
Option/Result/struct wrapper internals, auxiliary lanes, and recursive child
expressions remain bridge consumers.

The sixteenth executable delta made logical/comparison root facts precise:
separate `||`, `&&`, equality position, and equality-kind rows now drive
`if`/`while` lowering. The shape-aware condition function cannot call
`FindTopLevelOp2`; both paths share one String/enum equality projection.
Logical precedence and String equality fixtures pass C/LLVM parity. Recursive
child conditions remain the next expression-tree seam.

The seventeenth executable delta added stable semantic expression node handles
and child edges for `if`/`while` condition atoms. Recursive condition emission
now traverses those edges and cannot call `RewriteBool`, `FindTopLevelOp2`, or
`Substring` to rediscover precedence. The grouped `(a || b) && c` fixture
prevents flattening from changing meaning; C-built and LLVM-built codegen tools
emit byte-identical C and remain run-equal on logical and String-equality
fixtures. The owner remains a bridge because graph production still lowers the
compact parser payload instead of consuming parser-arena expression nodes.

The follow-up ratchet deleted the final dead codegen arena payload views:
atom, type, value, auxiliary value, parameter type, and parameter mode. The
remaining mixed-expression blocker is therefore exact: semantic owner rows
still carry expression text into lowering. Codegen no longer has a direct arena
payload recovery API that can bypass those owners.

The focused inferred-generic extension now covers assignment and value-return
roots in addition to local initializers. `SemanticAstGenericSpecializationFacts`
remains the only specialization owner; self MIR and hard codegen consume its
call-node and actual-type rows. C-built and LLVM-built DRV-2 drivers matched
native MIR, emitted C, and run output for the selected thirty-first MIR fixture.
Changing only the assignment or return graph leaf failed closed under both
backends, and the producer is ratcheted against `ExprType` and text slicing.
The complete 31-fixture DRV-2 matrix was not rerun in this slice, and released
or default replacement remains 0%.

The same bounded closure is now modeled in
`docs/semantics/proofs/SoTAuthority.v`. Rocq/Coq checks owner completeness,
uniqueness, required consumption, and zero semantic fallback, while
`tests/sot_authority_adequacy_smoke.sh` binds those names to the live semantic
owner and codegen consumer and mutation-tests missing-owner and fallback
reintroduction. The bounded model now covers the array-literal body, try-let
operand, collection-mutation statement, enum declaration, and nominal/field
declaration and role operator consumers; it
does not increase released/default replacement or close the remaining
mixed-expression consumers.

The whole compiler skeleton now has 36 machine-gated authority rows and 15
classified derived fact carriers in
`docs/semantics/sot_owner_spine_registry.md`, with sixteen `CLOSED`, seven
`BRIDGE`, and thirteen `ACTIVE` rows. Each authority row names its stable handle,
Coq fact/owner,
authority implementation, last consumers, forbidden fallbacks, gate, and open
reason. `tests/sot_authority_edge_smoke.sh` consumes the registry without a
copied owner list or status count, checks registry/Coq projection equality,
producer uniqueness, CLOSED-consumer fallback absence, and complete
classification of every self-host `*_fact_owner.pgy`. This defines the owner
outline; it does not raise the released/default replacement percentage.

The assignment type-verdict row is the first closure executed through this
catalog. Driver paths now assemble one `SemanticAstBodyTypeBundle`; codegen
receives an `own` bundle projection and cannot call the four body-type fact
producers directly. A focused C/LLVM probe is output-equal and fails closed for
missing assignment expected type and missing indexed-target type. This closes
that owner/consumer seam only; initializer and iteration type-verdict rows
remain `ACTIVE`.

## Headline Number

### Three-axis scorecard

These numbers must not be collapsed into one percentage:

| Axis | Current evidence | Meaning |
|------|------------------|---------|
| Implementation inventory | 30,720 frontend/backend LOC / 287,406 C-reference LOC = 10.69%; broader Pergyra compiler-core inventory = 48,246 LOC | Pergyra compiler code exists; this is not substitution. The ratio denominator is the C reference, not the Pergyra compiler-core inventory. |
| Bounded executable replacement | DRV-2 has 20 producer-first source semantic fixtures and 110 committed canonical MIR producer/consumer fixtures; the standalone fact-only MIR consumer has 102 fixtures. Fixture 110 passed focused hard/C/LLVM canonical-MIR, source/MIR-C, native compile, and runtime parity, while the complete 110-case matrix was not rerun in this slice. | Explicit Pergyra-owned paths run, fail closed, and compare against the C/LLVM oracle. `make self-host-compiler` now builds the bounded driver through Pergyra parser/codegen seeds. |
| Released/default replacement | 0% | default `pgy` still uses the C-owned native driver; explicit DRV-2 uses the Pergyra MIR producer and consumer. |

The scorecard prevents two false claims: implementation volume must not be
reported as native replacement, and native replacement at 0% must not erase
measured progress in bounded executable rungs.

**Hard self-host contract (2026-06-22):** hard self-host is now gated as
staged substitution rather than tracked as a separate cleanup project. The
contract lives in `docs/self_hosted/10_hard_self_host_contract.md`, and
`tests/self_host_hard_contract_smoke.sh` keeps the docs, Makefile wiring, active
hard rungs, C oracle, LLVM oracle, bridge/fallback split, codegen bootstrap, and
MIR JSON fact-only lowering aligned. The substitution percentage below is
unchanged by that contract gate; future percentage increases require a Pergyra
implementation to replace a real compiler stage/pass beside the C/LLVM oracle.

**Implementation inventory is live-measured, not a substitution percentage.**
Run `make self-host-progress-metric-test-smoke` to measure the current Pergyra
frontend/backend and compiler-core LOC beside the C reference inventory.
Implementation volume only proves that code exists.
**Released/native replacement remains 0%** because the default path still uses
the C-owned compiler driver. Stage-0 seed creation and final emitted-C
compilation also retain the native C toolchain. **Explicit bounded replacement:
DRV-2 is live** through `make self-host-compiler` and
`pgy --self-driver <source.pgy>`; unsupported inputs fail closed instead of
falling back to the C pipeline.
The verified component frontiers are the
lexer, parser, a bounded semantic verdict rung, and -- as of 2026-06-17 -- the
**first codegen rungs** (`src/self_hosted/codegen/`, 4,821 LOC; rung-0 string Log,
rung-1 integer let/arithmetic, rung-2 assign + `while`/`if`/`else`, rung-3
multi-function definitions + calls + `return`, rung-4 `String` types with a
variable/function type environment + runtime `pgy_concat`, rung-5 `for` loops +
`break`/`continue`, rung-6 `Bool` type + `StringLength`/`Substring` builtins,
rung-7/8 fixed `Array<Int>`/`Array<String>` literals + indexing +
`ArrayLength`/`ArraySet`, rung-9 `StringIndexOf` builtin + `Exit`, rung-10
**growable arrays** (`ArrayPush`) via a `{data,len,cap}` struct rep with
env-aware index-expression rewriting, rung-11 `StringTrim` builtin, rung-12
`FileExists`/`ReadFile` file I/O, rung-13 `Args()` user-argument snapshots,
rung-14 value-passed Int-field structs, rung-15 `Array<Int>` param/return flow,
rung-19 typed `Int` / `Bool` / `Float` / `String` struct field facts, and rung-20 nested struct-valued field facts).
The codegen entrypoint is now split into thin `main.pgy` orchestration plus
resource-owner folders: `input/` for AST path/read ownership and codegen-only
views over the shared HIR artifact. `hir/` owns compact AST-text inventory,
typed `CodegenAstTextRowFactInput` row facts, marker-node predicates and
function/return/enum/nominal/role/parameter/field payload accessors plus
statement row facts projected into typed arena rows for
`Let`, `Assign`, `Log`, `Return`, `Defer`, `ArrayPop`, `ArraySet`, `ArrayPush`,
`Exit`, `Break`, `Continue`, `For`, `While`, `If`, `Else`/`else if` routing,
and bare call statements for the transitional `pgy --ast` bridge. `run/` owns
the CLI boundary, `text/` owns codegen-specific expression facts, and
`type_facts/` owns type
evidence, compiler-world symbol rows for emitted-symbol spelling including
namespace-qualified call lowering,
`abi_layout/` for self-host C ABI type spelling, `runtime_abi/` for `Array<Int>` /
`Array<String>` plus bootstrap `Array<CodegenAstTextNode>` C collection runtime
helper symbol spelling, supported
math/random helper and target-library symbol spelling, supported host
file/stdin/argv/process helper, C process entrypoint ABI, and target-library symbol spelling, supported
`Option<Int>` / `Option<String>` / `Result<Int>` helper symbol spelling, and supported string/text
helper and conversion target-library symbol spelling, and `emission/`
for C-emission action participants. That keeps
`program_emit`, `function_emit`, `stmt_emit`, `expr_rewrite`, and
`struct_value_emit` out of fake zone folders while still making the real
resource owners visible. Parameter-mode facts (`inout` / `own` / `ref`) now
survive `pgy --ast`; the self-host C codegen consumes `inout` from function-env
`pm` facts and lowers it as value-result copy-in/copy-out instead of guessing
from `ArrayPush` or other statement text. Top-level comma-separated expression
sequences for array literals, call arguments, and struct literal field lists now
route through `text/expr_sequence_owner.pgy` instead of local emission loops,
payload-free enum literal projection routes through
`text/enum_literal_owner.pgy` instead of local enum-key reconstruction,
struct literal call-envelope facts route through
`text/struct_literal_call_owner.pgy`, and typed struct literal field-entry row
facts route through `text/struct_literal_field_owner.pgy`.
The M2 completeness ledger now inventories and ratchets
250 production self-host source files across lexer, parser, semantic, codegen,
and full-pipeline identity. The ledger itself is not a bootstrap-loop proof: it
still runs through the current C/LLVM oracle compiler path and proves source
breadth. A separate fixed-point gate now proves that the Pergyra-built bounded
parser/semantic-entrypoint/codegen driver can rebuild itself; broader semantic
and MIR inclusion remain the next compiler-pipeline bootstrap boundary. The
real-source semantic selfcheck uses the broad
203-source C/LLVM gate from the latest parity preparation refresh over the current accepted semantic subset,
including the codegen run boundary, lexer run/fixture-manifest owners, emission
action owners, type-fact owner, MIR-lower fact owners, and SEA execution-lane
mirror. The
AST-text bridge's root/body/block/then
structural marker checks now consume owner-owned `kind` facts rather than raw
line-text equality, and program/function/statement emission-depth traversal now
consumes typed arena indent/parent facts rather than raw `CodegenAstTextNode`
indent rows. `GenerateCUnit` builds that typed arena projection once and threads
the `AstArena` fact through function and statement emission participants instead
of letting recursive emitters rebuild it. Function signature emission consumes
`SemanticAstFunctionSignatureFacts` from the shared artifact; declaration
emission still consumes typed arena rows for role targets, enum names, and
fields instead of reading `CodegenAstTextNode.name`, `type_name`, or `mode`
directly. Statement emission consumes semantic-owned node/function/scope/
payload rows for `Log`, value `Return`, `ArrayPop`, `Exit`, `While`, `If`,
`Match`, match cases, and bare calls. `Let` and `Assign` consume semantic local,
initializer, target/base/index/RHS, expression-use, and type-verdict rows;
missing facts fail before codegen. `ArrayPush` emission consumes arena atom/value rows
for the receiver and pushed expression; `ArraySet` consumes arena atom/value/
aux-value rows for receiver, index, and assigned value; `For` consumes the
semantic statement/iteration owner projection for loop variable, range
start/end, foreach collection, and binding type. Program/function/statement
routing and marker checks consume arena
kind facts. Runtime/header usage facts now consume lane-specific arena facts:
type/header requirements read typed arena `type_name` rows, builtin-call
requirements scan only expression-bearing arena rows with string-literal-aware
call matching, and statement-only requirements continue to consume arena kind
facts. The deleted raw-node usage bridge cannot return.
The rest of codegen,
runtime and released/native compiler driver/LSP substitution are still 0%.
The compiler driver now has DRV-0/DRV-1 artifact rungs plus a bounded integrated
parser/codegen bootstrap fixed point, and LSP has LSP-0
diagnostic payload, LSP-1 squiggle-policy projection, and LSP-2a..LSP-2i
buffered transport/request/response/session/document-state/feature-shape/session-state/hover-content rungs
(docs/150).
The DRV-0/DRV-1 artifact rungs consume the same 69 committed codegen parity
fixture frontier as `codegen/fixture_manifest_owner.pgy`; this broadens
artifact assembly coverage. The fixed point proves a real Pergyra-built
source-to-C compiler loop, but neither it nor DRV parity counts as
released/native driver replacement until semantic and MIR stages enter that
same executable path.
The separate DRV-2 `--emit-c-verified` entrypoint makes artifact-body semantic
evidence a hard precondition without calling the source-scanning checker. It
joins initializer, iteration, assignment, expression-use, call, return, and condition
verdicts before C emission. Its semantic cost is isolated from DRV-0/DRV-1 and
ordinary codegen checks. It is a bounded hard-semantic rung, not yet the full
CFG/MIR body replacement claim.
The DRV-2 body gate runs twenty manifest-owned positive/negative fixtures
through both C-built and LLVM-built drivers, compares emitted C or diagnostics,
and C-compiles every positive artifact. The builtin signature table, canonical
type-name owner, and shared source character/trivia scanner are single owners;
the integrated driver may not recover these facts from source text.
The same executable owns a bounded typed-artifact MIR producer and accepts
`--mir-json <file>` through the existing MIR fact consumer. Source mode now
follows `artifact -> verified MIR rows -> pgy.mir.v1 -> MIR consumer -> artifact
verifier -> codegen`; it no longer calls the C MIR producer. Twelve intersection
fixtures (linear local/log, range loop, nested CFG with phi, indexed assignment,
payload-free enum return, explicit if/else phi, parameter carriage, recursive
calls, nominal method construction, typed array foreach, and parser-owned array
index read plus parser-owned logical-not)
require C/LLVM-built drivers
to produce stable canonical reconstructions, emit identical C, and run-equal.
This is not a raw native-MIR byte-equality claim: the indexed fixture therefore
also gates its pre-canonical self-MIR target and use rows directly. Indexed
assignment keeps its full semantic target text in the MIR emission payload
while SSA identity continues to use the base local name. The default native
compiler path remains unreplaced.

The foreach producer no longer assumes every loop binding is `Int`. DRV-2
carries the semantic iteration owner rows through `DriverRung2VerifiedFacts`,
projects the verified node/type rows into `SelfMirRoutineInput`, and emits
range or foreach MIR from that owner. The gate directly pins `Int` and `String`
foreach binding types plus collection SSA uses. Non-identifier collections now
carry their verified iterable type and one explicit hoist fact in those same
rows. The MIR producer evaluates the expression once into the reserved
`__pgy_forin_N` local and gives the loop only that local; MIR consumption and
codegen do not recover a callable return type from expression text. A nested
plus sibling call-foreach fixture fixes the native post-order names
`__pgy_forin_0/1/2`, and deleting a required synthetic source-local type fails
closed. C-built and LLVM-built full DRV-2 gates are green at 20 source and 22
MIR fixtures; all four canonical native/self JSON artifacts for this fixture
have SHA-256
`19815C3CD3E5C3B36AA9F70EF9241BC8105CAE5B7FFA739DA36E3B6D7F06FCCB`,
and the native/self executables both print `30`. Self-produced MIR also owns
the `parallel_capture_boundaries` inventory, including the empty bounded-subset
case, so `pgy.mir.v1` output remains consumable by the same self-host path.

The same slice removed two compiler-scale quadratic scans. Typed AST parent and
child rows now use an indentation stack plus CSR-style child offsets, and AST
text inventory uses one `Split(tree_text, "\n")` pass instead of one-character
`Substring` scanning. On the 996,867-byte, 24,340-row DRV-1 artifact, the cached
self-host codegen check measured 53,003 ms before the line-inventory change and
374 ms after it on the same machine and generated artifact.
C LSP also exposes `pgy-lsp --dump-diagnostics <src>` as a live oracle
plumbing path for LSP diagnostics shape checks and fixture-level canonical
event comparison across clean plus logical/undefined/type/condition/unary
diagnostic families, but those are not counted as released driver/LSP
replacement.
The MIR-lowering
substitution has now *started* (see below).

**MIR-lowering substitution started (2026-06-18, path (a) rung-0b):** the C
compiler now emits MIR JSON (`pgy --mir-json`, schema `pgy.mir.v1`) with the CFG
skeleton, explicit expression/source-shape facts (`expr0`, `expr1`,
`source_type`), source-local type facts (`source_locals`), and a transitional
`ast` compatibility text field captured by the MIR source-shape owner. A new
Pergyra owner graph `src/self_hosted/mir_lower/` consumes that JSON and
reconstructs the `--ast` tree, which the existing codegen lowers to C. It is
available both as a standalone tool and inside DRV-2 before the same semantic
artifact verifier. The whole MIR -> C path is Pergyra and run-equivalent to the
C backend on the supported rung-0b CFG
subset (linear code, signatures/return, if/else, nested if, while, and
`for i in a..b`), plus selected codegen fixture surfaces that already lower from
MIR facts (args, arrays, Bool/string/Float builtins, Bool-literal branch
reassignment, straight-line calls, direct integer arithmetic, builtin-name
string literals, directory walking, exit-guard branches, multiple Void routines
with bare-call statements, string concat/equality, `Result<Int>` `?`
early-return flow, recursion, loop-control
`continue`/`break` edge blocks, trailing-newline Log normalization, nested
string concatenation, string array concatenation, string case/index/trim
builtins, `Join`/`ToFloat` string utility flow, array pop, array for-each,
array sort/map/filter/reverse combinators, `Result<Int>` core constructors and
inspection helpers, typed struct field declarations/value flow,
plain class/subject/object/tobject/vessel declarations and class methods through MIR-owned
nominal-kind/field/method/owner facts,
payload-free enum declarations through MIR-owned variant facts,
break edges after non-empty statement blocks, inferred `Random()` Int locals,
match-case integer pattern conditions, runtime-aligned absolute-path I/O policy,
file read/write, Long scalar flow, array index assignment, `Option` `?`
propagation, string equality-plus-concat flow, C-reserved binding spelling,
payload-free enum match comparison projection, Float signatures, seeded random
flow, string-array index return flow, and phi-bearing loop headers classified
by CFG backedges rather than phi presence alone, plus MIR-owned array destructure
binding facts), gated by
`parity/mir_json_parity.sh`
(`make self-host-mir-json-parity-test-smoke`, 102 fixtures plus 0 clean-reject
fixtures). The gate now
requires the MIR JSON fact surface and checks the `for`
header is reconstructed from `arg0` plus `expr0`/`expr1` bounds, and checks
struct/class declarations, nominal family declarations, owner-qualified class methods, payload-free enum
variants, match-case integer branch conditions reconstructed from
`match_patterns`, and `Option<Int>` `Some(v)`/`None` branches reconstructed from
MIR-owned `match_variant` and `match_bindings` facts. It also checks nested `if`
branches inside loops are not misclassified as loops from phi facts alone. The
gate checks destructure binding-name facts and rejects unsupported declaration
facts before generated C emission. It rejects
reintroducing reads of the transitional `ast` compatibility text. This is the
first verified slice of the actual compiler-core (~96% of the LOC), not the
codegen subset. It is now fact-only for the supported MIR JSON statement,
expression, source-local, CFG, match-case, I/O policy, typed struct field
declaration, field-only class/subject/object/tobject/vessel declaration/method,
ability signature declaration, payload-free enum surfaces, and the Int role
operator dispatch surface. The committed MIR-lower/codegen fixture inventory is
currently **102 PASS / 0 gap plus 0 clean rejects** through this
path. The nominal family now flows through MIR-owned `nominal_kind`/field facts
and reconstructs `Class:` / `Subject:` / `Object:` / `TObject:` / `Vessel:`
instead of collapsing those labels to a generic class alias. Ability
declarations now flow through MIR-owned method signature facts and are treated
as zero-artifact declaration hosts by the self-host codegen pre-passes. Role
declarations now flow as MIR-owned `kind:"role"` facts with `for_type`, impl
ability spans, and method signature facts; the supported Int/`Arithmetic.Add`
operator path is consumed by self-hosted MIR lowering/codegen instead of being a
clean-reject boundary. Payload-free enum variant lists are consumed through
typed arena aux-value rows in self-host codegen.
Richer projection/identity semantics beyond field-only nominal
declarations and payload-bearing enum variants remain observable boundaries, so the
self-host path fails closed instead of silently
dropping operator-overload/domain nominal semantics or emitting undefined C
symbols. New fixtures must preserve that by adding owning facts rather than
text fallback.
`self_hosted_component_contract_smoke` now also ratchets that frontier against
the parity harness itself: the MIR JSON positive fixture inventory must stay at
102, the clean-reject inventory must stay at 0, the scorecard must cite the same
102 PASS / 0 gap plus 0 clean reject boundary, and stale fixture-count wording
is rejected. The positive inventory now includes `examples/binary_search.pgy`
as an example-origin fixture after all 73 committed self-host codegen fixtures,
not only purpose-built MIR-lower fixtures.

The self-hosted `mir_lower/` implementation is now split by source-of-truth
owner rather than living as one monolithic `main.pgy`: `error_owner` owns the
stage-specific `MirLowerFailClosed` boundary, `mir_json_input_owner` owns argv/file/schema input gating,
`json_fact_read` owns bounded JSON/MIR fact access, `decl_lower` owns declaration
inventory reconstruction, `program_lower` owns document-order Program assembly
and supported routine selection, `routine_inventory_owner` owns routine
discovery and bounded routine header facts, `routine_lower` owns CFG/body
reconstruction for a selected routine, and `stmt_render` owns instruction fact
-> AST statement rendering. The entrypoint `main.pgy` is orchestration only, and
each `mir_lower` source file is below the 600-line owner cap.

**Hard migration opened (2026-06-17):** the codegen rung is the first *hard
compiler-core* substitute, landed after the BDFL decision lifted the
`docs/self_hosted/README.md` freeze. Hard migration proceeds rung-by-rung, each
gated against the C/LLVM oracle before the next opens -- not as an unverified
compiler fork. See `src/self_hosted/codegen/README.md`.

**Self-hosting achieved for codegen (2026-06-17, strengthened 2026-07-02):**
the codegen tool *self-hosts*. A Pergyra-built copy of the owner graph emits C
that gcc-compiles and **reproduces its own source-compilation exactly** --
`gen2 == gen3` byte-identical (last observed 9916 generated-C lines) -- and the
Pergyra-built tool emits byte-identical C to the oracle-built tool on the sample
fixtures. Breadth: the same codegen also compiles the lexer (587 lines) and parser (3338 lines); each codegen-built binary matches its oracle-built counterpart on a sample source -- three real self-host components self-built. Wider survey: the codegen compiles **all 22 of 22** committed self-host components/tools to valid C, each verified run-equivalent to the oracle-built binary on a sample -- the entire committed self-host toolchain (lexer, parser, semantic, codegen itself, + 18 audit tools) is self-built by the Pergyra-written codegen. This includes namespace-imported audit tools (`TextScan::` qualified calls, flattened to `NS_Func` -- import/namespace + DirWalk support added). The earlier 18/22 ceiling was a `pgy --ast` bug (for-each `for x in lines` rendered as `For: x in (null)..(null)`, dropping the collection); FIXED in src/parser/ast_print.c (emit the iterable) + the self-host parser, regenerated 5 parser fixtures, and added for-each lowering + bare-void-return + word-boundary builtin matching to the codegen. The latest hard gap was the typed AST arena fixture exposing that the self-host codegen only knew `Option<Int>` ABI/runtime facts. FIXED by adding `Option<String>` to compiler ABI rows, runtime ABI owner symbols, expression kind facts, and typed `Some`/`None` emission. Parser parity (188 manifest rows) stays byte-equal. The bootstrap gate verifies codegen self-hosts (gen2==gen3) + builds lexer + parser + semantic + mir_lower + 13 audit tools and the backend fuzz generator, all matching oracle-built. Gated by `parity/codegen_bootstrap.sh`
(`make self-host-codegen-bootstrap-test-smoke`).

Reaching the fixpoint drove out and fixed real gaps: `else if` chains,
string-literal-safe builtin rewriting, recursive `Concat`/`ToString`/call-argument
lowering (`Concat` -> `pgy_concat` is a pure name rewrite -- same args -- so it
lowers anywhere), bare-call statements, **string `==`/`!=` -> `strcmp(...)==0`**
(C `==` on `char*` compares pointers; the silent root cause of a non-working
first attempt), and a latent **forward-declaration bug** -- Pergyra arrays pass by
value with a shared element buffer, so `ArraySet` persists across calls but
`ArrayPush` does not; the per-`EmitFunction` `protos` push never reached
`GenerateC`, leaving prototypes empty (fixtures worked only because callees
precede callers). Fixed with a `CollectProtos` pre-pass.

This is component and bounded integrated-driver self-hosting, not the whole
compiler. DRV-2 now composes the Pergyra MIR producer, MIR consumer, semantic
verifier, and codegen for the supported intersection. The C oracle produces MIR
only for canonical parity evidence. Whole-language source-to-MIR coverage, the
rest of codegen, runtime, and released/native compiler driver/LSP replacement
remain open.
The compiler driver has DRV-0/DRV-1 artifact rungs, and LSP has LSP-0
diagnostic payload, LSP-1 squiggle-policy, and LSP-2a..LSP-2i buffered
transport/request/response/session/document-state/feature-shape/session-state/
hover-content rungs. The C LSP dump flag
`pgy-lsp --dump-diagnostics <src>` provides live oracle plumbing for the LSP
payload gate plus fixture-level canonical event comparison across clean plus
logical/undefined/type/condition/unary diagnostic families, but neither LSP rung
is a shipped replacement (docs/150).

**Real-example round-trip (2026-06-17):** beyond the 35 hand-written parity
fixtures, the codegen tool was surveyed against all 118 `examples/*.pgy`. It
compiles **20** to run-stdout-equal output vs the oracle (binary_search,
hash_map, linked_list, queue, deque, graph_bfs, insertion_sort, union_find,
break_continue, for_test, class_test, etl_workflow, hello, + 7 contract/
projection/transfer minimals); 86 are correctly rejected as out-of-subset with an
observable `Exit(1)`; 12 fail under the oracle itself (C-skip). Two bugs surfaced
from the C/LLVM/Pergyra tri-compare: (1) a **codegen self-bug** -- `Log(<int>)`
logged directly (not via `ToString`) was emitted with `%s`; fixed by routing
`Log` / array-index element types through `ExprKind` (silent-failure examples
11 -> 0). (2) an **oracle bug** -- the C and LLVM backends miscount arity for
`Array<String>` parameters (the self-host emitter handles them correctly); filed
separately.

**Lexer parity (2026-06-23):** the committed lexer gate compiles the
Pergyra-origin lexer through both C and LLVM, then proves byte-equal token
output and live `pgy --tokens` drift on 8 source fixtures:
`hello`, `array_literal`, `break_continue`, `basic`, `heap`, and
`binary_search`, plus backend-compare `string_escape_sequences` and
`block_comment`. `main.pgy` is
now only the entrypoint; character/codepoint classification, token keyword/line
rendering, and the scan loop live in `char_owner.pgy`, `token_owner.pgy`, and
`scan_owner.pgy`; lexer tool input is only `Args()[0]` or the no-arg
`examples/hello.pgy` default. The broader lexer scale probe now measures
**993 of 993** examples + backend_compare sources byte-equal to the C lexer
oracle.

**Parser at scale (2026-06-23):** the Pergyra-origin parser produces
byte-equal output vs `pgy --ast` on **120 of 121** committed
`examples/*.pgy` files. There are now **zero byte-drift cases** in the
scale probe: every example that both the live C oracle and the self-host
parser complete is byte-equal. There are also **zero self-host parser exits**;
the one remaining non-match fails under `pgy --ast` itself and is a C-skip
(`secure_slots`). The scale probe is a
coverage probe, not a hard parity gate, but it now fails closed: it removes any
stale generated parser binary before compile and exits if compile does not
produce a runnable parser. The probe and parser entrypoint consume source paths
only through `Args()[0]`; the old `fixture/source.txt` side channel is retired.
The file-based probe exposed an `if let Some(resource)` payload loss in the
self-host parser's generated C, now closed by `ParseIfLetPayload` returning the
payload fact instead of relying on branch-local `String` reassignment.
Previous historical match counts:
105 -> 86 -> 83 -> 80 -> 79 -> 77 -> 72 -> 72 -> 63 -> 59 -> 58 -> 57 -> 53 -> 48 -> 46 -> 43 -> 37 -> 25 -> 11.
Refresh:
`bash tests/self_hosted/parity/parser_scale_probe.sh --failing`.

**Rung-1 parity (2026-06-16):** the committed
`parser_parity.sh` now consumes a **188-row** source/fixture manifest emitted
by `fixture_manifest_owner.pgy` vs `pgy --ast` on both generated C and LLVM parser binaries
(was 83 on 2026-05-29; +103 overall). The added fixture surface covers Option/Result
destructure, slot sugar, transfer short syntax, array literal,
common collection algorithms (queue, stack, deque, heap,
linked_list, hash_map, union_find, graph_bfs), string + stdlib +
io + math builtin surfaces, async/spawn/select/defer/for control
flow, pipe + try operator, ownership /
concurrency / event demos (event_basic, event_minimal,
event_lambda, event_lambda_full, event_closure_probe),
notebook_style_analysis, tagged_union, battle_*, calendar_*,
beta_*, intent contract minimal shapes, authority contract,
action contract inheritance, generic ability requires, four
ad-hoc bsd_test fixtures and the full 11-file bsd_test{,2..11}
family, qubit_test, qubit_quantum_ext, RemoteFuture, for_in_array,
generic_class, subject_object_tobject, vessel_method_test,
test_parallel, eda/etl workflows, channel_parallel,
producer_consumer, projection_*, collections_closure_probe,
class_method_test, channel_test, spawn_blocking_test,
import_test, slots, slots_simple, and a deep nested generic type fixture
(`HashMap<String, List<HashMap<Int, Array<String>>>>`). The parser parity
gate now compiles the Pergyra-origin parser through both C and LLVM before
comparing each fixture byte-for-byte.

Examples that **cannot** be added as fixtures (current state):
- `pgy --ast` itself fails (skipped):
  `secure_slots`.
- Self-host parser byte-drifts vs live `pgy --ast`:
  none as of the 2026-06-22 scale probe.
- Self-host parser exits before producing byte-equal AST:
  none as of the 2026-06-22 scale probe.

Reading this honestly: the self-host journey has *just started*. The
first compiler-internal substitute (`src/self_hosted/lexer/`) lands a
Pergyra-written lexer that handles the measured examples + backend_compare
token surface byte-for-byte.
The parser (`src/self_hosted/parser/`) follows at ~52%: it covers a real
domain grammar subset and has C/LLVM byte-equal parser parity over the
committed fixture set, but still stops short of the remaining scale-probe
exit list and the full parser recovery surface.

Compiler-stage substitutes mirror the C-side `src/<component>/` layout
as siblings of `src/self_hosted/` (`lexer/`, `parser/`, `semantic/`,
`codegen/`, `air/`, `hir/`, `mir/`, `compiler/`, `runtime/`, `lsp/`).
Everything listed under `src/self_hosted/tools/` is *peripheral audit
tooling*. Those tools do not substitute any compiler component; they
only observe text artifacts the C compiler produces. Their LOC is
**not** counted in the substitution percentage.

## Component Coverage

| Component       | C LOC   | Pergyra implementation LOC | Executable coverage | Status            |
|-----------------|---------|----------------------------|---------------------|-------------------|
| `src/lexer/`    |     921 |         825 | measured corpus parity | **993 of 993 sources byte-equal** (examples + backend_compare). `main.pgy` is entrypoint-only; run-boundary, fixture manifest, source input, character/codepoint handling, token classification/output formatting, and scan-loop state are separate SoT owner modules. `scan_owner.pgy` declares its real owner dependencies (`char_owner.pgy`, `token_owner.pgy`), and the lexer run/fixture-manifest owners are part of the real-source semantic selfcheck set. Escaped strings, interpolation, and doc/block comments are covered by the measured corpus. 7 representative sources are committed as parity fixtures. |
| `src/parser/`   |   20579 |        8355 | ~52%     | `src/self_hosted/parser/` parses 188 source/fixture rows byte-equal `pgy --ast` on both C and LLVM parser binaries, and **120 of 121** `examples/*.pgy` byte-equal at scale (2026-06-22; zero byte-drift, zero self-host parser exits, 1 C-skip). Parser ownership is now split into declaration, expression, statement, import/source, cursor, type-name, diagnostic, tree-text, run-boundary, and fixture-manifest owners; `main.pgy` is parser-tool entrypoint only. |
| `src/semantic/` |   46203 |        7792 | rung-2 subset | Checks a bounded function-body subset against the C compiler oracle on C/LLVM-generated binaries across 113 fixtures, including nested generic signature canonicalization, Long suffix/cast typing, Option `?` payload propagation, and Result core consumption. Artifact-native DRV-2 additionally owns signatures, nominal constructors, locals, assignments, iteration, statement typing, and ordered body verdicts without source rescanning. |
| `src/codegen/`  |  107123 |        7220 | rung-0..21 | **C-emit rung-0..21 (2026-07-12).** The Pergyra emitter covers the committed scalar/string/array/result/option/struct/defer/file/stdin/argv/random/float/long subset across 69 run-equal fixtures. HIR owns the compact AST inventory, row facts, `AstTreeArtifact`, and shared `AstArena`; parser produces that artifact and codegen consumes it without rebuilding the arena. Codegen owns only its arena view, type/symbol/ABI/runtime-call facts, and emission participants. The standalone codegen has a blocking byte-identical `gen2 == gen3` fixed point; the integrated driver runs seed/oracle parity by default, with its full-input stage2/stage3 fixed point explicit. TextBuilder now owns program assembly and hot token rewrites. Out-of-subset input is an observable `Exit(1)`; the released default path remains C-owned. |
| `src/runtime/`  |   29627 |           0 | 0%       | native runtime kernel stays C; portable runtime policy libraries may move later |
| `src/compiler/` |   43304 |        9389 | bounded producer-first DRV-2; released 0% | `driver_pipeline_owner.pgy` owns the shared source->AST spine. DRV-2 composes artifact-native semantics, Pergyra MIR production, MIR consumption, and codegen; C MIR is oracle evidence only. The default native driver remains C-owned. |
| `src/lsp/`      |    1037 |        2066 | bounded LSP-2i; released 0% | released/native LSP replacement remains 0%; LSP-0 diagnostic `publishDiagnostics` payload projection, LSP-1 squiggle policy, and LSP-2a..LSP-2i buffered transport/session/document/hover owners exist under `src/self_hosted/lsp/` and are tracked by docs/150. Full transport/session replacement has not landed. |
| **Live inventory** | `make self-host-progress-metric-test-smoke` | `make self-host-progress-metric-test-smoke` | not a substitution percentage | lexer/parser/semantic/codegen implementation volume and the wider compiler-core inventory are measured at gate time |

Notes:

- *Coverage %* is a rough functional estimate, not a LOC-equivalence
  number. The lexer is 646 LOC and is judged by byte-equal fixture coverage,
  not by line-count parity with the C lexer.
- *Runtime kernel stays C* by current design: allocator/OS/thread/panic/slot
  exports are what the target Pergyra program links against, so substituting
  that native kernel in Pergyra would create a bootstrap cycle. Counted as 0%
  intentionally. Runtime-adjacent Pergyra tools count as soft self-host evidence.
  They remain outside compiler-internal substitution until a Pergyra-written
  runtime component is linked into generated programs.
- `src/lsp/` is the Language Server Protocol implementation. Lower
  priority than the core compiler.

## Peripheral Audit Tools (Not Counted In Coverage)

These 20 tools live in `src/self_hosted/tools/` but do **not** count
toward compiler-internal substitution. They are dogfood validators
that read text artifacts and emit drift verdicts; the C compiler
keeps running fine with or without them.

| Tool                              | LOC (Pergyra) | Function |
|-----------------------------------|---------------|----------|
| `diagnostic_catalog_checker`      | 303           | docs/72 vs diag_codes.h drift |
| `stable_subset_section_checker`   | 122           | docs/107 canonical anchors |
| `air_graph_json_validator`        | 487           | `pgy --air-json` shape gate |
| `air_graph_id_uniqueness`         | 132           | AIR graph duplicate node-id check |
| `air_graph_node_count_integrity`  | 140           | live AIR graph id-count summary check |
| `air_graph_ref_live`              | 138           | live AIR graph back-reference range check |
| `air_graph_ref_integrity`         | 143           | AIR graph dangling endpoint check |
| `air_graph_reachability`          | 166           | AIR graph root reachability/worklist check |
| `backend_output_comparator`       | 135           | paired text diff verdict |
| `completeness_impact_planner`     | 351           | changed-path impact rows -> proof-gate/run-group plan |
| `compatibility_evolution_checker` | 65            | compatibility seed corpus coverage check |
| `module_manifest_resolver`        | 121           | language_module_manifest.json |
| `stdlib_dispatch_inventory_checker` | 107         | C/LLVM dispatch table count parity |
| `doc_link_checker`                | 143           | docs/INDEX.md dead-link audit |
| `production_header_size_checker`  | 108           | DirWalk-owned `.h` 600-LOC cap |
| `production_c_size_checker`       | 127           | DirWalk-owned `.c` 699-LOC cap |
| `examples_inventory_checker`      | 112           | DirWalk-owned examples/ count + non-empty |
| `ast_read_surface_checker`        | 219           | CFG/MIR SoT ratchet parity |
| `linter`                          | 182           | LSP-style diagnostic JSON parity |
| `runtime_boundary_checker`        | 82            | native-kernel vs portable-policy runtime boundary |
| **Total peripheral**              | **3210**      | |

Plus `src/self_hosted/lib/text_scan.pgy` (~47 LOC) shared across scan-based
tools.

## Substitution Roadmap (Honest Order)

The realistic incremental path toward genuine self-host:

1. **Lexer expansion** -- *substantially done* (2026-06-16). Handles
   common keywords, line + block comments, integer + float literals, string
   literals, and common operators. The committed executable gate is the
   8-source C/LLVM parity harness; the broader scale number below is a
   historical measurement and should not be treated as a committed scale gate.
2. **Lexer at scale** -- *historical measurement refreshed* (2026-06-23).
   Pergyra lexer was measured against `examples/*.pgy` plus
   `tests/cases/backend_compare/**/main.pgy`; **993 of 993 byte-equal** vs
   `pgy --tokens`. String interpolation, escaped strings, and doc/block comment
   lexing are now in the measured surface. Coverage target met for this corpus.
3. **Parser bootstrap** -- *expanding* (2026-06-22). `src/self_hosted/parser/`
   parses 188 manifest rows byte-equal `pgy --ast` on parser binaries
   generated by both C and LLVM, and **120 of 121** `examples/*.pgy` files at
   scale with zero byte drift and zero self-host parser exits. It now covers the domain declaration surface (`subject`, `object`,
   `tobject`, `vessel`, `ability`, `role`/`impl`, `zone`, `world`, `party`,
   `event`, `intent ... with retry(n)` metadata), imports, common statement
   forms, full expression precedence, lambda primaries, postfix calls/indexing/
   member access, and deep nested generic type names. Remaining parser work is
   replacing this text-mirror substitute with structured AST ownership and
   keeping the single C-oracle skip honest, not clearing completed-output drift.
4. **Semantic subset** -- *rung-2 active* (2026-06-23). The current rung
   checks `func`, typed `let`, literal/identifier types, return typing, scoped
   `if` / `while` / `for` bodies, unary
   and binary expression operators, call return/arity/argument typing, branch
   conditions, assignment, bare call statements, and simple/compound undefined
   identifier use in Pergyra, then compares against the C compiler accept/reject
   oracle. Recursive import expansion is now owned by `source_bundle_owner.pgy`,
   and the import-backed call fixture proves signatures are consumed from the
  source bundle instead of from a hidden single-file `main` assumption. The
  real-source selfcheck now feeds 204 accepted self-host owner/source files
   through that source-bundle owner rather than a generated import-stripped
   unit. The accepted manifest spans lexer/parser/mir-lower/codegen/compiler-world
  entrypoints, the lexer and mir_lower run/fixture-manifest owners, the compiler path manifest
  owner, target-capability envelope owner, stage-artifact envelope owner, hard-rung
  AIR/artifact/test-harness/subprocess/ABI-row/symbol-row
  compiler-world envelopes, codegen symbol-mangle, ABI-layout, collection-runtime,
  math-runtime, host-I/O-runtime, Option/Result-runtime, and string-runtime owners, semantic run/program/body/call/expression owner files, and audit-tool
   slices inside the current
   subset. The oracle parity runs on C and LLVM
   binaries across 113 fixtures. The same gate now validates the 17-code
   self-hosted semantic diagnostic vocabulary plus its C oracle JSON root-code
   mapping: committed expected `Code:` fields and literal
   `SemanticError...("code")` call sites must be registered in
   `diagnostic_code_owner.pgy`, and invalid fixtures must be rejected by the C
   oracle with that mapped JSON code. The implementation is split
   into source-of-truth owners (`text_scan_owner`, `source_bundle_owner`,
   `diagnostic_owner`, `env_owner`, `expr_type_owner`,
   `expr_validation_owner`, `call_check_owner`, `body_check_owner`,
   `program_check_owner`, `diagnostic_code_owner`, and `semantic_run_owner`) with a thin `main.pgy`
   entrypoint. Expression diagnostics consume `ExprType(...)` facts instead of
   living inside the type-query owner. The builtin/type table now includes the
   scalar math signatures `Sqrt`, `Pow`, `Floor`, `Ceil`, and `Random`,
   C-oracle string-plus and Bool arithmetic result typing, trig/log
   Float signatures from `Sin` through `Log2`, string split/join alias
   signatures, and the first-argument scalar utility contracts for `Abs`,
   `Min`, `Max`, and `Clamp`, newline-free `Print` output calls,
   `Some(expr) -> Option<ExprType(expr)>`, `None -> Option<Unknown>`,
   `None() -> Option<Unknown>`, `UnwrapOption(Option<T>) -> T`,
   `IsSome`/`UnwrapOption` builtin argument rejection for non-Option operands
   and non-concrete `Option<Unknown>` operands, comment-skipping brace/statement scanning,
   and the codegen entrypoint source.
   The next semantic expansion should broaden declarations
   only after that shared-code boundary or another equally narrow fact owner is
   available.
5. **AIR graph consumer passes** -- *rung-1 active* (2026-06-16). Five
   Pergyra-origin graph consumers now run in the self-host preparation suite:
   node-id uniqueness, live-dump node-count integrity, live-dump
   back-reference range checking, fixture-shaped edge referential integrity,
   and root reachability via a push-only worklist. These are still peripheral
   because they do not replace `src/self_hosted/air/`, but they prove the
   deterministic graph substrate the first middle-end pass needs.
6. **C-emit codegen subset** -- *rung-0..21 active* (2026-07-12). A Pergyra
   program (`src/self_hosted/codegen/main.pgy`) takes `pgy --ast` text and emits
   standalone C for: string `Log`/`Concat`, `Log(ToString(<intexpr>))`, integer
   `Let:`/`Assign:`, `while`/`if`/`else` and `for i in a..b` + `break`/`continue`
   (structural lowering), multiple `Int`/`Bool`/`String`/`Void` functions with
   calls, recursion, `return`, `String` types (routed by a variable + function
   type environment; `Concat`/`Substring`/`StringLength`/`StringIndexOf`/
   `StringTrim`/`StringJoin`/`Join` -> runtime helpers, `ToFloat` -> owner-routed
   target `atof`),
   `Bool` (`<stdbool.h>`), growable
   `Array<Int>`/`Array<String>` as a `{data,len,cap}` struct
   (`ArrayPush`/`ArrayLength`/`ArraySet`/`xs[i]` via env-aware index rewriting),
   `Array<Int>` `ArraySort`/`ArrayReverse`/`ArrayMap`/`ArrayFilter`, `Result<Int>`
   `Ok`/`Err`/`IsOk`/`IsErr`/`Unwrap`/`UnwrapOr`, `Option<Int>`
   `Some`/`None`/`IsSome`/`UnwrapOption`, block-local `defer`,
   enum `match` on supported enum facts, `Exit(n)`,
   `FileExists`/`ReadFile` file I/O, `Args()` snapshots, and
   value-passed `Int` / `Bool` / `Float` / `String` field structs plus nested struct-valued fields with
   literals/member reads/params/returns,
   and `Array<Int>` parameter/return flow.
   `lib/json.pgy` now owns the first document-level schema and numeric-field
   readers consumed by the AIR graph JSON validator, in addition to the shared
   JSON string/field/object/array emission helpers consumed by production size
   checkers, the stable-subset section checker, and the module manifest
   resolver. The module manifest resolver now consumes bounded module-array
   object/field counts from the JSON owner instead of global substring counts.
   Round-trip C-emit-by-Pergyra -> gcc -> run -> stdout matches the C/LLVM oracle
   on 69 committed fixtures, with the emitter built through both backends. The
   newest fixture proves scalar readonly-ref reads, aggregate member reads, and
   readonly-ref forwarding consume one value/raw-address fact pair.
   The M2 completeness ledger also now checks all 250 production self-host
   source files through the codegen `--check` path; that path still consumes
   C-oracle `pgy --ast` text, so it is a source-breadth ratchet rather than the
   final self-parser-to-codegen bootstrap. Allocation-free runtime-usage scans
   now reduce the 28,434-node pre-emission probe from 717,696 KiB to 51,968
   KiB while preserving 69-fixture C parity. TextBuilder rung 2 owns final
   C-unit assembly and binding-reference rewriting. Runtime builtin projection
   now uses one identifier scan instead of one full scan per builtin. Two-run
    same-input sampling lowers the codegen-only path from
    3,347.3-3,394.5 MB to 956.1-956.5 MB with byte-identical output; the latest
    fresh semantic-bundle series is 665.9-670.2 MB with byte-identical output
    on its current input. The latest measured codegen fixed point is 26,227
    lines. Remaining integrated-driver
   parser/semantic/MIR text lifetime is tracked separately. Next rungs:
   scope reclamation, block scoping, typed AST-node facts replacing text rows,
   then round-trip
   self-compilation.
7. **Bootstrap loop** -- the bounded parser/codegen compiler subset now compiles
   itself to a byte-identical fixed point and its sample output matches the
   oracle. The rung remains open for semantic/MIR inclusion and released-driver
   replacement.

Steps 1-4 are active staged substitution. Step 6 (codegen) opened 2026-06-17
after the hard-migration freeze was lifted; step 7 has reached the bounded
parser/codegen fixed point but not the whole-compiler terminus. Step 5 (AIR
consumers) and the semantic/MIR bootstrap expansion remain ahead.

## Surface Lifts Required Before Substitution Can Continue

These Pergyra surface gaps will block compiler-internal substitution
beyond the lexer:

- **Process arguments** -- `Args() -> Array<String>` has landed for generated
  binaries, returning the user arguments as an owned snapshot. The lexer and
  parser parity runners now pass source paths through argv, so the first
  compiler-internal substitutes consume the same tool surface they need for
  standalone dogfood runs.
- **Struct-over-arbitrary-types** -- needed to model AST nodes. Pergyra
  already exercises mixed tree shapes as parser/backend evidence:
  `node_traversal_sum`, `tree_walk_recursive`, `tree_grow_recursive`,
  `nested_generic_containers`, and the parser's deep
  `HashMap<String, List<HashMap<Int, Array<String>>>>` fixture prove user
  classes/records and nested generics across C/LLVM-facing gates. These
  mixed tree shapes are parser/backend evidence, not compiler-model
  substitution, and they are not yet a self-hosted compiler AST model. The first
  self-hosted compiler AST model contract now exists in
  `src/self_hosted/hir/typed_ast_arena_owner.pgy`: it owns a flat typed
  arena vocabulary, explicit child lookup, atom lookup, and a small traversal
  payload fixture. `PgyCompilerWorld` now requires that contract through
  `CompilerEmissionFactReady()` before `ProgramEmitter` can claim emission
  readiness. `GenerateC` now consumes `CodegenTypedAstBridgeReady` over the
  owned `CodegenAstTextNode` inventory before emitting, and that guard projects
  the real inventory into `AstArena` rows with node-count, kind, atom, parent,
  indent, and root child-edge checks. `program_emit` now consumes those arena
  facts for first-function indent and owner-body descendant traversal. Current
  parser and most codegen rungs still consume text AST artifacts; the next
  closure is replacing the remaining string-backed expression payloads with
  dedicated expression rows under oracle parity.
- **Raw pointer / FFI** -- if a Pergyra component needs to call into
  the C compiler's runtime (e.g. share the diagnostic emitter), there
  is no stable FFI today. This is intentional for the current compiler-pass
  path: `unsafe` is only a scoped marker, raw pointer helpers stay
  runtime-internal, and `raw_escape_contract_smoke` rejects system-tier escape.
  The alternative remains *no FFI*: build the Pergyra-side compiler as a
  parallel binary that emits text, not as a library that plugs into the C
  compiler. FFI remains intentionally absent from the compiler-pass path until
  a stable ABI contract exists.
- **Subprocess execution** -- needed for in-Pergyra drift guards that
  re-run the C compiler. Currently the parity scripts do this from
  bash; a Pergyra runner would need `Subprocess(...)`.
- **Deterministic collection iteration** -- compiler passes need stable
  output ordering, not just functional map/set lookup. `stage4_determinism_smoke`
  now compares insertion-order variants for `HashMap<String|Int|Long|Bool, T>`
  `MapKeys` and `Set<String|Int|Long|Bool>` `SetValues` through generated
  Pergyra programs on C and LLVM. Compiler-facing symbol/record-like identities
  are canonical string keys, and handle-like identities are stable integer or
  long IDs; the Stage 4 fixture exercises those canonical shapes instead of
  introducing raw aggregate keys as a second collection truth. Compiler passes
  should consume those stable snapshots (`MapKeys` / `SetValues`) rather than
  depending on hash storage traversal.
- **Allocator/arena ownership surface** -- `AllocatorSystem`,
  `AllocatorPool`, `AllocatorDebug`, `AllocatorTracing`, `AllocatorScratch`,
  `AllocatorResult`, and `AllocatorPersistent` now produce the single stable
  `Allocator` value on C and LLVM. The lane-named constructors carry distinct
  runtime kinds and lower through dedicated LLVM runtime init exports instead
  of aliases. `BoxArray(capacity, allocator)` consumes a named allocator local
  so fused array storage keeps an owner with a valid lifetime.
  `AllocatorDestroy(namedAllocator)` is the stable cleanup operation, so
  compiler pass lanes can use `defer { AllocatorDestroy(lane); }` instead of an
  out-of-language cleanup convention.
- **Filesystem directory walk** -- `DirWalk(String) -> Array<String>` has landed
  for generated binaries on C and LLVM. It returns a deterministic sorted
  regular-file snapshot with `/` separators and is gated by
  `filesystem_directory_walk_smoke`. `examples_inventory_checker` now consumes
  `DirWalk("examples")` directly, so the clean example inventory no longer has a
  committed manifest alias. `production_header_size_checker` and
  `production_c_size_checker` now consume `DirWalk("src")` directly, so their
  clean inventories no longer depend on committed file-list fixtures.
  `ast_read_surface_checker` now keeps only the metric/ceiling ratchet spec in
  `tests/ast_read_surface_ratchet.txt` and owns live file discovery through
  `DirWalk(scope)`. Remaining manifest-owned surfaces are document contracts,
  not clean directory file lists.
- **Parser LLVM depth/type-inference parity** -- `parser_parity.sh` now
  compiles the self-host parser through both C and LLVM and includes a deep
  nested generic type fixture. Remaining parser work is grammar breadth and
  the scale-probe exit list, not C-only backend evidence.
- **Try-let operand typing** -- `EmitTryLet` now consumes the operand type from
  the semantic expression graph that it already receives. It no longer reads
  operand text or calls `ExprKind` to rediscover `Option<T>`. The component
  contract rejects both regressions, and the 73-fixture C codegen parity suite
  remains green. Broader expression result-type classification is still a
  bridge in legacy shape consumers.
- **Array return emission** -- `EmitReturnValue` now sends every `Array<T>`
  result through the expected-value semantic graph. Literal and ordinary
  array-valued returns no longer choose a path by trimming source text or
  testing for a leading bracket. `array_return_literal` and `array_param`
  exercise both forms in the 74-fixture codegen frontier. Result returns and
  broader legacy expression leaves remain bridged.
- **Result return emission** -- `Result<Int>` returns now use the same
  expected-value semantic graph instead of the legacy return-expression text
  scanner. `result_int_core` covers direct `Ok`/`Err` calls and `result_try`
  covers a pipe-bearing `Ok(...)` return. `result_int_core` also joins the
  31-fixture DRV-2 MIR producer frontier, where missing and invalid expression
  graphs fail closed. Other legacy expression leaves remain bridged.
- **Nominal record array ABI** -- `Array<T>` where `T` is a declared record now
  consumes a compiler-owned derived ABI/layout fact and typed collection
  runtime symbols. `nominal_record_array` raises the C codegen frontier to 75
  fixtures; undeclared record element types fail closed instead of receiving a
  backend-local spelling or exact-type exception.
- **Exit argument graph ownership** -- parser statement production now stores
  the `Exit(...)` argument in the atom graph lane. Semantic statement typing
  requires `Int`, and codegen rewrites that same graph under the expected type.
  The former payload-text accessor and `IntEval` recovery path are deleted.
- **Foreach initializer refinement** -- initializer typing now performs a
  bounded second pass after the iteration owner proves loop-binding rows.
  Loop-body locals consume those rows; invalid loop bindings stay uninjected so
  the earlier iteration diagnostic remains authoritative. This lets the
  self-built codegen compile `diagnostic_catalog_checker` without source-text
  recovery.
- **Assignment target graph transport** -- DRV-2 now carries a semantic target
  graph for both plain and indexed assignments. MIR verification rejects a
  missing plain leaf or indexed graph, and JSON consumption preserves
  target-before-RHS lane order without target-text reconstruction.
- **Nominal constructor call targets** -- carried direct-call verification now
  consumes the typed nominal-constructor inventory together with function and
  builtin signatures. Constructor calls such as `Pair(...)` remain direct
  targets without reopening expression text; unknown callees still fail closed.
- **Namespace/member call provenance** -- carried call targets are no longer a
  resolver shortcut. The body fixpoint re-derives qualified namespace targets
  and receiver-typed method targets, then rejects any mismatch. The complete C
  DRV-2 frontier passes 20 source and 33 MIR producer fixtures with this rule.
- **Canonical MIR expression ownership** -- self-produced MIR canonicalization
  consumes `expr0_graph` through the MIR expression owner and rejects a
  missing or invalid graph. The native C oracle now projects the bounded
  `let`/`Log` scalar, binary, direct-call, array-literal, postfix-try, and named
  struct-literal slice into that same graph schema without reparsing expression
  text. Explicit generic calls additionally carry each parser-owned actual as
  an ordered `generic_type_actual` / `generic_callee` spine, so native MIR no
  longer compresses `Identity<Int>` to `Identity`. The public
  `--self-driver` live gate compares canonical facts and runtime output for
  `let_log`, `array_return_literal`, `option_try`, `struct_point`, and
  `generic_struct_field_value_flow` through freshly built C and LLVM
  self-drivers.
  The adjacent `generic_return_probe/explicit_ok` live row verifies ordered
  multi-actual carriage for `PickSecond<Int, String>` rather than inferring
  ordering from a single-actual example.
  Numeric `as` casts now use parser-owned `cast` / `type_name` nodes at the
  native cast precedence instead of collapsing the target into a leaf or
  reparsing expression text. The native MIR writer and the Pergyra producer
  emitted byte-identical canonical MIR for `cast_numeric`; the C-
  and LLVM-built focused drivers each passed the 20 body fixtures plus that
  selected MIR fixture, including rejection when a cast target was mutated
  from `type_name` to `leaf`. Float-to-Int/Long emission consumes checked-
  arithmetic runtime ABI rows, and the 245-row C/LLVM manifest parity is
  green. That cast evidence was slice-local.
  Native residual `MIR_INST_ASSIGN` instructions now carry both expression
  graphs as well: `expr0_graph` owns the target and `expr1_graph` owns the
  value. Self-produced SSA assignment definitions retain their existing
  inverse physical lanes (`expr0_graph` value, `expr1_graph` target), while the
  MIR consumer projects both forms into one target-before-value semantic
  sequence. `array_index_assign` is the 35th DRV-2 MIR fixture and uses the
  strict canonicalizer rather than the graph-recovering oracle bridge. Focused
  C/LLVM parity proved byte-equal canonical MIR, byte-equal directly consumed
  native/self C, and equal execution output; removing either native graph
  fails closed. The complete 35-case MIR matrix was not rerun here.
  Array-literal initializer type checking now consumes that same parser graph.
  The array spine view moved from codegen into a semantic owner, and declared
  element checks recurse over ordered element handles instead of trimming
  brackets or splitting arguments. `ast_node_array_literal` passed the focused
  C/LLVM DRV-2 source/MIR/runtime gate; the initializer owner is statically
  forbidden from calling the legacy text projection. Later assignment and
  return deltas closed the remaining array-literal type consumers. Broader
  expression-result classification remains a bridge.
  Unsupported native AST shapes remain fail-closed; the named oracle bridge is
  retained only for old graph-less artifacts and is unavailable to the hard
  consumer.
- **Expression emission SoT closure** -- the unreferenced top-level semantic
  shape emitter and its dead codegen accessors are deleted. Recursive graph
  emission is the only live expression consumer; shape rows remain only to
  verify that the graph root matches normalized semantic provenance. Static
  gates reject the retired file, import, and accessor names.
- **Program expression topology owner** -- the stable `AstExpressionArena`
  declaration now lives in `hir/program_graph_owner.pgy`. Parser/HIR keeps the
  existing node ordinals, while `SemanticExpressionGraphArena` borrows that
  topology and owns only normalized spelling, call-target, and place overlays.
  The semantic bridge no longer copies node-kind or child arrays. The blocking
  graph gate tightened from three structural stores to exactly one program
  topology and rejects any retired HIR, semantic, or MIR structural store
  returning. Initializer projection passed its C/LLVM probe;
  the current C-built driver passed all 20 body fixtures and six selected
  graph-heavy MIR fixtures. The first official full-driver pressure observation
  after the repoint still stopped at the 3 GiB boundary: 2,531.5 MB peak working
  set / 3,076.7 MB peak private, with initializer row 5,214 complete and row
  5,215 started. This falsifies the removed semantic topology copy as the sole
  memory cause. The MIR bridge now carries only root/range handles and obtains
  topology and call-target facts through semantic accessors; missing or
  foreign handles fail closed. The combined graph target runs the one-owner
  storage ratchet and this projection-negative gate. Routine-scoped semantic
  lifetime and revision-scoped stable IDs remain open. Until revision identity
  lands, foreign-graph rejection uses a correctness-first whole-graph equality
  preflight; repeated comparison is an explicit performance falsifier, not a
  stable identity substitute. MIR topology copying is closed for this
  projection.
  The destructure executable consumer now validates its semantic Value graph,
  derives SSA uses before registering the destructured bindings, and attaches
  that same view to MIR. Its owner gate rejects text-use recovery and
  post-binding use derivation. C- and LLVM-built self drivers each passed all
  20 body fixtures plus the focused `array_destructure` MIR/emit/host/runtime
  lane; the LLVM run crossed an unrelated match-owner commit while the
  destructure slice fingerprint remained stable, so it is focused evidence
  rather than a fixed whole-tree matrix.
  Iteration lowering now derives collection-hoist and foreach branch uses from
  semantic or synthetic graph views; range loops retain an explicit no-use
  path. Missing collection or foreach graph facts fail closed. The iteration
  owner gate rejects the retired identifier-text scan, and the component
  contract now ratchets graph-owned branch uses instead of requiring that old
  fallback.
  Graph-complete simple statements (`Log`, bare call, and `Exit`) now validate
  one Atom view, derive uses from it, and attach that same graph. Collection
  mutation statements remain a named bounded bridge until receiver/target,
  value, and auxiliary lane facts are all carried. The focused simple-
  statement gate and the refreshed component contract reject the retired
  fallback in the graph-owned path.

The remaining work is mostly actual semantic and codegen pass work against the
C compiler oracle. The one substrate-shaped item that remains as compiler-core
design work is mixed AST-like tree ownership inside a Pergyra pass; current
evidence proves language shape and backend/parser behavior, not compiler-model
substitution.

## How to Update This Document

When a tool lands or expands, update three things:

1. **Headline Number** -- recalculate Pergyra LOC vs C LOC for
   *compiler-internal substitutes only* (not peripheral tools).
2. **Component Coverage table** -- bump the relevant row's `Pergyra LOC`
   and `Coverage %` (be honest -- LOC equivalence is a bad proxy; use
   functional coverage estimates).
3. **Substitution Roadmap** -- check off completed steps, add detail
   where the next step diverged from the plan.

Do **not** add peripheral audit tools to the substitution percentage.
Their job is to keep the C compiler honest, not to replace it.

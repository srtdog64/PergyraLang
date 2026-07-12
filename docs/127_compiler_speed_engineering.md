# Compiler Speed Engineering Notes

Status: `post-beta-engineering-reference`

Last updated: 2026-05-12

Related documents:

- `docs/125_source_of_truth_spine.md`
- `docs/124_syntax_pattern_matrix.md`
- `docs/117_backend_strategy_positioning.md`
- `docs/self_hosted/README.md`

## 0. Purpose

Pergyra's beta closure has mostly focused on semantic trust, backend parity, and
source-of-truth cleanup. That remains correct. After beta dogfood starts,
compile speed must become a measured engineering constraint rather than an
occasional side effect.

D is a useful reference here: not because Pergyra should copy D's CTFE,
templates, mixins, or GC, but because D/DMD treats compile latency,
order-independent declarations, and simple build iteration as core product
qualities.

## 1. Baseline Principles

- Measure first. Do not optimize compile speed without a repeatable baseline.
- Keep declarations order-independent. The type-resolution DAG should be the
  source of truth for declaration dependency order, not source file order.
- Avoid preprocessor-style expansion as a language feature. It harms source
  trust and makes diagnostics harder.
- Keep grammar decisions cheap to parse. Syntax that requires deep semantic
  guessing during parsing should be rejected or delayed.
- Prefer module-level incremental artifacts over whole-program rediscovery.
- Keep codegen consumers from re-walking AST to rediscover semantic facts.
- Treat backend string rendering as a consumer, not a semantic owner.

## 2. Post-Beta Metrics

Dogfood should record at least:

- cold parse + semantic time;
- HIR/DIR/RIR/MIR lowering time;
- AIR evidence synthesis/validation time;
- C backend emission time;
- LLVM backend emission time when LLVM is enabled;
- C compiler/link time for generated output;
- binary size for generic-heavy dogfood programs;
- number of type-resolution DAG metadata hits/fallbacks.

The first useful gate is not "fast"; it is "repeatable and non-regressing."

## 3. Pergyra-Specific Risks

| Risk | Why it matters | Direction |
| --- | --- | --- |
| Backend rediscovery | C/LLVM walking AST or strings repeats semantic work. | Consume DAG/MIR/DIR/RIR facts. |
| Generic specialization growth | Monomorphized containers/functions can grow binary size. | Measure in dogfood before changing strategy. |
| Over-broad evidence scans | AIR should verify evidence, not rebuild lower-layer facts. | Evidence-node inventory and JSON schema. |
| Large owner churn | Mechanical splits improve LOC but may slow development. | Split only by source-of-truth responsibility. |
| Toolchain variance | LLVM-enabled builds differ by platform. | Freeze support matrix and measure per profile. |

## 4. Candidate Language/Tooling Ideas

These are not beta blockers:

- UFCS-style call unification after intent compression is specified.
- In-source `test` / `unittest` block for self-host dogfood tools.
- `pure`-like capability as either derived evidence or explicit annotation
  only after the orthogonality audit decides whether it duplicates zones/effects.
- `Iterable<T>` / range ability after collection ABI and ownership policy are
  stable.

## 5. Rejected For Core

- D-style CTFE as a general compile-time programming axis.
- String mixins.
- Template metaprogramming as a substitute for abilities/generics.
- Default GC as a memory model.

These conflict with Pergyra's type-as-domain-medium mandate and systems
language baseline.

## 6. First measured baseline (2026-07-05)

§1 said "measure first"; until now `review/` held no baseline, so no headline
compile-speed number existed. This is the first rough measurement. **Caveats:
single Windows/mingw host, tiny corpus, best-of-N wall clock, no per-stage
breakdown, no cross-compiler comparison. Rough characterisation, NOT a
repeatable non-regression gate yet** (the §2 per-stage instrumentation and a
`make timing` baseline artifact remain unbuilt).

Method: `pgy <file> --emit-llvm` (isolates front + HIR/DIR/RIR/MIR + LLVM-IR,
no gcc link) and `pgy <file> -o exe` (full C backend incl. gcc). Best of 5-7.

| Signal | Measurement |
| --- | --- |
| Startup floor (38-line file, --emit-llvm) | ~85-90 ms |
| Full source -> exe, small program (C backend) | ~260-300 ms (of which ~180 ms is gcc, not Pergyra) |
| Frontend marginal throughput | order ~1000 lines/s, but noisy 600-2000 depending on content (function/generic/MIR-node density dominates line count) |

Reading: small-program compile latency is **overhead-dominated** — fixed pgy
startup (~85 ms) plus the C backend's gcc invocation (~180 ms). Pergyra's own
front-to-LLVM-IR work is a minor fraction at this scale. A meaningful
throughput/scaling number needs a large single-file compilable corpus, which we
lack today (see below).

**Corpus finding (blocks a clean benchmark and flags a launch risk):** most of
`examples/*.pgy` does not compile with current pgy. Sampled: `basic.pgy` OK;
`battle_sim.pgy` frontend-clean but LLVM path aborts ("MIR-only LLVM path
missing source-local type metadata"); `party_system_demo.pgy` 69 semantic
errors (beta return-on-every-path rule); `secure_slots`, `role_ability_demo`,
`vessel_action_design`, `world_roster_city`, `structured_comments`, `parallel`
all fail on the C backend too. The compilable corpus is `tests/**` (916 green)
and `src/self_hosted/**`. Implication for docs/164 P3/P8: **showcase examples on
the launch site must be drawn from what actually compiles; `examples/` needs an
audit/repair pass before it is public.**

Next real step (still §2): instrument per-stage timing (cold parse+semantic,
each lowering layer, AIR, C-emit, LLVM-emit, gcc/link, binary size) behind a
`make timing` artifact so the number becomes repeatable and non-regressing
rather than a one-off wall-clock sample.

## 7. Conditional-Value If-Conversion Policy

Small source-shape changes can make Clang choose `csel`/`cmov` for an
unpredictable conditional store while GCC keeps a branch. Pergyra must not make
performance depend on that spelling accident.

The eligible optimization unit is a MIR conditional value, not an arbitrary
`if` statement. A future if-conversion pass may project it as a C ternary and an
LLVM `select` only when both alternatives are proven pure, non-trapping,
non-volatile, non-atomic, and free of observable allocation or runtime checks.
Without those facts, the branch remains. Predictable branches and expensive
alternatives may be faster than branchless code, so profile/target data remains
a profitability input rather than a semantic promise.

Required gate before landing:

- identical C/LLVM behavior for branch and select forms;
- emitted C and LLVM IR shape goldens;
- Clang and GCC assembly inspection on x86-64 and AArch64;
- random, sorted, skewed, and duplicate-heavy data distributions;
- regression thresholds for runtime, code size, and compile time.

The first candidates are scalar min/max/clamp and DOP partition/compaction
loops. Slot, authority, bounds, checked-arithmetic, and host-call branches are
ineligible unless their retained checks are independently proven erasable.

Rung 0 landed on 2026-07-12: `mir_speculation_facts` captures explicit
`pure`/`non_trapping` facts for every MIR instruction carrying `expr0`, exports
them in MIR JSON, and fails validation when an expression lacks the fact.
Scalar literals and MIR-confirmed source locals are covered, as is logical-not
over an already safe operand. Composite expressions remain ineligible: AST
operator spelling cannot prove overflow, overload, allocation, or effect
behavior. No backend consumes the fact yet.
The next rung is the PHI/branch diamond `MIRConditionalValueFact`; only after
that verifier lands may C ternary/LLVM select projection begin.

The 2026-07-11 audit found a semantic prerequisite in the existing C scalar
projection. `Abs`, `Min`, and `Max` emitted ternaries by repeating operand text,
so a selected side-effecting operand ran twice while LLVM `select` evaluated
each operand once. The C projection now captures each operand in a block-local
temporary before selection. `scalar_select_single_evaluation` reproduces the
old 11-line C versus 8-line LLVM trace and gates the corrected 8-line parity.
This is the minimum contract for every future if-conversion: **branchless must
not mean duplicated evaluation**.

## 8. Self-Host Text Scan and Allocation Evidence (2026-07-11)

The compiler-scale codegen probe showed that source spelling matters before
branch selection when a spelling allocates. The AST runtime-usage scan used
one-character `String` values and `Substring(text, i, n)` comparisons inside
repeated builtin-family scans. Repointing those observations to `CharCode` and
`SubEqualsWithLen` preserved the owner and removed allocation from the scan.

Measured on the same 28,434-node self-host AST artifact under a 4 GiB virtual
memory cap:

| Isolated stage | Before | After |
| --- | ---: | ---: |
| AST text -> typed artifact | not isolated before this audit | 12,416 KiB, 0.03 s |
| artifact -> semantic facts | not isolated before this audit | 15,232 KiB, 0.09 s |
| pre-emission facts/runtime usage | 717,696 KiB, 2.00 s | 51,968 KiB, 0.34 s |
| full codegen attempt on the measured artifact | 4,192,000 KiB, signal 11 | 3,719,552 KiB, deterministic diagnostic exit |

At that pre-rung-2 checkpoint, the remaining 3.7 GiB was not a branchless-code
opportunity. It was lifetime
debt: compiler `String` transforms allocate outside an owned text-assembly
boundary and are not reclaimed per emitted function. The allocator names alone
do not close this: `AllocatorScratch()` is currently a system-backed lane label,
not a bulk-reset arena, and `AllocatorDestroy()` only releases pool backing
storage.

Text-builder rung 2 landed on 2026-07-12. `TextBuilder` is a typed, move-only
builtin owner with `New`, `Append`, `Finish`, and `Drop` operations. A
`MIRTextBuilderRuntimeRow` owns the C and LLVM runtime symbols and their distinct
call shapes; both backends consume the row attached to each MIR instruction.
The runtime owner checks length/capacity overflow, keeps intermediate storage in
the system lane, and requires exactly one explicit `Finish` promotion or
`Drop`. `Finish` transfers the system-backed buffer into a system/result lane
without copying and copies only when the destination is a distinct pool domain.
The bounded self-host emitted-C helper currently admits `AllocatorResult` only
and aborts on other allocator domains; pool promotion remains native-runtime
coverage rather than a self-host codegen claim. Its runtime-call manifest uses
the distinct `selfhost-c-text-builder` domain; it does not alias or replace the
native MIR row's LLVM `_export` symbol and out-parameter `New` call shape.
The ABI, MIR mutation, C/LLVM differential,
negative ownership, runtime-bitcode symbol, and memory-layout gates cover this
bounded contract.

This is deliberately not a general linear-owner claim. The accepted surface is
an immutable function local initialized directly by `TextBuilderNew`.
Non-consuming `Append` may occur in nested blocks of the same function;
`Finish` or `Drop` must consume the owner in its declaration scope. Parameters,
returns, fields, containers, generic moves, mutable rebinding, and
branch-sensitive consumption remain fail-closed.

The first compiler-internal consumers are now live. Program-level C-unit
assembly and binding-reference rewriting use the typed builder. Runtime builtin
call projection now performs one identifier scan and delegates C spelling to
the existing runtime ABI owners; the retired per-builtin repeated scan is
gate-forbidden. The Pergyra-built emitter still reaches a 14,673-line
gen2==gen3 fixed point and
matches both native backend oracles on all 69 codegen fixtures.

An apples-to-apples Windows process-tree measurement used the same
1,289,598-byte AST artifact, 100 ms sampling, a 4 GiB ceiling, and two runs per
binary. The pre-rung-2 binary was built from commit `1b1b4208`; the new binary
was built from the active rung-2 source.

| Metric | Before range | TextBuilder rung 2 | Single-pass runtime rewrite |
| --- | ---: | ---: | ---: |
| peak private memory | 3,347.3-3,394.5 MB | 1,989.5-1,990.4 MB | 956.1-956.5 MB |
| elapsed | 20,263-21,091 ms | 22,690-23,205 ms | 15,792-15,877 ms |
| emitted C | 1,191,490 bytes | 1,191,490 bytes | 1,191,490 bytes |

All six emitted artifacts have SHA-256
`CC3460FAA069352C50FE5739194C80A759691A247AE9BDA200338F9672D90BAC`.
This closes the first measured assembly owner, not compiler text lifetime as a
whole. The current codegen-only path is below 1 GiB, but parser/semantic/MIR
composition and other expression/statement transforms still allocate ordinary
`String` temporaries without scope reclamation. Do not hide that remainder
behind `Array<String>`, a higher CI memory limit, or claims that the current
scratch lane is a checkpoint arena.

The measured artifact was stale enough to omit `SelfMirExpressionKind`; a
regenerated 6,338,740-byte MIR artifact contains that enum and closes that
specific diagnostic explanation. It did not close text-lifetime debt or
constitute a completed current-artifact bootstrap run; the later rung-2
measurement above is the current codegen-only result; the full integrated
driver remains a separately measured CPU/lifetime boundary.

The next integrated-driver probe found a different owner mistake. Every typed
arena accessor reopened `TypedAstArenaParallelRowsReady`, so one field lookup
recounted all parallel rows even though `AstTreeArtifactReady` had already
sealed their common shape. The artifact boundary still performs that complete
validation. Readers now bounds-check only the node-kind row and the payload
rows they consume; missing rows return `None` rather than reading unchecked
storage. `self-hosted-component-contract-test-smoke` rejects moving the full
validator back into `TypedAstArenaHasNode` and requires the artifact-boundary
seal.

On the same generated integrated driver, this changed peak private memory from
223.4 MB to 159.4-161.0 MB. Elapsed time remained 34.883-35.663 seconds versus
the 34.275-second exploratory baseline, so this is a memory/ownership closure,
not a CPU-speed claim. The generated artifact SHA-256 remained
`A7760C88DCAD10D7EEA87195800ABE642C506640AFAE4147E8A5A2DEEF12044F`.
The next CPU target remains allocation-heavy character and substring scanning
in parser/semantic/MIR composition.

The next bounded codegen scan slice removed five allocation-returning
`Substring(...) == token` comparisons from literal and expression scanning.
Those sites now consume the existing `SubEqualsWithLen` runtime fact; no new
comparison owner or compatibility alias was added. On the pinned 1,289,598-byte
artifact, two HEAD-control runs measured 948.4-985.5 MB peak private memory and
15.798-16.942 seconds. Two candidate runs measured 947.6-951.6 MB and
16.286-17.031 seconds. All four outputs remained 1,191,490 bytes with SHA-256
`CC3460FAA069352C50FE5739194C80A759691A247AE9BDA200338F9672D90BAC`.
An LLVM-built candidate emitted the same bytes and hash.
This is an allocation-removal and memory-ceiling closure, not a runtime-speed
claim: elapsed time did not improve. It also does not make
`AllocatorScratch()` a checkpoint arena or reclaim ordinary `String`
temporaries at function boundaries.

A follow-up allocation census found that the remaining peak was not uniformly
distributed. On the same artifact, checked String primitives requested about
497.4 MiB: `Concat` accounted for 423.9 MiB, one-character reads for 32.5 MiB,
`Substring` for 27.3 MiB, and `StringTrim` for 13.6 MiB. The dominant single
site copied the roughly 178 KiB program-global function/type row string into
each routine's local environment. Across 1,215 routines that site alone
requested about 216 MiB.

The type environment now has one structured owner, `CodegenTypeEnv`, with
separate `global_rows` and `local_rows`. The C/LLVM-compatible state carriage
stores those lanes separately and only the owner may project or update them;
it never concatenates the program-global rows into a routine-local string.
The owner preserves the previous global-then-local first-row lookup order, and
the local lane owns its leading `|` row delimiter. Scope-aware shadowing must
arrive as typed scope facts rather than as an accidental storage-migration
precedence change.

With 100 ms process-tree sampling, the C-built emitter measured 811.1 MB peak
private memory and 14.440 seconds; the LLVM-built emitter measured 782.0 MB and
20.832 seconds. Both emitted 1,191,490 bytes with SHA-256
`CC3460FAA069352C50FE5739194C80A759691A247AE9BDA200338F9672D90BAC`.
Against the immediately preceding 948.4-985.5 MB control range, this removes
137.3-174.4 MB of peak private memory. The component contract rejects the old
global/local concatenation, scalar `LookupKindType` authority, and unsupported
`Array<CodegenTypeEnv>` or `inout CodegenTypeEnv` carriage paths.

That next scan-owner slice is now landed without changing `String` ownership.
`source_scan_owner.pgy` owns allocation-free byte reads, ASCII class facts,
exact-window comparison, and whitespace/comment traversal. Parser cursor and
semantic text scanning consume those facts; their compatibility String
character accessors remain available to unmigrated call sites but are forbidden
inside the converted hot regions.

On the same integrated driver and `mir_lower/main.pgy` input, two sequential
Windows runs moved from 37.915-38.071 seconds to 36.891-37.131 seconds. All
outputs were 151,762 bytes with SHA-256
`A7760C88DCAD10D7EEA87195800ABE642C506640AFAE4147E8A5A2DEEF12044F`.
Peak private memory varied between runs, so it is recorded but not claimed as
a closed improvement. Parser 188/188 and semantic 111/111 expected artifacts
match under both C and LLVM; C- and LLVM-built integrated drivers emit the same
MIR-lower C artifact. `self-host-source-scan-owner-test-smoke` hashes the owner
set and rejects allocation-returning reads in these hot regions.

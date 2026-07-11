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

The remaining 3.7 GiB is not a branchless-code opportunity. It is lifetime
debt: compiler `String` transforms allocate outside an owned text-assembly
boundary and are not reclaimed per emitted function. The allocator names alone
do not close this: `AllocatorScratch()` is currently a system-backed lane label,
not a bulk-reset arena, and `AllocatorDestroy()` only releases pool backing
storage.

Text-builder rung 1 landed on 2026-07-12. `TextBuilder` is a typed, move-only
builtin owner with `New`, `Append`, `Finish`, and `Drop` operations. A
`MIRTextBuilderRuntimeRow` owns the C and LLVM runtime symbols and their distinct
call shapes; both backends consume the row attached to each MIR instruction.
The runtime owner checks length/capacity overflow, keeps intermediate storage in
the system lane, and requires exactly one explicit `Finish` promotion or
`Drop`. `Finish` copies the result into the caller-provided result allocator and
releases the intermediate buffer. The ABI, MIR mutation, C/LLVM differential,
negative ownership, runtime-bitcode symbol, and memory-layout gates cover this
bounded contract.

This is deliberately not a general linear-owner claim. The accepted surface is
an immutable function local initialized directly by `TextBuilderNew`; it must
be finished or dropped in its declaration scope. Parameters, returns, fields,
containers, generic moves, mutable rebinding, nested-scope lifecycle, and
branch-sensitive consumption remain fail-closed. The first self-host emission
owner has not been repointed, so the measured 3.7 GiB peak has not changed and
must not be presented as closed. The next rung is one typed self-host consumer
plus a repeat of that same full-emission measurement. Do not hide the remaining
debt behind `Array<String>`, a higher CI memory limit, or documentation that
calls the current scratch lane a checkpoint arena.

The measured artifact was stale enough to omit `SelfMirExpressionKind`; a
regenerated 6,338,740-byte MIR artifact contains that enum and closes that
specific diagnostic explanation. It does not close the 3.7 GiB text-lifetime
debt or constitute a completed current-artifact bootstrap run.

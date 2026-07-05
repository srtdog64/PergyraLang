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

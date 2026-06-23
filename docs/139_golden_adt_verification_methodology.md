# Golden, ADT, And Verification Methodology

Status: `methodology-contract`

This document fixes how Pergyra should use golden tests, ADTs, and adjacent
verification methods. It is not a new language feature. It is a review rule for
compiler, self-hosted, runtime, and proof work.

Executable anchors:

- `tests/verification_methodology_smoke.sh`
- `docs/semantics/proofs/VerificationMethodology.v`
- `docs/semantics/proofs/VerificationMethodology.md`

The governing principle is:

> A fact is useful only when its owner, consumer, oracle, and regression gate are named.

## Non-Goals

- Do not claim whole-language mathematical proof from smoke tests.
- Do not claim semantic correctness from golden output alone.
- Do not add a second data model just because a golden fixture is convenient.
- Do not let compatibility fallback become a hidden oracle.

Golden tests, differential tests, property tests, ADT owners, and mechanized
proofs are separate tools. They must not be used as aliases for each other.

## Evidence Ladder

| Level | Evidence | What It Proves | What It Does Not Prove |
|---|---|---|---|
| Prose contract | A doc names the invariant | The intended rule is reviewable | Implementation compliance |
| Smoke gate | A script rejects a known regression | A concrete failure stays closed | Full semantic correctness |
| Golden fixture | Output equals a committed baseline | Output shape did not drift | The baseline is correct |
| Differential oracle | C, LLVM, and self-hosted legs agree | Implementations match on a corpus | The shared behavior is ideal |
| Property/metamorphic test | A relation holds across generated inputs | A general law is sampled | Exhaustive proof |
| Verifier | An IR owner checks required facts | Missing facts fail closed | Source-level design soundness |
| Mechanized model | Coq/Lean/Rocq model type-checks | A small calculus has the stated property | The whole implementation matches it |

Promotion rule: a surface can move upward only by adding evidence. A golden
fixture can be a rung on the ladder, but it cannot replace a verifier or proof.

## Golden Testing

Golden tests freeze observable artifacts:

- diagnostics and diagnostic JSON;
- lexer token text;
- parser compact tree text;
- AIR and MIR JSON;
- ABI layout facts;
- emitted C/LLVM text where shape matters;
- runtime traces and panic classes.

Golden tests are strongest when the artifact is itself the public contract. They
are weaker when the artifact is an implementation accident.

Required golden-test metadata:

- `owner`: the pass or component that owns the output shape;
- `oracle`: C compiler, LLVM backend, self-hosted tool, or spec fixture;
- `normalization`: CRLF stripping, path scrubbing, timestamp removal, sorting;
- `negative`: a fixture that must fail when the fact is missing or malformed;
- `scope`: exact corpus, not "the language".

Anti-patterns:

- golden output generated from a fallback path;
- one baseline used to prove multiple unrelated facts;
- committed output without a live drift guard;
- text comparison standing in for a structured fact verifier.

## ADT: Two Meanings, Both Needed

Pergyra needs both common meanings of ADT.

### Algebraic Data Types

Algebraic data types model closed variants and payloads:

- `Option<T>`;
- `Result<T, E>`;
- enum variants with payloads;
- tagged AST/MIR nodes;
- proof certificate variants.

Use these when the set of cases is part of the semantic contract. Every consumer
must handle all cases or fail closed.

### Abstract Data Types

Abstract data types hide representation behind an owner API:

- `SourceBundle`;
- `TokenStream`;
- `ParseTree`;
- `DiagnosticSet`;
- `EvidenceNode`;
- `MIRRoutine`;
- `LayoutFact`;
- `SlotHandle`;
- `AuthorityEvidence`.

Use these when many callers need the fact but must not know how it is stored.
For Pergyra compiler work, this is usually the stronger SoT discipline than raw
struct sharing.

Rule: a self-hosted compiler component should prefer a named abstract owner
before exposing raw nested records. Raw records are acceptable only after the
owner and pass contract are named.

## Method Stack

| Method | Pergyra Use | Best Owner |
|---|---|---|
| Operational semantics | `if`, `while`, `for`, `defer`, panic, `inout`, slot state transitions | small-step core docs/proofs |
| Trace semantics | intent/effect/authority/coordination observability | AIR evidence and runtime trace owners |
| Axiomatic semantics | pre/post conditions for authority, slot, zone, and ABI effects | pass contract manifest |
| Typestate | slot lifecycle, pin/lease, channel state, file handle state | runtime ABI and semantic type facts |
| Linear/affine ownership | move/consume/release, unique mutable access, single-use tokens | CFG/MIR ownership verifier |
| Effect system | `io_read`, `io_write`, `env`, `nondet`, `panic`, `authority` | semantic/AIR effect owners |
| Capability calculus | who is allowed to perform an effect | authority and slot capability evidence |
| Session/protocol types | channel send/recv/close, async handoff, coordination phases | parallel/channel contracts |
| Refinement types | `NonZero`, `NonNull`, `NonEmpty`, bounded indices, valid ids | semantic proof facts and ABI layout |
| Abstract interpretation | escape, lifetime, deterministic ordering, effect reachability | compiler analyzers/verifiers |
| Separation logic | heap/resource separation, arena/index lifetime, raw pointer scopes | runtime/raw boundary docs and proofs |
| Model checking | async, zone crossing, authority delegation, slot state machines | TLA/Alloy-style future models |
| Differential testing | C vs LLVM vs self-hosted equivalence | parity harness |
| Property/metamorphic testing | parse-print-parse, deterministic order, import reorder invariance | fuzz/generator harness |
| Mechanized proof | small core soundness, no ambient authority, typestate safety | Coq/Lean/Rocq proof pack |

No one method is the language. The architecture is the composition.

## Compiler Artifact Matrix

| Artifact | Minimal Owner | Golden | Differential | Verifier/Proof Need |
|---|---|---|---|---|
| Source input | `SourceBundle` | import expansion fixture | C parser vs self-host semantic source bundle | import cycle/dedup fail-closed |
| Tokens | `TokenStream` / lexer owners | `*_tokens.txt` | `pgy --tokens` vs self-host lexer | malformed literal diagnostics |
| Parse tree | `ParseTree` / parser owners | compact AST text | `pgy --ast` vs self-host parser | structured AST owner, not text mirror |
| Semantic diagnostics | `DiagnosticSet` | `.diag` / JSON | C semantic oracle vs self-host checker | code/reason/fix catalog ownership |
| AIR graph | `EvidenceNode` | AIR JSON | AIR dump vs self-host AIR tools | missing evidence fail-closed |
| MIR routine | `MIRRoutine` / CFG facts | MIR JSON | MIR JSON lowering vs C backend | branch/join/cleanup/ownership verifier |
| ABI layout | `LayoutFact` | ABI golden | C/LLVM layout parity | niche/layout proof obligations |
| Runtime materialization | `RetainFact` / boundary fact | runtime trace | C/LLVM runtime parity | erase-vs-retain verifier |
| Proof certificate | `ProofEnvelope` | certificate JSON | deletion/mismatch negative fixtures | checker-core mechanized proof |

## Golden Rules For Self-Hosting

Hard self-hosting must not mean "Pergyra code exists." It means the Pergyra
implementation replaces a real compiler path under oracle parity.

For every self-hosted increment:

1. Name the C-side path being substituted.
2. Name the Pergyra ADT owner that owns each fact.
3. Compare against the C oracle and, when available, the LLVM oracle.
4. Add at least one negative fixture for a missing fact or malformed input.
5. Document what remains out of subset.
6. Reject fallback text parsing when a structured owner fact exists.

## Erasure And Materialization

Runtime materialization is not automatically bad. Hidden materialization is bad.

Required classification:

- `erase`: evidence proves no runtime artifact is needed;
- `retain`: runtime artifact is required and named;
- `summarize`: a compressed fact remains for diagnostics or observability;
- `reject`: evidence is missing and no explicit materialized boundary exists.

Golden tests should count emitted runtime symbols per axis only after the
classifier exists. A zero-runtime-symbol golden without classification is a
fragile implementation check, not an architectural proof.

## Niche And Explicit Layout

Niche optimization and explicit layout need refinement evidence first:

- `NonZero<T>` or equivalent proof for value niches;
- `NonNull<T>` or handle validity proof for pointer niches;
- alignment and aliasing facts for explicit field layout;
- raw/extern capability boundary for unsafe layout control.

Golden layout tests come after the proof facts. They must not become the proof
that the fact exists.

## Review Checklist

Before accepting a new compiler or runtime slice, ask:

- What is the single owner of the fact?
- Which artifact is golden-tested?
- Which implementation is the oracle?
- Is there a C/LLVM/self-hosted differential leg?
- Is there a negative fixture?
- Is missing evidence rejected, retained explicitly, or silently guessed?
- Does the code consume a typed owner fact, or re-parse source/AST text?
- Is the proof claim limited to the model that exists?
- Is the remaining compatibility path named and counted?

If any answer is missing, the work is not ready to be cited as hard
self-hosted or proof-aligned.

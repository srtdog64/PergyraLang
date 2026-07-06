# 169. Agent Boundary Sentinel Library

Status: `repository-gate`, agent-facing guidance (2026-07-06)

This document is not a language feature and not a stdlib package. It is not part of the Fortran-derived data-parallel evidence work. "Library" here means a
repository sentinel catalog. Fortran-derived parallel evidence is a Pergyra
language/compiler competitiveness axis; this file is a codebase gate for future
LLM-written and agent-written changes.

The rule is simple: when a recognizable code pattern appears, the agent should
stop treating it as a local implementation detail and turn toward the named
source-of-truth owner. The machine-readable source is
[`169_agent_boundary_sentinel_library.json`](169_agent_boundary_sentinel_library.json).

## Purpose

Pergyra's codebase is intentionally split by owners: AIR evidence, MIR facts,
DAG/type metadata, ABI/layout rows, runtime materialization, diagnostics, and
self-hosted parity owners. A future assistant can easily drift into a wrong
boundary by adding a small fallback, a local parser, a helper wrapper, or a
backend guess.

The sentinel library is the opposite of a style guide. It is a steering table:

```text
pattern -> wrong_boundary -> turn_toward -> owner -> gate
```

## Plane Split

Do not merge this catalog with data-parallel language work:

- `docs/168_fortran_parallel_evidence.md` owns language semantics for
  no-alias, disjoint iteration, reductions, layout, projection replacement, and
  visible fallback facts.
- This document owns repository authoring sentinels for future agents. It warns
  when a code pattern is drifting toward the wrong owner boundary.

If a change needs SIMD, NPU, tensor, worker-pool, or Fortran-class bulk lowering
evidence, it belongs to the language plane. If a change needs "when this pattern
appears, stop and turn toward this owner/gate", it belongs here.

## Contract

Each sentinel has:

- `id`: stable machine name.
- `if_pattern`: the code or build pattern that should trigger review.
- `wrong_boundary`: where the implementation is drifting.
- `why_wrong`: the source-of-truth violation.
- `turn_toward`: the owner fact, API, document, or gate to use instead.
- `owner`: the repository owner layer that should carry the decision.
- `gate_candidate`: the existing or planned check that should prevent return.

Adding a sentinel is allowed only when it names a real owner boundary. Do not
use this file as a bucket for preferences, naming taste, or broad architecture
opinions.

## Current Sentinels

| If this appears | Wrong boundary | Turn toward |
|---|---|---|
| Backend, verifier, or self-hosted code rereads source/AST to recover a semantic fact | source recovery after a typed fact should own the answer | MIR/AIR/DAG owner fact or fail-closed verifier |
| A C/LLVM backend accepts missing facts through `i32`, empty string, zero, or best-effort fallback | compatibility path becomes hidden semantics | typed MIR/ABI fact plus backend fail-closed gate |
| A helper only renames a local standard operation | owner vocabulary gets diluted | inline code or a real owner module |
| A generic `*_helpers` file grows because an owner is too large | responsibility split is postponed | owner-named module split |
| `main.pgy` parses JSON/text to reconstruct stage facts | self-hosted tool recovers facts from artifact shape | shared self-hosted fact owner/library |
| A runtime call appears on an erasure fast path without a retain/materialize fact | hidden physicalization | AIR/MIR materialization fact and visible artifact evidence |
| A `parallel` or `async` site passes growable container storage by raw pointer | concurrency boundary leaks mutable storage | channel/result handoff, copy, or pinned read-only view owner |
| A lane decision is reused as data-parallel/vectorization proof | execution safety is confused with data independence | separate data-parallel evidence fact |
| Backend code hardcodes layout, bit order, endianness, or address space | backend-local layout source of truth | ABI/layout row or explicit boundary conversion |
| Security code logs free text for machine-audited events | observability boundary becomes unstructured | structured diagnostic/log event |
| A shell script computes semantic truth that a self-hosted tool claims to own | test oracle hides a second implementation | self-hosted tool output plus negative fixture |
| A broad CI target is run for an isolated owner edit | validation boundary ignores impact isolation | `152_validation_isolation_policy.md` owner-impact decision |

## Gate Shape

The first gate is intentionally small: parse the JSON manifest, require the
stable fields, require unique sentinel ids, and require the index/doc links.
Deeper gates can later consume the same JSON to scan code for high-confidence
patterns.

This keeps the warning library useful to both humans and agents without
pretending that every natural-language pattern can be mechanically detected
today.

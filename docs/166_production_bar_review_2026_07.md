# 166. Production Bar Review - Gate-First Status

Status: `accepted, routed`. Date: 2026-07-06.

This note records the production-bar review as an engineering contract, not as a
release verdict. The review uses a stricter rule than the beta checklist: every
capability claim must name the executable gate that blocks regression.

## Production-Bar Rule

- Gate-less claim = FAIL.
- Partial executable coverage = PARTIAL.
- A claim can move toward PASS only when a smoke, golden, parity, verifier, or
  negative regression gate blocks the old path from returning.
- A document-only assertion is useful routing, but it is not production
  evidence.

## Current Verdict

| Surface | Verdict | Reason |
|---|---|---|
| Language identity | PASS | The systems-language-with-domain-extensions identity is explicit and gated by beta docs. |
| AIR verification-only role | PASS | AIR is treated as evidence, not as a second codegen truth. |
| IR architecture | PASS/PARTIAL | The source-of-truth spine is correct, but several consumer paths still need hard gates. |
| ABI ownership | PARTIAL/BLOCKER | Slot/resource rows are increasingly MIR-owned, but aggregate/generic ABI surfaces must keep moving out of backend-local spellings. |
| C/LLVM backend parity | PARTIAL | Many parity rows are gated; whole-backend equivalence is still fixture and shard dependent. |
| Compatibility evolution | PARTIAL | Compatibility vocabulary and a seed versioned breaking-change corpus now have a manifest parity gate; full consumer/corpus coverage is still incomplete. |
| Concurrency semantics | PARTIAL | Execution lane facts exist; precise producer coverage and negative rows remain P0. |
| Runtime executor | FAIL | The production executor split is not proven by lane-specific implementation gates. |
| Sandbox/runtime safety | FAIL/PARTIAL | Several guards exist, but capability manifests and platform-specific atomicity are incomplete. |
| Performance maturity | PARTIAL | Guard amortization has evidence; a continuous dashboard and frame-budget contract are not closed. |
| Stdlib discipline | PARTIAL | Layering docs exist; L2/domain-module doctrine gates are not complete. |
| Tooling/LSP | PARTIAL | Useful tooling exists, but diagnostic golden and latency contracts are incomplete. |
| Release maturity | FAIL | This is not production-ready and must not be described as such. |

## Accepted P0 Blockers

The review adds these P0 production-bar closures. They augment the existing
beta-closure targets; they do not replace them.

1. Compatibility evolution gate:
   source, ABI/binary, behavior, diagnostic, AIR, MIR, runtime trace,
   capability profile, and stdlib-module compatibility must have one owner.
2. Obsolete migration gate:
   every obsolete surface needs `diagnosticId`, replacement, migration URL,
   warning version, error version, removal version, and codefix status.
3. MIR-owned ABI layout:
   layout rows, runtime function spellings, materialization policy, and
   target-size/align policy must be facts consumed by C, LLVM, and self-hosted
   projections rather than backend-local reconstruction.
4. Backend dumb-emitter gate:
   backend code must consume MIR/ABI/runtime facts and fail closed when a fact is
   missing; it must not infer layout, runtime function names, or semantic facts
   from HIR/AST/AIR fallback paths.
5. LLVM runtime bitcode integration:
   runtime primitives that C sees as inline headers must be visible to LLVM
   before optimization through the runtime bitcode link policy.
6. Precise `BoundaryCaptureFact` producer coverage:
   boundary facts must distinguish value-only, pin, live view, raw slot, raw
   channel, authority crossing, and movability requirements.
7. `ExecutionLane` negative regression coverage:
   Inline, PinnedZone, BlockingPool, LocalAsync, WorkerPool,
   MovableScheduler, and Reject need positive and negative fixture rows.
8. AIR/backend access lint:
   backend code must not consume AIR as a codegen source of truth.
9. Sandbox capability and frame-budget gate:
   filesystem, network, clock, random, subprocess, storage, render, input,
   host-call count, fuel, memory, queues, streams, and blocking calls must be
   explicit capability/profile facts before sandbox claims become active.
10. Stdlib L2 doctrine pass:
   L2/domain modules must remain sketch or pass their layer doctrine gate before
   being presented as active language capability.

## Compatibility Surfaces

The compatibility gate must cover these surfaces together:

- source syntax,
- semantic behavior,
- diagnostics and LSP squiggles,
- MIR JSON,
- AIR evidence,
- ABI/binary layout,
- runtime trace,
- capability profile,
- stdlib module surface.

## Non-Overclaim Rules

- Do not claim native WASM, WIT, NPU, GPU, or dataflow backend readiness from
  projection architecture alone.
- Do not claim a production runtime executor until lane-specific executors and
  negative lane rows are gated.
- Do not claim L2/domain stdlib capability until `docs/148_stdlib_architecture.md`
  doctrine gates mark it active.
- Do not call self-hosting complete beyond named parity rungs and replacement
  gates.
- Do not call C/LLVM parity production-complete until backend output, ABI shape,
  diagnostics, and IR artifacts are covered by the same compatibility owner.

## Owner Routing

| Production-bar item | Owner or next source of truth |
|---|---|
| Compatibility evolution | `src/self_hosted/compiler/compatibility_evolution_owner.pgy`, `src/self_hosted/compiler/compatibility_evolution_manifest.pgy`, `tests/self_hosted/parity/compatibility_evolution_manifest_parity.sh`, `src/self_hosted/tools/compatibility_evolution_checker/main.pgy`, `tests/self_hosted/parity/compatibility_evolution_checker_parity.sh`, plus future production consumers over diagnostics, stable-subset, package, runtime-trace, and native backend policy |
| Obsolete migration | Diagnostic registry plus migration metadata gate |
| MIR-owned ABI layout | `src/compiler/mir_abi_layout.c`, `src/runtime/pgy_abi_spec.h`, `tests/abi_ownership_shape_smoke.sh`, and self-host ABI row parity |
| Backend dumb emitter | `tests/backend_fail_closed_smoke.sh`, `tests/abi_ownership_shape_smoke.sh`, and MIR/ABI fact consumers |
| LLVM runtime bitcode | `src/codegen/llvm_api.c`, runtime bitcode strip policy, and performance parity gates |
| Boundary capture | `docs/146_sea_execution_lanes.md` and MIR/RIR producer code |
| Execution lane negatives | `tests/sea_execution_lane_golden_smoke.sh` and self-host SEA parity |
| AIR/backend access lint | `docs/104_air_compiler_architecture.md` plus backend-access smoke |
| Sandbox capability/frame budget | `docs/semantics/15_capability_sandbox.md`, capability manifest gates, and future frame-budget fixtures |
| Stdlib L2 doctrine | `docs/148_stdlib_architecture.md` and stdlib conformance gates |
| Self-host replacement | `docs/self_hosted/10_hard_self_host_contract.md`, `docs/self_hosted/15_pre_self_host_expansion_ledger.md`, and `docs/160_m2_completeness_execution_plan.md` |

## Immediate Closure

The current self-host source-owner work is aligned with this review, and the
latest full preparation run makes that evidence stronger than the earlier
source-owner slice. On 2026-07-08, `make self-host-preparation-test-smoke`
completed green with:

- 171 real self-hosted sources accepted by the C and LLVM selfcheck legs.
- M2 completeness ledger at `sources=171`, with `lexer=171`, `parser=171`,
  `semantic=171`, `codegen=171`, and `full_pipeline=171`.
- Parser parity over 188 source/fixture rows on C and LLVM parser binaries.
- Semantic parity over 108 fixtures on C and LLVM checker binaries.
- Codegen parity over 68 fixtures on C and LLVM-built codegen tools.
- Bootstrap fixpoint `gen2 == gen3` at 8053 generated-C lines.
- `SELF-HOSTING OK`: the Pergyra-built codegen builds lexer, parser, semantic,
  `mir_lower`, 13 gate/audit tools, and the backend fuzz generator with outputs
  matching the oracle-built tools.
- DRV-0 artifact parity and DRV-1 CLI parity over C and LLVM driver rungs.
- LSP diagnostics, transport, request/response, session, document-store,
  session-state, and hover-content parity over C and LLVM.
- MIR JSON rung-0b parity over 86 fixtures through
  `pgy --mir-json | mir_lower | codegen == C oracle`.
- Follow-up compatibility slice: compatibility vocabulary, obsolete migration
  fields, and a seed source/ABI/diagnostic breaking-change corpus now emit a
  stable `compatibility_evolution` artifact, gated by
  `make self-host-compatibility-evolution-parity-test-smoke`.
- Follow-up consumer slice: `compatibility_evolution_checker` consumes the same
  owner rows through the TestHarness manifest and proves that the seed corpus
  covers source, ABI/binary, and diagnostic changes, gated by
  `make self-host-compatibility-corpus-parity-test-smoke`.
- Follow-up owner-scoped M2 completeness refresh: `sources=173`, with
  lexer/parser/semantic/codegen and `full_pipeline` all at 173/173.

That is enough to close the stale "hidden main staging" concern for the active
self-host preparation path. It is not enough to call production readiness done:
released/native driver and LSP replacement are still runged substitutes, the
runtime executor and sandbox quota surfaces remain partial, and the
compatibility-evolution corpus is still a seed corpus until diagnostics,
stable-subset, package, runtime-trace, and native C/LLVM/self-host production
consumers read it as their shared upgrade policy.

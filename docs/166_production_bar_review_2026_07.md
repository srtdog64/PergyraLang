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
| Compatibility evolution | PARTIAL | Compatibility vocabulary, a seed versioned breaking-change corpus, and its first self-hosted consumer gate exist; full production consumer coverage is still incomplete. |
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
| MIR-owned ABI layout | `src/compiler/mir_abi_layout.c`, `src/runtime/pgy_abi_spec.h`, `tests/abi_ownership_shape_smoke.sh`, `self-host-abi-layout-row-parity-test-smoke`, and `self-host-backend-abi-layout-contract-parity-test-smoke` |
| Backend dumb emitter | `tests/backend_fail_closed_smoke.sh`, `tests/abi_ownership_shape_smoke.sh`, and MIR/ABI fact consumers |
| LLVM runtime bitcode | `src/codegen/llvm_api.c`, runtime bitcode strip policy, and performance parity gates |
| Boundary capture | `docs/146_sea_execution_lanes.md` and MIR/RIR producer code |
| Execution lane negatives | `tests/sea_execution_lane_golden_smoke.sh` and self-host SEA parity |
| AIR/backend access lint | `docs/104_air_compiler_architecture.md`, `tests/air_backend_nonimpact_smoke.sh`, `src/self_hosted/tools/backend_air_access_checker/main.pgy`, and `self-host-backend-air-access-parity-test-smoke` |
| Sandbox capability/frame budget | `docs/semantics/15_capability_sandbox.md`, `src/self_hosted/compiler/sandbox_capability_owner.pgy`, `src/self_hosted/compiler/sandbox_capability_manifest.pgy`, `self-host-sandbox-capability-parity-test-smoke`, capability manifest gates, and future runtime frame-budget fixtures |
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
  fields, and a seed breaking-change row for every compatibility surface now
  emit a stable `compatibility_evolution` artifact, gated by
  `make self-host-compatibility-evolution-parity-test-smoke`.
- Follow-up consumer slice: `compatibility_evolution_checker` consumes the same
  owner rows through the TestHarness manifest and proves that the seed corpus
  covers all nine compatibility surfaces plus diagnostic-id, version-ladder,
  migration-URL, and codefix-status rows, gated by
  `make self-host-compatibility-corpus-parity-test-smoke`.
- Follow-up row-shape slice: the same checker now rejects malformed
  compatibility rows by requiring every change row to keep the `change|...`
  prefix and the eleven-field compatibility envelope. This keeps the corpus
  from passing through loose substring matches alone.
- Follow-up negative slice: the checker now has a malformed-row self-test mode
  that must exit non-zero and byte-match the TestHarness-projected negative
  JSON artifact on both C and LLVM legs when LLVM is available.
- Follow-up obsolete-migration exactness slice: the checker now validates
  diagnostic IDs, warning/error/remove versions, migration URLs, codefix
  statuses, and obsolete migration envelopes by canonical field position rather
  than loose substring presence. A second negative artifact rejects an 11-field
  row with an invalid codefix status on both C and LLVM legs when LLVM is
  available.
- Follow-up ExecutionLane slice: the self-host SEA mirror now emits a named
  33-row policy/evidence artifact. The parity gate locks positive
  `MovableScheduler` rows and negative `Reject` rows for pin, live-view,
  raw-slot, and raw-channel resource capture under movability requirements on
  both C and LLVM. The same suite now emits
  `pgy.selfhost.lane-executor-contract.v1` over `PgyLaneScheduler`, which
  honestly records the current `worker_join_scaffold` executor depth. Live AIR
  JSON full-matrix coverage and lane-specific production executor depth remain
  separate P0/P1 work.
- Follow-up AIR/backend access slice: `backend_air_access_checker` now walks
  `src/codegen` from Pergyra, rejects AIR header/type tokens in backend sources,
  and emits `pgy.selfhost.backend-air-access.v1` plus forbidden-hit negative
  artifacts from both C-built and LLVM-built tools under
  `make self-host-backend-air-access-parity-test-smoke`. This moves the
  verification-only AIR boundary toward hard self-hosted parity while the
  existing Bash full-sweep non-impact gate remains the broad production
  backstop.
- Follow-up native/backend ABI layout slice: `backend_abi_layout_contract_checker`
  now consumes `abi_layout_row_owner.pgy` rows through the TestHarness manifest,
  requires selected native MIR ABI layout rows and runtime-function consumers,
  rejects old `_rel` alias and backend-local runtime-name synthesis terms, and
  emits clean, missing-required, and forbidden-hit artifacts from both C-built
  and LLVM-built tools under
  `make self-host-backend-abi-layout-contract-parity-test-smoke`. This starts
  moving the production ABI ownership blocker into hard self-host parity while
  `abi-ownership-shape-test-smoke` remains the broad native backstop.
- Follow-up backend-emitter negative slice: the first self-host
  dumb-emitter contract checker now runs missing-required and forbidden-hit
  negative artifacts through both C-built and LLVM-built tools when LLVM is
  available, gated by `make self-host-backend-emitter-contract-parity-test-smoke`.
  This strengthens the hard self-host parity path; it does not replace the broad
  native `backend-fail-closed-test-smoke` gate.
- Follow-up sandbox capability slice: `SandboxCapabilityZone` now owns the
  capability/frame-budget vocabulary for filesystem, network, clock, random,
  subprocess, storage, render, input, fuel, host calls, command buffers, memory,
  queues, streams, wall-clock, ambient-denial, and blocking host-call rules.
  `sandbox_capability_manifest.pgy` emits clean and missing-budget artifacts
  under `make self-host-sandbox-capability-parity-test-smoke`. This is a
  production-bar routing gate for the sandbox surface, not a claim that the
  runtime executor or sandbox quotas are fully implemented.
- Follow-up owner-scoped M2 completeness refresh: `sources=173`, with
  lexer/parser/semantic/codegen and `full_pipeline` all at 173/173.
- Follow-up owner-scoped TestHarness split refresh: `sources=175`, with
  lexer/parser/semantic/codegen and `full_pipeline` all at 175/175.
- Follow-up backend AIR access source-count refresh: after adding the
  self-hosted backend AIR access checker, focused
  `make self-host-completeness-smoke` completed at `sources=182`, with
  `lexer=182`, `parser=182`, `semantic=182`, `codegen=182`,
  `lex_parse=182`, `lex_parse_semantic=182`, and `full_pipeline=182`.
- Follow-up backend ABI layout contract source-count refresh: after adding the
  self-hosted backend ABI layout contract checker, focused
  `make self-host-completeness-smoke` completed at `sources=183`, with
  `lexer=183`, `parser=183`, `semantic=183`, `codegen=183`,
  `lex_parse=183`, `lex_parse_semantic=183`, and `full_pipeline=183`.

That is enough to close the stale "hidden main staging" concern for the active
self-host preparation path. It is not enough to call production readiness done:
released/native driver and LSP replacement are still runged substitutes, the
runtime executor and sandbox quota surfaces remain partial, and the
compatibility-evolution corpus is still a seed corpus until old source, old
ABI layout, old diagnostic JSON, old AIR/MIR JSON, old runtime trace,
capability manifest, stdlib surface, and native C/LLVM/self-host production
consumers read it as their shared upgrade policy.

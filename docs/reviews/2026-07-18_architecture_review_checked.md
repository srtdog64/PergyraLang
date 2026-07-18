# Architecture Review Check, 2026-07-18

Source review: external review of repository head `e9a44b98`. Checked against
`main` at `42144fb6`, 27 commits later. This document routes claims to current
owners and gates. It is not a production-readiness declaration.

## Objective Card

- Objective: preserve one executable erasure owner and route the review's
  current blockers without reopening closed work.
- Priority: semantic identity, one SoT, fallback deletion, negative gate, then
  implementation breadth.
- Evidence owner: AIR for evidence/compression classification;
  `VerifiedProjectionPlan` for executable projection disposition.
- Last legitimate consumers: C, LLVM, SelfHosted, and future projection
  backends consume plan plus MIR/ABI facts.
- Forbidden fallback: backend reads AIR or recovers erasure, layout, place, or
  materialization policy from source/AST/HIR text.
- Gates: `abstraction-loss-contract-test-smoke`,
  `verified-projection-plan-test-smoke`, AIR backend-nonimpact gates, and each
  bounded self-host parity rung.

## Accepted Current Findings

- Released/default compiler replacement remains 0%. Bounded DRV-2 and codegen
  fixed points are executable evidence, not default-path substitution.
- The Pergyra source-to-MIR producer's historical approximately 68 GB private
  allocation remains an open blocker. The later 976.7 MB result is the distinct
  Pergyra `mir_lower`/codegen consumer over a native-oracle MIR artifact; it
  does not close source-to-MIR production.
- Ref/inout addressability is now graph-owned and negative-gated, but its final
  owner is still a codegen graph walker. Semantic value/place/borrow facts and
  HIR/MIR carriage remain open.
- `VerifiedProjectionPlan` is the correct executable owner, but its current
  native row covers intent observability only. Layout, cleanup, checks,
  capability retention, composed loss, and artifact residue remain partial.
- Linux ASan/UBSan is blocking, but no Linux TSan path exists. Pool shutdown,
  blocked channel waits, cancellation, and capability/budget state therefore
  lack race-detector evidence.
- Capability grants and budget counters remain process-global and default-open
  for trusted native programs. This is not an untrusted multi-content sandbox.
- The machine-contact layer has explicit grant, region, provider, and plan
  identity, but physical MMIO, atomics, DMA/cache coherency, interrupts, and
  revocation refinement remain target obligations.
- Stable IDs, sparse demanded analysis, incremental artifacts, stage lifetime
  release, opaque Channel handles, logical-work budgets, and instance-local
  sandbox failure remain valid work orders.

## Superseded Or Narrowed Findings

- **Per-translation-unit static-inline runtime:** superseded after the reviewed
  head by the `fb8778c5` through `c03e9a82` extern-runtime workstream. Stateful
  and concurrency runtime families now have one external runtime object owner;
  program-generic families remain local because their concrete C types exist
  only in the generated translation unit. `runtime-cext-contract-test-smoke`
  and codegen parity guard the split.
- **C runtime is recompiled for every generated program:** narrowed. The driver
  owns a fingerprinted runtime-object cache and freshness checks. Program-local
  generic bodies still compile with generated C by design.
- **All integrated compiler paths are operationally infeasible:** too broad.
  The full `mir_lower`/codegen fixed point now completes in 68.5 seconds at
  976.7 MB peak private memory and reaches `gen2 == gen3`. The self-host
  source-to-MIR producer and released default substitution remain blockers.
- **Signed overflow depends on an invisible flag:** narrowed. The driver passes
  `-fwrapv`/`-fno-strict-aliasing`, generated C emits a compile-profile guard,
  and runtime-object/bitcode gates carry the same policy. Standalone generated
  C still requires that declared profile, so this remains a portability
  contract rather than hidden behavior.

## Contract Correction Landed With This Check

The previous docs called AIR the canonical executable erasure decision point
while also forbidding backends from consuming AIR. The corrected contract is:

```text
MIR / ABI / target facts
        +
AIR evidence certificate
        |
        v
VerifiedProjectionPlan  -- sole executable disposition owner
        |
        v
C / LLVM / SelfHosted / future backends
```

AIR still owns evidence completeness, compression classification, and drift.
It may reject a candidate plan. It does not directly control emitted behavior.
The abstraction-loss smoke gate now ratchets this wording.

## Next Executable Order

1. Add semantic value-category/place facts for the currently gated ref/inout
   slice, carry them through HIR/MIR, migrate the codegen consumer, then delete
   `CodegenExpressionAddressabilityFromGraph`.
2. Profile the Pergyra source-to-MIR producer by owner and allocation site;
   preserve the native-oracle MIR path only as a bridge, not as closure.
3. Expand projection-plan rows one family at a time, beginning with runtime
   checks and cleanup because both already have MIR owners and backend
   consumers.
4. Add a focused Linux TSan runtime gate before widening pool/channel behavior.
5. Introduce per-instance capability/budget ownership before making any
   untrusted-content claim.

Current rule:

```text
AIR certifies evidence.
The projection plan owns executable disposition.
A bounded self-host consumer does not close its missing producer.
```

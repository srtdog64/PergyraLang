# Teardown authority implementation

- Status: IMPLEMENTATION COMPLETE (bounded model); not native closure.
- Base: `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, shared dirty main.
- Direct user request overrides the CL1 edit allocation, not the compiler hold.

## Objective card and closure chain

Preserve the single `OwnershipTeardown` forest while separating copied identity
from owner-held cleanup responsibility. Priorities: sound whole-unit admission,
one forest/retirement transform, affine responsibility, explicit refusal, then
cost. Doc 28 owns the required language boundary. The importing authority model
owns executable issuance, transfer, loan/pin state, and retirement admission;
the original forest owns exact units and destructive transforms. The final
model consumers are the permanent redteam proofs and extracted OCaml observer.

Forbidden fallback: admitting a retirement through raw `Step`, accepting a
caller-selected boolean permission, ignoring descendant loans, or destructively
checking half a unit. Raw forest `Step` remains an internal specification, not
an authority API. No second heap, teardown implementation, compiler annotations,
or changes to native runtime/installed drivers.

## Fixed implementation order and edit scope

1. Import the forest in `OwnershipTeardownAuthority.v`: private-state rights
   keyed by root identity, issued at root creation, moved atomically, removed
   at root retirement; leases have monotonic identities and holder checks.
2. Validate complete unique units before invoking the existing transforms.
   Use a bounded model certificate verifier, not a claimed production walker.
   Bind the acting principal to trusted execution context in model state;
   requests cannot choose a caller identity. Physical context binding remains
   a production refinement obligation, not a cryptographic theorem.
3. Migrate redteam/extraction/observer/gate consumers; retain the two raw-forest
   counterexamples and add refusals at the admitted boundary on the same input.
4. Ask installed Claude for read-only review; repair confirmed findings.
5. Run focused gate, full kernel/inventory gates, and refresh evidence/handoff.

GPT exclusively edits this importing core and its integration files for this
scope. Claude reviews read-only after the first implementation. No parallel
proof edits, process-name kills, worktree split, commit/push, or GUI messaging.

## Acceptance and falsifiers

Integration owner: this GPT chat. Shared gate:
`tests/ownership_teardown_redteam_smoke.sh`. Static checks 60 seconds, focused
gate 5 minutes, full kernel/inventory integration 30 minutes. Rocq 9.3.0 only.

Positive: actual root creation/allocation, same-owner subtree retirement,
legitimate root cleanup, transfer followed by new-owner cleanup, own lease end
followed by cleanup. Negative: reference-only node/root release, unissued or
consumed responsibility, stale generations after reuse, wrong/incomplete or
duplicate unit, child loan/pin, forged lease end by another context, old lease
identity replay. Rejected execution returns the identical state; authority
admission preserves the original forest invariants.

Remaining outside this boundary: native issuer/call-frame binding, exact indexed
production walker, concurrency/atomicity refinement, bounded generation
exhaustion, finalizer behavior, physical allocation and compiler integration.

## Observed completion

Final owner hash `ca8aa6f11df6bccad9127ebe01e3cc9cb49c28a3c72f8b808f906f4d95960a86`.
Existing focused gate PASS: four kernel-verified modules / zero assumptions,
62 extracted authority cases, 18 source guard mutations, one extracted approval
mutation and two cost-oracle mutations. Full formal smoke/kernel integration
PASS (72 modules, existing Slot assumption budget unchanged). Actual installed
Claude completed two read-only reviews; first High and proof obligations repaired,
second no new High/Medium within scope, four Low statement/gate issues strengthened
and rerun by GPT. Receipt: `docs/audits/ownership_teardown_authority_2026-10-08.md`.
No compiler hold lift, stage/commit/push, native substitution or CI claim.

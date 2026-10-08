# Ownership teardown authority — executable model receipt

Date: 2026-10-08 KST. Base HEAD:
`3658548d24bca3d721e4f1974ac7a10da99f7aa8`, shared dirty main.
Status: MODEL IMPLEMENTATION VERIFIED on the hashes below, after two completed
Claude read-only reviews and the final source revalidation. No stage, commit,
push, installed-driver replacement or
native/compiler closure claim.

## Implemented boundary

The user directly requested CL1 implementation followed by Claude co-review.
Under the collaboration directive's exception 2, this GPT chat implemented
`OwnershipTeardownAuthority.v` and its permanent consumers. The forest and
destructive transforms remain owned by the unchanged `OwnershipTeardown.v`.
No second heap, permission-boolean whitelist, compiler annotation workaround,
runtime change or unrelated SoT row was introduced.

The protected machine state holds one root cleanup-right cell (epoch, holder).
Creation actually issues it; transfer actually replaces its holder; root end
actually removes it. Subtree release retires its node identities while preserving
responsibility for other surviving nodes. Requests cannot choose their caller:
the acting context belongs to trusted scheduler state. Issuance/provenance,
whole-run invariants and consumed-node/root non-resurrection are proved.

Both retire routes check current target, real cleanup owner, complete unique
unit and every active lifetime loan/pin before destruction. All six executor
operations return the identical state on refusal. Only the cleanup owner may
issue a lifetime lease, including to a different borrower; that borrower can
return it but does not gain cleanup rights. Lease ids never reset/reissue.
Canonical forest invariants and legitimate no-loan node/root cleanup follow
from the actual functions, not assumed desired postconditions.

The unit verifier checks finite membership closure over the forest bound.
Soundness/completeness are proved under the forest invariant. Bounded parent
lookup completeness uses unique owner heights and a duplicate-free finite
path, not a guessed larger fuel allowance. Neither algorithm is a production
indexed unit walker or a speed claim over GC.

## Claude co-review and repairs

Actual installed Claude Code was run in read-only print mode with only
Read/Grep/Glob, no MCP, edit or shell tools, no model override. First result
identified its configured model as `claude-opus-5-5[1m]`; terminal result was
completed, not an inferred review. Raw prompt/results are in the receipt
directory `.tmp/ownership-teardown-authority-2026-10-08/`.

The first review found one concrete High model defect: copied references could
start a third-party pin and indefinitely block the owner's cleanup. Repaired:
`begin_lease` now checks owner approval before issuing a designated-borrower
lease. Permanent proof and extracted negative controls reject foreign and
previous-owner issuance. The `lease-approval` guard mutation is a falsifier.

Additional review obligations repaired:

- real one-step grant provenance (`execute_right_origin`), with the reachable
  coherence theorem renamed to avoid pretending coherence alone proves issuance;
- `next_lease_monotone_runs`, `ended_lease_identity_never_reissued`;
- `node_root_complete`, `legitimate_owner_node_release_executes`;
- `ab_reader_invariant`, same `st_ab` owner cleanup and incoming-link clearing
  positive controls, alongside both reference-only refusals;
- `execute_refusal_preserves_state`, explicit `RightAlreadyIssued` diagnostic;
- extraction/observer trust-scope warning and doc 28 evidence refresh.

The second completed read-only review validated the first review's High/Medium
repairs and found no new High/Medium defect within the declared model scope.
Its four Low findings were subsequently strengthened by GPT:
`ended_lease_identity_never_reissued` now proves actual absence and replay refusal
in every later admitted run, not merely an arithmetic inequality;
`lease_issuance_requires_owner_approval` binds the real output lease list/counter;
`execute_right_origin` ties transfer to the preserved epoch. The approval guard
mutation must fail within its semantic issuance theorem, and an independent
extracted-code guard mutation must exhibit wrongful acceptance of the foreign
pin request. These final proof/gate strengthenings are kernel/execution checks;
they are not represented as a third Claude review.

## Observed gates

`tests/ownership_teardown_redteam_smoke.sh`: PASS on the hashes below, Rocq
9.3.0 / Stdlib 9.2.0 / OCaml 4.14.1. Four focused modules freshly compiled and
kernel-verified, zero assumptions/admits/unsafe kernel features. Actual freshly
extracted functions passed 62 authority success/refusal cases. Earlier forest
oracles still passed 98,304 field updates, 16,384 teardown cases, 16 parent
cases, four schedule controls, 64 reuse rounds / 20,608 identity checks.

On the final focused rerun, eighteen guard/check mutations, one extracted
approval mutation and two cost-oracle mutations were refused. The source
approval mutant failed at its actual semantic issuance theorem (line 808),
and the extracted mutant wrongly accepted `foreign pin issuance`, which the
independent observer caught. Neither witness relies only on proof-script shape.
unknown-command failure remained observable. Mutation rejection requires an
actual proof compilation failure, not missing tools or timeout. Cost observers
retain the fixed previous finite inputs; no independent optimization track was
opened, and costs are bounded functional-model/OCaml observations only.

Documentation quality, evidence lifecycle adequacy (including its RED self-test),
shell syntax and scoped whitespace checks also passed. An initial command used
the nonexistent `evidence_lifecycle_adequacy.sh`; the actual
`evidence_lifecycle_adequacy_smoke.sh` was subsequently run and its PASS observed.

`tests/formal_semantics_smoke.sh` on the final `ca8aa6f1` source: PASS.
Its full `tests/coq_kernel_check.sh` integration kernel-verified 72 modules
(65 proof owners plus seven permanent consumers), with approval export/binding
consumer checks. The only declared assumptions remain the two existing
SlotCalculus abstractions (`verify_token`, `MaxSlotId`), no new assumptions,
admits or unsafe kernel features. Existing corpus warnings for name masking,
deprecated list notation, nested induction schemes and approval load-path
remapping remain visible; this is not a warning-free claim.

Final checkpoint: unchanged HEAD `3658548d`, 453 observed dirty Git entries,
zero staged paths. Counts describe the shared checkout, not this change's size.

| Source | SHA-256 |
|---|---|
| `OwnershipTeardown.v` (unchanged imported forest) | `4babfc268dee283f377c0e0ae337d8afffdda594133e4f0ea1a3f416c0b93369` |
| `OwnershipTeardownAuthority.v` | `ca8aa6f11df6bccad9127ebe01e3cc9cb49c28a3c72f8b808f906f4d95960a86` |
| `OwnershipTeardownRedteam.v` | `7d195d6a5ac59f0ee24b173accab59f44aa2c82002e06b0cb499eb49ddd814f0` |
| `OwnershipTeardownExtraction.v` | `2877fad81f889a5dd242ccd7d04e4c8e3c86757b5497cb45770e08f3c4faf771` |
| `ownership_teardown_driver.ml` | `978cd34d0a03dbad05f83d3f621e744e154ad583b8bb70c3bd0402fdb73e58a8` |
| `ownership_teardown_redteam_smoke.sh` | `23311a343432c596947c3863f00998fd1762883f13231c603996fdef8faf65a4` |

The shared integration receipts are `kernel-extraction.log`, `controls.log`,
`focused-final.log`, `formal-final.log`, each `mutation-*.log`, and the fresh
extracted source pair in `extracted/`. The earlier `formal-full.log` validates
the first implementation snapshot; it is not the final source receipt.

## Exact remaining boundary

This is sequential trusted-state lifetime admission, not a native opaque API.
The scheduler fixture and extracted record constructors can construct contexts
and states; machine protection and non-reused authenticated context issuance
are production obligations. Live transfer/loan recipients and root namespace
authority are not modeled. There is no accepted snapshot rollback operation.

Lifetime borrow/pin does not promise immutable fields or concurrent exclusivity.
An outside node's non-owning link can be cleared while that node is leased.
Access/mutation/reparenting consumers and composition with Slot/ordinary storage
remain CL2 work, not a raw `Step` fallback. The bounded full-bound verifier has
bound-times-membership cost, and unique lists cost quadratically; CL4 must own
the real exact indexed walker and measured production cost.

Finite identity exhaustion, physical allocation/free and protected current-state
linearity, concurrency, finalizers, async/FFI and compiler cleanup synthesis remain
OPEN. No native C/LLVM, installed-driver, CI, SoT or self-host substitution is
claimed. Compiler hold and I1–I8 order remain unchanged. Existing dirty work was
preserved; no shared process-name kill or worktree operation was performed.

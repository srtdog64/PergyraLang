# Ownership lifecycle review: hardening evidence

2026-10-09. Base observed before/after focused work:
`main @ 85fff5aa339ae136ac0797c4a6a95a5285ada70e`, shared dirty checkout.
This report covers model/contracts and implementation-boundary documentation,
not production ownership-cutover P1, a SoT closure or a Git publication.
The [objective and closure plan](../agent_work_directives/ownership_lifecycle_review_hardening_2026-10-09.md)
fixes the chain; the [boundary matrix](../semantics/ownership_lifecycle_implementation_boundaries.md)
records implementation obligations without taking semantic authority.

## Findings and dispositions

| Review issue | Change / evidence | Remaining boundary |
|---|---|---|
| A failed forward step retains its successful prefix | Canonical `StepReceipt` records successful body length. `run_body_receipt_sound` and `run_body_receipt_projects` prove actual state/result correspondence; bounds and full-success/zero-acquisition-prefix results accompany it | No transaction rollback or source effect issuer |
| An empty admitted compensation can finish without undoing a write | Replaced ambiguous result with `SagaCompensationFinished`; no compatibility alias. Permanent admitted witness observes A=20 after starting at 10 | Domain restoration contract remains OPEN; completion is not success |
| A compensation can itself partially fail and erase the original cause | `SagaStuck` retains forward and compensation receipts. Witness records acquisition failure/0 and compensation body refusal/1 with A=15 | No automatic retry or full-step compensation of a failed prefix |
| Hold cleanup was read as effect cleanup | Existing no-dereference-failure and borrow-restoration proofs migrated; outer-hold and read-only-prefix witnesses added | Held graph table remains ghost state; runtime representation not selected |
| Mathematical original-state return was read as native atomic free | Doc 209/28 and boundary matrix separate same-original-state admission, private functional staging and physical stable/non-refusing commit | Native allocation/reentrancy/commit/publication not implemented |
| Root/count/authority parameters were read as issued facts | Matrix names `CountsExact`, `AllEdgesIssued`, `SimInv`, real MIR/layout producer and canonical store/root/epoch/holder binding | Producer, synchronized forest/graph retirement and whole-unit lease/pin binding OPEN |
| `fuel` or candidate selection was read as a work bound | Docs distinguish closure rounds, full slot-spine folds, lookup/membership, validation, temporary allocation and commit cost | No physical locality, latency or total-work theorem |
| Store identity reuse and GC terminology | Existing monotone `gsid` preserved and reuse witness added; finite identity/counter exhaustion separated. Optional reachability reclaim explicitly classified as GC-family, not the baseline | No finite native identity/counter implementation or universal GC performance claim |
| DX/automatic propagation could become manual annotations | Boundary fixes one compiler-owned fact path and forbids missing inference being repaired with lifetime/root/deep-drop/own-ref ceremony | Actual inference and new-type integration cost remain to be measured on production |

No graph/tree/value heap semantics were replaced. `OwnershipCleanCore.v`,
`OwnershipGraphLinks.v`, `CompensationCore.v`, tree authority and counted
reclamation models were not edited by this task. The result migration reaches
all current executable model consumers and doc 209; there is no native caller
of this model API. The full kernel gate now includes `GraphActionScopeAudit.v`
as well as its existing consumers.

## Observed validation

All successful commands use the pinned Rocq 9.3.0 / Stdlib 9.2.0 wrapper in
`Ubuntu-E-WSL`, with `OPAMROOT=/home/c/.local/share/pergyra-rocq/opam`.

| Gate | Observed result | Receipt/log |
|---|---|---|
| Focused action gate, final source | PASS, 4 modules, zero assumptions | `.tmp/graph-action-scope/run.TV1tC5/` |
| Action planted regressions | PASS, all 11 refused for the expected reason | `.tmp/graph-action-scope/selftest.EjQBkQ/` |
| Existing graph/root/atomic bridge gate | PASS, 8 modules, zero assumptions | `.tmp/graph-cycle-reclaim/run.TkyM7o/` |
| Full kernel snapshot | PASS, 84 modules (74 models + 10 consumers), plus approval export/binding consumer; only the two pre-existing approved Slot abstractions | `.tmp/ownership-lifecycle-hardening-2026-10-09/full-kernel.log` |
| Full wrapped formal entrypoint | PASS, static contracts/inventory plus the same 74-model/10-consumer kernel and approval binding checks | `.tmp/ownership-lifecycle-hardening-2026-10-09/formal-final.log` |
| Documentation quality | PASS | `.tmp/ownership-lifecycle-hardening-2026-10-09/docs-quality.log` |
| Five scoped docs: strict UTF-8, whitespace and local file links | PASS, 25 file links; fragment anchors are not checked by this filesystem test | Read-only PowerShell check; no generated source rewrite |
| Whitespace | `git diff --check` PASS for tracked changes; untracked action model/consumer/gates checked against their pre-edit snapshots | Read-only Git checks |
| Snapshot freshness | All 84 source hashes in the final formal/kernel snapshot still match the workspace | Read-only SHA-256 comparison after the run |

The 11 planted controls cover missing falsifier, admitted uncovered reads,
weakened no-dereference theorem, erased successful prefix, weakened exact
receipt projection, overwritten original failure, false forward success,
unconditional full-step compensation on a failed prefix, added axiom,
source drift and receipt overwrite. These are bounded regression checks,
not a claim of complete mutation coverage.

The first new prefix proof failed to compile because its list equality was
not discharged; the proof was corrected, without weakening its proposition.
An initial invocation of `formal_semantics_smoke.sh` outside the pinned
wrapper passed its static checks then failed its missing-Rocq guard; it is
not recorded as a green formal gate. The full wrapped entrypoint subsequently
passed, as recorded above. No `PGY_ALLOW_MISSING_COQ` skip or budget relaxation was
used. Existing nested-list induction-scheme, module-name masking and approval
load-path warnings remain in the logs; they are not hidden or a native result.

## Exact focused input hashes

| Input | SHA-256 |
|---|---|
| `docs/semantics/proofs/OwnershipGraphActionScope.v` | `faefa5de9d84b25fdfd01623eaae3f40effb4ef251212e415346adb33ab1d099` |
| `tests/coq/GraphActionScopeAudit.v` | `a546bbaafd9b47bd333ec21644142139ce673c40b278b15e1be65b5cea59cc64` |
| `tests/graph_action_scope_smoke.sh` | `b0ed7f18de85178414ed52f1523458255149cc21f19b4ecc5b44da5a328999ff` |
| `tests/graph_action_scope_selftest.sh` | `98ced39aec61fc873f238a801de0bbf4e2d4538d6692687aa3086f25c52f43fb` |
| `tests/coq_kernel_check.sh` | `39a5e59c9ec989b39fdac4a279beb353978c84cf09971e7ce3f8c93e22e12a6b` |

Focused receipts also bind imported source, kernel gate and toolchain owner
before and after each run. These checks do not imply continuous source
immutability or a clean checkout. Pre-edit copies of touched files are in
`.tmp/ownership-lifecycle-hardening-2026-10-09/baseline/` for this task's diff;
they are not a rollback of the user's concurrent production work.

## Handoff limit

No production source, native binary, installed driver, source inference,
physical rollback, collector work bound or CI was delivered. No stage,
commit, push, new worktree or external message was performed. Other-lane
changes, including reached self-host ownership producers and their fixtures,
remain dirty and preserved. The current active P1 card remains authoritative
for that production work. Next graph integration must first discharge the
specific reached row of the boundary matrix with a real producer/last consumer
and falsifier; the matrix is not authorization to open all rows in parallel.

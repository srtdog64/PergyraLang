# Evidence lifecycle compression core

Companion to [`EvidenceLifecycleCore.v`](EvidenceLifecycleCore.v). The semantic
owner is [`../09_abstraction_loss_contracts.md`](../09_abstraction_loss_contracts.md);
this note and the Rocq model are supporting proof artifacts, not a second authority
for compiler policy.

## Objective card

- Objective: make "carry the authority established by evidence, not every byte
  of its construction history" a precise, falsifiable Pergyra design rule.
- Priority: preserve semantic identity and authority; fail closed on missing
  admission; erase construction payload only after its last semantic consumer;
  then minimize representation.
- Fact owner: `docs/semantics/09_abstraction_loss_contracts.md`, with each live
  `AIREvidenceKind` lifetime row owned by
  `docs/semantics/evidence_kind_manifest.md`.
- Last legitimate consumer: the consumer declared by the concrete evidence row;
  the generic model deliberately does not invent a compiler stage.
- Forbidden fallback: downstream semantic re-decision, erasure before discharge
  or before the last consumer, and runtime materialization without an explicit
  runtime need.
- Verification gate: `make evidence-lifecycle-adequacy-test-smoke` plus
  `bash tests/coq_kernel_check.sh`. Removing a cited theorem, a lifecycle state,
  the proof registration, or a live manifest row is the falsifying case.

## Model

The model keeps four evidence classes distinct:

| Class | Intended lifetime |
|---|---|
| Identity | Keep a stable reference while downstream still needs identity or authority. |
| Validity | Compress a discharged proof into a sufficient receipt when removing it would force semantic re-decision; materialize an explicit checked path only when runtime meaning requires it. |
| Diagnostic | Keep source/provenance through a side reference, not as main-IR semantic authority. |
| Construction | Retain while a semantic consumer still needs the rich payload; erase after discharge and the last consumer. |

The lifecycle operations are `Retain`, `Reference`, `Summarize`, `Materialize`,
and `Erase`. They are not a replacement for the live AIR compression-budget
vocabulary (`retain`, `summarize`, `erase`, `forbid`). `Reference` describes
identity carriage and `Materialize` describes the verified projection into a
runtime representation; `forbid` remains a budget verdict when no sound
lifecycle transition is available.

`EvidenceProjection` deliberately separates the rich payload's disposition from
the compact carrier of established authority. This makes the central case
representable: construction evidence can be erased while its admitted authority
continues as a fact/receipt/identity.

## Theorems

- `missing_admission_fails_closed`: absence of admission never becomes
  downstream success.
- `identity_reference_carries_authority`: admitted identity may become a compact
  reference without dropping required authority.
- `validity_summarizes_to_receipt`: admitted validity evidence becomes a compact
  authority-carrying receipt when downstream still needs the decision.
- `construction_erasure_preserves_established_authority`: after the last
  semantic consumer, construction payload erases while the established
  authority remains carried.
- `construction_erasure_requires_discharge_and_last_consumer`: the model cannot
  erase construction evidence early or without admission.
- `materialization_requires_explicit_runtime_need`: only admitted identity or
  validity evidence with an explicit runtime need can materialize in this
  bounded model.
- `no_redecision_means_no_receipt_justification`: a receipt is ceremony when
  removing it would not make downstream decide semantics again.
- `receipt_justified_iff_redecision_when_authority_required`: while downstream
  still needs the admitted authority, the receipt criterion is exactly whether
  its removal would reopen semantic decision.
- `justified_validity_receipt_carries_authority`: a justified receipt both
  summarizes and carries the established authority.
- `compression_trace_nonincreasing`: possible interpretations and abstract
  representation units never increase along a valid compression trace.
- `admitted_interpretation_does_not_reopen`: once one interpretation is sealed,
  downstream compression cannot reopen multiple interpretations.

All theorems are constructive and add zero axioms or admits.

## Honest boundary

"Semantic entropy" here is the finite count of interpretations still admitted
by the model, not Shannon entropy. `representation_units` is an abstract measure,
not heap bytes, compile time, or binary size. The theorem proves monotonicity for
a transition already classified as `CompressionStep`; it does not prove that the
live compiler's every transition is so classified.

The adequacy gate binds vocabulary and registrations to current repository
artifacts. It does not prove that C/LLVM implements every projection correctly,
does not close any SoT registry row, and does not count as self-host
`SUBSTITUTING` progress. Live per-kind lifetime correspondence remains the job
of `tests/evidence_lifetime_smoke.sh`; physical erasure remains the job of
`tests/air_erasure`.

Verify with:

```sh
make evidence-lifecycle-adequacy-test-smoke
bash tests/coq_kernel_check.sh
```

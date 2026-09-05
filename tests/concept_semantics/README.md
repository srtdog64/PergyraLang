# Concept deletion and strengthening tests

These are bounded source rewrites and semantic controls, not removal of a
concept from the compiler and not a proof against every possible encoding.

Run the currently supported contracts with installed binaries:

```sh
bash tests/concept_semantics/run.sh
```

The four lanes compare typed Intent terminal attribution, capability/effect/
participant authority, domain call/binding contracts, and immutable object/
struct behavior. Native C/LLVM execution, self MIR fact carriage, and self
capability-manifest checks have separate scopes. See each runner and the
[integrated audit](../../docs/audits/2026-09-05_language_axes_semantic_integration.md).

Run the **open self-host source-admission claims** separately:

```sh
timeout 300 bash tests/concept_semantics/source_admission_parity.sh
```

This second command currently fails. It requires eight semantically invalid
sources to be rejected before MIR publication and requires the valid typed
Intent source to publish its v3 execution plan. Every negative first checks a
native semantic diagnostic. It does not execute invalid MIR, accept failures
as expected success, or claim full plan correctness from a schema check.
Missing tools/timeouts are not successful rejection evidence.

The [full-vocabulary experiment intake](../../docs/audits/2026-09-06_language_word_deletion_intake.md)
reopens every registered language word and each grammatical context, including
previous KEEP-CORE concepts. The four lanes above are not that complete census.
Spelling removal, source rewrite, ordinary library encoding and deletion of a
compiler mechanism are different interventions. A library is not disqualified
merely for retaining the same information in structs, enums or receipts.
Compare static rejection, effects/alias, failure/cleanup phases and boundary
costs as well as output. Six native MIR/admission rechecks in the intake are
not runtime or self-host parity, and the earlier in-memory LSP batches have
not been preserved as the same completed fixtures.

The later [executed word-deletion matrix](../../docs/audits/2026-09-06_language_word_deletion_execution_matrix.md)
adds 35 bounded experiments and 104 durable programs under `word_deletion/`.
Its retained native/public C records cover all 104 programs, with 34 outcome
differences. Primary independently reproduced 22 outcomes for the enum-match,
capability-bound and subject-parameter experiments; see the intake follow-up.
`word_deletion/run_matrix.py` is a manual observation collector, not an
expected-result gate: exit zero alone does not validate recorded outcomes,
and its timeout/missing-executable summaries are not semantic rejection or
equivalence evidence. Full LLVM and per-word/context equivalence remain open.

These are manual focused entrypoints, not newly wired CI jobs. The supported
runner's green result must not hide the second command's red result. Fix one
named production owner/consumer seam at a time; do not add a blanket native
retry or weaken the rejection assertions to make this suite green.

`PGY_BIN` and `PGY_SELFHOST_PREBUILT_DRIVER` select binaries. `PYTHON_BIN`
selects the Python JSON validator. Git Bash is required on this Windows host.
Each lane has a five-minute outer budget in the supported runner; the open
admission command has a five-minute total budget in the invocation above.
Logs stay under `.tmp/self_hosted/concept_semantics_20260905/`; logs and case
counts are evidence, not semantic owners or self-host replacement progress.

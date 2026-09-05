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

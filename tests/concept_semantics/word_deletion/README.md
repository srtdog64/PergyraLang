# Language-word deletion matrix

Executed source pairs for the full-vocabulary deletion experiment. Each
directory under `cases/` is one bounded experiment: `orig.pgy` uses the word,
`subst.pgy` does without it, `neg_orig.pgy` and `neg_subst.pgy` write the same
mistake both ways, and any other file is an extra probe.

`run_matrix.py` compiles every program with the C backend, runs it, and
records compile exit code, diagnostics, stdout and run exit code. Run it once
on the public path and once with `PGY_EXTRA_FLAGS=--native-pipeline`; the two
result files disagree on purpose, and the disagreement is part of the record.

Verdicts, provenance and the per-word table are in
`docs/audits/2026-09-06_language_word_deletion_execution_matrix.md`. This lane
is a manual entry point, not a CI job, and it owns no language semantics.

`collect_admission.py LAUNCHER DRIVER` observes the same corpus through native
and public source-to-MIR routes without compiling or executing any input. It
records exact binary/source hashes, diagnostics, status differences and
operational failures under `.tmp/concept_semantics/word_admission/`. Run its
`--self-test` first. The census has a five-minute total budget; timeouts, blank
failures, missing MIR and an unexpected native public route are not refusals.
An equal status is not equivalent behavior or even an equal refusal reason.
This measurement cannot replace the historical runtime-difference denominator.

`collect_runtime_divergence.py LAUNCHER DRIVER` is the executing complement of
that census. It builds and runs every program on both routes and reports the
ones both routes build whose observable result differs, which is precisely the
class a source-to-MIR status pair counts as agreement. Reports land under
`.tmp/concept_semantics/word_runtime/`. Run its `--self-test` first. Budgets
are `PGY_WORD_BUILD_TIMEOUT`, `PGY_WORD_RUN_TIMEOUT` and
`PGY_WORD_TOTAL_BUDGET`. It compares stdout and exit status only; equal
results are not equivalence, and neither route is treated as the oracle.

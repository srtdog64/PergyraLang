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

# CI assertion repair review — 2026-09-05

Status: AUDIT COMPLETE. The initial canonical-inline-input finding is repaired
and independently rechecked below; the existing JSON/Python and literal-include
limits remain. This is a read-only review, not CI closure or compiler
substitution evidence.

Base: `f34355b37dbd9e86ef574399e895a78fd41dd0a3`, with the primary task's
uncommitted CI assertion repairs. The working tree also contains concurrent
compiler, documentation, and other-task changes. This lane changed only this
report; it did not stage, commit, push, rebuild, install, or query remote CI.

Directive: [CI semantic integration review](../agent_work_directives/ci_semantic_integration_review_2026-09-05.md).
Primary owns implementation and the eventual semantic parity integration gate.

## Observed ownership and assertion changes

### Windows MSYS2 Git dependency

- `.github/workflows/platform_full.yml:157` adds `git` specifically to the
  `platform-full-windows` MSYS2 installation. That job invokes `make ci-windows`
  inside `shell: msys2 {0}`; its inherited host checkout is not the package list
  checked by this repair.
- `tests/self_host_ci_profile_smoke.sh:40` scopes its new AWK assertion to that
  exact job and its folded `install: >-` list. A `git` mention in another job,
  an ordinary comment, or the name of a surrounding step does not satisfy the
  current predicate. No broad substring acceptance was introduced.
- This is package-declaration evidence only. No new Windows runner was started.

### Intent / spawn JSON diagnostics

- `tests/diagnostics_json_smoke.sh:778` retains the invalid `on: spawn DoWork()`
  source and the required nonzero compiler result. The changed assertions at
  lines 805–810 require exactly two diagnostics with exactly the two named
  codes, semantic/error classification, and location `{line: 11, column: 13}`.
  Order is deliberately not significant. Each named diagnostic must also retain
  its own layer, cause, fix, and message fragment. An extra third diagnostic,
  duplicate of one code, missing code, or swapped cause/fix ownership fails the
  Python predicate.
- These are independent source owners, not a test-only invented second error:
  `src/semantic/type_checker_intent_control.c:237` reports the forbidden Intent
  control construct; `src/semantic/type_checker_future_lifecycle.c:44` rejects
  an unowned spawn completion handle. `src/semantic/diag_codes.h` owns the code,
  cause, and fix spellings. `src/common/diagnostic_layer.c:141` classifies the
  Intent case as domain, while its semantic fallback at line 160 classifies
  this task-lifecycle case as type.
- The repair strengthens the inspected Python assertions; it does not suppress
  either compiler diagnostic. This lane did not run the full diagnostic suite.

### Semantic fixture frontier transport

- `src/self_hosted/semantic/diagnostic_owner.pgy:265` remains the declared
  frontier owner, currently returning 115. Its manifest at line 369 admits
  source files through the existing expected-verdict lookup; the frontier is
  not a recursive count of all `.pgy` inputs.
- `diagnostic_contract_owner.pgy:347` already compares the actual manifest
  count with that owner. The new CLI in `semantic_run_owner.pgy:20` transports
  the same owner value with `ToString`, without a second policy table.
- `tests/self_hosted/parity/semantic_parity.sh:73` requires a canonical positive
  decimal string and exact string equality with the parsed row count. This
  avoids arithmetic overflow/wrap and rejects zero, leading zeroes, negative,
  missing, multiline, and mismatched values. The caller at line 253 makes
  emission failure explicit, normalizes the Windows trailing CR, and does not
  fall back to the old literal 114 or a filesystem count.
- The existing manifest format/status/diagnostic-mapping checks remain before
  the final count check. Reusing the already compiled C manifest executable
  for C verdicts at line 393 preserves the same semantic owner; the inspected
  diff removes duplicate compilation, not the C verdict loop or LLVM leg.
- `compile_semantic_backend` now surfaces captured stdout when compilation
  fails and returns failure. This lane did not claim that compilation or the
  complete 115-fixture C/LLVM parity passed.

### Border registry and runtime twin checker

- The parser's exact `callable_contract_vocabulary.h` face is registered in
  `docs/154_border_registry.md` and excluded by fixed-string spelling in the
  existing parser-to-semantic allowlist. This matches the existing
  [Callable Contract Vocabulary](../semantics/callable_contract_vocabulary.md)
  contract: parser membership/canonical-order consumers ask the `.def` owner.
  `src/parser/parser_decl_clause.c:3` and `src/parser/ast_print.c:8` already
  include this face. No parser-owned vocabulary is introduced.
- `check_runtime_twin_include_boundary` now reads the actual extern owner
  `pgy_runtime_lib_authority_file_core.h`, not the obsolete missing filename.
  It requires its enumerated inputs, distinguishes grep's no-match status from
  an inspection error, and matches include directives instead of any filename
  mentioned in prose. The current comment at
  `pgy_runtime_panic_checked_inline.h:183` is correctly accepted.
- Quote/angle include controls, both crossing directions, and the current
  missing-input cases passed the existing checker self-test. The complete
  text-only border gate also passed in this lane.

## Initial findings and bounded follow-ups

1. **Initial finding: canonical inline twin presence was not pinned.** At the
   initial review snapshot, observed source:
   `tests/border_registry_smoke.sh:24` enumerates `pgy_runtime.h` and the existing
   `*inline*.h` matches. If `pgy_runtime_panic_checked_inline.h` alone disappears
   while another inline header remains, that absent twin is not an input and
   cannot fail the file-presence loop. The test named `missing_inline` in
   `tests/border_registry_checker_smoke.sh:20` instead omits `pgy_runtime.h` and
   still creates the canonical inline twin. This is a statically identified
   gap in the new mandatory-input evidence, not an observed production failure.
   Proposed focused follow-up: explicitly require the canonical inline twin
   and add the missing-twin/other-inline-present negative alongside the
   existing missing-umbrella control. Primary was notified; this lane made no
   implementation change or new mutation run.
2. **Exact JSON assertions require Python.** The pre-existing
   `check_json` fallback at `tests/diagnostics_json_smoke.sh:78` checks bracket
   shape and occurrence of quoted literals, not `len`, numeric locations, or
   per-diagnostic field association. Thus exact-two/error-attribution claims
   above apply to the Python path. The Windows job under review explicitly
   installs Python, so this is not evidence that the changed job will take the
   weaker fallback. Proposed separate hardening: fail explicitly when exact
   structured validation cannot run, or provide an equally strict owned
   validator. The no-Python branch was not executed in this lane.
3. **The border predicate remains a literal-line inventory, not a C
   preprocessor.** From its inspected regex, an include-shaped line inside a
   multiline block comment would still match, while a macro-based or continued
   include is not generally resolved. Do not promote this bounded smoke into
   a complete preprocessor/include-graph proof. These are source-level limits;
   no new comment/macro mutation was run here.

## Commands and observed results

All three invocations used `C:/msys64/usr/bin/bash.exe` with the configured
UCRT64/MSYS2/Git PATH and `timeout 60s` per invocation.

| Command inside Bash | Observed result |
|---|---|
| `timeout 60s bash tests/self_hosted/parity/semantic_fixture_frontier_owner.sh` | PASS, exit 0; matching owner frontier and missing/malformed/mismatched refusals |
| `timeout 60s bash tests/border_registry_checker_smoke.sh` | PASS, exit 0; comment-only references, both cross-includes, existing missing inputs |
| `timeout 60s bash tests/border_registry_smoke.sh` | PASS, exit 0; backend/stage/twin/AIR text checks including the checker self-test |

The border test's initial command yielded session `40803`; its terminal result
was then observed with exit 0. No timeout or unknown-session result is counted
as green. The checker created only its own ignored scratch fixtures under
`.tmp/border_registry_checker/`; this lane did not delete them.

`self_host_ci_profile_smoke.sh` was source-reviewed but not executed: its full
body performs `git init/add/commit/mv` in a temporary repository, beyond this
lane's explicit no-Git-write boundary. Full diagnostics, semantic C/LLVM parity,
compiler rebuild/bootstrap, package installation, and remote CI were not run.
These three local checker passes do not establish remote CI green, installed
driver currency, a new closed SoT row, or hard self-host substitution progress.

## Follow-up — canonical inline twin input

Base remains `f34355b37dbd9e86ef574399e895a78fd41dd0a3` plus the primary task's
uncommitted repair. This section supersedes only the open status of initial
finding 1; it does not rewrite the earlier observations.

- Primary reported that the newly added missing-canonical-inline/other-inline-
  present case first failed against the old checker with exit 1 and
  `accepted missing_inline`. That initial RED run was reported by primary, not
  independently executed by this review lane.
- Independently inspected current source: the checker now names
  `pgy_runtime_panic_checked_inline.h` as `inline_twin` and includes it in the
  mandatory file-presence loop. The existing wildcard scan remains for all
  other inline inputs; a surviving wildcard match cannot hide the missing
  canonical twin anymore.
- The current self-test has separate `missing_runtime` and `missing_inline`
  cases. It creates `pgy_runtime_other_inline.h` in every case, and omits only
  the canonical twin for `missing_inline`. Thus the new negative exercises
  the specific gap, while the umbrella/extern missing-input and both real
  crossing controls remain.
- Independently reran
  `timeout 60s bash tests/border_registry_checker_smoke.sh` with the same MSYS2
  environment: PASS, terminal exit 0, observed tool wall time 1.032 seconds.
  All six cases ran through the actual extracted checker. No compiler or
  remote CI was invoked.
- Primary separately reported the repaired complete border gate PASS, exit 0,
  in 2.617 seconds. This lane did not rerun that entire gate after the follow-up;
  its independent post-repair execution evidence is the focused self-test.

Initial finding 1 is resolved at this bounded checker level. The no-Python JSON
fallback and non-preprocessor nature of the include inventory remain as stated
in findings 2 and 3. This lane again edited only this report.

# Ownership decision reuse: observed validation

2026-10-24, bounded implementation follow-up to the duplication review.
Plan: [ownership reuse](../../agent_work_directives/ownership_reuse_2026-10-24.md).
This record does not close canonical ownership, self-host generality or CI.

## Source and executables

Baseline: `fdc661ff9bbc97033cc2a9615ab4ab3e0d7f5f99`.
Copy traversal commit: `71b54aebc9d5ca61d6f5bbdfd033a3f2c8fb190e`.
The region candidate adds only `llvm_mir_region_scope.c`, its internal API,
the Clone/concatenation consumers and `string_clone_region_unit.c` to that
copy candidate. The existing parameter-flow script and runtime remain
unchanged in the validation export.

Validation directory: `/tmp/pgy-ownership-reuse.oBISPV`, Ubuntu-C-WSL.
It is an export of the baseline plus this lane's listed changes, not a Git
worktree. Other workers' dirty String-landing changes are not in this export.
The unavailable Ubuntu-E-WSL disk was not repaired or modified.
The build uses GCC and LLVM 18; emitted artifacts use Clang sanitizers.

| Executable | SHA-256 |
|---|---|
| `bin/pgy-before` | `7168b145040cc79e8ff7a46583f363d6e21a9238c0b5e6f32ec6d0994b544c05` |
| `bin/pgy-copy` | `d5727069d54baaf773c643d7d4b35cb0bcdc3435fa0b62d34c729d5eb7221ef6` |
| `bin/pgy` (copy + region) | `966564d9283a7715d18e2088486591fa9c8625981c740d82c53db1a640c1cd88` |

No installed binary was replaced. No push was performed.

## Observed gates

| Gate | Result and evidence file in validation directory |
|---|---|
| Baseline native build + original region unit | PASS; `baseline-build.log` |
| Unchanged parameter-flow suite on baseline | PASS, exit 0; `baseline-flow.log` |
| Copy traversal unit | 2,670 checks, zero failures; `copy-build.log` |
| Unchanged parameter-flow suite on copy candidate | PASS, exit 0; `copy-flow.log` |
| Same suite with `PGY_VERIFY_FLOW_CURRENCY_MEMO=1 PGY_VERIFY_FLOW_WORKLIST=1` | PASS, exit 0; `copy-verify-flow.log` |
| Changed copy/move owners + unit, ASan/UBSan | 2,670 checks, zero failures, leak detection enabled; `copy-asan.log` and a direct rerun |
| Expanded region unit on original region implementation | 115 checks PASS; `region-before.log` |
| Expanded region unit on shared scope implementation | 115 checks PASS; `region-build.log` |
| Existing physical-release gate on baseline | PASS; `release-before.log` |
| Existing physical-release gate on cumulative candidate | PASS; `release-after.log` |
| Neighboring owner units | 13,431 readonly-map, 48 flow memo, 31 closure-worklist checks PASS; `owner-units.log` |

The copy unit compares all 64 undirected four-binding graphs, with alternating
edge directions, against an independent adjacency-matrix closure. Additional
checks cover nested/sibling/name-prefix fields, source-side preference, early
loan selection, overflow refusal and a forced row-storage relocation while
the traversal is live. Appended rows do not enter that traversal. Both actual
copy-change and indexed-assignment consumers are exercised.

The ASan/UBSan run instruments `default_aggregate_copy_owner.c`,
`default_aggregate_call_move_owner.c`, `default_aggregate_move_owner.c` and
the new unit; other frontend objects are the ordinary identified build.
It is not a whole-compiler sanitizer result.

Region faults run through actual Clone and binary-add emitters: inactive or
wrong scope, missing allocation, wrong function owner and missing runtime
remain errors without a heap call. Missing/unverified/nonmatching grants and
explicit heap disposition retain existing behavior. An Option's String
payload does not inherit the root call's region grant.

The release gate executes C with ASan/UBSan/leak detection and LLVM with
LSan. Both normal and early returns preserve expected output. Deleting the
emitted String-region cleanup and local collection cleanup is detected as a
leak on each backend. Escaping copies remain outside the region. LSan on
emitted LLVM is not full ASan instrumentation of every IR load.

Existing `ATOMIC_VAR_INIT` deprecation and LLVM target-triple override warnings
appear on baseline and candidate. They were not suppressed or called clean.

## Reuse boundary and handoff

The copy owner now owns edge traversal; callers retain their distinct
diagnostic/copy-selection and loan-conflict effects. LLVM scope admission is
shared in its existing region owner; operand coercion, nested-value eligibility,
runtime-call emission and existing consumer diagnostics remain where they were.

The project-local authoring skill now routes duplication/refactoring work to
`references/decision-reuse.md`: existing-owner search, policy versus target
representation, borrowed storage across growth, actual caller migration and
behavioral falsifiers. The skill validator passed under Ubuntu-C-WSL Python.
Its existing `.agents/` Git exclusion is preserved; this is local guidance,
not a new mandatory execution hook or proof of future compliance.

The new copy unit is a prerequisite of the existing parameter-flow Make target;
the expanded region unit keeps its existing `test-mir` connection. No source-
text presence test, safety exemption or skipped comparison was added.

Main owns verification after combining its separately dirty work, canonical
admission, bootstrap/gen2==gen3, installed C/LLVM driver and exact-SHA remote
CI. Neither those gates nor whole-language memory safety are claimed here.

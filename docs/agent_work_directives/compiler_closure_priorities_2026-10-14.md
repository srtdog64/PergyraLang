# Compiler closure priorities after text-scan removal

Status: PLAN / INTEGRATION CHECKED; compiler CI remains RED. Navigation only.
Base: `c08dfc6e9f6bcfd88163a2de35e232126a32621c`, shared `main`.
User request: integrate and check the cleanup, then fix the overall priority
order and prepare bounded parallel execution. Do not open all tracks at once.

## Objective card and finish line

- Objective: restore the actual full self-host compiler chain before adding
  new concepts or reopening unrelated SoT families.
- Priority: semantic identity, current owner facts, consumer migration,
  fail-closed behavior and executable negative evidence; then cost and elegance.
- Fact owner: the existing Pergyra ownership/body-bundle owners reached by the
  production source-to-C route. The semantic registry remains authoritative.
- Last consumer: admitted codegen, gen1/gen2/gen3 fixed point, installed driver
  and the compiler jobs which consume its checked artifacts.
- Forbidden: native fallback, empty successful facts, user annotation/copy
  inflation to hide missing inference, new owner proliferation, reduced
  correctness inputs, larger budgets or resurrected source/doc text tests.
- Acceptance: zero ownership refusals on the fixed DRV-2 input, unchanged full
  canonical program admitted, behavior positives and negatives preserved,
  bootstrap/installed-driver/dependent compiler jobs actually run green on
  the same published implementation SHA. A docs-filtered green is not this.

## Integration and current evidence

The cleanup is already committed and pushed: `13d93fe59`, `95262d972` and
`c08dfc6e9`. Root fast-forwarded to the last revision and checked the remote
main SHA; no duplicate cleanup commit or extra branch is needed.

Before integration, 560 dirty/untracked paths were SHA-guarded in
`C:/Users/user/PergyraLang_Backups/pre-test-removal-main-20261014-6743402465ce4c4aa0ce9b714836afda/`.
Manifest SHA256: `5C88FA33541FCDB9D673164170BC030732DD152710BB8C23CD4A6A7724D6DA70`.
The 13-overlap stash `c4ff38f295c811c3c8ac6afd113f3c2ec679ff12` is retained.
All 547 backup paths outside the incoming delta retain exact bytes/deletion
state. No unresolved conflicts remain. Unverified source WIP is preserved,
not silently included in this documentation checkpoint.

Root selected the incoming removed-text-test paths, rather than restoring
local additions to them. Actual execution/query/poison additions in the two
collection parity scripts are preserved as WIP; their removed generic-source
string scans are not restored. The formal kernel gate discovers proof files,
so the old explicit proof-list insertion is not reintroduced. The deleted
source-scan benchmark remains recoverable in the backup and stash, not live
progress evidence.

Exact CI [38418999774](https://github.com/srtdog64/PergyraLang/actions/runs/38418999774)
at the base revision completed RED:

| Boundary | Observed result |
| --- | --- |
| Windows, macOS C-only, TSan, Rocq | PASS |
| Codegen bootstrap | FAIL at gen1, `borrow_boundary_escape`, node107421, `unproved_exclusive_member_mutation`, `Array<String>` |
| Installed toolchain and six compiler-dependent jobs | SKIPPED, not acceptance |
| Native gen0, parser, argument graph, failure-status and nominal-array controls | Ran before the failed gen1 step |

The published Pergyra/stdlib program and bootstrap entrypoint did not change
in the cleanup. Its compiler failure is the same reached ownership boundary
as before, not evidence that deleting wording tests fixed compilation.

The report's 325-script comparison and 239 failures on both versions are
reported historical evidence. The 239 are not uniformly "not run": scripts
failed, often before their driver/Rocq-dependent behavior could execute.
Without their per-script receipts, do not independently claim every reason,
all semantic coverage or full green from the aggregate counts.

The explicitly retained security twins are `run_budget_twin_parity.sh`,
`run_cap_env_manifest_parity.sh` and `run_channel_twin_op_parity.sh` under
`tests/capability/`. Preserve the four tree-wide string hazards. Replace lost
semantic negatives with input-to-result checks, not symbol/file-list scans.

Root independently ran the committed archive, excluding shared WIP: memory
hazard lint, size-policy checker mechanics and all three retained twins pass
at the original60-second per-target limit. The registry schema/link check
also passes (95 authorities/201 derived carriers). This is not verification of
its CLOSED rows. An initial archive-only invocation failed because git grep
requires a Git context; replay supplied the unchanged repository index plus
the archive worktree. Both receipts and the source/archive guards are retained
under `.tmp/test-cleanup-base-check-20261014/`. No compiler or Rocq execution
is inferred from these six local checks; exact base CI above owns that evidence.

## Priority order and activation boundaries

| Priority | Work | Exit / activation condition |
| --- | --- | --- |
| P0 | Restore the one active bootstrap/DRV-2 ownership chain | Full input accepted; actual installed artifacts and exact-SHA compiler CI green |
| P1 | Close the SoT seams reached by that executable replacement | Producer and last consumer migrated, missing facts rejected, old path deleted, behavioral falsifier executed |
| P2 | Replay remaining public memory/lifecycle and C/LLVM contracts on the installed pair | Exact source/executable identity, success and refusal paths, reuse/drop/view/authority obligations remain visible |
| P3 | Advance the next production self-host substitution rung | Explicit C bypass deleted; Pergyra fact owner and installed-driver evidence, not readiness-only topology |
| P4 | Framework/GUI handoff and real-use refinements | Verified installed compiler and the GUI's actual inout/cleanup case pass; no compiler-to-Alrescha dependency |

P0 and its reached P1 seam are one active chain, not simultaneous independent
projects. P2 is not authorization to open every model or blanket memory-safety
claim. New GC policies, group-borrowing syntax, general caches, unrelated
optimization and hypothetical precision remain inactive. Optimize only a
measured operation which blocks the next named closure step within budget.

Two public SoT rows were downgraded to BRIDGE because behavioral negatives
are missing: `selfhost.expression_place_kind` and
`selfhost.statement_result_type`. Keep them open. Do not repair their former
text scans or close them from registry counts. Local
`semantic.function_param_flow_summary` migration is also unfinished WIP;
its historical CLOSED receipts do not admit the new consumer/publication path.

## Current P0 candidate: known chain, unknown decision

Published node107421 maps to compensation-row ArrayPush. The frozen focus17
candidate handles joint field restoration and failure exits. Its existing
two-owner HashMap receiver repair removes the earlier nominal-field type
loss; local query/source negatives and independent review support that slice,
not the whole candidate or installed compiler.

The fixed MIR control still refuses in the candidate at artifact
81942/digest1311964338, node67139, `unproved_formal_indexed_read_entry`.
The actual argument is source formal67100 (`ref inferred_types`) at ordinal3
of `SemanticAstCollectionFocusPartRepack`. The original permission facts have
unknown/retiring/descriptor site0, relevant at the loop entry; escape is absent
and this is not a deferred use. Mode4 here is source mode3 plus1, not a new
language mode. No permission or guard has been relaxed.

Required next dependency order:

1. Bind the candidate to the new base and re-observe the parser generation.
2. Execute the prepared v2 observation-only producer trace over the unchanged
   MIR control. Enumerate the original source67100 events and their callee
   formal/effect and sentinel provenance. It is not yet executed.
3. Distinguish missing readonly summary facts from actual element retention
   into returned metadata. `PartForLocal` and `PartRepack` are not interchangeable.
   Identity/currentness is not independent storage or lifetime evidence.
4. Decide one lifetime/transfer contract for the whole reached chain before
   approving production edits. List every caller/consumer and falsifier. No
   one-site copy, annotation or site0 exception to make admission appear green.
5. Run the matching source/query/retirement negatives, then the original full
   bootstrap, fixed point, breadth and installed-driver routes.
6. Publish only a coherent verified implementation unit; inspect same-SHA
   compiler CI. Remaining diagnostic count is UNKNOWN until a real census,
   not the historical 97 or one current first error.

The old 37.701-second static inventory result is historical only: the new
rules removed those text checks. Allowed source-size measurements remain,
but source-text presence must not be added back as closure evidence.

## Three implementation lanes, one integration owner

Activation: P0 source/event census and contract decision first. These scopes
prepare parallel work on that same chain; no independent successor tracks.

| Lane | Responsibility | Independent edit boundary after approval |
| --- | --- | --- |
| A | Existing formal-use/effect fact producer and its complete dependency closure | Only the named producer files selected after event census |
| B | Existing lifetime/publication and body/codegen consumers | Only disjoint admitted-consumer files; no alternate source of permission |
| C | Executable source/query/poison falsifiers and portable integration wiring | Dedicated runtime/query fixtures and targets; no production edits or text scans |
| Root | Integration, verification, commit/push, installed chain and CI | Shared final contract, selected diff and publication; resolves overlap before work |

The common integration gate is unchanged full `codegen_bootstrap.sh` followed
by the installed-driver/compiler jobs. Static60s, focused300s, integration1800s
and existing memory policy remain. Use one identified execution per unique
semantic target, attribute reuse explicitly, retain source/binary guards and
warnings. The external review chat reviews the frozen integration candidate;
review approval cannot replace execution.

No agent may commit/push, modify another lane's files, introduce fallback,
delete tests/data without scope, circumvent a denied action or kill shared
processes. Root schedules heavy builds. A newly reached owner/lifetime
dependency reopens the compact chain plan before the next edit.

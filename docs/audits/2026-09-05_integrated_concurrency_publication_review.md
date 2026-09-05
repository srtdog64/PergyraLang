# Integrated concurrency publication review — 2026-09-05

Status: focused native verification PASS; docs/204 summary/target corrections
independently verified. Primary subsequently corrected the historical syntax-
check record as noted below. This report is not a self-host closure or remote-CI
verdict.

Base: `b4d22cf2c6e68fcbd42a1ce4a44444de0e7899fa` plus the existing dirty work.
The user reopened the previously separate session's changes for integration.
Scope and authority follow
[Integrated Publication Review](../agent_work_directives/integrated_publication_review_2026-09-05.md).
Primary owns all shared-document/test corrections and publication; this lane
edited only this report and ran the permitted existing gates.

## Reviewed scope and owner reconciliation

Reviewed the scoped diffs in `docs/204_concurrency_direction_pscc_review.md`,
`docs/concurrency/Main.md` and `tests/async_model_positioning_smoke.sh`, and the
new `advanced_examples.md`, reconciliation audit, example runner and eight
`expected.stdout` files. Read all eight unchanged canonical source programs
and the relevant existing contracts and comparator consumer.

- The new guide separates native fixtures, contracts, and model-only/open
  combinations. It does not introduce executable pseudo-syntax, claim native
  examples as self-host substitution, or close the documented aggregate-Future,
  exceptional-cleanup, general capability-carriage, and resume-validation gaps.
- The `ConcurrencyPlan` clarification matches the current source:
  `src/self_hosted/mir/parallel_capture_fact_owner.pgy:3` defines
  `SelfMirParallelCaptureRows`, including `names` and `kinds` string arrays.
  An integrated all-ID plan is a target, not a currently completed single
  structure. AIR verification is not made a backend lowering stage.
- `Cancel` not retiring a Future and `await` owning completion join agree with
  docs/113's Future Await/Cancellation contracts and docs/05. The new guide
  does not equate cancellation request with task completion.
- docs/181 R3 and R4 own first-`give` selection and the fixed index-order left
  fold respectively. Neither implies unordered collection, first-success,
  quorum, or arbitrary floating-point reassociation.
- The ping-pong fixture genuinely needs interleaved communicating arms. Its
  source and the new guide correctly distinguish that requirement from the
  serial-equivalence claim for independent computation.
- `tests/compare_backends.sh:623` onward already lists these eight cases; at
  lines 583–593 it compares both backend outputs with a present
  `expected.stdout`. The new goldens therefore strengthen the existing
  registration, rather than inventing a parallel fixture inventory.

## Golden review and observed native execution

The guide's listed stdout matches the eight new files. The values also agree
with the existing source computations/comments; in particular the scheduler's
four integer vruntime terms sum to 34552 per half and 69104 overall. The any
fixture deliberately gives 42 in every arm, so its golden does not prescribe
which nondeterministic arm wins.

These traces remain bounded examples, not proofs of every related property.
For example, summing exact binary 0.5 values does not by itself distinguish all
possible Float reassociation orders; the fixed left fold is the separate owner
contract, not a general conclusion established by that one output.

| Canonical case | Exact stdout, `/` separating lines |
|---|---|
| `parallel_snapshot_read` | `42/1` |
| `parallel_disjoint_split_write` | `110` |
| `parallel_pingpong_witness` | `10/10/20` |
| `parallel_join_expr` | `204/182/2/72` |
| `parallel_join_stencil` | `33/33/33/0/0/100/103` |
| `parallel_join_reduce` | `204/24/3/-2/0/1` |
| `parallel_join_any_blocked` | `42` |
| `parallel_scheduler_showcase` | `8/0/4/4/4/1/34552/34552/69104/69104/1` |

The runner explicitly exports `PGY_NATIVE_PIPELINE=1`; the native driver at
`src/pgy_driver.c:260` consumes that opt-out. This is an explicit native oracle
test, not a retry after a self-host refusal. The runner hashes the native binary
and canonical source/golden pairs, uses one five-minute budget, rejects timeout
or missing tools rather than skipping, and retains its evidence.

Exactly one focused example invocation was made, with no compiler rebuild:

```text
PGY_BIN=/d/PergyraLang/.tmp/native_await_integration_20260905/pgy.exe
timeout 300s bash tests/concurrency_examples_smoke.sh
```

Observed terminal exit 0, session `93990`. Eight unique canonical programs ran
once per C/LLVM backend: 16 successes with exact stdout after CR normalization,
exit 0 and empty runtime stderr. Five existing source-negative fixtures produced
10 exit-1 semantic refusals with their owned diagnostic identifiers/text and no
requested `.exe`, `.c`, `.ll` or `.o` artifact. Those negative inputs were not
executed. Per-case source/golden hash checks and the final compiler hash check
passed.

Evidence: `.tmp/concurrency_examples.bhvG0K/receipt.txt` and its per-case logs.
The receipt records this base revision, `pipeline=native`, all source/golden
hashes, all positive/negative outcomes, `positive_runs=16`, `negative_checks=10`
and `exit_status=0`.

Independently verified the specified binary hash before and after the run:
`2E0468AAE6521FE804C08ABE679EAF3A166CA1624C208774EE2142D3C870EDCE`.
No self-driver rebuild or installation was performed.

The C compilation of `parallel_join_any_blocked` retains the existing unused
local `parked` warning and GCC notes about unrecognized
`-Wno-c23-extensions`/`-Wno-parentheses-equality`. They are present in
`parallel_join_any_blocked_c.compile.err`; successful execution is not a
warning-free build claim. No fixture or golden was edited to hide a warning.

## Documentation items sent to primary

1. **Propagate the `unordered` correction to the summary.** At the inspected
   snapshot, docs/204 §0 item 3 (line 40) still calls `unordered` an already
   existing join surface, whereas the newly corrected §2.3 explicitly explains
   that first-`give` is not unordered collection/first-success/quorum. The §1
   proposal-21 row should retain that distinction too.
2. **Use the exact reduction contract in summary rows.** The §0 table's
   "fixed tree" wording and §1 proposal-21 row can imply arbitrary fixed-tree
   reassociation. The current contract is the index-order left fold; clarify
   that in the short summaries as in the new advanced guide.
3. **Label the integrated plan as a target consistently.** §2.2 now accurately
   qualifies the all-ID `ConcurrencyPlan` as a goal. Repeat that qualification
   in §0 item 2 and §3.10's title/diagram introduction so the summary cannot
   imply a current completed all-ID implementation. This is wording
   synchronization, not a request to implement a new plan architecture.
4. **Correct the historical syntax-check description.** The reconciliation
   audit lists one `bash -n script-a script-b` invocation. Bash syntax-checks
   only the first script in that command; it is not evidence for both files.
   This review ran the two syntax checks separately and observed exit 0 for
   each. Preserve the old audit's historical checkpoint instead of rewriting
   its old binary/HEAD into the current run.

These items were reported before publication. This lane did not edit the shared
documents; their later acceptance/correction remains with primary. The native
run itself exposed no new blocking source/golden mismatch.

### Integration correction follow-up

After primary announced the correction, independently inspected the current
docs/204 diff:

- §0 and the §1 proposal-21 row now name the index-order fixed left fold and
  reject the general `unordered`/first-success/quorum equivalence.
- §2.3's heading and surface example now distinguish existing join forms from
  `unordered`, and describe `any` as the first `give`, not generic completion.
- §0 item 2 and §3.10's heading, introduction and diagram explicitly identify
  the integrated all-ID `ConcurrencyPlan` as a target that is not yet landed.
  The diagram introduction also qualifies Dynamic evidence and per-capability
  edge carriage rather than implying their complete implementation.

This resolves documentation items 1–3 at the inspected source level. Ran only
`timeout 60s bash tests/async_model_positioning_smoke.sh` again: observed exit 0
in 1.255 seconds. The summary corrections were verified by source review; this
positioning gate remains an adjacent document-boundary check, not a semantic
proof of docs/204. Scoped docs/204 whitespace checks passed. The already observed
16/10 native example run was not repeated.

At this follow-up snapshot the old reconciliation audit still displayed the
combined `bash -n` invocation. Primary stated that it will correct that record;
item 4 is therefore not marked resolved by this lane. The independently observed
separate syntax checks above remain valid current evidence without rewriting
the old checkpoint.

Primary integration note: item 4 is now corrected in the reconciliation audit.
The original combined invocation is explicitly limited to the first file and
the independently observed separate checks are recorded without rewriting its
historical compiler/HEAD. This note is primary's readback, not a new agent run.

## Other independently observed checks

With the configured MSYS2/UCRT64 PATH, all these separate invocations completed
with exit 0:

```text
timeout 60s bash tests/async_model_positioning_smoke.sh
timeout 60s bash -n tests/concurrency_examples_smoke.sh
timeout 60s bash -n tests/async_model_positioning_smoke.sh
```

The positioning gate passed in 2.212 seconds. It is a document-boundary marker
check, not a formal or runtime proof of every lifecycle combination.

Strict UTF-8, final newline and no trailing whitespace checks passed for all
14 scoped documents/scripts/goldens. All 45 local Markdown path targets exist;
this is path integrity, not semantic validation of every linked claim or URL
fragment. Scoped tracked `git diff --check` also passed.

External papers are presented as references, not Pergyra implementation
authority. This lane did not newly validate their external text, availability,
or reported research findings, and did not rerun Rocq/kernel checks or the
earlier reconciliation audit's adequacy test. Its historical CI observation is
not a fresh remote query or evidence for this dirty integration.

The test retained only its bounded small run. D: free space was observed at
92,286,976 bytes before and 89,047,040 bytes after the example run; no deletion
or budget expansion was used. No full matrix, self-host parity, full MIR,
bootstrap, Git write, installation, or remote publication was performed by
this lane. Primary owns exact-pushed-revision CI and Platform full validation.

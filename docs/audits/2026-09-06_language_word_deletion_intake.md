# Full-vocabulary deletion experiments: intake and bounded recheck

Status: REPORTED OBSERVATIONS + DURABLE EXECUTION MATRIX RECEIVED;
FULL SEMANTIC EQUIVALENCE OPEN.

Received from side task `01a071ee-7ebc-7a93-84af-5587c2bfb459` on
2026-09-06. Its reported source checkpoint was `da818c3d`; primary intake
started at `5653165b` and the six rechecks ran at
`91119c45683c10efdccaad18c95fdc2a9ebf0d41`. The intervening primary change
was the reviewed macOS checker repair, not a compiler implementation change.
This audit records evidence and an experimental method. It does not own
language semantics, registry status, self-host progress or the next compiler rung.

## Executed fixture follow-up — 2026-09-06

The later commit `16b2f894cec5e0478639d68e89a6fb98e9168dea` adds the
[execution matrix](2026-09-06_language_word_deletion_execution_matrix.md) and
`tests/concept_semantics/word_deletion/`: 35 experiment directories, 104 source
fixtures, a manual runner and its README. This fills the earlier absence of
durable source/runtime/two-pipeline records for these shapes. It does not
retroactively turn the earlier 112 in-memory LSP observations into these 104
fixtures, nor establish full-vocabulary or all-context equivalence.

Primary verified the committed fixture counts and the retained `public.json`
and `native.json` under `.tmp/self_hosted/word_deletion/`: each contains 104
records, with 34 differences in compile exit, run exit or stdout (`f8c047`).
This is a reconciliation of existing records, not a fresh full-matrix run.
The installed driver SHA-256 is
`7928ED6BE2D38A9C36FF6B09BA3F1BFCDAB3D4FEDD0B5CC006506F3788BBACFB`;
the native binary matches the full `0F9F4F30...` hash recorded below. Neither
binary is asserted to be rebuilt from the new documentation/fixture commit.

Primary independently reran `04_match_enum`, `15_caps` and
`35_subject_param_alias`: 11 programs on each of the public and native paths,
using separate fresh output directories and a 120-second outer bound per
pipeline (`080727`, `e0c041`). A separate comparison checked compile exit,
run exit and stdout against the retained records; all 22 matched, without
timeouts (`6bf836`). Both binary hashes also matched after execution.
Fresh results are under
`.tmp/self_hosted/word_deletion_primary_recheck_16b2f894/{public,native}/`.
Public acceptance of the missing enum arm and the two capability-bound
violations, and native/public subject-alias output `90`/`100`, reproduce.
These observations do not identify 34 independent defects or a successor rung.

The runner is an observation collector, not a regression oracle: it has no
expected-result comparison and returns zero after collecting results. It also
labels a timeout with `compile_rc=-1` and may print it as `REJECT`; a zero
compile exit with no executable can leave two missing runtime results that
compare `SAME`. Such outputs are not successful semantic refusals or runtime
equivalence. The 22 rechecks above use an explicit independent comparison;
the runner itself was not changed here and is not wired into CI.

Per-word coverage still needs the bounded evaluation contract below. The new
table names all 146 unique registry words, but names and nearby experiments
are not execution evidence for every word/context. For example, these fixtures
contain no `backoff` or `default` use, and only `sum`/`max` of the six listed
fold selectors were executed. The parser's shared timeout/backoff refusal is
a source-inspection fact, not a backoff execution. Higher-level table labels
must not expand the four bounded verdicts into universal deletion conclusions.

## Scope: no protected concept

The primary independently counted the current
`src/lexer/language_keyword_registry.def`: 146 words, consisting of 70 reserved,
73 contextual and 3 soft words. Every word and each of its grammatical uses
is a candidate. The registry remains the inventory owner; this report does
not introduce another maintained keyword list or equate a context mask with
completed implementation evidence.

Previous `KEEP-CORE` labels do not exempt Slot, Zone, World, Capability,
Subject, Ability or Intent. Semantic concepts without a corresponding
spelling, such as Scope, remain candidates too; do not invent a `scope` or
`capability` registry row to make the experiment fit the word inventory.

The earlier side report gave an initial classification of the whole list. That is not
146 completed deletion experiments. Its exact per-word classification table
and complete in-memory source pairs were not supplied as repository files.
The four existing `tests/concept_semantics` lanes are also not full-vocabulary
or all-context coverage.

## Evaluation contract

Keep four interventions separate:

1. Delete a spelling or clause while preserving facts through inference.
2. Rewrite the source using other current language syntax.
3. Encode the operation with current types, functions, generics and libraries.
4. Remove a compiler semantic mechanism or its admission rule.

A struct, enum, receipt value, graph or state machine implemented with current
language facilities is a legitimate replacement candidate. Merely storing
the same information does not disqualify it as a renamed concept. That would
make the non-expressibility argument circular. Instead identify any new
compiler-only rule, plugin or hidden semantic oracle the replacement needs,
and distinguish that change from an ordinary library implementation.

Compare normal results, observable effects and aliasing, failures, cleanup
and compensation order, lifetime, authority and attribution, rejection of
invalid programs before execution, and boundary costs. The diagnostic spelling
need not be byte-identical if its semantic distinction is preserved. Equal
stdout alone is insufficient. Sequentializing a parallel program or making
an async computation synchronous is not full equivalence.

Use these bounded verdicts, each tied to a source pair and tested boundary:

| Verdict | Required interpretation |
| --- | --- |
| Spelling removable / same fact retained | The tested inference or desugaring preserves the named fact; no semantic-mechanism deletion is claimed. |
| Behavior replaceable / guarantee or cost differs | A current-language encoding exists, with the missing rejection, alias, phase or cost explicitly recorded. |
| No same-guarantee replacement found in this search | A bounded negative search, not a theorem against all possible libraries or encodings. |
| Unverified / contaminated control | Missing execution evidence, unapplied rewrite, parser-only failure, or a confounding change prevents a verdict. |

Syntax errors after deleting a token, or a nominal-kind mismatch after changing
one declaration, do not prove non-expressibility. Convenience syntax can still
be justified by lower code and error burden without adding an independent
static guarantee; `for`, `loop` and `type` are not automatically deletion targets
for implementation merely because they have rewrites. No language word was
removed or compiler rule weakened by this intake.

## Provenance and evidence limits

The side reported a separate `bin/pgy-lsp.exe --native-pipeline` child using
stdio initialize, didOpen with an in-memory document at virtual URI
`file:///D:/PergyraLang/__side_virtual_probe__.pgy`, shutdown and exit. It did
not connect to an editor session. Its positive/type-error/immutable-field
controls reportedly distinguished the intended source diagnostics.

The reported batches contained 55 + 43 + 11 + 3 = 112 observation rows. One
rewrite did not apply and was not executed; a corrected later case was run.
These are source-admission observations, not runtime or backend equivalence
results. The side reported no repository edits, new fixtures, logs, build
artifacts or binary changes. Primary has not independently replayed those
in-memory LSP batches. The ephemeral task does not expose a turn-list through
the task reader, so this section preserves its supplied handoff as reported
evidence rather than claiming access to unseen transcripts.

Primary verified the installed binary hashes before and after its own checks:

- Native `bin/pgy.exe`:
  `0F9F4F30255D6850B5A773E21D5815F776B305E5C01A7A2C3DF6D373BB15A29E`.
- Native LSP `bin/pgy-lsp.exe`:
  `FA35CBF7B75D92B6FD422E1FD3B9957B89DA5E0B72C323568C4226E284AFC5FE`.

These match the reported binaries, but are not asserted to be rebuilt from
current HEAD. Primary ran no LSP child, runtime program or full matrix for
this intake. Its six existing-source native checks used separate bounded
children with `--native-pipeline --mir-json --error-format=json`, captured
stdout/stderr in memory and wrote no fixture, MIR, executable or log artifact.

## Primary recheck: six existing sources

Observed execution `a134ca`, exit 0 for the enclosing check. Each accepted
case published exactly one `pgy.mir.v1` document. The rejected case exited 1
and published none; unexpected exit, timeout or missing JSON was not accepted.

| Existing source | Actual observation | Scope |
| --- | --- | --- |
| `tests/concept_semantics/domain_axes/where_explicit.pgy` | Exit 0; `IntentZoneWhere=(CounterZone, Advance)` and `IntentZoneAlias=(counter_zone, Advance)` | The two exact named facts were checked. |
| `tests/concept_semantics/domain_axes/where_inferred.pgy` | Exit 0; the same two exact facts | Supports this omission/inference pair, not whole-program equivalence. |
| `tests/concept_semantics/intent/single_step_exact.pgy` | Exit 0; one MIR document | Single-step typed terminal accepted; no runtime replay. |
| `tests/concept_semantics/intent/single_step_rebuilt_reject.pgy` | Exit 1; `PGY_SEM_INTENT_STEP_INVALID`, exact admitted payload binding `receipt` required; no MIR | Same-valued `AuditReceipt(7)` reconstruction does not preserve this checked attribution. |
| `tests/concept_semantics/intent/function_rebuilt.pgy` | Exit 0; one MIR document | Ordinary value reconstruction is expressible. It does not promise the Intent terminal's exact attribution. |
| `tests/self_hosted/fixtures/domain_runtime_zone_copy_threadsafe_rejected.pgy` | Exit 0; one MIR document | This installed native source-admission path does not reject this Zone copy. |

The final row contradicts the blanket claim that every Zone copy is rejected
at source admission. It neither proves Zone removable nor identifies which
current self-host/runtime/backend boundary should reject the case. A filename
ending in `rejected` is not rejection evidence. Preserve this observation as
a boundary-specific counterexample and inspect the intended consumer before
diagnosing the whole mechanism.

## Other reported source rewrites: not primary runtime results

| Family | Reported rewrite/admission | Unproved boundary |
| --- | --- | --- |
| `move` / `transfer` | Both forms accepted in the same intent-step control. | Runtime crossing, effect and failure equivalence. |
| `loop`, `type`, local `mut` | `while true`, expanded type alias and omitted local `mut` accepted. | Every exit/context and contextual DX cost. Local and field mutation rules differ. |
| `action` / `func` | Simple state updates accepted both ways. Removing action-only clauses allows the function form. | Authority, zone, contract and attribution preservation. Parser refusal alone is not non-expressibility. |
| `class` | Read-only method replaced by struct plus free `ProbeRead` accepted. | Identity, aliasing, extension and module boundaries. |
| `vessel` | Mutation method replaced by struct plus an `inout` free function accepted. | Caller update, aliasing, returns and failure behavior. |
| `party`, `roster` | Simple shared fields/query and nested party-slot records replaced by structs/free functions. | Domain authority, identity, composition and open-world behavior. |
| `object`, `tobject` | Simple immutable reads replaced by struct let fields; object/struct writes rejected. A full struct + `Project` rewrite accepted where token-only replacement failed. | Refresh, source binding and projection semantics. |
| Dynamic role | Two fixed implementations replaced by enum mode + struct + match, both accepted. | Open implementations, multiple abilities, module coherence, include/override. |
| `inout` | Deletion around ArrayPush rejected; explicit new-array copy, return and caller reassignment accepted. | Allocation/copy cost, alias identity and failure boundaries. |
| `defer` | Cleanup copied to normal/return exits accepted, but a rewrite missing one cleanup also accepted. | Automatic cleanup guarantee and all exceptional/control-flow exits. |
| `transaction` | One compensation/failure case replaced by explicit state and compensation calls. | Nested/multiple failures and reverse-order compensation. |

Primary also checked the current source anchors for two surface facts:
`src/parser/parser_intent_step.c` routes `move` and `transfer` to the same
`transfer_from_alias` / `transfer_to_alias` fields; `parser_statement_dispatch.c`
constructs `loop` as a while node with a true condition. This static inspection
is not an execution replay of the reported source pairs.

The side reported actual lost rejections after removing these constraints:

- Subject copy rejected as `PGY_SEM_ANCHORED_HANDLE_COPY`; a class rewrite
  accepted the copy. An enum parameter rejected Int 2 while an Int encoding
  admitted that invalid alternative.
- Removing `where T: Sortable` admitted the formerly invalid generic use;
  removing an ability `fields length: Int` requirement admitted a missing-field
  role that otherwise produced `PGY_SEM_ROLE_CONTRACT_INVALID`.
- Slot parameters required explicit `own`/`ref`; these two modes are not
  interchangeable. Released Slot reads were rejected while valid controls
  were accepted.
- Structured task join/own-transfer controls were accepted, nested escape and
  post-transfer reuse rejected. Removing Future ownership or `async` produced
  the corresponding ownership/suspension diagnostics; a synchronous rewrite
  did not retain concurrency.
- Disjoint parallel access was accepted and overlapping slice access rejected;
  an `if true` sequential rewrite is not a concurrency-preserving substitute.
- Same-type participant substitution and capability/effect bound violations
  produced different semantic refusals. Removing the promise and obtaining
  acceptance is not same-guarantee replacement.

These reports motivate falsifiers, not a universal KEEP-CORE conclusion.
Normal library encodings remain eligible for a same-guarantee comparison.

## Controls, phase and context corrections

The initial free function named `Read` collided with builtin Slot Read. The
side renamed both originals and replacements to `ProbeRead` and reran them.
The initial class/party refusals are excluded from deletion verdicts. A
parallel deletion that left an invalid bare block was also excluded; the
corrected sequential control used `if true`, with concurrency loss explicit.

Local `let` reassignment and field `let` mutation are separate contexts. Do
not generalize either observation to all uses of the word. `give` is a parallel
body result and `between` is a relation-participant clause, not transfer aliases.
The current parser/registry distinguishes these contexts.

Predicate acceptance after rewriting `guard`, `pre`, `post`, `invariant` or
removing `expect` does not show phase equivalence. Primary read
`src/codegen/transpiler_intent_step_completion_emit.c`: completion checks
guard, expect, post, then invariant-post. The existing executable gate
`tests/self_hosted/parity/intent_guard_post_compensation_execution_owner.sh`
expects `first_guard.a_calls=1` and `first_guard.undo_a=1` on failure, as well
as ordered later compensation observations. That gate was inspected, not
executed here. Rewrites must retain predicate evaluation count, side effects,
failure phase and compensation timing instead of assuming guard means a
pre-action test.

## Remaining experiment record

For each word/context, preserve original, current-language replacement and
invalid control separately. Record the exact source/binary revision, replaced
mechanism, normal result, effects/alias, failure/cleanup trace, lifetime and
authority/attribution, static rejection, cost model, and observed execution
boundary. Reuse existing `tests/concept_semantics` runner/fixture conventions.
Retrieve the exact side-source pair or label a newly authored reconstruction
as new; do not claim that the in-memory batches are already durable fixtures.

Still open: C/LLVM runtime comparisons; matching self-host admission; module
visibility and innate/external implementation boundaries; open polymorphism,
role include/override; event subscribe/unsubscribe; relation/effect frontiers;
source-bound versus detached projections; pin/secure/pool/capacity behavior;
and phase/cost-preserving guard/expect/post/rollback/retry encodings.

This intake does not start parallel compiler implementation tracks, interrupt
CI, authorize literal keyword deletion, alter the SoT census, or increase a
self-host percentage. The main ability/enum compiler rung remains separately
recorded in `docs/current_work_handoff.md`; its earlier storage-choice blocker
was cleared by the user's bounded artifact-reclamation request.

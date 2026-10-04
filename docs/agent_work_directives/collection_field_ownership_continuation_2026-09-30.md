# Collection Field Ownership Continuation

Status: AUDIT COMPLETE; next implementation packet READY, not implemented.
Observations and implementation candidates only.
Base: `667f11ec01d361f1f1c17bbb3b0dab46de3c4190`, 2026-09-30, Asia/Seoul.
The live checkout had 64 dirty status entries before this packet. Preserve all
existing changes and recheck HEAD/status before the implementation turn.

This directive coordinates work; it does not own compiler semantics, registry
status, completion, or a successor self-host rung. Resume from the active card
in `docs/current_work_handoff.md`, then verify the current owners and gates.

## Shared objective card

- Objective: close the reached aggregate-formal field element-lifetime seam in
  `semantic.hashmap_collection_ownership`. A borrowed `Array<String>` field must
  not become eligible for deep release merely by extraction into a local;
  legitimate callable-table cleanup must still compile and release exactly once.
- Priority: stable binding/field identity and one owner; explicit producer-to-
  release proof; removal of the native UNKNOWN parameter exemption; negative
  ratchet and current-source C/LLVM parity; then patch size and cost.
- Fact owner: Pergyra collection ownership verdict, member-move identity, and
  transition owners. `src/semantic/collection_ownership_fact.c` remains the
  native bootstrap oracle, not an alternate final authority. The registry family
  stays ACTIVE until its full substitution conditions are executable.
- Production entrypoint: `pgy-self-driver --emit-mir-json-verified`, followed by
  the installed/public direct C and LLVM routes. Name the exact reached path in
  the implementation, rather than treating all collection cases as one closure.
- Direct bypass to delete: aggregate-current-parameter admission of an UNKNOWN
  member-moved array in `semantic_collection_admit_owned_string_drop`, together
  with the Pergyra formal-root identity gap that lets the same case escape proof.
- Last legitimate consumers: `SemanticAstExpressionFunctionTableFactsRelease`
  for `names`, `returns`, and `params`; its body-analysis and DRV-2 callers;
  source-C tables stay live through `CodegenAdmittedCViewFromFactsOrDie` and
  source intent admission; then the ownership MIR reader and direct C/LLVM
  cleanup projections.
- Forbidden fallback: type, field spelling, `inout`, `ok`, a same-name builtin,
  or a bootstrap admission promoted into element ownership. No blanket formal
  exemption, old/new dual read, second descriptor owner, or missing-fact success.
- Integration gate: extend and execute
  `tests/self_hosted/parity/collection_ownership_semantic_owner.sh` against one
  recorded current-source native/driver pair. Its carrier validation is a
  prerequisite, not an independent implementation track. Include the borrowed
  formal-field falsifier, legitimate callable-table cleanup, landed binding
  moves, and direct-MIR HashMap lifetime controls in this single acceptance slice.

## Verified starting boundary

The local move implementation is committed in `d2dfb686` and `3e15305a`;
direct HashMap lifetime work is committed in `667f11ec`. Current source permits
the tracked initialization moves and requires use-after-move refusal. The older
handoff's blanket alias-refusal contract is historical, not the current contract.
These source observations are not a fresh executable acceptance result.

Installed-only baseline at 21:45 KST used a unique run directory:
`.tmp/collection-continuation-baseline.unFHlE`. All four invocations of installed
`--emit-mir-json-verified` returned 0 and emitted MIR:

| Fixture | Current acceptance obligation | Installed observation |
| --- | --- | --- |
| `borrowed_string_array_shallow_copy.pgy` | Valid binding move | MIR emitted |
| `unknown_string_array_alias_without_drop.pgy` | Valid binding move | MIR emitted |
| `borrowed_string_array_move_use_after_move.pgy` | Reject moved source use | MIR emitted: acceptance mismatch |
| `collection_parameter_member_move_deep_drop.pgy` | Next borrowed-field falsifier | MIR emitted: seam remains exposed in this binary |

No unsafe output binary was executed. Installed `bin/pgy-self-driver.exe`
SHA-256: `8AB5C9D75040195862FC67407638AE44DAB53C2B65BEB7A98EA417D239FDB1B7`.
Installed `bin/pgy.exe` SHA-256:
`A3525D2C302A4912648C2C3984793B5BCDB2A10A674E242070F7961FE351860A`.
Neither this baseline nor the older private alias-refusal driver proves the
landed move/HashMap source. No fresh full ownership gate, fixed point, or CI was
observed for this packet.

## Independent review scopes

Root is the sole editor and integration owner. Two reviewers return read-only
findings; root records them under `docs/audits/`. Do not open parallel compiler
implementation tracks or edit another agent's files.

### Field proof review

Trace the exact callable-table producer, aggregate construction, formal field
extraction, and last release. Identify the missing binding/field fact, its owner,
and a smallest implementation candidate that rejects the borrowed fixture while
preserving this real cleanup path. Check whether the candidate silently infers
ownership from `inout`, type, or construction spelling. Return source locations,
one positive control, one falsifier, and unresolved obligations. No implementation.

### Integration gate review

Inspect the current semantic/carrier gate and landed binding-move/direct HashMap
tests. Separate installed-binary acceptance from a source-current backend probe.
Propose the narrow command sequence, artifact identity checks, and missing
negative cases needed for the shared gate. Inspect build prerequisites and test
run-directory handling; do not launch a Make target that publishes a driver or
clears an existing shared evidence directory. No implementation.

## Root implementation order for the next turn

1. Recheck revision/dirty state; bind every executable result to source and
   binary hashes. Establish landed move/HashMap acceptance using a private
   current-source pair; never substitute the old private alias-refusal snapshot.
2. Resolve the missing producer-to-field-to-formal lifetime fact or a genuine
   source-owned cleanup contract before enabling formal-root refusal. Keep pure
   fact computation in `func`/`struct`; add no decorative world/action layer or
   user-facing ownership syntax to hide a compiler proof gap.
3. Migrate the reached release consumers, remove the UNKNOWN parameter bypass,
   and add the borrowed-field negative to the shared gate. Preserve valid empty,
   MapKeys, Clone, member move/restore, and binding move cases, including moved
   source refusal and exactly-once C/LLVM release on normal/early exits.
4. Run the carrier prerequisite and shared integration gate. Refusals must not
   publish a new artifact or replace an existing sentinel artifact. A missing or
   forged field identity/provenance must not be admitted as OWNED.
5. Report source-current evidence separately from installed/public acceptance;
   preserve ACTIVE status for every family-wide obligation still open. Refresh
   the top handoff card with the next exact falsifier, not a general research queue.

## Commands, budgets, and forbidden overlap

Read-only review: `rg`, `Get-Content`, `git status/log/show/diff`, binary hashes,
and shell syntax checks. Do not execute unsafe fixtures. Root may use unique
temporary directories for bounded probes. A future gate run must use a verified
owned run directory; the current semantic script clears a fixed WORK_DIR.

Static gate budget: 60 seconds. Focused parity: 5 minutes. One integration shard:
30 minutes. If rebuilding exceeds an edit-loop budget, record the exact build
boundary and do not expand into a full matrix or parallel rebuilds.

No staging, commit, push, publication, installed binary replacement, full matrix,
registry CLOSED claim, broad UNKNOWN redesign, generic query/cache engine,
tooling extraction, or unrelated vision/world topology edit in this preparation
packet. Keep user-local compiler, cap, vision, fixture, and audit changes intact.

## Handoff result

Both read-only reviews completed. Root rechecked their material source/runtime,
last-consumer, gate, and build-publication observations and input hashes. Findings
and the unexecuted private-pair command proposal are recorded in
`docs/audits/collection_field_ownership_continuation_review_2026-09-30.md`.
The concrete implementation seam is producer-inout effects -> aggregate field/
return -> caller/formal lifetime -> deep-release requirement and empty writeback.
The current gate also lacks LLVM execution of the borrowed/UNKNOWN binding-move
direct probe and runtime coverage of the HashMap normal exit. Treat these as
acceptance obligations in the same slice, not new parallel implementation tracks.
No compiler fix or source-current green ownership gate is claimed by this packet.

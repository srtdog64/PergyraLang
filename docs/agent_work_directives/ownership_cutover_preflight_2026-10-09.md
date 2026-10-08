# Ownership cutover: four prerequisite closures

Status: `BASELINE FREEZE / CONTRACT PREFLIGHT`. Base HEAD:
`3658548d24bca3d721e4f1974ac7a10da99f7aa8`, shared main worktree.
2026-10-09 KST user authorization: resolve the ABI owner, multi-inout
normalization, Slice lifetime evidence and an approved local checkpoint
before starting the complete ownership-clean cutover. Local commits are
authorized for the reviewed gate inputs; push, official installation and
GUI delivery are not. This is coordination, not semantic authority.

Latest clarification: ABI terminology is chosen with the multi-output and
view contracts; production C/LLVM consumer migration stays in P2-P7, after
P1. The user confirmed all other shared-tree writers stopped. From checkpoint
commit until baseline gates finish, no gate input is edited by this task.
Only ignored/generated logs and receipts may be written in that interval.

## Objective card

- Objective: remove the four named prerequisite uncertainties without
  weakening the adopted value, write-through-view or cleanup contract.
- Priority: stable identity and one owner; executable issuance/checking;
  source/refinement preservation; negative evidence; then patch size.
- Fact owners: doc 27 and its importing proof layers; admitted builtin and
  runtime-call identity owners; the reached Slice backing/place producer.
- Last consumers: the call normalizer/ownership checker, then the existing
  C/LLVM/self-host projection routes. A model test is not their migration.
- Forbidden fallback: source-name or pointer ownership guesses, all-borrowed
  missing summaries, stale evidence, silently copied Slice, manual own/ref
  restoration, a second heap or cleanup machine, unrelated reset/stash.
- Verification: baseline ownership-clean extraction/kernel and documentation
  gates; focused success/refusal tests for each new owner; actual relevant
  consumer checks; final fresh kernel/semantic checks after proof changes.

## Whole-chain survey and dependency order

| Prerequisite | Existing reached chain | Required closure / falsifier |
|---|---|---|
| Baseline | shared sources/proofs/gate definitions -> local checkpoint SHA -> same-worktree gate inputs | Before source edits, review and commit the exact input set, recheck diff 0, record executable hashes and observed gates. Generated profiling output is not an input. A checkpoint is not CI PASS or complete P0 measurements. |
| ABI/value lifecycle | admitted builtin identity + limited runtime ABI rows + String/collection origin facts -> MIR -> C/LLVM/self-host | Preserve existing logical IDs; name one complete contract join and its input owners. Return origin and mutation obligations must be explicit; missing ownership facts refuse, not infer from layout. Enumerate all reached operations/consumers before editing. |
| Multi-inout | typed call/argument/place identity -> source normalization -> single-inout/value core + exits -> elaboration | Produce checked multiple recovery outputs plus independent result, ordered single evaluation, disjoint places and continuing exit recovery. Import the core; do not claim the existing one-output SCallIO covers this. |
| Slice lifetime | admitted Array place -> Slice borrow issuance -> reads/mutations/calls -> backing liveness/drop | Produce current-lifetime evidence and extend backing through all view uses. Preserve write-through and refuse live-view grow/reset/drop/transfer or stale issuance. A two-field descriptor is not ownership authority. |

Baseline comes first. The ABI and lifetime survey must fix the actual fact
issuers before normalization consumes them. If a discovered dependency would
require the entire P2-P10 migration, record that boundary rather than using
this preflight as implicit authority to start the full cutover.

## Scope, commands and integration

GPT is the single integration/editor owner in this worktree. No additional
agent/parallel implementation lane is opened. User-directed collaboration
exception 2 permits the necessary importing proof/contract work here. Existing
other-lane edits remain intact; drift of a gate input invalidates that run.

Use Rocq 9.3.0 via the project wrapper. Budgets remain static 60 seconds,
focused 5 minutes, integration 30 minutes; 3 GiB is not raised. Baseline
inspection may run independent read-only gates concurrently; never terminate
shared compiles by name. Add the owner/consumer edit inventory after the
source survey and before the corresponding source slice.

The shared final integration boundary is doc 27 §5.10 and this preflight's
explicit evidence receipt, not a new self-host rung. Preserve the full plan's
eight landing gates and full-cutover hold. Report model closure, producer
implementation, consumer migration and production validation separately.

## Observed starting checks

Fresh `tests/ownership_cleanup_smoke.sh`: PASS with Rocq 9.3.0/Stdlib 9.2.0,
five kernel-checked extraction modules, zero assumptions; decision, refusal,
summary, place/view and exit controls passed. This is model/extraction only.
Fresh documentation-quality gate and `git diff --check`: PASS. Existing CRLF
normalization warning in doc 19 is preserved. Fresh full kernel: 72 modules
plus approval export/binding consumer PASS; only the two approved Slot
abstractions, no admits/unsafe kernel features. Existing masking/load-path
and nested-induction warnings are not suppressed.

The checkpoint input set covers changed tracked files and non-ignored new
source/proof/test/docs/workflow inputs. `.tmp` and `gmon.out` are excluded.
These shared changes are recorded as existing WIP, not reviewed feature
completion. Full compiler-scale P0 memory/native/installed/CI evidence remains
separate and must record reds or omissions instead of substituting this gate.

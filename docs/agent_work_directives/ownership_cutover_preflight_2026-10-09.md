# Ownership cutover: four prerequisite closures

Status: `BASELINE FIXED / BOUNDED PROOF REVIEW; FULL P1 OPEN`. Base HEAD:
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

## Fixed baseline and production-implementation boundary

Checkpoint `a75da80435e0d051f71cfacf76d44ea825ed87d7` records 456 reviewed input
paths. The user-confirmed freeze covered commit through baseline execution;
tracked inputs remained diff 0. Only `gmon.out` remained untracked. Fresh
post-commit cleanup extraction, full kernel and documentation gates PASS.
No push or installed artifact replacement. The baseline is reproducible
source identity, not a green full P0 matrix or a memory go/no-go verdict.

Latest user clarification fixes ABI decisions rather than migrating production consumers at this stage. Doc 27
§5.10.1–3 owns the coupled output/origin/lease vocabulary and the bundle/error
ordering. No compiler/runtime source is edited in this preflight. Necessary
importing proof work remains in scope under exception 2; production consumer
migration is deferred to the transition. A design decision is not a checked
theorem or a production lifetime issuer.

## Importing proof edit inventory (fixed before source edits)

The reached core is value semantics with a single recovered call result;
`OwnershipCleanExits` carries exit-specific live sets over that same machine.
`OwnershipTeardownAuthority` protects lifetime, not mutable access. Keep these
owners unchanged and import them, rather than constructing a second heap.

| Edit scope | Producer / last consumer | Acceptance and remaining boundary |
|---|---|---|
| `OwnershipCleanCallRecovery.v` | alias/freshness-refusing ordered bundle/call/unpack and one recovery adapter -> importing INV/CORR theorem | Normal/early-return/handled-error recovery includes all inouts plus independent payload; value-level error dispatch follows restoration. Caller-scope membership/private-name, body loop-exit and output shape/tag checks refuse malformed inputs. Complete/current scope issuance, ordinary core callee/caller lowering, language type schema, pointer ABI and source expression/place refinements are reported separately. |
| `OwnershipCleanViews.v` | current-footprint dynamic ghost oracle -> local scalar view write and guarded backing drop | Scalar write-through preserves footprint, INV and source CORR. The dedicated drop consumer refuses active views; end frees no backing. Lease vocabulary is reused without graph-ID casts. Static final-MIR currentness, linear evidence/snapshot binding, whole-instruction view frame and general payload/glue remain OPEN. |
| `tests/coq/OwnershipCutoverPreflightAudit.v`, focused smoke | independent success/refusal consumers of the new propositions and functions | Wrong output shape, repeated outputs, escaping loop control, readonly/out-of-range/stale view and active-view retirement falsifiers. Fresh kernel check, no admitted assumptions. |
| formal inventory/docs/receipt/handoff | actual proof scopes -> navigation and validation registration | No existing SoT status broadened; no proof count as implementation progress. |

GPT owns these edits in the frozen shared worktree. Claude co-review is
read-only. One integration gate: `tests/ownership_cutover_preflight_smoke.sh`,
then the full kernel and formal-semantics gates. Compiler/runtime/backend,
the four existing cleanup cores and teardown authority are outside this edit
inventory. Revisit the plan if a new dependency would change those owners.

Next dependency order: importing multi-output/continuing-outcome refinement
and write-through lease/frame refinement together -> admit the joined call
contract -> type/allocator/glue gates -> production facts and C/LLVM/self-host
consumer migration. No P2-P7 consumer work before the relevant P1 gate.

## Final scoped integration (observed, 2026-10-09)

Receipt: `../audits/ownership_cutover_preflight_2026-10-09.md`. Local baseline
remains `a75da804`; 15 importing docs/proof/gate changes are uncommitted and
`gmon.out` is preserved. No compiler/runtime edit, installation or push.
Final focused kernel PASS (seven modules, zero assumptions); formal/kernel
PASS (75 modules plus approval consumer, only the two existing Slot
abstractions); SoT structural/documentation/shell/diff checks PASS. Final
75-source hash matches and strict UTF-8 on 15 inputs PASS. Two actual read-only
Claude CLI reviews ran. The second's Low follow-ups were strengthened by GPT
and independently rerun, not approved by a third review.

This closes the baseline method and bounded importing slice, not the four
items' complete production refinement or full P1. Actual callee/caller
lowering, complete/current scope and static lifetime issuers, whole-instruction
view frame/effect completeness, source place/expression and physical ABI/glue
obligations remain as recorded in doc 27 §5.10.4 and the receipt. Do not start
P2-P7 consumers from a model-only green result.

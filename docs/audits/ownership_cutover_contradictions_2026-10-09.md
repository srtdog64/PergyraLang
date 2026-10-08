# Ownership cutover: documentary contradiction review

Status: `DOC REPAIR COMPLETE; IMPLEMENTATION HOLD`.
Date: 2026-10-09 KST. Base HEAD:
`3658548d24bca3d721e4f1974ac7a10da99f7aa8`. Entry shared tree: 442 dirty Git
entries, zero staged. Final read-only Git snapshot at 00:39:33 KST: same HEAD,
443 short-status entries (456 with untracked paths expanded), zero staged.
Shared work is preserved. This is an observation/proposal receipt, not a semantic
owner, new self-host rung, implementation approval or closure evidence.

## User scope

Latest direction: "우선 모순쪽만 정리해놔 바로 구현은 마지막 체크한번 더 하고".
It supersedes immediate implementation. Only documentation reconciliation and
read-only final review are authorized here. No compiler/runtime/proof changes,
bootstrap, installed-binary replacement, commit/push or GUI message.

## Reconciled contradictions

| Conflict | Documentary resolution | Executable obligation still OPEN |
|---|---|---|
| Atomic landing versus skipping all intermediate red gates | One public landing, focused dependency gates before next consumers or old-owner deletion | Complete native/self-host/C/LLVM cutover |
| Preserve old semantic owners versus member inout/readonly temporary goals | Preserve type/identity/exclusivity facts, replace old variable/named-only and source-own-only policy | Safe rooted places, single evaluation and temporary admission |
| Core's one inout result versus real multi-inout/independent results | Explicitly limit `SCallIO` proof and require checked normalization | All continuing exit recovery and ordered call/short-circuit evaluation |
| Heap/static/region versus actual ownership | Separate storage class, result origin and backing owner; Slice is a write-through view | View-aware liveness and getter/mutation provenance |
| Whole-container final drop versus overwritten/discarded payloads | Per-operation lifecycle postconditions alongside deep copy/drop glue | Replacement/pop/remove/clear/grow/failure C/LLVM tests |
| I4 inference-before-propagation versus doc 27 | Normalize, propagate, infer; final-generation analysis before elaboration | Def/use rewrite invalidation and native/self-host certificate producer |
| ABI row CLOSED versus duplicate BRIDGE prose | Live table status unchanged; older bridge narrative marked historical, no new metadata inherits closure | New ownership ABI columns/consumers/negative gates |
| All landing gates green versus pre-existing reds exempt | Mandatory landing/DRV-2 failures cannot be excepted; unrelated exact baseline failures remain explicit failures | Fresh baseline and full landing execution |
| No push before I8 versus I8 exact-SHA CI | Authorized candidate snapshot/branch CI precedes final main landing | Commit contains all validated inputs; actual same-SHA CI |
| Installed driver needed before landing versus no official install before final review | Actual installer/launcher in isolated prefix before landing; official install/recheck only after approval | Confirm isolated-prefix support; standalone exe is not installer evidence |
| Zero manual calls versus retaining forbidden-call negative fixtures | Zero calls in admitted ordinary programs; exact negative manifest exceptions with no-artifact oracle | Migrate each old safety property instead of deleting tests wholesale |
| Manual deep-drop fixture flips positive versus builtin retirement | Positive successor has synthesized cleanup, retired call stays in a separate refused fixture | Actual static-to-owned String and inout cleanup behavior |

HashMap correction: existing String-value drop paths free values as well as
keys. General nested/nominal payload glue and emitter migration remain open.
Do not describe all current maps as keys-only or discard the working path.

Doc 27 §5.10 records production refinement obligations OPEN, not new checked
theorems. Separate graph CL2–CL4 work is not an invented prerequisite for
ordinary-value cutover. Existing 3 GiB cap, one worktree and dirty-change
preservation remain unchanged.

## Observed checks

- `tests/documentation_quality_smoke.sh`: PASS (60 s budget).
- Strict UTF-8, replacement-character, local Markdown-target and text-hygiene
  checks over all eight edited documents: PASS, 48 local link targets.
  Intentional two-space Markdown hardbreaks are retained. These checks and
  documentation-quality were rerun after the last substantive review repair.
- `scripts/sot_registry_gate.py /mnt/d/PergyraLang`: PASS; existing 95 owner
  rows and 201 derived carriers. This checks live structural edges/status,
  not runtime semantics, ownership-clean implementation or fresh ABI closure.
- Scoped `git diff --check`: PASS for tracked edited files. The explicit
  eight-document text check also covers the untracked owner documents;
  Git's diff check does not cover them automatically.
- The four ownership-clean core source hashes are unchanged:
  - Core `57c55889218b1f27075105d21573eb060b1709bdae2aacae756e7729c2ad15ce`
  - Composition `3bb896b33bd8e7a50724d94beb76550e171a8de6fbdb152fbaff42b7c24cb85f`
  - ReadOnly `ff572b832a37ec410607657bb4c0da88f6e22b3aa0cda1217894293b9456e1b7`
  - Exits `0258fb65e1e3102b02ff3b3b9d5c779ec36b4cd43af494c0e1ee496a96070d36`

No fresh Rocq compile/kernel, native C/LLVM, sanitizer, bootstrap, actual
peak-memory, installed-driver or remote CI run is claimed by these checks.

## Claude review and next boundary

The installed Claude CLI completed its first read-only review: terminal JSON
`subtype=success`, `is_error=false`, duration 368827 ms, session
`26fa2837-a4aa-42e4-b48a-1ef3436168a4`; configured model reported
`claude-opus-5-5[1m]`. Only Read/Grep/Glob were available; hooks disabled and
empty strict MCP configuration. No source/proof/test edits or compiles.
Prompt/raw result: `.tmp/ownership-cutover-review-2026-10-09/claude-review*`.

The review reported four High documentary defects, thirteen Medium items and
five Low groups. All are addressed as documentary repairs or explicit OPEN
pre-start/refinement decisions, not as implemented fixes:

- H1: verified the actual Allocator/TextBuilder-only join and Slot ABI scope;
  collection/String owner declaration is now OPEN rather than falsely present.
- H2: dirty diagnostic versus approved zero-input-diff SHA evidence separated;
  ancestor/WIP publication authority, baseline binding and final-SHA rechecks
  explicit. Same worktree remains the default; no reset or extra worktree.
- H3: actual Active self-host card points at the current plan/directive and
  2026-10-09 hold; superseded work-split is historical.
- H4: I5/G migration now retains retired-call negatives and creates manual-
  call-free positive successors rather than flipping them wholesale.
- M1–M3: required reachability/DRV-2 failures are not unrelated exemptions;
  baseline allowlist cannot waive required CI; isolated-prefix support is
  a pre-start check, not implicit tool implementation in P1.
- M4–M6: source establishes current behavior, not target semantics; OPEN
  production proof/check obligations linked; source normalization and target
  `cleanup_equiv` are distinct relations.
- M7–M9: view core boundary, absent generation issuer, live-view mutation/
  transfer negatives and observable D1 cost recorded; ordinary-temp repair
  cannot remove Slot/subject named-boundary guards; I2 includes origins/views/
  postconditions and missing-fact refusal.
- M10–M13: backend disagreement must unify before a common row; existing
  HashMap release seam is not denied; tombstone/diagnostic selection is OPEN;
  continuing failures and terminal panic/abort have separate evidence policies.
- Low groups: historical numbers/line references labelled, stale section names
  clarified, one normalization coordinator/order, one eight-condition landing
  owner with unconditional final kernel gate, official install failure/recovery
  policy recorded.

Additional source observation: `.github/workflows/ci.yml` push trigger is
main-only; candidate branch publication alone does not start CI. Existing
workflow_dispatch/approved PR ref, required full profile and external authority
must be confirmed before that later action. No CI request was made here.

The second read-only review also completed: terminal JSON `subtype=success`,
`is_error=false`, duration 175593 ms, session
`ad363d97-e651-4b20-bed4-f568fc873e52`. Raw result:
`.tmp/ownership-cutover-review-2026-10-09/claude-rereview.json`.
It confirmed the first four High contradictions were repaired and the other
groups repaired or explicitly OPEN, without inventing proof or completion.

It left two Medium documentary issues: no authorized way to obtain a formal
zero-input-diff P0 baseline, and one residual general-value ABI owner phrase.
GPT subsequently marked the formal baseline BLOCKED pending an approved
snapshot method and replaced the residual phrase with the P1-designated
collection/String owner. Six smaller wording/coordination issues were also
reconciled: pass placement, OPEN Slice lifetime issuer, continuing-failure
scope, consistent installation blocking, historical line citations and the
user-directed collaboration exception. These last repairs received GPT's
read-only source/text checks, not a third Claude review.

Additional source-level preflight observations:

- Makefile `BIN_DIR`, `PGY_BIN` and `PGY_SELF_DRIVER_BIN`, the build script's
  path-bound output stamp, the launcher and installed-driver probe expose
  candidate path injection. This narrows the unknown; a full isolated
  installer/bootstrap/launcher route has not run and remains an obligation.
- `scripts/ci_change_scope_owner.sh` classifies an unavailable base with a
  valid head as full execution, not markdown-only. No remote event/ref/profile
  was executed. A PR merge SHA is not assumed equal to candidate HEAD.

Implementation remains held. Documentary reconciliation and two completed
reviews do not approve implementation or close P1. Formal baseline snapshot
authority, actual collection/String ABI ownership, multi-inout source
normalization refinement, Slice lifetime issuance, mutation/drop glue,
isolated installation and actual same-SHA CI remain unresolved executable
or authority obligations.

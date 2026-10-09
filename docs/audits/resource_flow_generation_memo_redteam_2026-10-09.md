# ResourceFlowUniverse memo-generation review

Observed: 2026-10-09 KST. Base `main @
a75da80435e0d051f71cfacf76d44ea825ed87d7`, with the preserved 30-path dirty
snapshot. Status: `SCRATCH REPAIR VERIFIED / PRODUCTION MIGRATION OPEN`.
The user's "계속" continues the active cutover, not a new memory architecture.

## Claim, owner and reached consumers

A memoized stable index must resolve the retained symbol's CURRENT declaration.
The owner is ResourceFlowUniverse, not its Symbol epoch/index cache. Function
analysis starts that owner; direct bind, snapshot, nested declaration and
function seal issue the rows. HIR attaches them to the source routine;
DIR does not duplicate this fact family; RIR copies its declared identity;
MIR validation/serialization is the last measured consumer.

The existing declaration-entry matcher already compares kind, syntax
identity (or the legacy source-position boundary) and name. Symbol resolution
uses that matcher, but the bind memo fast path checks only epoch and index
bounds. Thus the fast path can bypass its own authoritative identity owner.
The scratch objective/change boundary was fixed BEFORE editing:
`.tmp/ownership-cutover-2026-10-09/generation-redteam-2653728bc3254a67aa6318d063601641/objective.md`.

## Executed local counterexamples

The fixture keeps valid, live Symbol/Scope allocations and uses the real
candidate's frontend objects. It does not dereference freed storage, mutate
production input or terminate shared workers.

| Controlled case | Previous candidate | Repaired candidate |
|---|---|---|
| Ordinary same-context re-entry | PASS: indices 0/1, two correct sealed rows | PASS |
| Fresh context, retained symbol memo | FAIL: both values resolve to index 0, snapshot disagrees, only one sealed row | PASS: distinct indices, correct current resolution, snapshot and both sealed rows |
| Same-context boundary epoch reuse | FAIL: identical wrong identity after SIZE_MAX wraps to 1 | PASS: current declaration identity still distinguishes the values |

This confirms a binding-API boundary defect, NOT a reproduced source-level
exploit or demonstrated production reachability. Epoch exhaustion is forced
in the fixture; practical time-to-wrap and a production path reusing a live
symbol across fresh contexts were not established. Reference fixture command
exit is 1 with two failed cases; repaired exit is 0 with zero failures.

## Scoped repair and forbidden alternatives

Only a newly named scratch universe owner changes: validate a memo hit through
the EXISTING declaration-entry matcher before accepting it. On a mismatch,
the same owner performs its normal lookup/bind. The memo is an acceleration
hint, not a second semantic authority. No new identity issuer, global reset,
cleared-symbol workaround, summary default or downstream dropped fact.

The previous CLASS/TYPE_PARAM value-admission repair remains unchanged in its
universe and snapshot/declaration/seal producers. No function signature,
MIR/JSON schema, value semantics, manual annotation or cleanup policy changes.
Production `src`, proof definitions, official binaries and registry status
remain unchanged; this is NOT automatic ownership cleanup or SoT closure.

Snapshot objects carried through universe retirement, concurrency, allocation
failure and physical call/view refinement are not covered by this memo
fixture. Reached context-restore/loop consumers operate within their existing
function-scoped lifetime; this patch does not certify arbitrary stale-frame
restoration. Preserve that boundary rather than claiming universal reuse safety.

## Observed verification

Scratch directory: `.tmp/ownership-cutover-2026-10-09/generation-redteam-2653728bc3254a67aa6318d063601641/`.

- `reference-probe.log`: one PASS, two FAIL, command exit 1.
- `repair-probe.log`: all three identity/snapshot/seal cases PASS, exit 0.
- `admission-v3-repaired.log`: existing kind/snapshot/nested/seal admission,
  same-context generation reuse and missing-owner refusal PASS.
- Native unit batteries, observed output: semantic 3107/0, HIR 26/0,
  DIR 15/0, RIR 26/0, AIR 147/0, MIR 217/0.
- Existing ArrayDrop gate, explicitly `PGY_ARRAY_DROP_STAGE=native`: C/LLVM
  positives and preserved-artifact/diagnostic refusals PASS. Evidence:
  `.tmp/self_hosted/public-array-drop.R3sNyJ/`. Public/self-host/MIR-input
  lanes were NOT run; the gate's all-stage capability text is not their result.
- Fresh focused ownership preflight: nine kernel-checked modules, zero
  declared abstractions/admits/unsafe features; importing model only,
  production refinement OPEN. Documentation quality and direction self-test
  PASS; the latter rejects both residue matches and search-tool failures,
  and remains structural rather than execution evidence.
- Same complete import-composed `driver_bootstrap_main.pgy` input, unchanged
  3072 MiB threshold and 1800 s budget: exit 0, 50397 ms, observed peak
  private 1319.2 MiB (1.288 GiB), 56 samples. 2542 declared endpoint bindings
  unchanged: previous 2535 inputs plus four local source/directive inputs,
  command/probe and separately bound previous compiler.
- Full MIR stdout: 378,143,208 bytes, SHA-256
  `52290d05634c1a95e81d10c3804f7745cd6d05f3fb6b18edbe7252a92985b402`,
  byte-identical to the preceding complete candidate output. Capture complete;
  0 semantic errors, 21 existing warnings. No speedup/heap-counter claim.

Identities:

- old candidate `0daa8b6b9818b9d9d137ca56d53a33127fef811be92b156b0cf673e6c3e60531`;
- new native candidate `5377a5e09353dbf019e04efcf5406cc60eb3da3e9151542883ba9ff40d3872fc`;
- scratch owner `fa13d06778b50ebdfe23bb021167a8cfcdca4fbaee83a1fc6cc472a09be672c6`;
- fixture `e9079ad7ae5e21738c7ed06ff28ef76a60313b2e07c318ab1a8575999243e30b`.

The first build command found no mingw32-make on this host; no semantic test
ran. The existing MSYS2 make then built the reference successfully. These
operational failures are not counted as identity counterexamples.

## Next boundary

Fresh all-writer freeze confirmation is still pending. Preserve a reviewed
second checkpoint, run its actual P0, then migrate value admission and this
validated memo guard together at their existing production owner, with
permanent importing negatives and the same fixed compiler input. Do not
delete old consumers or mark a registry row CLOSED from the scratch result.
No commit/push/install/remote CI/GUI message in this scope.

Final snapshot: 31 short-status paths, staged 0, 30 strict UTF-8 text inputs
plus preserved/excluded `gmon.out`. Native Git diff check PASS; no production
source or workflow diff. Official binary hashes remain
`f6559da9...1c9` / `707dcd40...78ec7`. No input-freeze confirmation arrived
during this scoped scratch investigation.

# 2026-10-21: native MIR iteration cleanup, bounded candidate

Recorded by GPT root. **Task A is not wholly closed; Task B has no deletion
delta.** The no-drop native reproducers now work without weakening the
empty-birth rule. Declared-call publication, the installed consumer and
canonical admission remain open. No independent part review or CI-green
claim is made.

## Source and executable

- Main fast-forwarded from `7cd004fab` to
  `fa66a2fb231bb6b5e1f15e0ffa8a2f175f89194d` before this patch. This contains
  Claude's `914b3bbb0` source and its MIR task handoff.
- Build copy: WSL `Ubuntu-E-WSL -u root`,
  `/root/gpt-loop-local-20261021-A`, a private non-Git archive copy.
  All **25 changed/new task source files** matched D: byte-for-byte before
  the final runs. Semantic sources and the Claude-owned gates were not edited.
- Release `bin/pgy` SHA256:
  `24aa8fa992a5c6b367042b0110119b21ba406db185365f5e603b99ec82994fe0`.
- Release `bin/test_mir` SHA256:
  `a2cfc305f64029b6add98fbdab365ff3c4b2fa0840d38c228f498699465a158a`.
- Sanitized `bin-loop-san/test_mir` SHA256:
  `56b1b8a70274e12b3c639852b406fcc0687cc5874a1b9a276f7390b3e82b50f3`.

## Chain implemented

[Decision](../decisions/2026-10-21_loop_local_array_cleanup.md).
HIR seals the source loop identity/exit with its execution witness. Existing
empty-Array ownership facts and SSA definitions select one incarnation.
The MIR lowering consumes the existing synchronous builtin retention
registry and scalar/Void result facts; it grants no cleanup to aliases,
transfers, deferred uses, unknown declared calls or explicit releases.

It issues one MIR cleanup fact per actual normal exit, including continue,
break and return. Validation rechecks coverage, binding, SSA storage, glue,
edge and liveness operand; predecessor dataflow sees retirement only on
that validated edge. UNBORN/RETIRED birth admission is unchanged. C and LLVM
consume the same fact after evaluating the condition/return result. Existing
AIR binding currency includes the new cleanup identity. The JSON emits the
fact; the legacy scalar reader refuses it rather than silently discarding it.
That reader refusal is source-reviewed, **not runtime-validated** below.

The authoring skill kept the fixtures as ordinary functions with no manual
cleanup in the positives. Production user source gained no own/ref annotation,
manual drop, carrier restoration or copy workaround.

## Observed gates

Logs have prefix `/root/gpt-loop-local-20261021-A`.

| Gate | Result / log suffix |
|---|---|
| Original plain/owned-String loop, explicit-drop control | all native MIR admit; C/LLVM print `3`; `.focus.log` and `.tmp/loop-local-baseline/` |
| New integrated loop gate | PASS C/LLVM; `.release-focus.log`, `.tmp/loop-local-collection.9ros5l/` |
| Gate behavior | early return, continue, break, nested loops and explicit drop; field escape and deferred use refuse with no output artifact |
| Output parity | positive `1026`, explicit `3`, isolated ownership case `ok`; all three C/LLVM stdout files also passed `cmp` |
| MIR unit | **691 passed, 0 failed**; `.release-mir.log` |
| MIR ASan+UBSan unit | **691 passed, 0 failed**, leak detection OFF; `.release-san-mir.log` |
| AIR unit | **161 passed, 0 failed**; `.release-air.log` |
| Function-flow target | PASS both ordinary and currency/worklist verification modes; `.release-flow-summary.log`; phase 13429/0, memo 48/0, worklist 31/0 |
| Native speculation facts | PASS; `.speculation.log` |
| Header size / documentation quality | PASS; `.header-size.log`, `.doc-quality.log`; state/lane sizes checked again at publication |

The final isolated emitted-program memory check uses
`tests/self_hosted/parity/fixture/loop_local_collection_cleanup_sanitized.pgy`
(SHA256 `742227b804f74a50940dbe0afa9ede3863408fcf4d66ccee1c599cdcaac14f0b`).
C runs with ASan+UBSan and leaks ON. LLVM IR first receives
`opt-18 -passes=forceattrs -force-attribute=sanitize_address`; its body is
ASan-instrumented, and its C runtime has ASan+UBSan. Both return `ok`, exit 0,
stderr empty and leak detection ON. Artifacts and logs are under
`.tmp/loop-local-sanitizers/`; harness is
`C:\Users\user\.codex\tmp\pergyra-loop-local-20261021\sanitize.sh`.
This isolates Array backing and owned payload cleanup; it does not establish
cleanup of unrelated `Concat`/`ToString` temporaries or whole-program leak freedom.

## Required integration still red/open

- `make test-mir`: units, new loop gate, domain topology C/LLVM and
  destructure facts pass, then match-binding gate fails because no current
  self-host driver is installed (`.release-test-mir.log`). The standalone
  statement-render gate has the same unavailable-driver limit
  (`.stmt-fail-closed.log`). Neither is counted green.
- String-result control fails before MIR at 22:27: `ArrayDropOwnedStrings`
  rejects `fields` after shallow mutation (`.release-string-control.log`).
  The same refusal was observed with the pre-existing Claude binary
  `932c91d58311674abb83b61c161e9f5edef56e081a9b283c4900b8bb4fa0fb53`;
  this is not an exact-baseline binary equivalence claim.
- The scalar-reader probe fails in imported `MirRoutineArrayObjectFactTable`
  on two borrowed-ref escapes through `JsonObjectFactArrayObjectTable`.
  The reader guard is therefore not an executed installed-driver result.
- Ordinary unchanged canonical compilation still aborts before MIR:
  **33,644 reported errors (includes cascades) / 21 warnings / MIR 0**, rc1.
  The current reader input and an exact `fa66a2fb` self-host input archive,
  both on this same release binary, have identical diagnostic streams
  (ERROR/WARNING-line SHA256
  `c3b1900b3b4a535d57bf6bee7022ae5130078e5693fbc7e0041f1518c36551d7`).
  These are **not** the downgraded all-final-owner census of 1070 independent
  refusal sites; that census was not rerun here. Logs:
  `.release-canonical.err`, `.baseline-input.err`; both MIR files are empty.
- Published main remains `39a8fc1db`; CI run `38860052641` and Platform full
  run `39150389845` are failures, verified remotely on 2026-10-21. No push.

## Exact next dependency and Task B boundary

The user chose Claude for semantic publication and GPT for MIR consumption.
[Producer request and complete consumer plan](../../agent_work_directives/mir_loop_cleanup_array_preservation_projection_2026-10-21.md).
A direct `Count(ref rows)` that only returns ArrayLength still refuses with
`invalid-empty-birth` (rc1/MIR0): Slot masks alone do not authorize retirement.

Source survey confirms direct MIR dispatch still branches through
`direct_mir_backend_projection_owner.pgy`; C source projection still calls
`DriverRung2ScalarCSubstitutionIfClaimed` before AST reconstruction in
`driver_rung2_owner.pgy`. No general installed consumer was verified to
replace them. No route was deleted merely to subtract its refusals. No SoT
row or V1 finish-line obligation is marked CLOSED.

## Failed attempts retained

The new escape negative caught the first implementation ignoring local
binding identity; that candidate was not executed as a program. A borrowed
SSA use-name caused a compiler double-free, fixed by issuing an owned copy.
Missing-kind and liveness mutations now refuse; the finished-loop case no
longer inherits a later local's lifetime. One older MIR fault fixture also
needed its reallocated instruction capacity stated so it reaches its intended
control-flow check rather than the new storage-bound check.

An intermediate function-flow rerun overlapped our relink of `bin/pgy` and
failed with PermissionError (`.final-flow-summary.log`). The executable was
then held fixed and the **full target** was rerun successfully
(`.release-flow-summary.log`); the failed record was not removed.

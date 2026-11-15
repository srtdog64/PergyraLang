# 2026-11-16: native hand-off fact and by-value formal member release

Run by Track G of the Claude root lane on WSL `Ubuntu-C-WSL`, tree
`/root/tg-own` (branch `worktree-agent-aeaa4c45e52766d61`, base `98bcf67ec`).
Main-equivalent native binaries: `/root/td-own/bin` (its tree lacks
`41ab40e3f`) and an exact `98bcf67ec` build in `/root/tg-base`.

## What changed

- Native semantic issues `handed-off` (decision 2026-11-15):
  `src/semantic/collection_handoff_owner.c` selects the rows before the
  effect snapshot take and marks them after it. HIR, MIR validation, the MIR
  transition owners and `mir_json_dump.c` accept or write the value; a
  member-move row may not carry it.
- Direct MIR: a callee releases no local taken from a member path of a
  by-value formal (`ByValueFormalMember`, storage-sharing owner, consumed by
  `ArrayStringLocalCleanupRequired`).

## Native verdicts (against `/root/td-own/bin`)

| Input | main | branch |
|---|---|---|
| 868 lifetime fixtures, native C | 569 admitted, 8 not clean | same verdicts, stdout and C bytes |
| 868 lifetime fixtures, native LLVM | 568 admitted, 8 not clean | same verdicts, stdout and LLVM bytes |
| `tests/cases`, native C (2258 common) | 1760 admitted, 68 not clean | same verdicts and C bytes; one stdout differs (DirWalk listing) |

The bootstrap entry `driver_bootstrap_main.pgy`, native C against the exact
`98bcf67ec` build: 0 errors on both (27 warnings, RSS 2.75 GB). The C differs
only by the two edited self-host owner functions and renumbered temporaries.

## Self-host legs over the 868

- Self-host C admits 321 and self-host LLVM 167, as on main; verdict lines
  are identical before and after the driver fix.
- The ASan LLVM leg (`clang -O1`) reported both legs clean. Rebuilt at `-O0`
  without ASan and run under glibc heap checks, main's driver double-frees
  `bundle_field_nested_read_positive` (the callee and the caller's record
  both released one member) and `post_field_return_negative` (`Array<Int>`
  record, another chain). With the fix only `post_field_return_negative`
  remains, plus the known endless recursion.

## `--mir-json` route (native MIR into the installed backend)

- C and LLVM admit 90 each (main 88). `bundle_field_nested_read_positive`
  and `nested_sibling_member_paths_positive`, refused before for a missing
  constructor hand-off, run clean on both under ASan and under glibc.
- No program of the 868 reaches the by-value branch on this route, on main
  or on the branch; most stop at earlier admission stages.

## Hand-off agreement on the 868

Fixtures both producers admit: 278. Native issues 157 hand-offs and the
self-host 148; 119 agree, 38 are native-only and 29 self-host-only. Causes
seen: native follows a formal's local alias (`grown_handoff`) where the
self-host does not; native treats a descriptor returned inside a record as
lent (`exclusive_branch_rebind_positive`) where the self-host hands it off;
native lands a Clone where the self-host hands off; a returned constructor's
inputs are retired by the self-host and handed off by native.

## Gates on the branch binaries

- Carrier, cleanup (15 refused, 5x2 admitted, native MIR 2x2), copy landing
  (2 landed, 12 refused), semantic owner and projection smoke: PASS.
- `native_unsafe_negative_refusal_smoke.sh`: 39/17/8. `make test-semantic`:
  3394/3394. `test_mir`: 754/754 (7 new).
- `ci-push-linux` prebuilt: core 61 steps ok, self-host 14 steps ok.
- Codegen bootstrap: gen2 == gen3 (121411 lines). SoT registry gate: PASS.

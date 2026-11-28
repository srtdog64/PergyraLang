# Self-host cleanup leaks: four release causes in the general emitters

Observed 2026-11-28 KST on main `8ba5fef25` plus `03f36fe2b`..`122c94992`.
Trees: WSL `Ubuntu-C-WSL` `/root/lk2` (census, sweep, gates), `/root/lk3`
(codegen bootstrap, baseline gates), `/root/lk4` (driver builds, CI steps;
scratch diagnostics never committed). Baseline driver: a copy of
`/root/sweep/driver_lf2` (SHA-256 prefix `bc6c3e8b1b4ef3b3`, main's
compiler). Change driver: test profile (`-O0`) from this source (SHA-256
prefix `f3ee418b76794e36`).

## Method

- Leak census: every program of the 2,746-program sweep list plus the 868
  lifetime fixtures (3,613 present), on the four legs (`nc`, `nl`, `sc`,
  `sl`; LLVM IR at `-O0`). Each admitted program runs under ASan without leak
  detection (output, invalid access, double free), then under LeakSanitizer
  with stack and register roots off, so storage only `main`'s frame still
  names counts as leaked. Leak records are grouped by allocation helper; a
  default-C program is split by route (general emitter or the MIR-to-AST
  round trip).
- Each fix has its run of `tests/self_hosted/parity/ownership/selfhost_cleanup_release_owner.sh`
  (`replaced-call-result`, `record-buffer`, `log-temporary`,
  `observed-argument`): source, verified MIR, then
  `--mir-json-backend=c|llvm`; ASan on C and on LLVM at `-O0`; output equal
  to native. A `_released` fixture must also be LeakSanitizer-clean; a
  `_kept` fixture checks memory safety only (decision 2026-11-15 hand-off,
  or an open cause).

## Baseline census (`driver_lf2`)

| Leg | Admitted | Leaking | Invalid access or double free |
|---|---:|---:|---:|
| `sc` (default C) | 1,836 | 1,079 (993 round trip, 86 general emitter) | 0 |
| `sl` (default LLVM) | 1,244 | 619 | 0 |
| `nc` / `nl` (native) | 2,603 / 2,603 | 1,648 / 1,654 | 0 |

Of the self-host leaks, 15 on C (13 round trip) and 7 on LLVM leak where
native is clean; native leaks every other one too. By allocation group
(programs; one program can count in several): LLVM String text 414,
`Array<Int>`/`Array<Bool>` storage 204, `Array<String>` storage 166, record
buffers 15; general-emitter C String text 73, `Array<String>` 15.

## Causes fixed

| Cause (gate run) | Allocation and owner | Decision | Fact owner and consumer changed |
|---|---|---|---|
| `replaced-call-result` | A loop-local `Array<String>` bound from a fresh call result, clone or key snapshot: each earlier pass's storage and strings | defect | Owned-collection flow publishes the definition's replacement; transition admission carries it; the replacement owner issues it and keeps a generation whose owned text a loan may still read; the cleanup policy prints it on C and LLVM |
| `record-buffer` | The buffer of an `Array` of storage-free records built by push or set | defect | Owned-local lifetime owner: a record push or set keeps the local's descriptor (release mode 9 existed) |
| `log-temporary` | A fresh String (`ToString`, `Concat`, `Substring`, ...) written by `Log` or discarded by a statement | defect (2026-11-16: temporaries are released at their last use) | Owned temporary owner decides; the C and LLVM Log and statement emitters print the release |
| `observed-argument` | An owned `Array<Int>`, `Array<Bool>` or storage-free record buffer handed to a by-value formal the callee only reads | defect (2026-10-18: a read-only callee borrows) | Owned-local call carriage takes `value-observed` from MIR's descriptor preservation proof, in a new mode where a by-value formal forwarded to a value-result formal is not observed |

Gates: each run PASSes on the change and FAILs on `driver_lf2`
(`break_exit_released` 182 bytes, `element_set_released` 256,
`fresh_text_released` 190 in 20 allocations, `loop_and_record_buffer_released`
92). An intermediate driver without the value-result mode double-freed
`function_param_flow_summary/by_value_array_grown_by_inout_callee_last_use_valid`
on LLVM; `observed-argument/forwarded_to_inout_kept` fails on it.

Brief fixtures on the change (bytes leaked, default route): owned-strings
wide 55,541,760 to 0 on C and LLVM; array-of-records (both scopes) 16 MiB
to 0 on LLVM; int-buffer, nested-record and two-collection-record 0 on
LLVM; `array_param` 64 to 0 on LLVM; record-field `Weighted` 60 to 12 on
LLVM. Array-of-records, int-buffer and `array_param` are clean on the
general C emitter too (`--mir-json-backend=c`), but their default-C numbers
stay as they were: default C sends every program with a declaration, or
without ownership facts, through the round trip (open cause below).

## Census and sweep on the change

| Leg | Leaking before -> after | Leak -> clean | Leak smaller | New leak | New invalid access or double free | Admission changes |
|---|---:|---:|---:|---:|---:|---:|
| `sc` | 1,079 -> 1,069 | 10 | 3 | 0 | 0 | 0 |
| `sl` | 619 -> 398 | 221 | 41 | 0 | 0 | 0 |

LLVM groups after (programs, before in brackets): String text 290 (414),
`Array<Int>`/`Array<Bool>` storage 82 (204), `Array<String>` storage 165
(166), record buffers 15 (15). Self-host LLVM leaks where native is clean:
5 (7). Default C keeps its 993 leaking programs on the round trip; its
general-emitter part went from 86 to 76.

Sweep of 2,746 programs against `/root/sweep/lf1_*.tsv`: C 0 changed rows
(1,512 admitted); LLVM 0 changed rows (1,008 admitted).

Other checks at the change: codegen bootstrap rc 0 with gen2 == gen3
(126,056 lines) at `122c94992`; native oracle emission of the driver source
rc 0 (1,128 s under load); component contract (line caps, generator drift),
language keyword registry, SoT edge and adequacy (Rocq skip declared), gate
reachability PASS. Self-host CI push steps, run as CI runs them (make targets through
make): all 59 `bash` steps PASS with the change's test-profile driver,
including the four new gate runs and the compile-memory gate (1,572 s; the
loaded baseline run timed out at 1,800 s); 16 of the 18 `make` steps PASS
with its release driver. The other two stop on this machine for reasons the
baseline shares. `self-host-replacement-frontier-installed-test-smoke` stops
at `one_mir_struct_argument_projection.sh` (`declaration source module path
escapes repository`, the same with `driver_lf2`); the target's last gate,
`role_override_mir_replacement.sh`, PASSes with both drivers.
`self-host-preparation-contract-test-smoke` stops at the Rocq model check (no
Rocq installed); rerun with `PGY_ALLOW_MISSING_COQ=1` declared, the target
PASSes.
No front-end rule changed (the change is in the direct-MIR general
emitters), so the tool verdicts and DRV-2 were not rerun.

## Found and held: a member rebind's ordinal read as a local row (S2)

The recorded S2 leak (the `Bag` field of `direct_mir_inout_member_restore`,
32 bytes per backend) has this cause: the owned-local lifetime owner blocks
the right operand of every non-definition operation as a local row, but a
member rebind (`bag.names = names`) and an identity-cell store keep the
member ordinal there, so a rebind of member k in any routine blocks
program-wide local row k (Main's first locals). A driver that skips those
two kinds released the `Bag` record (0 bytes on LLVM) and passed its own
three-fixture gate, but on the census it:

- turned `collection_field_lifetime/member_formal_restore_positive` into a
  double free on LLVM: unblocked, Main releases `pair`, whose field the
  inout callee had moved into a local and released;
- made `function_param_flow_summary/ref_loan_stored_into_inout_output_landed_valid`
  refused on LLVM (`owned local lent to a ref formal is released while an
  inout argument of the same call may hold its Strings`), so the sweep lost
  one admitted program.

The block was hiding both, so the change is not in this landing. The first
is an existing defect on `driver_lf2`: a member moved out of an inout formal
and not restored is released by the callee while the caller's record still
names it. Falsifier (heap-use-after-free on the general C and LLVM routes;
native and the default-C round trip run it clean):

```
struct Pair { value: Array<String>; }
func Count(inout pair: Pair) -> Int {
    let first: Array<String> = pair.value;
    pair.value = first;
    let second: Array<String> = pair.value;
    return ArrayLength(second);
}
func Main() -> Void {
    var pair: Pair = Pair(["borrowed"]);
    Log(Count(pair));
    Log(pair.value[0]);
}
```

The [2026-10-23 decision](../decisions/2026-10-23_field_take_before_own_handoff.md)
admits detaching an inout field only with a fresh replacement on every path,
and keeps a duplicate field extraction refused without an independent copy;
here the second extraction has neither. Order to land S2: first refuse (or
copy) that extraction, or keep the detached local unreleased, then skip the
ordinal operand.

## Open causes

Each names the missing fact, its owner, the last consumer and a falsifier.

- Default C round trip (993 leaking programs). A program with declarations,
  or without collection ownership or full identity claims, goes MIR -> AST
  text -> the AST C emitter, which consumes no owned-value lifetime fact.
  Owner: the default C claim (`driver_rung2_scalar_c_substitution_owner.pgy`)
  and general-emitter coverage of declarations; last consumer:
  `src/self_hosted/codegen/emission`. Falsifier: `array-of-records.wide` on
  `bin/pgy --backend=c` leaks 16 MiB; `--mir-json-backend=c` is clean.
- Fresh String operands outside a Log root, and multi-definition String
  locals. Missing: MIR value-retire rows and the copy at a borrowed
  definition of a multi-definition binding (2026-11-16, points 1 and 4).
  Owner: the MIR owned-value lifetime plan; consumers:
  `owned_local_lifetime_owner.pgy` (`definitions == -2`) and
  `owned_expression_temporary_owner.pgy`. Falsifier:
  `tests/cases/backend_compare/string_format_tens/main.pgy`.
- Unnamed aggregate call results and literal arguments (`Log(Words()[0])`).
  Missing: a temporary row for a fresh aggregate whose only use is an
  observer. Owner: the MIR owned-value lifetime plan; consumers: the C and
  LLVM expression emitters. Falsifier:
  `tests/cases/backend_compare/array_inline_access/main.pgy`.
- A record handed to a by-value formal (`Weighted(holder, 2)`). Missing: a
  read-only class for a record formal; the preservation proof covers Array
  formals of plain elements only, and the self-host hand-off ledger
  publishes classes for `Array<String>` alone. Owner: the one hand-off rule
  ledger; consumer: `owned_local_call_carriage_owner.pgy`. Falsifier:
  `tests/self_hosted/fixtures/direct_mir_record_field_array_int.pgy` (12
  bytes, general LLVM).
- Record buffers whose elements hold storage (release mode 0). Missing: an
  element recipe for an Array of records with String or Array fields.
  Owner: `owned_storage_release_plan_owner.pgy`; consumers: the owned-local
  emitters. Falsifier: `src/self_hosted/codegen/fixture/ast_node_array_push.pgy`.
- Multi-definition Array locals and an `own` formal the callee never
  releases. Falsifiers: `src/self_hosted/mir_lower/fixture/array_literal_assignment.pgy`,
  `src/self_hosted/mir_lower/fixture/param_carriage.pgy`.
- Decision-covered (2026-11-15): an `Array<String>` moved into a
  default-mode callee that grows it is released by neither side; kept as
  `replaced-call-result/handed_off_kept.pgy`.

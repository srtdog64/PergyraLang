# Native checker: an inout call that frees a String element retires the caller's loans (2026-11-20)

Source: main `9c4ac12af` plus this change, on branch
`claude/beautiful-bassi-3701d5`. Tree: WSL `Ubuntu-C-WSL` `/root/pgy-inout`
(base binary kept as `bin-base/pgy`); Linux gcc 13.3 with ASan/UBSan,
LLVM 18. Rule:
[decision](../decisions/2026-11-20_inout_call_freeing_an_element_retires_loans.md).

## Before

The String loan owner retired loans only at a builtin call on the receiver
itself (an owned `ArraySet`, `ArrayDropOwnedStrings`). A loan held across a
call whose `inout` formal frees the element was admitted, on an exact
`Array<String>` source and a `List<String>` alike. On base each of these
compiled and failed ASan with heap-use-after-free on native C and on the
LLVM leg (`/root/legs_lib.sh`: IR at -O0 with `sanitize_address`):

| Program | Shape | Base C | Base LLVM |
|---|---|---|---|
| reported 1 | `let name = names[0]; Replace(names); Log(name)`, `Replace` does `ArraySet(names, 0, ...)` on its `inout` formal | UAF | UAF |
| reported 2 | `let name: String = ListGet(items, 0); Replace(items); Log(name)`, `Replace` does `ListSet` | UAF | UAF |
| pop | `let name = names[1]; Shrink(names); Log(name)`, `Shrink` does `ArrayPop` | UAF | UAF |
| ListRemove | as reported 2 with `ListRemove(items, 0)` | UAF | UAF |
| later callee | reported 1 with `Replace` declared after its caller | UAF | UAF |
| branch | reported 1 with the call inside `if` | UAF | UAF |
| loop | `while { Log(name); Replace(names); }` | UAF | UAF |
| continue | `while { if first { Replace(names); continue; } Log(name); }` | UAF | UAF |
| reassigned local | `name` assigned `names[0]` or `names[1]` on two paths, then the call | UAF | UAF |
| forwarded | `Outer(inout names) { Shrink(names); }`, then the read | UAF | UAF |
| List forwarded | the same with `ListSet` | UAF | UAF |
| List loop | the loop shape with `ListSet` | UAF | UAF |

ASan-clean on base and unchanged after: a read before the call, a callee
that only reads its `inout` formal, a callee that only pushes, a loan of
another Array, the read in the other branch, the read after a branch that
calls and returns, a loop whose call is on the path that breaks out.

## Change

- `call_argument_move_order_owner`: the formal release summary gains
  `frees_elements`. An owned `ArraySet` or `ArrayPop`, `ListSet` or
  `ListRemove` on a `List<String>` rooted at a formal records it
  (`semantic_formal_release_record_element`); a whole release implies it;
  the closure after Pass 2 carries it over hand-offs, and to a callee with
  no body. `semantic_formal_release_frees_elements` answers it.
- `indexed_string_call_release_owner.c` (new, in the loan owner family):
  after every argument of a user call, each String local that may borrow
  from an `inout` actual (its exact `Array<String>` source, or unknown)
  gains an edge naming the call. A read records the edges the local has
  there. The edges join at `if` branches like the invalidation flag (a
  branch that does not fall through drops its edges), are dropped after a
  `defer` body and start empty at a `let`. Inside a loop, the edges the
  body's locals of the enclosing scopes hold at its end and at each
  `continue` are joined into their reads since the loop began.
- The String receipts prepare phase judges every read: behind a call whose
  callee may free an element, an exact-source loan is refused at its first
  read; an unknown-source loan is copied at each reaching definition, or
  refused where the definition owner cannot issue that copy. Copies are
  selected before refusals are emitted.

Objective card: no admitted program reads a String an `inout` callee freed.
Fact owners: the formal release summary (what a callee frees) and the
String loan owner (which loans a storage source has). Last consumer: the
receipts prepare phase, after the summary closes. Forbidden fallback: a
callee whose summary is missing or unpublished counts as freeing.

## After

| Program | New C | New LLVM |
|---|---|---|
| reported 1, pop, later callee, branch, loop, continue, reassigned local, forwarded | refused at the read | refused at the read |
| reported 2, ListRemove, List forwarded, List loop | copied where `name` is defined; prints `5`, ASan-clean | same |
| the seven positives above | unchanged output, ASan-clean | same |

The member-field case (`let name = table.names[0]; Replace(table)`) keeps
the member-carrier owner's refusal at the call, and an `own` hand-off keeps
the existing refusal at the read; neither changed.

Still heap-use-after-free on C and LLVM, before and after: a release on a
path that `break`s, read after the loop. The `if` holding the `break` does
not fall through, so the join drops its edges, and a direct owned
`ArraySet` there loses its invalidation flag the same way.

## Gate

`tests/ownership/inout_call_string_loan_refusal_smoke.sh`
(`make inout-call-string-loan-refusal-test-smoke`, a push core step in
`scripts/ci_push_linux_steps.sh`), fixtures in
`tests/cases/inout_call_string_loan/`: 8 refusals at the read, 12
admissions with their output, on C and LLVM. PASS on the new binary; on
`bin-base/pgy` it fails at its first negative
(`array_set_in_callee_invalid (c): expected a semantic refusal, exit 0`).

## Other checks on the new binary

- `test-semantic` 3404 passed, `test-transpile` 1035 passed.
- `native-unsafe-negative-refusal-test-smoke`, `build-source-inventory`,
  `gate-script-reachability`, `gate-subject-declaration` PASS.
- `call_argument_evaluation_order_smoke.sh` and
  `string_window_extent_smoke.sh` need a self-host driver this tree has not
  built; with `PGY_SELF_DRIVER_BIN=/root/fe/bin/pgy-self-driver` (a
  `9c4ac12af` tree with 6 modified tracked files; it serves only the
  default-route legs, which this change does not reach) both PASS on the
  new and the base binary.
- One heavy program (`tests/self_hosted/fixtures/callable_capability_fact.pgy`,
  `--native-pipeline --mir-json`): peak 881,760 KiB on base, 883,356 KiB
  on the new binary. User time 186 s and 207 s, measured back to back while
  other sweeps loaded the machine (load average about 24 on 16 CPUs); no
  time claim is made from it.

## Sweeps

Each program was compiled with `--native-pipeline --emit-c` by the base
and the new binary; the verdict (exit status) and the emitted C were
compared. The WSL machine was shared with another session's sweep.

- **868 lifetime fixtures** (`/root/fx_all.sh`: native C under ASan):
  verdicts and ASan outcomes identical (both: 49 negatives admitted unsafe,
  99 non-negatives not clean; pre-existing).
- **All 4,465 `.pgy` under `tests/`** (the gate's fixtures excluded):
  no verdict moved. Most ran on an intermediate binary that differed from
  the final one only in diagnostic order and in `continue` handling; the
  174 of them that contain `continue` or an `import`, and the 392 heavy
  self-host fixtures, ran on the final binary. Two heavy fixtures timed out
  on base at 1,500 s and were rerun without a limit: admitted, as on the
  new binary. 69 programs emit different C: 66 only by added
  `StringConcat(x, "")` copies (310 in all), 3 also by argument staging
  (below). All 69 are self-host fixtures or import self-host modules,
  except `tests/cases/function_param_flow_summary/inout_defers_alias_written_by_inout_callee_landed_valid.pgy`
  (one copy).
- **80 self-host entry programs** (every `src/self_hosted` file with
  `func Main`, fixtures excluded): no verdict moved. 15 emit different C,
  only by copies: the canonical `driver_bootstrap_main.pgy` and
  `driver_rung2_main.pgy` 27 each, `semantic/main.pgy` 9, `codegen/main.pgy`
  (the bootstrap's native-built gen0 tool), `parser/main.pgy`,
  `mir_lower/main.pgy` and the rung 0 and 1 drivers 3 each, six tools 3 to
  27. Five of them, and the 3 test programs above, also stage arguments
  differently in one shared routine: its `match_subject_type` local now gets
  a copy where it is defined, and its 7 call sites then pass it directly
  instead of through temporaries. Every argument there is a local, a
  literal or `&env`.

Bootstrap first stage: `codegen/main.pgy` built natively as gen0 by each
binary (`--native-pipeline --backend=c`), then run as
`gen0 --source src/self_hosted/codegen/main.pgy`. Both emit the same gen1 C
(sha1 prefix `90eba25afa02`, 14,739,851 bytes), so the later self-built
stages get the same input. gen0's emit peaked at 5,420,416 KiB (base-built)
and 5,439,432 KiB (new-built). The full bootstrap and fixed point were not
run locally.

The copies are the unknown-source rule at work: a String local held across
an `inout` call whose callee frees an element of some String collection is
copied where it is defined, whether or not it came from that collection
(the cost the decision names).

# MIR loop-local String Array birth, and the direct_mir split (2026-10-21)

Status: ACTIVE. Written by the Claude lane at the owner's request, for the
GPT lane. It coordinates work; current source, the
[V1 directive](v1_scope_ownership_and_selfhost_generality_2026-10-14.md) and
the executable gates win where they disagree with it. Outputs of the tasks
below are implementation candidates; the integration owner rechecks source
and runs the gate before landing.

## Base revisions

- Claude branch `claude/p0-native-checker`, worktree
  `C:\Users\user\pgy-wt\p0`, head `914b3bbb0`. Local, not pushed. Its merge
  base with local `main` is `7cd004fab`; published `origin/main` is
  `39a8fc1db` and has diverged from local `main`
  ([main state](../truth/state/main.md)).
- The canonical census on that head: 1070 refusals at the final analysis,
  306 of them in `direct_mir_*` modules
  ([record](../truth/evidence/2026-10-21_native_census_loop_walk_push_lands_next_analysis.md),
  [blocker](../truth/state/blocker.md)).

## Objective card

| | |
|---|---|
| Objective | the unchanged canonical driver input reaches MIR and passes validation once the native checker admits it |
| Priority | MIR validation wall first (it is the next wall after admission); the `direct_mir_*` routes second |
| Fact owner | `src/compiler/mir_branch_collection_transitions.c` (MIR collection transitions); MIR lowering for the cleanup it expects |
| Last consumer | MIR validation in the native pipeline (`--mir-json`, `--backend=c|llvm`) |
| Forbidden | weakening the transition check so a live owned Array can be born over; editing the canonical source; deciding the two OPEN questions below |
| Gate | the reproducers below, C and LLVM printing the same, plus `make test-mir` and `tests/string_result_control_ownership_smoke.sh` |

## Task A: an `Array<String>` born `[]` inside a `while` body

MIR validation refuses it, on the branch and before this session's work
(`4f7692f38`); `mir_branch_collection_transitions.c` is unchanged on the
branch since `7cd004fab`:

```
MIR collection ownership transition is invalid (routine=Main binding=16
local=actuals stage=invalid-empty-birth)
```

The canonical source has this shape:
`src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy:224`
(`let actuals: Array<String> = [];` in a `while` body, no drop). The native
checker does not admit the canonical yet, so the canonical itself cannot
show the wall; these programs do:

```
// v7: refused (invalid-empty-birth)
func Main() -> Int {
    let n: Int = 0;
    let total: Int = 0;
    while n < 3 {
        let actuals: Array<String> = [];
        ArrayPush(actuals, "x");
        total = total + ArrayLength(actuals);
        n = n + 1;
    }
    Print(ToString(total));
    return 0;
}
```

- The same with `ArrayPushOwnedString(actuals, Concat("x", ""))`: refused.
- The same plus `ArrayDropOwnedStrings(actuals);` at the end of the body:
  passes; C and LLVM print `3`.
- `Array<Int>` in the same place, a non-empty literal, and an `if` block:
  pass.

What the validator sees: the loop header joins the back edge, which
carries the previous iteration's state of `actuals` (born, not retired), and
the next `[]` birth requires UNBORN or RETIRED (line ~598). Nothing in MIR
retires a body-local Array<String> at the end of an iteration unless the
source drops it.

The choice is the GPT lane's (record it in `docs/truth/decisions/`):

| | Lowering retires the body-local at the end of each iteration | Validator treats the re-birth as a new incarnation |
|---|---|---|
| Ownership | the compiler-owned cleanup V1 asks for | the old storage leaks each iteration, as emitted code does today |
| Risk | must not retire a value that escaped or was moved (the checker's facts say which) | must not accept a birth over storage that is still reachable |
| Falsifier needed | an iteration that moves `actuals` out (into a result or a field) must not be retired | a back edge where `actuals` is read after the next birth must still be refused |

Either way, add one positive and one negative program to a gate, and
mutate the new condition until a fixture fails.

## Task B: `direct_mir_*` routes

306 of the 1070 refusals are in `direct_mir_*` modules. The V1 directive
deletes those routes as the general emitter covers their programs; when a
route goes, its refusals go with it. Count them per route with the census
and attribute deletions in the deletion commit. This needs no semantic
change from the Claude lane.

## Edit scopes

| Lane | May edit | Must not edit |
|---|---|---|
| GPT | `src/compiler/mir_*`, MIR lowering, `src/codegen/*`, `direct_mir_*` modules and their gates | `src/semantic/*`, `tests/function_param_flow_summary_smoke.sh`, `tests/cases/function_param_flow_summary/*`, `docs/truth/state/blocker.md` |
| Claude | the files the GPT column forbids | the files the GPT column may edit |

If a task needs a file on the other side, write it down and ask; do not
edit it.

## Commands and budgets

Build with `make bin/pgy`. Budgets per AGENTS.md: 60 s for static gates,
5 min for focused parity, 30 min for an integration shard. A canonical
census (four analyses) takes about 15 minutes in a Linux tree.

## Integration

- Owner: the GPT root lane, which owns `main`, the push and
  [main state](../truth/state/main.md).
- Gate: on one tree that contains both lanes' commits, the Task A
  reproducers pass or refuse as stated, the native checker's flow summary
  target passes (`make function-param-flow-summary-test-smoke`), the unit
  suites are unchanged, and the canonical census does not rise.
- The Claude branch is local. Its commits since `7cd004fab` are listed by
  `git log 7cd004fab..claude/p0-native-checker`; each has an evidence record
  under `docs/truth/evidence/`. Landing them is the integration owner's call;
  the Claude lane keeps committing on the branch meanwhile.

## Not for this directive

Two questions wait on the owner and neither lane decides them:

- [copy a moved argument used again](../truth/decisions/2026-10-21_open_copy_a_moved_argument_used_again.md)
  (266 refusals; the Claude lane recommends copying);
- [an exactly replaced inout field handed to an exact releasing own formal](../truth/decisions/2026-10-23_field_take_before_own_handoff.md).

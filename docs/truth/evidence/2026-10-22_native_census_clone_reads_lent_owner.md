# 2026-10-22: canonical census after a Clone of a lent owner counts as a read

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`316e8a30c` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-22_clone_reads_a_lent_owner.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `316e8a30c` | 3404, 565, 436, 434 | 392 |
| Clone reads the owner | 3404, 476, 385, 383 | 341 |

Outside `direct_mir_*`, moves 153 -> 102; no other kind changed and none
was added. `direct_mir_*` stays at 42.

## How the cause was found

A census with the owner walk's refusal points numbered (LENT2) put 19 of
59 failed loans at one point: a builtin call receiving the owner that
the walk could not prove read-only, always `Clone`. Most were copies
landed for a moved argument used again (`Clone(env)` at
`RewriteExpectedValueWithSemanticGraph` in `EmitLet`), after `env` was
lent to `CodegenTypeEnvStateAppendTypedValueBinding`.

## Fixtures (719 compared against `316e8a30c`)

Only the new `by_value_lend_then_owner_cloned_valid` changed: refused
before ("'env' was moved into 'Append'"), now one landing (a `ref` call's
loan inside `Append`, unrelated to this change) and `a41` on C and LLVM.
The new `by_value_lend_then_shallow_clone_grown_invalid` is refused at
26:40 on both trees.

## Gates and mutations

`function-param-flow-summary` gate passed (161 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the `test-mir`
target still exits 2 at its self-host driver step, as on main).

| Mutation | Caught by |
|---|---|
| Clone of the owner counts as a change again | `by_value_lend_then_owner_cloned_valid` refused at 44:27 and 45:38 |

## Measured and not taken

Clearing the syntactic RETURN of a formal when its backing frame was
complete and the routine's result scalar, for routines with an `inout`
or `own` formal, gave `CodegenTypeEnvStateAppendTypedValueBinding`'s
`env` the summary 0, but admitted `named_string_field_forward_project_drop_invalid`
(an owned String element kept in an `inout` output, then dropped: a
use-after-free). Reverted before any census.

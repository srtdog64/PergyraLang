# 2026-10-24: the lent proof's verdict is kept per owner

Decided by: the Claude lane, from a red-team review of the branch before
its handoff. The defect is older than the branch: main has it too.

## Rule

The move owner keeps one lent-proof verdict per loan, that is per lending
call and lent owner binding (`MoveWalk.lent`, keyed by `call` and
`owner`). A call that lends two by-value arguments gives each owner the
verdict of its own proof.

## Why

The verdict was keyed by the call alone, while the proof it caches takes
the owner as input. A call lending `x` and `y` answered `y`'s later use
with `x`'s verdict: when `x`'s loan stayed, a change of `y` under the
result's loan was admitted with no copy. On the native C build
`by_value_lend_two_owners_second_changed_landed_valid` printed `17`, the
result reading the change made to `y` after the call.

## Falsifier

`by_value_lend_two_owners_second_changed_landed_valid`: `Pick(x, y, false)`
returns `y`; `Main` reads `x`, then pushes onto and sets `y.items`, then
reads the result. It now lands one copy and prints `15` on C and LLVM.
Without the rule it lands nothing and prints `17`.

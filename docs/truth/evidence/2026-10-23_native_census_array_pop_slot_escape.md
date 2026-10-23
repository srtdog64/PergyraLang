# 2026-10-23: canonical census after ArrayPop adds no call escape to a formal's summary

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`7dde1b8e9` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_array_pop_adds_no_call_escape_to_a_formal_summary.md).

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `7dde1b8e9` | 198 | 156 |
| ArrayPop is local in the slot escape walk | 194 | 152 |

Removed: `Array argument call requires preserved array storage` in
`ParseDeclsInto` three times and `ParseDecls` once, all calls that take the
`inout` import chain after a nested parse popped it. Added: none. The
census took 1075 s and 3978248 KiB at peak beside other runs on the same
machine.

## Gates

`function-param-flow-summary` gate passed (201 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units passed, 0 failed (the target exits 2 at its
self-host driver step, as on main).

## Fixtures (802 compared against `7dde1b8e9`)

Errors, landing analyses and `Clone` counts per routine changed only in
`inout_string_chain_popped_by_recursive_parser_valid`, new: refused at
20:13 on `7dde1b8e9`, prints `00000101101011110` on C and LLVM now, ASan
clean. Its negative, a callee that keeps the chain before popping it, is
refused the same on both trees.

## Mutation (227 fixtures)

| Mutation | Caught by |
|---|---|
| ArrayPop's receiver keeps its call escape | `inout_string_chain_popped_by_recursive_parser_valid` refused |

The rule has no other condition: `ArrayPop` is reserved by the
statement-form builtin family, so no declared routine takes its name, and
it takes one argument.

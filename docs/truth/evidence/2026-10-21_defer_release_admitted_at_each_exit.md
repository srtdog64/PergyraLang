# 2026-10-21: a defer's Array release was checked only where it is written

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`4f7692f38` (local, not pushed). Linux scratch tree, C backend, `cc` with
`-fsanitize=address`, `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`.

A defer cleanup runs when the routine exits. The type checker checked its
body once, at the `defer`, against the state there, and restored that
state after it. `ArrayDrop(rows)` or `ArrayDropOwnedStrings(rows)` in the
body was admitted if `rows` owned its storage at the `defer`. Nothing
checked `rows` again at the exits where the cleanup actually runs, so a
later handoff of `rows` was admitted and the cleanup freed storage that
was no longer the routine's.

Each fixture below is in `tests/cases/function_param_flow_summary/`. On
`4f7692f38` the checker refused none of the first eight:

| Fixture | What takes `rows` after the `defer` | Run on `4f7692f38` |
|---|---|---|
| `defer_array_drop_return_keep_invalid` | `return Keep(rows)`, `Keep` returns it in a struct | heap-use-after-free |
| `defer_array_drop_own_handoff_invalid` | `Eat(rows)` with an `own` formal that drops it | double free |
| `defer_array_drop_block_kept_invalid` | `held = Keep(rows)` in an `if true` block | heap-use-after-free |
| `defer_owned_strings_return_constructor_invalid` | `Names(true, names)` returned | heap-use-after-free |
| `defer_own_handoff_cleanup_kept_invalid` | the cleanup is `Eat(rows)`, after `return Keep(rows)` | heap-use-after-free |
| `defer_array_drop_return_binding_invalid` | `return rows` | runs; the caller reads only the length |
| `defer_array_drop_fallthrough_kept_invalid` | extern `KeepRows(rows)` before a fallthrough | admitted; no extern body to link |
| `defer_array_drop_lambda_kept_invalid` | the same inside a lambda | admitted; no extern body to link |
| `defer_array_drop_reader_then_keep_invalid` | `Peek(rows)`, then `return Keep(rows)` | refused (22:12) |

The last one was refused only because `Peek`, a call before `Keep`, left a
pending handoff on `rows`, and the call check then counted `Keep`'s own
handoff against itself. Without `Peek` the same return is the first row.

String Arrays were covered only in part. An own handoff of the Array after
the `defer` was refused by the deferred String read owner ("a later
cleanup still reads this source"); a struct constructor holding it was not.

No canonical compiler source releases an Array in a `defer`. The earlier
fixtures that release one and then let it go (`array_result_owned_defer_*`)
were refused by other checks: a pending call result the Array came from,
or the String read owner.

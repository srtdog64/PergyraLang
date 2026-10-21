# 2026-10-21: canonical census after every local is recorded

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`e300b8cff` plus the change measured (local, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** the program-point index records every `let` of the
  routine. Before, it recorded them inside the walk that decides whether the
  routine needs a mutable output, and that walk stops at its first `true`:
  at the first call that hands on an `inout` output, or at any node it does
  not know, such as a `match` or a lambda. A local declared after that was
  not a local, so assigning it was not a replacement. When the local held a
  loan of a `ref` input, the frame took the assignment for a store into that
  input and gave the input a call escape.

| Run | Refusals by analysis | Final, in `direct_mir_*` |
|---|---|---|
| `d931d7af2` | 3936, 2055, 1850, 1832 | 600 |
| every local recorded | 3606, 1597, 1440, 1431 | 361 |

No flow store failed in any analysis. Outside `direct_mir_*`:

| Kind | Before | After |
|---|---|---|
| a borrowed `ref` escapes through a call | 276 | 176 |
| other borrowed `ref` escapes | 79 | 26 |
| an Array argument whose storage a prior call may retain | 91 | 85 |
| moves, Strings and every other kind | 786 | 783 |

## How it was found

A census-mode trace logged every call escape a backing frame adds, with its
site, every callee edge that carries one, and every store into an output.
Followed from each refusal back to its seeds, the largest set had only an
output store as its seed. Its stores were assignments to a local whose
destination was the routine's own `ref` input (`actual = ...` in
`SemanticExpressionGraphGenericCallFactCaptureFromResolvedTarget`). A small
program with that shape (`ref_loan_local_after_output_call_valid`) was
refused at `e300b8cff`; the local was missing from the index.

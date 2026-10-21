# 2026-10-21: canonical census after unadmitted String arguments are copied

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`62d1fb753` plus the change measured (local, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** a String element argument of a declared call that no
  receipt admits is selected, and the next analysis passes `Clone` of it
  ([decision](../decisions/2026-10-21_string_argument_no_proof_admits_is_copied.md)).

| Run | Refusals by analysis | Final, in `direct_mir_*` |
|---|---|---|
| `62d1fb753` | 3606, 1597, 1440, 1431 | 361 |
| String arguments copied | 3606, 1447, 1306, 1297 | 315 |

The first analysis is the same: it selects the copies, which the next
analysis lands. No flow store failed in any analysis. Outside
`direct_mir_*`:

| Kind | Before | After |
|---|---|---|
| a String element crosses an opaque boundary | 171 | 86 |
| a borrowed `ref` escapes through a call | 176 | 172 |
| every other kind | 723 | 724 |

Of the 86 left, 15 are `UnwrapOption` arguments. The other 71 are not
arguments of a declared call: 43 identifiers, 20 fields and 8 elements.

`string_element_tostring_opaque_call_invalid` kept `ToString(owned[0])`
in an output and then dropped `owned`. It now lands a copy at the call:
MIR shows `Clone` in `Main`, and C and LLVM print `a`. It is renamed
`string_element_tostring_opaque_call_landed_valid`.

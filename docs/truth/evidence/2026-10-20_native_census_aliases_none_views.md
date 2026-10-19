# 2026-10-20: canonical census after path aliases, None() and Slice views

Recorded by the Claude lane on branch `claude/p0-native-checker` (local
commits, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the
  [final-owner record](2026-10-19_native_census_final_owners.md). Every
  error is downgraded and every final owner runs. Scratch probes tag each
  move refusal with why it moved and record the storage-proof leaves and
  failed loan walks.
- **Flow analysis:** complete in the rows below, 14616 entries, 3.47 GB peak.

## Downgraded refusals, by commit

| Commit | Refusals | Moves | Change |
|---|---|---|---|
| `a6c221188` | 5311 | 2668 | MapSize borrowed for the call |
| `98f4b7747` to `733b546cf` | 46130 | 9547 | flow store failed; every later refusal is fail-closed noise |
| `bf4761fe8` | 4903 | 2535 | None() handed to a destination closes the return relation |
| `2d30ec766` | 4401 | 2127 | Slice view of a formal followed as an alias of its storage |

`98f4b7747` modelled None(). That let frames be analyzed that hand a None()
local to a routine (`DirectMirScalarCfgLlvmLocalDeclarations`). The
backing destination rule for a call with no arguments then left such a
frame incomplete but supported. The return relation check failed the whole
store, and the census was not rerun until two more commits were stacked on
it. Path alias tracking (`99a31fe0f`, `733b546cf`) is measured only from
`bf4761fe8` on.

## At `2d30ec766`

| Count | Diagnostic |
|---|---|
| 2127 | moved value used again |
| 1151 | borrowed `ref` value escapes through a call (mostly a `ref` formal whose own summary has a call escape) |
| 315 | String element crosses an opaque boundary |
| 131 | String borrows a released `Array<String>` element |
| 112 | `ArrayPushOwnedString` on escaped storage |
| 96 | Array argument call requires preserved storage (169 at `a6c221188`) |
| 81 | borrowed parameter storage moved |
| 388 | other |

| Moves | Why the call moved |
|---|---|
| 1188 | a getter whose loan proof failed (135 `TypedAstArenaAtomText`) |
| 673 | the callee changes, keeps or releases the argument (76 `SemanticAstExpressionMemberRootNames`) |
| 158 | a read-and-return callee whose storage proof fails (638 at `a6c221188`) |
| 125 | a getter on a local |

| Count | Module family |
|---|---|
| 1273 | `semantic/` |
| 1036 | `compiler/direct_mir_*` (per-program-shape routes V1 deletes) |
| 583 | `codegen/` |
| 470 | `mir/` |
| 235 | no module attributed |
| 225 | `compiler/` (other) |
| 219 | `mir_lower/` |
| 210 | `parser/` |
| 150 | other |

## What the failed loans show

At `bf4761fe8`, 91 lending calls failed their loan walk and about 235
failed the owner scan, for 1232 refusals. The failed walks escaped at a
call that keeps the loan (51), at a return the owner's summary does not
carry (33), at an assignment (5), or at no recorded site (2). Many are
String elements that leave the routine:

- `AppendLine(acc, line)` returns `line` itself, so a String element of a
  local Array returned through it outlives that Array;
- getters such as `SemanticAstNominalConstructorNameAt` return a String
  of their by-value argument, which the caller pushes into an `inout`
  output.

Admitting these needs the
[String copy landing](../decisions/2026-10-19_string_landing_copy_then_move.md)
at the store or return where the loan leaves. Today it covers only an
indexed String pushed into an Array.

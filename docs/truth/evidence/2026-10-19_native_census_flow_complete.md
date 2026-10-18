# 2026-10-19: the canonical census runs the flow analysis to completion

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`73e3a3187`. Input: unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
Method as in the [flow progress record](2026-10-18_native_census_flow_progress.md):
a scratch tree with errors downgraded, plus traces of incomplete frame
sites. Times are on that scratch build, not a production measurement.

## First flow failure, by commit

| Commit | Flow entries | First failing frame | Cause fixed next |
|---|---|---|---|
| `61f1e9358` | 10938 | `CompileSourceToC` (`world.pgy:333`) | host field read as `self` storage |
| `ae75a2467` | 10938 | `CompileSourceToC#0-#3` at the intent call | intent call model |
| `c39f95195` | 11513 | `DriverSourceCRequestMode#0` (recursive seal) | match statement in frames |
| `955359774` | 11521 | `CompileSourceToCThroughPgyCompilerWorld#0` | receiver into a changing method |
| `6ae6bdf1b` | 11518 | same | intent step relation owned by the caller frame |
| `e111b9563` | 14455 | `DebugSessionReadCommand#0` | destination walk of borrowed and scalar builtin actuals |
| `73e3a3187` | 14489 | none: no fail-closed summary | — |

At `73e3a3187` the census takes 589 s and 3.47 GB peak; the largest
closure is 7 work units of the 4096 cap.

## Refusals that remain (downgraded, other owners)

9730 sites, down from 42538 at `61f1e9358`.

| Count | Diagnostic |
|---|---|
| 5802 | borrowed ref value escapes through a helper/function call |
| 1524 | borrowed ref value escapes through return |
| 1056 | borrowed ref value escapes through channel send |
| 633 | Array argument call requires preserved array storage |
| 192 | String element crosses an opaque boundary |
| 131 | String borrows a released `Array<String>` element |
| 112 | `ArrayPushOwnedString` on escaped array storage |
| 280 | other (each kind under 60) |

## Not landed: stdlib result relations

Relations for `ToString` (a String argument is returned), `Split` (every
part copied) and `xs.Slice` (a view of the receiver) let the analysis
model those calls. With them the census stopped earlier, at 4600 entries:
the C emitter's statement/expression SCC (`EmitStmtList#5`) had been
uniformly unsupported through `ToString`, and once supported its
declared-result edges were refused by the row-currency check while their
callees were still pending in the same recursion. The change is kept in
scratch, not on the branch.

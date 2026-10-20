# 2026-10-20: canonical census after rebound formals and returned aliases

Recorded by the Claude lane on branch `claude/p0-native-checker` (local
commits, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** a bare formal or alias may take another value that
  holds the formal, and a returned alias counts when the formal's summary
  returns the formal
  ([decision](../decisions/2026-10-20_returned_alias_counts_when_the_summary_returns.md)).
- **Wall:** 834 s for four analyses (3.98 GB peak).

| Run | Refusals |
|---|---|
| `cf8c11c9b` | 2218 |
| rebinding only, with a local of a result as that result | 2214 |
| both rules, a local of a result an alias (final) | 2213 |

- Final run: 5 refusals cleared and none appeared (3 uses after
  `EmitFunctionSet`, one after `EmitFunctionWithSpecialization`, one after
  `SelfMirCfgSetInstructionUses`). The rounds: 3969, 2552, 2347, 2213.
- More formals' storage proofs pass (the two shapes were the commonest
  first causes, in 52 and 50 routines), but the callers' refused uses of
  those formals mostly have other causes as well.

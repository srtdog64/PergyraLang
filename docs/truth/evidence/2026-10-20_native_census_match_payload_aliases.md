# 2026-10-20: canonical census after match payload bindings became aliases

Recorded by the Claude lane on branch `claude/p0-native-checker` (local
commits, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** a match whose subject holds a formal's storage binds
  its payloads as aliases
  ([decision](../decisions/2026-10-20_match_payload_bindings_are_aliases.md)).
- **Wall:** 853 s for four analyses (3.98 GB peak).

| Run | Refusals |
|---|---|
| `cfa395119` | 2213 |
| after | 2182 |

- 40 refusals cleared, all uses after the driver's request, outcome and
  admission readers (`DriverSourceCRequestDiagnostic`,
  `DriverRung2MirCMachineMode` and siblings), and 9 appeared: uses after
  `DriverSourceCRequestDeclaration` and `DriverRung2MirCMachineDeclaration`,
  which return a matched payload and so now lend the request. The rounds:
  3940, 2521, 2316, 2182.

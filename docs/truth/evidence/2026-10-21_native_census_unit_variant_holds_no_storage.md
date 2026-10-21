# 2026-10-21: canonical census after a bare unit variant holds no storage

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`fa66a2fb2` plus the change measured (local, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Cause:** `DriverRung2ExecuteReadRequest` and
  `DriverRung2InstalledPublishMirC` read `MirCDefaultMachine`, a unit variant
  of `DriverRung2MirCMachineRequest`, whose other variant holds a
  `SelfHostMachineLayerDeclaration`. The backing walk treated an identifier
  with no backing row as storage-free only when the whole enum is copy-only,
  so the frame became incomplete, and a routine with an `inout` output then
  counts every formal as escaping through the call: each caller passing its
  `ref compatibility_view` on was refused.
- **Change measured:** an identifier bound to its enum's declaration is a
  unit variant (payload variants are constructor symbols) and carries no
  backing.

| Run | Refusals by analysis | Final, in `direct_mir_*` |
|---|---|---|
| `fa66a2fb2` | 3483, 1204, 1079, 1070 | 306 |
| unit variant holds no storage | 3463, 1184, 1059, 1050 | 306 |

No flow store failed. Outside `direct_mir_*` (764 before, 744 after) only
"a borrowed ref boundary value cannot escape through a call" changed,
170 -> 150. None was added.

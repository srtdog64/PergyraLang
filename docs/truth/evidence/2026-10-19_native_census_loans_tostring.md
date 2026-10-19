# 2026-10-19: canonical census after loans and ToString

Recorded by the Claude lane on branch `claude/p0-native-checker`.

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the
  [flow complete record](2026-10-19_native_census_flow_complete.md): a
  scratch tree with errors downgraded, so every owner reports. Times are
  on that scratch build, not a production measurement.
- **Flow analysis:** completes in every row below, 14490 to 14563
  entries, 3.47 GB peak.

## Downgraded refusals, by commit

| Commit | Refusals | Change |
|---|---|---|
| `ebf6f2742` | 8397 | Split and Slice views modelled |
| `4f1c33b93` | 7778 | String loans of borrowed aggregates |
| `0fc29f133` | 4988 | ToString modelled; reused frames checked by the last evaluation |
| `4d5fc0ae0` | 4957 | a moved loan owner refused; an unchanged by-value owner admitted |
| `aa4fc2022` | 3667 | results of any type lend `ref` inputs; receivers not lent |

The `aa4fc2022` row was measured before that commit's handle exclusion,
which only keeps slot handles and subjects out of lending.

## Where the refusals are at `aa4fc2022`

| Count | Module family |
|---|---|
| 1360 | `compiler/direct_mir_*` (per-program-shape routes V1 deletes) |
| 1115 | `semantic/` |
| 336 | no module attributed |
| 257 | `mir/` |
| 210 | `mir_lower/` |
| 176 | `codegen/` |
| 213 | other |

| Count | Diagnostic |
|---|---|
| 2194 | borrowed ref value escapes through a helper/function call |
| 320 | Array argument call requires preserved array storage |
| 192 | String element crosses an opaque boundary |
| 136 | borrowed ref value escapes through channel send |
| 131 | String borrows a released `Array<String>` element |
| 112 | `ArrayPushOwnedString` on escaped array storage |
| 582 | other (each kind under 60) |

## Cost

The census wall time doubled with ToString modelled: 741 s at `ebf6f2742`,
1571 s at `0fc29f133`, 1676 s at `aa4fc2022`. Flow work stayed flat
(about 18900 body evaluations), so the added time is in the owners that
run after the flow analysis. It is not profiled yet.

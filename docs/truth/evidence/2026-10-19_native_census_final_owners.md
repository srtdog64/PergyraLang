# 2026-10-19: canonical census with every final owner

Recorded by the Claude lane on branch `claude/p0-native-checker`.

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the
  [flow complete record](2026-10-19_native_census_flow_complete.md), and in
  addition every final owner runs even after one before it fails, so the
  refusals a failed owner would hide are counted. Scratch probes tag each
  move refusal with why the call moved. Times are on that scratch build.
- **Flow analysis:** completes in every row, about 14600 entries, 3.47 GB
  peak.

## Downgraded refusals, by commit

| Commit | Refusals | Moves | Wall | Change |
|---|---|---|---|---|
| `a4e2649c8` | 6915 | 3654 | | stores into a named Array modelled |
| `b68ae872c` | 6297 | 3149 | 1970 s | by-value formal lent to a getter; stores through access paths move |
| `e23761a3a` | 6293 | 3149 | 1912 s | returned String element lends; DirWalk result relation |
| `bc2aefb30` | 5511 | 2786 | 2606 s | text finished with a non-result allocator is a loan of it |

`collection_owned_element_requirements_finalize` fails on the canonical
program in every row, so in a normal compile the move, allocator lane and
named-call owners never run.

## At `bc2aefb30`

| Count | Diagnostic |
|---|---|
| 2786 | moved value used again |
| 1430 | borrowed `ref` value escapes through a call |
| 315 | String element crosses an opaque boundary |
| 169 | Array argument call requires preserved storage |
| 131 | String borrows a released `Array<String>` element |
| 112 | `ArrayPushOwnedString` on escaped storage |
| 99 | borrowed parameter storage moved |
| 85 | borrowed `ref` value escapes through a channel send |
| 384 | other |

| Moves | Why the call moved |
|---|---|
| 803 | a formal lent to a getter whose loan proof failed (129 `TypedAstArenaAtomText`) |
| 768 | the callee changes, keeps or releases the argument |
| 647 | a getter whose argument is a local, which is moved, not lent |
| 586 | a read-and-return callee whose Array descriptor proof fails (254 `SemanticAstExpressionVerdictFromGraph`) |

| Count | Module family |
|---|---|
| 1816 | `semantic/` |
| 1318 | `compiler/direct_mir_*` (per-program-shape routes V1 deletes) |
| 697 | `codegen/` |
| 517 | `mir/` |
| 308 | no module attributed |
| 240 | `compiler/` (other) |
| 222 | `parser/` (519 at `e23761a3a`) |
| 220 | `mir_lower/` |
| 173 | other |

Calls that still leave a frame unsupported: 11 `None` constructors, all in
`direct_mir_*` modules, and one `ArraySort`.

## Cost

Wall time rose from 1912 s to 2606 s at `bc2aefb30`: frames that were
unsupported are analysed now (work units 5 to 239). Not profiled yet.

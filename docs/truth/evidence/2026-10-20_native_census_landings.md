# 2026-10-20: canonical census after the String and loan copy landings

Recorded by the Claude lane on branch `claude/p0-native-checker` (local
commits, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the
  [final-owner record](2026-10-19_native_census_final_owners.md). Every
  error is downgraded and every final owner runs. Since `c19013296` the
  driver reanalyses the program up to three times to land selected copies;
  the counts below are from the last analysis.
- **Flow analysis:** complete in every valid row.

## Downgraded refusals, by commit

| Commit | Refusals | Moves | Change |
|---|---|---|---|
| `2d30ec766` | 4401 | 2127 | (previous record) |
| `c425c09d2` | 3890 | 1724 | a formal's String stored into an `inout` output lands as an owned copy |
| `c19013296` | 4039 | 1668 | invalid: inflated by the ArraySet fault below |
| `5cf9d7553` | 46141 | n/a | invalid: the flow store failed in the third analysis |
| `db8350709` | 2870 | 1247 | getter loan copies land at creation and at the escape, by the analysis's own node identities |

- **`c19013296`.** An ArraySet checker returned `TYPE_UNKNOWN` after its
  own refusal. That unknown type gave 202 summaries a call escape, so 539
  ref escape refusals appeared that no source change caused.
  `5cf9d7553` keeps checking the ArraySet after the refusal.
- **`5cf9d7553`.** The driver wrapped selected getter calls by the node
  identities of the freshly loaded tree. Semantic analysis renumbers every
  node when it starts, so a lowered `for` loop over a call shifted every
  later identity. The third analysis wrapped other nodes, and its flow
  store failed ("parameter program-point index is unavailable or stale").
  Every later refusal is fail-closed noise. `db8350709` wraps by the
  identities the analysis assigns.

Wall time: `c425c09d2` 2489 s for one analysis (3.47 GB peak);
`db8350709` 10110 s for four analyses (3.98 GB peak), run alongside other
analyses. Neither is a controlled timing.

## By kind

| Kind | `c425c09d2` | `db8350709` |
|---|---|---|
| moved value used again | 1724 | 1247 |
| borrowed `ref` value escapes through a call | 1319 | 945 |
| String element crosses an opaque boundary | 315 | 265 |
| Array argument call requires preserved storage | 107 | 104 |
| other | 425 | 309 |

| Why the call moved | `c425c09d2` | `db8350709` |
|---|---|---|
| a getter whose loan proof failed | 816 | 571 |
| the callee changes, keeps or releases the argument | 633 | 431 |
| a read-and-return callee whose storage proof fails | 158 | 177 |
| a getter on a local | 125 | 69 |

## By module family at `db8350709`

| Count | Family (count at `c425c09d2`) |
|---|---|
| 808 | `compiler/direct_mir_*` (972), per-program-shape routes V1 deletes |
| 800 | `semantic/` (993) |
| 306 | `codegen/` (571) |
| 196 | no module attributed (234) |
| 173 | `mir/` (360) |
| 172 | `compiler/` other (219) |
| 170 | `mir_lower/` (199) |
| 140 | `parser/` (210) |
| 58 | `dir/` (77) |
| 47 | `air/`, `debug/`, `hir/`, `fmt/`, `lexer/` (55) |

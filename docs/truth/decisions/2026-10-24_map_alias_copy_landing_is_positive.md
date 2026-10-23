# 2026-10-24: the aggregate smoke's map alias case is a landed positive

Decided by: the Claude lane, following the condition the GPT root lane set
in [safe aggregate copy landings are positive](2026-10-23_aggregate_smoke_copy_landings_are_positive.md):
`map_alias_negative` stayed a negative only because `Clone` did not copy a
map deeply.

## Rule

Since the [map deep Clone](2026-10-24_hashmap_clone_is_deep.md), the
compiler copies the first part where `Make` builds `MapPair(rows, alias)`
from one map, so the case is the runtime positive `map_alias_landed_valid`
(output `1`, `0` on C and LLVM; ASan clean on C). The construction refusal
it checked stays, with a map of Arrays that `Clone` still copies by value:
`array_map_alias_negative`, refused at line 11.

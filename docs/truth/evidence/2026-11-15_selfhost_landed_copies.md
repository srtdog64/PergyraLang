# 2026-11-15: self-host copy landings and refusals (`667e45554`)

Run by the Claude root lane on WSL `Ubuntu-C-WSL`. The tree is
`/root/ag-selfhost` at `34151bbfd`, the branch that `667e45554` squashes,
with `pgy` and `pgy-self-driver` built there. Main `0d8b9f2bc` differs from
it only in docs/truth and the weekly driver shard scripts.

## The 50 crashes from the wide sweep

These come from the [wide sweep](2026-11-15_four_leg_wide_memory_sweep.md):
47 programs that crashed on the self-host C (sc) or LLVM (sl) leg.

| Change | sc | sl |
|---|---|---|
| crash to clean, with a landed copy | 12 | 0 |
| crash to refused | 27 | 7 |
| still crashing | 2 | 1 |

The three crashes that remain:
- `array_result_unclosed_recursive_array_invalid` (sc) is endless
  recursion, not a heap error. Native refuses it for a missing result
  transfer proof.
- `by_value_getter_result_passed_grown_landed_valid` (sc): no fact
  follows growth that is reached only through a call result.
- `constructor_owned_string_array_reuse_batch_landed_valid` (sl): two
  records each release a member they share.

The three `readonly_map_query_string_*` programs are refused on sc and sl
with `map_string_use_after_mutation`.

## 868 lifetime fixtures, against main

- LLVM leg: no verdict changed. There is 1 crash, as on main.
- C leg: 321 admitted, against 289 on main.
  - Native admits 35 of the new admissions with the same landing, and all
    35 run ASan-clean.
  - Lost: `duplicate_array_landed_valid` and
    `push_pop_duplicate_landed_valid`. The member growth check does not
    see the landing.
  - Lost: `array_alias_landed_valid`. Main admitted it while sharing a
    buffer the callee reallocates.

## Gate inputs moved from refused to admitted

`collection_inout_effect_owner.sh` now expects 33 `*_negative` inputs to
be admitted, and `member_indexed_read_permission.sh` its `missing_restore`
input. Each was compiled on four legs and every
admitted program was run under ASan/UBSan for at most 10 s:

| Leg | Admitted | Sanitizer report |
|---|---|---|
| native C | 34 | 0 |
| native LLVM | 34 | 0 |
| self-host C | 34 | 0 |
| self-host LLVM | 0 (refused) | n/a |

- Self-host C and native C give the same stdout and the same exit status
  for all 34. Most exit statuses are non-zero; each is the `Int` that
  `Main` returns.
- Self-host LLVM refuses because MIR does not carry a copy landed at a
  declared routine's argument.

## Gates run

- push self-host shard: 14/14.
- push core shard: 61/61, run at `507bac9d3`. Later commits change only
  expected labels in `collection_inout_effect_owner.sh`.
- native `--emit-c` of `driver_bootstrap_main.pgy`: 808 s.
- codegen bootstrap: gen2 == gen3.
- full driver fixpoint: gen2 == gen3, 239566 lines.

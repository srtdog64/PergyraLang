# 2026-10-20: canonical census after the formal store landing

Recorded by the Claude lane on branch `claude/p0-native-checker` (local
commits, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md):
  every error downgraded, every final owner run, the last of up to four
  analyses counted.
- **Change measured:** a formal's backing an `inout` output keeps lands as
  a copy of the stored value
  ([decision](../decisions/2026-10-20_formal_storage_kept_by_output_is_copied.md)).
- **Flow analysis:** complete; the flow memos changed no answer (the
  census at `0087ac5a4`, before this change, matched `db8350709` line for
  line at 2870).

## Refusals

| Run | Refusals | Moves | Ref escapes | Wall |
|---|---|---|---|---|
| before (`0087ac5a4`) | 2870 | 1247 | 945 | 863 s |
| after | 2637 | 1077 | 890 | 850 s |

- 237 refusals cleared and 4 appeared. The 4 are borrowed `ref` escapes
  in `direct_mir_*`, driver and semantic routines: uses that were refused
  as moves now reach the escape check.
- Each analysis round still clears some: 3890, 2743, 2666, 2637.

| Why the call moved | before | after |
|---|---|---|
| a getter whose loan proof failed | 571 | 571 |
| the callee changes, keeps or releases the argument | 431 | 227 |
| a read-and-return callee whose storage proof fails | 177 | 211 |
| a getter on a local | 69 | 68 |

The storage row grew because some calls that the store no longer moves
still fail their storage proof, which now names them.

| Family | before | after |
|---|---|---|
| `compiler/direct_mir_*` | 808 | 782 |
| `semantic/` | 800 | 697 |
| `parser/` | 140 | 62 |
| `codegen/` | 306 | 306 |
| other | 816 | 790 |

## An experiment that cleared nothing

Taking an `inout` argument's escapes from the callee's summary instead of
an unconditional call escape, in the slot analyzer alone, left all 2870
refusals. Doing it in the backing frame as well cleared only 32, and
admitted a loan replaced or grown by an `inout` callee. So that
unconditional escape carries loan safety, and it was not changed.

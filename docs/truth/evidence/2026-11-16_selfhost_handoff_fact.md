# 2026-11-16: self-host hand-off fact and the last ownership crashes (`98bcf67ec`)

Run by the Claude root lane on WSL `Ubuntu-C-WSL`. The tree is
`/root/td-own` with `pgy` and `pgy-self-driver` built from the content of
`37c866294`, the branch `98bcf67ec` squashes. The baseline is the 868-fixture
measurement on main-equivalent binaries in `/root/slr`
(see [selfhost landed copies](2026-11-15_selfhost_landed_copies.md)).

## The two crashes

Both programs are now clean on native C, native LLVM and self-host C.
Self-host LLVM refuses both.

- `by_value_getter_result_passed_grown_landed_valid` was a use after free
  on self-host C. The Clone now lands at the grown call.
- `constructor_owned_string_array_reuse_batch_landed_valid` was a double
  free on self-host LLVM. Direct MIR cannot carry the landed Clone or
  the constructor input has no hand-off fact, so it is refused.

## 868 lifetime fixtures, self-host legs, ASan/UBSan

| Leg | main | `98bcf67ec` |
|---|---|---|
| self-host C admitted | 321 | 321 |
| self-host LLVM admitted | 140 | 167 |
| sanitizer reports, C / LLVM | 1 / 1 | 1 / 1 |

- On both builds the one report per leg is the stack overflow of
  `inout_cycle_read_positive`, which recurses endlessly. Native LLVM
  reports it too.
- Self-host LLVM: 28 programs changed from refused to admitted, and one
  from admitted to refused.
  - All 28 run clean. 27 give the same stdout and exit status as native.
  - The 28th, `exclusive_failure_value_before_growth_positive`, is refused
    by native because it names a parameter `fail`, a reserved word
    (blocker 5).
  - The one now refused is `field_ctor_duplicate_readonly_positive`.
    Direct MIR cannot carry its constructor-argument Clone.
- The 31 programs refused for "storage another holder still has":
  - 27 are admitted;
  - 1 is still refused for the same reason;
  - 2 are refused for no hand-off fact (their inputs are member moves);
  - 1 has no ownership row.

## Wide sweep (reported by the track)

Over 2597 programs, against older `/root/cc-chk` binaries:

| Leg | heap use after free, before | after |
|---|---|---|
| self-host C | 39 | 0 |
| self-host LLVM | 10 | 0 |

The only sanitizer report left is the endless recursion on self-host C.
A new hole the sweep found was fixed in the same commit:
`ref_loan_local_stored_into_inout_output_landed_valid`. In it, a released
ref loan sits beside an inout argument that can hold a String.

## CI

Run 43741218196 on `98bcf67ec`: all 33 jobs green.

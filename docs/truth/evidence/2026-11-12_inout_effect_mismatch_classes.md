# 2026-11-12: the collection_inout_effect mismatches, by cause

Run by the Claude root lane on the side tree `/root/cc-phi` (main `c7e599f58`
plus the working changes named below), WSL `Ubuntu-C-WSL`. The verdict loop
of `tests/self_hosted/parity/collection_inout_effect_owner.sh` was run with
its `cmp` replaced by a collector, so every mismatch is listed, not the first.

## Counts

| Probe built from | Mismatches |
|---|---|
| `1acc31c5b` (first merged-probe run) | 59 |
| `c7e599f58` (after steps 1 to 3) | 41 |
| plus the receiver copy check and the defer fix | 35 |
| plus the fixes and expectation changes below | 0 |

## Classes

1. **Regression from step 3** (`13dea684c`):
   `exclusive_map_exit_branch_deferred_read_negative`. The field-copy
   relaxation in focus admission counted the alias read inside a `defer`
   as a read before the group. Fixed: a routine with a defer keeps every
   alias inside the group.
2. **A grown builtin receiver whose copy is read later** (5):
   `exclusive_map_observed_source_{growth,backedge}_negative`,
   `exclusive_{set,list,queue}_shared_mutation_negative`. The sink entry
   check followed copies only for user `inout` formals and left Set, List
   and Queue mutators out. Native refuses the Set/List/Queue inputs and
   copies the Map ones (`pgy_map_clone_int` in its C).
3. **Borrowed String push now copied** (14): the `inout_event_*`,
   `inout_borrowed_push_*`, `inout_*_drop_negative` family,
   `inout_shallow_accumulator_owned_mutation_negative`,
   `builtin_completion_nested_borrowed_negative` and
   `builtin_retention_unregistered_call_negative`. Both front ends lower
   `ArrayPush(values, "borrowed")` as a push of an owned copy (native C
   `StringConcat("borrowed", "")`, self-host driver C
   `pgy_concat("borrowed", "")`), as the 2026-10-19 String landing decision
   requires, so no borrowed element remains. Native C runs clean under
   ASan (leak detection off) for all 13 it admits; the installed self-host
   driver (2026-11-11 build) runs clean for the 10 it compiles and refuses
   the other 4 at a direct-MIR route.
4. **Element loan carried by a call result into a struct** (1):
   `owned_element_retaining_text_negative`. The self-host tracked an
   indexed String loan only into a `String` binding; `NamedFrom(a[0])`
   returned a struct holding it. The installed driver's C reads freed
   memory (ASan heap-use-after-free); native copies and runs clean.
5. **Focus write-back versus native copy** (3):
   `exclusive_map_loop_{missing,break,continue}_restore_negative`. The
   self-host admitted them through focus recovery, which writes the part
   back at each exit; native copies the part. For the same program with
   a final `Log(MapSize(tables.rows))`, native prints `0` and the
   self-host emitted C prints `100`. Decided in
   [the 2026-11-12 decision](../decisions/2026-11-12_unwritten_field_copy_is_a_copy.md):
   the self-host now takes a Clone where the part is defined. Its emitted C
   then prints what native prints on every focus input both admit (13
   baselines, 4 Map controls, the 3 loop inputs), and the canonical driver
   stays admitted (1,310 s). A refusal-only variant was tried first: the
   canonical driver was refused at `MirIntentExecutionCaptureCompensations`.
6. **Positives native admits and the self-host refuses** (8):
   `descriptor_text_escape_{read,loop,forward}_positive`,
   `own_formal_shallow_forward_positive`, `exclusive_thread_inout_positive`,
   `exclusive_inout_repeat_alias_positive`,
   `exclusive_alias_inout_then_read_positive`,
   `exclusive_scalar_set_holder_positive`. Self-host generality gaps.
7. **Positives both front ends refuse** (5):
   `borrow_formal_{alias_read,seed_read,assign_alias}_before_positive`,
   `inout_index_{formal_shallow,before_unknown}_positive`.
8. **Both refuse with another code** (3):
   `descriptor_text_escape_opaque_negative` and
   `inout_index_string_lambda_negative` (a lambda capturing a local:
   self-host `undefined_symbol`, native "Lambda capture of local ... is not
   supported"), `exclusive_scalar_set_holder_growth_negative` (self-host
   `move_from_released`; native admits it).
9. **Copy into a constructor** (1): `field_ctor_local_set_negative`.
   Native clones `values` into `Carrier`; the self-host admits with a
   compiler Clone in its semantic C (output `1`, equal to native). Its MIR
   route does not carry compiler Clones.

## Gate rows that were never reached

The verdict loop had never run to its end before the merged probe, so the
diagnostic tables after it had not been checked against the current tree:
24 rows named an older boundary or an earlier refusal site of an input that
is still refused, two identity mutations shared numbers with another check
(129 to 132, 125 to 128), the multi-ordinal check pinned the 33-row
retention registry (now 47 rows), and two inputs were passed as absolute
paths the probe cannot open. The formal key probe recomputed formal effects
without the owned String landings the body uses, so it reported drift on
every input that pushes a literal into an `inout` formal.

# Native checker: a foreach String binding lends its element (2026-11-20)

Source: main `9c4ac12af` plus this change, on branch
`claude/wizardly-mcclintock-207654`. Trees: WSL `Ubuntu-C-WSL` `/root/fe`
(base binary kept as `/root/fe-base-bin/pgy`) and `/root/fe2` (final
change); Linux gcc 13.3 with ASan/UBSan, LLVM 18. Rule:
[decision](../decisions/2026-11-20_foreach_string_binding_lends_its_element.md).

## Before

The native checker (`--native-pipeline`, both backends) recorded the
binding of `for name in names` over `Array<String>` as an independent
String with no loan. On base, each of these compiled on C and LLVM, printed
freed memory and failed ASan with heap-use-after-free:

| Program shape | Base |
|---|---|
| `ArraySet(names, 0, ...)` then `Log(name)` in the body | admitted, UAF |
| `ArrayPop(names)` then `Log(name)` (second iteration) | admitted, UAF |
| `ArrayDropOwnedStrings(names)` then `Log(name)` | admitted, UAF |
| a foreach over a `Slice<String>` of `names`, then `ArraySet(names, ...)` | admitted, UAF |
| `kept = name` in the loop, `ArraySet` after it, `Log(kept)` | admitted, UAF |
| `ArrayPush(out, name)` in the loop, `ArraySet(names, ...)`, `Log(out[0])` | admitted, UAF |
| index form `let name = names[1]; ArrayPop(names); Log(name)` | admitted, UAF |
| a loan read at the top of the next iteration after the body set its element (outer `let` in `for` and in `while`, a kept binding) | admitted, UAF |

Already refused on base: `let name = names[0]; ArraySet(...); Log(name)`.
Not a use-after-free on base: reassigning the iterable from a call result
(the old storage leaks; emitted code frees nothing on reassignment), for the
binding and for `let`.

## Change

- The loop owner binds the foreach variable through the String loan owner,
  at its declaration and again at the start of every analysed iteration
  (`semantic_indexed_string_borrow_bind_iteration`, new TU
  `indexed_string_loop_loan_owner.c`): an `Array<String>` iterable's storage
  source, any other iterable an unknown source.
- An owned `ArrayPop` retires the loans of its receiver's source, as an
  owned `ArraySet` does (`semantic_indexed_string_borrow_invalidate_owned_element`,
  renamed from `..._owned_set`).
- `for` and `while` analysis checks the body again when a pass retired a
  loan declared outside the loop, at most once more per visible loan, and a
  loop summary is not replayed while a visible loan is retired.
- Refusal text names ArrayPop among the releasing builtins.

Objective card: no admitted program reads a String an owned release freed.
Fact owner: the indexed String loan owner (`indexed_string_borrow_owner.c`),
which already owned `let x = xs[i]`. Last consumer: the use check
(`semantic_indexed_string_borrow_validate_use`) and the store landings that
read the loan source. Forbidden: a scan of the loop body for mutator names,
a per-iteration copy, a larger pass budget for other loop facts.
Falsifiers: the new gate's negatives, each admitted by an earlier build.

## Builds

- base: `/root/fe-base-bin/pgy`, main `9c4ac12af`.
- intermediate: the binding and pop rules without the loop re-check
  (`/root/fe/bin/pgy`).
- final-pre: the loop re-check, extending a range known to run once
  (`/root/fe2/bin/pgy`).
- final: this change (`/root/fe2/bin3/pgy`). It differs from final-pre only
  in giving such a range no extra pass.

## Results

| Check | Result |
|---|---|
| `foreach_string_borrow_refusal_smoke.sh` (new; 9 negatives refused at the read with no executable, 5 positives with expected output, each on C and LLVM) | final PASS. base FAIL at `set_while_bound_invalid (c)`; intermediate FAIL at `next_iteration_outer_read_invalid (c)`; final-pre FAIL at `single_iteration_range_valid (c)` (refused) |
| 22 probe programs, ASan on native C plus native C and LLVM runs, final-pre | every set, pop, drop, Slice, kept and next-iteration case above refused at the read; the push runs equal and ASan-clean as `ArrayPushOwnedString`; reading before the release, reassignment from a call, a nested member iterable and the Slice index form unchanged; `ArraySet` on a by-value parameter stays refused as a non-`inout` mutation and `names = []` in the body still stops at MIR validation (`invalid-empty-birth`), as on base; the three `List<String>` programs still heap-use-after-free (OPEN in the decision) |
| Lifetime fixtures (`collection_field_lifetime`, `aggregate_storage_transfer`, 868) under ASan, `/root/fx_all.sh` | verdicts identical to base for intermediate and final; the only ASan reports are the two endless-recursion stack overflows (`inout_cycle_*`), as on [2026-11-16](2026-11-16_llvm_legs_at_o0.md) |
| Native ownership gates on final-pre: native unsafe-negative (39/17/8), loop-local collection cleanup, String result control, array member mutation, aggregate storage transfer, collection fact projection, memory String safety, String clone release, loop flow summary, iteration type fact | PASS |
| Same, needing a self-host driver (driver built below), on final and base: memory fail-closed, string-window builtins, TextBuilder owner, TextBuilder diagnostic isolation | PASS on both |
| `function_param_flow_summary_smoke.sh` | FAIL on base and final-pre at `local_copy_collection_changed_invalid`: its `ListRemoveCopy` declares `let copy` and is now refused as an immutable rebinding. With that one fixture removed in a scratch copy: PASS on base, intermediate and final-pre (C and LLVM) |
| `hashmap_owned_string_provenance_smoke.sh` (not in CI) | FAIL on base and final alike: `map_keys_shallow_push` and `map_keys_shallow_set` are admitted |
| Native-oracle self-host gates on final with the driver below: general-route collection iteration (14 programs, 2 releasing String walks refused), collection programs, default-route native runtime parity, loop stack storage | PASS |
| `make self-host-compiler` with the intermediate native checker | PASS (gen0 seed admitted, DRV-2 installed; 3184 s beside other jobs) |
| The 20 sources the codegen bootstrap compiles natively (codegen, MIR lower, parser, semantic, lexer, fuzz generator, 14 tools), `--emit-c` | admitted by base, final-pre and final with byte-equal C |
| MIR verdict sweep, `--mir-json`, 5224 programs (`tests/`, self-host fixtures, `examples/`), final-pre | no program changed its MIR or its verdict, apart from runs that hit the 90 s limit (next row) |
| Gate reachability, gate subject, build source inventory, docs truth size | PASS |
| `semantic-tu-size` (weekly) | 32 TUs over 599 before and after; the two loop-facing functions are a new 35-line TU |

The sweep's 90 s limit stopped 72 large self-host probe fixtures in at
least one run (59 in both; the base run had more load). Rerun on base and
final with a 1200 s limit, all 72 finished: 65 admitted with byte-equal MIR
and 7 refused with identical error lists. So none of the 5224 programs
changed verdict or MIR.

Canonical DRV-2 input (`src/self_hosted/compiler/driver/driver_bootstrap_main.pgy`),
native `--mir-json`, run one after the other beside other jobs:

| Build | Exit | Errors | Peak RSS | Wall | MIR |
|---|---|---|---|---|---|
| base | 0 | 0 | 2,596,784 KiB | 38:43 | 458,765,537 bytes, `22532d0dbc1c` |
| final | 0 | 0 | 2,597,200 KiB | 20:33 | byte-equal |

The wall times differ with the machine's load. Equal MIR and no new error
mean the change found nothing to refuse or copy in the canonical program.

## Not run

- Required CI, and the codegen bootstrap, gen2 == gen3 and installed-driver
  jobs with the final native checker. `make self-host-compiler` ran with the
  intermediate one; the final one emits byte-equal C for every source the
  bootstrap compiles natively.
- `List<String>`: no change (decision, "Not covered").
- The self-host general route's own refusal for these bodies is slice 2 of
  the [foreach directive](../../agent_work_directives/general_route_runtime_foreach_2026-11-19.md).

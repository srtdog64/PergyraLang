# 2026-11-16: a record or call result is the only owner of what it takes

Run by Track I of the Claude root lane on WSL `Ubuntu-C-WSL`, tree
`/root/ti-own` (branch `worktree-agent-a2bbd43ca236d727c`, base `f1456722d`).
Baseline: `/root/tg-own/bin` (main `f1456722d`) and the -O0 sweeps in
`/root/o0-sweep` (868) and `/root/o0-wide` (2602), see
[LLVM legs at -O0](2026-11-16_llvm_legs_at_o0.md).

## Cause

`post_field_return_negative` double-freed on the self-host LLVM leg:

- `pair = Pair(first, second)` got the release recipe of both fields, and
  the record's closure marked `first` and `second` transferred;
- `return first` had already marked `first` transferred (the return
  closure runs before the record pass);
- `transferred` is one flag per local and the closure stops at a set
  flag, so the second owner went unnoticed. `Make` freed `pair.first` at
  exit and `Main` freed the returned array again.

The same pass let two records take one local, a record and an alias call
result take one local, and a record defined in a loop take, on every pass,
a local defined before the loop (the replacement release frees what the
next pass takes again).

## Rule

Owner: `DirectMirOwnedLocalLifetimeFromPlan`, in the pass that gives a
named record or alias call result its release recipe
(`owned_local_lifetime_owner.pgy`). Before the grant,
`DirectMirOwnedLocalStorageTakenTwice` (`owned_storage_exclusive_take_owner.pgy`)
walks the locals the recipe follows:

- a local another owner already took (a return, an `own` callee, another
  record) is refused with `owned local taken by a record or call result
  is already held by another owner`;
- a local the taker takes again on a loop pass that does not define it
  anew is refused with `owned local taken by a record or call result is
  taken again on each pass of a loop`.

A local with a granted recipe keeps the inputs it took, so a record that
takes another record whole still has one owner.
`DirectMirScalarProgramOperationInCycle` takes a renewing operation for
the loop test; the `Array<String>` replacement check passes -1.

## Probes (self-host LLVM, -O0; ASan and glibc `MALLOC_CHECK_=3`)

| Program | main | branch | native LLVM |
|---|---|---|---|
| record field, then `return` the local (`Array<Int>`, `Array<Bool>`, `String`) | double free / use after free | refused | refuses `Array<Int>`, `Array<Bool>`; admits `String` with a copy |
| record of a record, then `return` the inner local | double free | refused | refuses |
| record, then the local returned on one branch | double free | refused | refuses |
| two records take one local; two wrappers take one record | double free | refused | admits with a copy |
| record and alias call result take one local | double free | refused | admits with a copy |
| `return Rows(values)` beside a local record of `values` | double free | refused | admits with a copy |
| record in a loop (or inner loop) takes a local defined outside | double free | refused | admits with a copy |
| record of a record; record and a read of its local; record of a local the loop defines anew; returned record | clean | clean | clean |
| two records of one local on exclusive branches | clean | refused | admits |
| `own` callee and a record take one local | clean (the callee keeps it) | refused | refuses |

The last two rows are refusals of programs main ran clean. The first is
conservative: the rule does not separate exclusive paths. Neither shape is
in either corpus.

## Sweeps (LLVM IR at -O0)

Each admitted self-host and native LLVM program ran under ASan and again,
built without ASan, under glibc `MALLOC_CHECK_=3` with
`libc_malloc_debug.so.0` preloaded.

| Corpus | Leg | main | branch | Changes |
|---|---|---|---|---|
| 868 lifetime fixtures | self-host LLVM | 167 admitted | 166 | `post_field_return_negative`: double free -> refused (new rule) |
| 868 | self-host C | 321 (Track G) | 321 | none; no program reaches the new refusal |
| 868 | native LLVM | 568 | 568 | none; verdict lines equal |
| 2602 programs | self-host LLVM | 670 | 670 | none; verdict lines equal |
| 2602 | self-host C | not swept on main | 1461 | no program reaches the new refusal |
| 2602 | native LLVM | 1995 | 1995 | none (four panic programs timed out under load and panicked as on main when rerun; one known garbage ASan kind string) |

Reports left on the self-host LLVM leg: the endless recursion of
`inout_cycle_read_positive` (ASan stack overflow, glibc SIGSEGV) and the two
intended panics of `derived_divide_*`. No double free or use after free on
either corpus, under ASan or glibc. Six programs still emit IR clang
rejects (`ccfail`), as on main.

The new refusal only stops a program; when it does not fire, the lifetime
fact and the emitted C and LLVM are unchanged. The `Array<String>`
replacement check calls the cycle test with no renewing operation, as
before. The self-host LLVM sweep was repeated with the final driver (walk in
its own owner file, rebuilt after a WSL restart): same verdict lines.

## Gates

- `direct_mir_array_string_cleanup_fact_owner.sh`: PASS (refused 14+4,
  admitted 6x2 under glibc heap checks). Main's driver fails it:
  `record_then_return --emit-llvm cleaned up storage it does not own`.
- Native oracle C of `driver_bootstrap_main.pgy` on the final sources:
  0 errors, 27 warnings, RSS 2.75 GB.
- Codegen bootstrap: gen2 == gen3 (121411 lines). None of the changed
  files is in the import closure of `src/self_hosted/codegen/main.pgy`.
- `ci-push-linux`, prebuilt pair, shard core: all 61 steps ok. Shard
  self-host: all 14 steps ok (an earlier run with the walk in
  `owned_storage_result_recipe_owner.pgy` failed its 264-line cap, so the
  walk moved to `owned_storage_exclusive_take_owner.pgy`, cap 60).

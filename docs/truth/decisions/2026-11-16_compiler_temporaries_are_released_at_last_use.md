# 2026-11-16: compiler temporaries are released at their last use

The user chose these points from the Claude root lane's design study
([design](../../audits/compiler_temporaries_release_design_2026-11-16.md),
[measurement](../../audits/selfhost_source_c_memory_study_2026-11-16.md)).

## The problem

The installed self-host driver is built by the self-host AST-consuming C
emitter, and that emitter releases almost nothing. The driver's own C has
17,358 `pgy_concat` sites and about 300 drop calls.

On `semantic/main.pgy`, 87-92% of the live heap at peak is unreachable
garbage:
- 42% are String temporaries (`Concat`, `Substring`, `ToString`);
- 57% are Array and record buffers.

Memory therefore grows with the total bytes allocated, not with the live
facts. The one-process source-to-C compile of the DRV-2 entry needs about
23 GB, which does not fit a 16 GB runner.

## Decisions

1. **One owner decides releases.**
   - It is a MIR-level value lifetime plan.
   - It reads the one hand-off rule table
     ([decision](2026-11-16_one_handoff_rule_for_both_front_ends.md)), the
     callee summary rows and the MIR CFG.
   - That table gains rows for builtin result origins and argument
     consumption.
   - MIR carries owned-value facts and a `value-retire` instruction, a
     generalisation of `local-collection-retire`. Emitters only print the
     retires.
2. **Checking and timing.**
   - The validation follows Swift OSSA: every path of every owned value
     ends its lifetime exactly once. A missing or forged row is refused.
   - A value with no fact stays `unproved` and is not released.
   - The release happens right after the last use (Perceus, Mojo, ASAP).
   - There is no reference count and no tracing GC. When a value is not
     provably unique, it is cloned or the program is refused, as before.
3. **The direct MIR emitter consumes the retires first.** It is the V1
   path; the AST-consuming emitter is to be deleted.
   - Slice 0 counts every reason direct MIR refuses the DRV-2 entry.
   - If that distance is long, the AST emitter may take String
     statement-end retires as a temporary step only.
4. **A binding with a fresh value on one path and a borrowed one on
   another** gets a compiler copy on the borrowed definition, so it is
   always owned. There is no runtime ownership flag.
5. **Handed-off storage is released by the callee from slice 3.**
   - This reopens the [2026-11-15 decision](2026-11-15_handed_off_array_storage_is_not_released.md):
     the compiler is the real program that needs that memory back.
   - Slice 3 adds the consuming-parameter fact, and native uses the same
     fact.
6. **The self-host comes first.** Native takes the value-retire rows by
   slice 3; until then a native MIR without them leaks, which is safe.
7. **A peak-RSS ceiling becomes an executed CI gate** on fixed mid-size
   inputs.

## Slices (expected bounds, estimated)

| Slice | What it releases | DRV-2, one process |
|---|---|---|
| S1 | fresh String temporaries, the old value in `x = Concat(x, …)`, String locals after their last use | about 15-17 GB |
| S2 | Arrays that die inside a routine, including per-iteration Arrays | about 9-11 GB, which reaches the 16 GB target |
| S3 | records, fact bundles, handed-off storage | about 3-5 GB |

## Deleted when the plan owns releases

- The decisions inside the emitters:
  - the `owned_local_lifetime_owner` verdict part;
  - the fresh-result fixed point;
  - `pgy_retiring_string_observer_*`;
  - the Array<String> legacy default.
- Native's region arena path and `private_uses`.
- Manual drops in the compiler sources.

## Gates

- ASan on C and on LLVM IR built at `-O0`.
- A double-free witness.
- Four forged-retire witnesses: a retire duplicated, added, moved, or
  missing.
- gen2 == gen3.
- A peak-RSS ceiling.

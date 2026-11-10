# Ownership finish line, measured on 2026-11-11

Measured by the Claude root lane on branch `ci/all-green` (main `e875e9e90`
plus the branch commits up to `839f89d54`). This record is not edited after
it is written.

The V1 ownership finish line has four parts
([V1 scope](../../agent_work_directives/v1_scope_ownership_and_selfhost_generality_2026-10-14.md)).
Each is measured below against the canonical compiler program
(`src/self_hosted/compiler/driver_bootstrap_main.pgy` and its import closure).

## 1. Zero ownership refusals on the fixed DRV-2 input

Met for the native checker. The native compile of the canonical program
ended `0 error(s), 26 warning(s)` at `e6ca0e3f3`, and CI at `e875e9e90`
built the driver through it and reached gen2 == gen3 (237,109 lines).

## 2. No manual drops; only explicit Clones at real sharing sites

Not met. Counted outside comments and string literals in the import closure,
at the 2026-10-25 decision (`3dac8b4ac`, 1,948 files) and now (2,161 files):

| Call | 2026-10-25 | now |
|---|---|---|
| `ArrayDrop` | 144 | 144 |
| `ArrayDropOwnedStrings` | 70 | 67 |
| `AllocatorDestroy` | 46 | 47 |
| `Clone` | 46 | 143 |

The manual drops were not reduced after the decision. The Clone count grew
by 97, 67 of them in `mir/intent_execution_fact_owner.pgy`, which copies a
fact's arrays one by one into a new fact. None of the added Clones has the
record the 2026-10-25 decision and `AGENTS.md` ask for (reason, missing fact,
last consumer, removal condition).

## 3. Positive and negative gates preserved

Not met at `e875e9e90`. Examples:

- the self-host checker accepts the string window negative
  `reject_cloned_record_source_unproven`, which the 2026-11-07 staged driver
  refused;
- copies that the checker inserts silently dropped writes the source relied
  on through shared storage: the future lifecycle checker and the canonical
  identity epoch (both fixed on the branch, found by a scan for locals copied
  from another value whose writes are never read again).

## 4. Both goals on one published commit with CI green

Not met. Required CI at `e875e9e90`: build-linux 18 of 55 steps red, the
contracts shard 8 of 11, backend compare 7 shards (8 cases), and the fixed
point job ran out of its 60 minutes after gen2 == gen3.

## Reading

The native checker admits the canonical program without refusals. That is
the first of four conditions; the other three are open. The re-evaluation
the 2026-10-25 decision set for 2026-11-08 has these numbers as its input.

# 2026-10-24: landing census with the chain walk

Recorded by the Claude lane on branch `claude/p0-native-checker`, on the
tree of `df32ffb0e` plus the chain walk
([decision](../decisions/2026-10-24_refused_loan_copies_its_chain_in_one_analysis.md)),
local and not pushed. Census tool as in the
[receipts record](2026-10-24_native_census_string_receipts_after_rows_repair.md)
(receipts pass forced after every final owner, every error downgraded), on
the unchanged canonical input `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
plan1 is that record's run (`f22d406d9`).

## Runs

| Run | What the move owner copies for a refused loan | Analyses | Wall | Copies | Final (outside `direct_mir_*`) |
|---|---|---|---|---|---|
| plan1 | the first place it leaves | 10 | 2734 s | 3382 | 116 (75) |
| plan2 | every place the one walk saw it leave | 4 | 1047 s | 3390 | 116 (75) |
| plan3 | the chain, each walk reading the earlier copies as made | 4 | 1655 s | 3378 | 119 (78) |
| plan5 | plan3, also reading selected stores as copies and copies as going nowhere | 4 | 1015 s | 3376 | 119 (78) |
| plan6 | plan5, keeping the chain when the owner still changes and the getter cannot be copied | 4 | 1156 s | 3390 | 116 (75) |

Peak memory was 3.98 GB in every run. plan3 ran beside other builds and
smokes, so its wall time is not comparable. In each run the last analysis
selected nothing new. plan6's final refusals have the same kinds as plan1's.

## Where the sets differ

- plan2's 8 extra copies sit in three routines; its walk read a binding's
  origins without flow order, so every read of a variable the loan reached
  counted as a place it left, including one before the loan.
- plan3 and plan5 add three refusals, all of `base` in `ApplyPostfixFact`
  (`expr_postfix_owner.pgy:199, 251, 276`). With `PGY_CENSUS_CHAIN` the
  walks for the loans made at lines 159 and 228 find ten copies each, after
  which the loan stays but its owner still changes; the getter cannot be
  copied, so nothing was selected. plan6 copies those chains.
- plan6 against plan1: 22 extra, 14 missing. 5 of the extra copy a getter
  call where plan1 copied one place the same loan left. 17 are later links
  of chains (`EmitStmtList` 7 with the chain at its cap of 16,
  `SelfMirRoutineExpressionRuntimeAbiAttach` 6,
  `SelfMirIntentCleanupInvalidationsAppend` 2, `SelfMirIntentRoutineBuild` 2).
  9 of the missing are plan1 copies plan6 does not need (5 in
  `DirectMirScalarCfgGraphPlanFromAdmitted`).

## Gates on the plain plan6 tree

- `tests/function_param_flow_summary_smoke.sh` passes (C and LLVM runtime
  outputs included).
- `tests/aggregate_storage_transfer_smoke.sh` passes.
- `tests/string_result_control_ownership_smoke.sh` fails on plan1's tree
  and on plan6's alike: its C fresh factories do not compile there.

Mutations, with the flow summary smoke made to list every failure:

| Mutation | Failures |
|---|---|
| no second walk | 5: the three getter-loan push fixtures, the owned-then-handed fixture taking 2 landing analyses, and `string_element_kept_by_callee_landed_valid` (an extra Clone in `Lines`) |
| selected stores not read as copies | 4: a Clone beside each owned push, and 2 landing analyses |
| copies follow their source's destinations | none |
| owner change never keeps the chain | none (the census difference of plan5 and plan6 is its record) |

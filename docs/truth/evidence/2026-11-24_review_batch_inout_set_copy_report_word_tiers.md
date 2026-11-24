# Inout set releases, copy report, word tiers and CI shards (2026-11-24)

Source: main `0a9053745` (CI [45396651059](https://github.com/srtdog64/PergyraLang/actions/runs/45396651059)
green, 33 jobs) plus this change. Trees: WSL `Ubuntu-C-WSL` `/root/ts2` and
`/root/ts3`. Native reference: `bin/pgy` built from `cc27d42c6`. Drivers:
base `driver_f5` (the `0a9053745` source, release); `driver_g2` (the inout
change alone); `driver_h2` (the whole change). Plan:
[review improvements](../../agent_work_directives/review_improvements_and_sot_closure_2026-11-24.md).

## 1. An inout callee's set releases an element

Of the 718 programs native refuses, the base driver's default C route ran 91.
Under ASan on default C: 79 clean, 5 stopped by the runtime slot guard
(`slot_contract/reject`, exit 134, as intended), 1 endless recursion, and 6
read freed memory, all in `inout_call_string_loan/`: a String loan of an
array is read after a call whose `inout` callee does an owned `ArraySet` on
that array. The self-host requirement producer
(`ast_collection_owned_element_parameter_requirement_owner.pgy`) seeded
"the callee releases an element" only for `ArrayPop` and a deep drop; native
records the set too (`frees_elements`, 2026-11-20). It now seeds an owned
`ArraySet` statement on an `inout` or `own` `Array<String>` formal.

- The six are refused on the MIR producer and both default routes.
- Three `_valid` programs native runs are refused as well
  (`early_return_branch`, `else_branch_read`, `loop_call_then_break`): the
  release is in a branch that returns, breaks or is the other arm. The loan
  analysis follows syntax order, not paths; the same programs with the set
  written in place of the call were refused before this change.
- An indexed assignment in the callee (`names[0] = v`) is not seeded: it is
  refused earlier (`unproved_formal_element_use_entry`) on both drivers, so
  no executable case reaches the seed.
- Sweep (2,746 programs, `driver_g2` against `driver_f5`): only those nine
  moved. Default C: 1,421 -> 1,418 equal, runs-a-native-refusal 91 -> 85;
  LLVM 831 -> 828, 24 -> 18. Canonical DRV-2 MIR byte-equal to `driver_f5`'s
  on the same tree; 38 tool verdicts and MIR unchanged.
- Gate `element_release_loan_refusal_owner.sh`: one more refusal
  (`inout_callee_set`).

## 2. `pgy --report-copies`

A copy the compiler inserts (a Clone landing) was visible only in the MIR
JSON. `pgy --report-copies FILE` runs the installed self-host driver's front
end (`--report-copies-verified`) and prints one line per inserted copy, in
source order:

```
copied_initializer.pgy:3:5: copy of `items` (value of `before`)
moved_twice.pgy:23:5: copy of `a` (argument 1 of `AddBlock`)
element_argument.pgy:17:9: copy of `parts[i]` (argument 2 of `Join`)
```

The lines come from the ownership verdict's `compiler_clone_expression_node_ids`
(`driver_rung2_copy_report_owner.pgy`); nothing is re-derived. A program
with no inserted copy prints nothing; a refused program prints its
diagnostic and exits 1; `--native-pipeline --report-copies` exits 1. The
nine landed fixtures of `selfhost_compiler_clone_carriage_owner.sh` all
report their landings. The place is read from the graph (call argument,
initializer); the reason a copy was needed is not reported yet.

- Gate `copy_report_owner.sh` (push core step): PASS on `pgy_h2` +
  `driver_h2`; on the base binaries it fails at the first case
  (`unknown option '--report-copies'`).
- Sweep: `driver_h2` equals `driver_g2` on all 2,746 programs, both routes.

## 3. Word tiers (docs only)

No word is removed. `docs/language/language_word_tiers.tsv` assigns each of
the 148 registry rows tier 1 (core, 31 words) or tier 2 (for when the domain
grows, 117 words); `scripts/render_language_word_tiers.py` checks it against
`src/lexer/language_keyword_registry.def` (every word once, in registry
order) and renders `language_word_tiers.generated.md`. The registry gate
(`language_keyword_registry_smoke.sh`) runs the check; changing a tier in
the table without regenerating fails it. README's declaration section starts
from `struct`/`enum`/`func` and names the other five type words as tier 2.

## 4. CI shards

`build-linux` of `0a9053745` took 29.5 of its 30 minutes on a runner 1.27x
slower than the last run before it (steps 1,383 s -> 1,752 s); the release
debug step no longer rebuilding (that commit) is what kept it under.
`selfhost_compile_memory_owner.sh` (138 s normal, 209 s on that runner)
builds nothing, so it moves from the core shard to the self-host shard,
which took 20.5 minutes on the same run. No limit is raised and no step is
removed.

## 5. Decision recorded

[After V1 the native pipeline becomes a frozen seed and oracle](../decisions/2026-11-24_native_pipeline_becomes_frozen_oracle_after_v1.md).

## Gates

Run in `/root/ts3` on main + this change:

- With `driver_h1` (the change before two line-cap folds): the 45 ownership
  gates the Makefile or push steps reach, the semantic owner probe and the
  default-route parity gate: 41 PASS; the 7 that fail fail as on main
  ([list](2026-11-21_selfhost_element_release_loan_refusal.md#ownership-gates)).
  The component contract stopped at two caps
  (`driver_rung2_cli_read_execution_owner.pgy` 152/150,
  `driver_self_host_selection_owner.c` 142/140), folded without raising them.
  `copy_report_owner.sh` failed there only because the gate helper prefers a
  stale `bin/pgy.exe` left in that tree; with `PGY_BIN` naming the new
  launcher it passes. Codegen bootstrap: gen2 == gen3 (125,741 lines).
- Final tree, `driver_h2` (sha256 `95d9e0a2544b3fa3`) and launcher `pgy_h2`
  (`8669b2f3a68fba31`): component contract, `copy_report_owner.sh`,
  `element_release_loan_refusal_owner.sh`, the default-route parity gate,
  `language_keyword_registry_smoke.sh` and gate reachability PASS; canonical
  DRV-2 exit 0, 160 s, peak 4,691,408 KiB.
- The installed-driver and driver fixed-point jobs run in CI.

## Not covered

- Branch-sensitive loan release (the three `_valid` programs above).
- The reason a copy was inserted, in the copy report.
- The 79 native-refused programs that run clean on the self-host route are
  verdict divergences, not memory errors; they are not reconciled here.

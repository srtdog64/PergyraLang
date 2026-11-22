# `resource.region_allocation_plan` closed (2026-11-22)

Source: main `c6c7ff395` plus this change. Tree: WSL `Ubuntu-C-WSL`
`/root/ts2`. Plan: [directive](../../agent_work_directives/selfhost_region_allocation_2026-11-22.md),
slice 4.

## Before

The row was `BRIDGE`. Native semantic analysis owned the region escape
facts and native C/LLVM consumed them. The self-host route allocated the
certified `+` spines from a function region on source C (`8be12fd7a`) and on
direct-MIR C/LLVM (`c6c7ff395`). Its open reason named one remaining
obligation: `src/self_hosted/compiler/region_plan_owner.pgy`, a contract
model with its own manifest, golden and gate
(`selfhost_region_plan_smoke.sh`), that no production route reached.

## Change

- Deleted: `region_plan_owner.pgy`, `region_plan_manifest.pgy`,
  `expected_region_plan_manifest.txt`, `selfhost_region_plan_smoke.sh`, its
  Makefile target and its entry in `redteam-repair-contract-test-smoke`, its
  two line caps, its two OWNERS entries and the artifact kind `region_plan`
  (`artifact_zone_owner.pgy`, 29 -> 28 kinds). No `.pgy` imported the model.
- Registry: the row is `CLOSED`, the model is removed from its consumers
  and `selfhost_unreached_region_plan_model` is added to its forbidden
  fallbacks. Status counts `CLOSED=62 BRIDGE=17 ACTIVE=2`.
- The enforcement gate stays `selfhost_region_allocation_owner.sh`: region
  allocation on source C, direct C and LLVM under LeakSanitizer, heap for
  bound, returned and stored concatenations, and three forged MIR sites
  refused on both backends.

## Results

Driver: release-profile installed driver built from this change.

- PASS: `selfhost_region_allocation_owner.sh` (7 call-duration
  concatenations in 2 function regions on source C, direct C and LLVM),
  `region_user_callee_installed_self_host_owner.sh`,
  `self_hosted_component_contract_smoke.sh`,
  `selfhost_spawn_lane_plan_smoke.sh` and
  `selfhost_reachability_contract_smoke.sh` (the two other artifact-zone
  manifest gates), `region_backend_wiring_smoke.sh`,
  `region_call_temporary_smoke.sh` (C/LLVM, ASan/LSan).
- `tests/repo/gate_script_reachability_smoke.py`: 888 scripts, 0
  undeclared, 0 stale.
- `scripts/sot_registry_gate.py`: 81 authorities, 168 carriers,
  `CLOSED=62 BRIDGE=17 ACTIVE=2`.
- The 75 scripts behind the CI push make steps, run in the tree that
  produced the baseline: every script has the baseline's exit status (61
  pass; the 14 failures are the baseline's environment-local ones).


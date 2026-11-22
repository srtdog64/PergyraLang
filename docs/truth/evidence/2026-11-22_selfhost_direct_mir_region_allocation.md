# Self-host direct MIR allocates call-duration concatenations in a routine region (2026-11-22)

Source: main `3ef78341c` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` and `/root/ts3`. Plan: [directive](../../agent_work_directives/selfhost_region_allocation_2026-11-22.md),
slice 3.

## Before

Source C allocated the certified `+` spines from the function region
(`8be12fd7a`), but the self-host MIR carried no region fact, so the
direct-MIR C and LLVM routes (`--mir-json-backend`, source LLVM, public
LLVM) still emitted `pgy_concat` for them. The installed gate
`region_user_callee_installed_self_host_owner.sh` pinned that heap output on
direct C and LLVM.

## Change

- Producer: `mir/region_site_projection_owner.pgy` turns the semantic region
  escape sites into the program facts' `region_sites`;
  `artifact_lower_owner.pgy` computes them before the AST arena retires; the
  instruction writer emits `expr0_region_sites` / `expr1_region_sites` (graph
  window indices) only when a lane has a site, so other programs keep their
  MIR bytes.
- Admission: `direct_mir_scalar_program_region_site_owner.pgy` reads the
  lane's list. A malformed list or an index past the graph is refused before
  the node loop, a listed node that is not a String `+` inside it
  (`region-site-list`, `region-site-window`, `region-site-kind`); a listed
  String `+` becomes kind 159 `DirectMirScalarProgramExprRegionConcatString`.
- Emission: C `pgy_region_string_concat(&__pgy_region, ...)`, LLVM
  `call ptr @pgy.region.concat(ptr %pgy.region, ...)`. The retiring String
  observer takes the region pointer, so fresh operands such as `ToString`
  are still released. `direct_mir_scalar_cfg_region_scope_owner.pgy` decides
  per routine from its admitted expression rows: the region opens with the
  owned-local initialization and is freed with the owned-local exit cleanup
  and on the Option<Int> `?` early return. LLVM defines internal
  `@pgy.region.concat` / `@pgy.region.free` over malloc, so an artifact
  links with bare `clang -x ir`.
- Gates: `selfhost_region_allocation_owner.sh` gains direct C and LLVM legs
  for three programs (region calls, heap escapes, a `?` early return), runs
  both under LeakSanitizer, and refuses three forged MIR sites (a leaf, a
  malformed list, an index past the graph) on both backends;
  `region_user_callee_installed_self_host_owner.sh` now expects the region on
  direct C and LLVM (direct.ll still equals source and public LLVM) and runs
  direct C under LeakSanitizer.
- Caps: nine scalar-program owner caps and the String core kind owner cap
  are set to their measured sizes.

## Results

Drivers: test profile (`-O0`) and release profile builds of the installed
driver from this change; the previous driver is the `8be12fd7a` build.

- `selfhost_region_allocation_owner.sh` and
  `region_user_callee_installed_self_host_owner.sh` PASS on the new driver.
  Both FAIL on the previous driver (direct C and LLVM still call
  `pgy_concat`). A build of this change without the `?` exit cleanup FAILS
  the region gate: LeakSanitizer reports 8,216 bytes leaked on direct C for
  `region_try`.
- Forged MIR sites (a leaf, a malformed list, an index past the graph) are
  refused on direct C and LLVM.
- 307 candidate gates (every `tests/self_hosted/**/*.sh` that names
  `pgy_concat`, `pgy.concat`, `--mir-json-backend`, `--emit-mir-json` or
  `sha256sum`), run with the release driver: 148 fail. All 148
  were rerun on main `3ef78341c` with the previous driver: 145 fail there
  with the same last line, and `driver_bootstrap.sh` and
  `completeness_ledger.sh` time out at 900 s on both.
  `slice_length_codegen_source_owner.sh` failed once on its 240 s native
  emitter build while other jobs loaded the machine. It PASSES on a quiet
  rerun (277 s; main 286 s).
- MIR bytes: every MIR identity gate that fails reports the same hash on
  both drivers except `one_mir_string_builtin_program_projection.sh`. That
  program has region sites, and its pin was already stale on main
  (`486EC65C...`). The gate now pins `DCCEE508...` and PASSES.
- The 75 scripts behind the CI push make steps, release driver: no script
  fails that passes on the baseline (8 environment-local failures on both;
  6 baseline environment failures pass here).
- `self_hosted_component_contract_smoke.sh` PASS (2,777 line-cap requests);
  `scripts/sot_registry_gate.py` PASS (81 authorities, 168 carriers,
  `CLOSED=61 BRIDGE=18 ACTIVE=2`).
- Codegen bootstrap (`codegen_bootstrap.sh`) PASS in 1,121 s: gen2 == gen3
  (125,504 lines), "SELF-HOSTING OK".

Not changed here: source C leaks the `ToString` operands in `region_try`
(48 bytes; 60 bytes before the region work). A variant with a
`ref String` formal is refused on both routes (source C: "ref argument must
be addressable"; direct MIR: "three-routine structural shape is
unsupported").

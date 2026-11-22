# Self-host source C allocates call-duration concatenations in a function region (2026-11-22)

Source: main `bcd9fbea0` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` (drivers, gates) and `/root/ts3` (bootstrap, CI make-step
scripts). Plan: [directive](../../agent_work_directives/selfhost_region_allocation_2026-11-22.md),
slices 1 and 2.

## Before

The self-host routes had no region fact. `region_plan_owner.pgy` modelled the
native plan but no driver route reached it. Source C emitted
`pgy_print(pgy_concat("a", "b"))` and `Sink(pgy_concat("left", "right"))`
and never freed those strings: LeakSanitizer reported them. The installed
gate `region_user_callee_installed_self_host_owner.sh` pinned that heap
output.

## Change

- `semantic/region_escape_site_owner.pgy` certifies, per function, the
  native rule's `+` shapes (a String literal operand, or a left child that is
  one; every `+` of the left spine) given straight to an argument the builtin
  retention registry marks borrowed, to a Log statement, or to a readonly
  `ref` formal of a direct user call. A `Concat`/`StringConcat` call there is
  left to the owned String temporary plan, which already frees it.
- `codegen/emission/region_scope_emit_owner.pgy`: the sites reach the
  emitters as environment rows. A function with a site creates
  `__pgy_region` in its prologue and destroys it in the cleanup every return
  and the function end run; a certified `+` emits
  `pgy_region_string_concat(&__pgy_region, ...)`; the region runtime headers
  are included when a site exists.
- Gates: new `selfhost_region_allocation_owner.sh` (push step);
  `region_user_callee_installed_self_host_owner.sh` now expects the region
  allocation and runs source C under LeakSanitizer. The direct-MIR outputs in
  that gate are unchanged (heap).

## Results

- `selfhost_region_allocation_owner.sh` (installed driver, test profile):
  7 sites in 2 function regions, at least one free per exit, the program
  runs as native does, LeakSanitizer clean; the bound, returned and stored
  concatenations name no region. On the main driver it fails (0 sites).
- `region_user_callee_installed_self_host_owner.sh` with the new
  expectations and the LeakSanitizer run: PASS; main driver FAIL at the
  region create line.
- Region fixtures through installed source C: `region_string_concat` and
  `region_user_callee` LeakSanitizer clean (they leaked before); a mixed
  program (struct `ref` formal, Log/Print/StringLength/ref sink, early return)
  runs as native does; its remaining leaks are `ToString` operands, which no
  rule covers.
- `tests/self_hosted/parity/codegen/codegen_bootstrap.sh`: SELF-HOSTING OK,
  fixed point gen2 == gen3 (125,502 lines); the codegen's own C (gen2.c)
  now has 18 function regions with 125 sites.
- 75 CI make-step scripts with a release-profile driver: no rc difference
  from the baseline (14 fail locally on both for environment reasons).
- Other gates that pin self-host C text with `pgy_concat` read direct-MIR
  output or bound/assigned concatenations; none of those outputs change.

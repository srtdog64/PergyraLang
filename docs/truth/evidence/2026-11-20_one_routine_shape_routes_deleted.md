# One-routine shape routes deleted (2026-11-20)

Source: main `9c4ac12af` plus this change. Tree: WSL `Ubuntu-C-WSL`
`/root/ts2` (driver) and `/root/ts3` (gates). Driver built from the changed
tree with the existing gen2 codegen and the release C profile: SHA-256
`b66d4f026a90f01e0f65e7507fc1bd3a4f532ebae2d2e91f5ff3e8f5232aa7ed`. The comparison drivers are the `6ea9e53a8` build of the
[one-routine claim record](2026-11-20_general_route_one_routine_claim.md)
(sweep) and the `bfc4280e3` build (gate reruns).

## Before

After the [one-routine scalar CFG deletion](2026-11-20_one_routine_scalar_cfg_route_deleted.md),
the one-routine dispatcher still held five per-program-shape routes around
the general route:

- ahead of it, the enum value match route;
- behind it, the one-block `Array<Int>` plan, the one-block scalar
  projection, the Option match CFG and the 3-to-6-block CFG shape plan.

On the 1,398-program default-LLVM sweep the route-logging scratch driver
found the enum route and the CFG shape route serving one program each, both
refused, and the other three serving none. A scratch driver with all of them
switched off moved no program but the two returned-array ones, which the
[hoisted iterable change](2026-11-20_general_route_hoisted_iterables.md)
has since put on the general route.

## Change

- The dispatcher runs the nominal literal route, then the general route's
  admission, its runtime-declaration and literal-log cases and the general
  route itself. A program the general route does not claim is refused with
  the admission's reason, as one-block programs already were.
- Every function left unreachable from the driver mains is deleted with each
  file left empty: 37 files, among them the CFG plan, the AIR CFG and
  Option match certificates, the enum value match plan and route, the
  one-block `Array<Int>` plan and emitter and the Option match CFG plan and
  emitter.
- Registry: five CLOSED rows and their spine facts go
  (`air.mir_cfg_certificate`, `projection.direct_mir_cfg_plan`,
  `air.mir_option_match_cfg_certificate`,
  `projection.direct_mir_option_match_cfg_plan`,
  `projection.direct_mir_array_int_plan`). Two carriers whose files remain
  move to the rows that still read them. Census `CLOSED=62 BRIDGE=19
  ACTIVE=2`, 83 authorities, 182 carriers.
- Tests:
  - `one_mir_option_match_projection.sh` is the gate of
    `abi.mir_option_match_layout_admission`, whose owner the general route
    still reads. It now runs `option_match.pgy` through the general emitter,
    42/42 on C and LLVM under ASan, and requires its seven forged pattern,
    binding type, use, successor and layout rows to be refused with no
    artifact.
  - The CFG plan gate with its five sourced case scripts, the enum value
    match gate and mutations, and the one-block `Array<Int>` gate go with
    their routes; their make targets and the manual parity workflow's entry
    go too. No push or merge CI job ran them.

## Results

| Check | Result |
|---|---|
| Option match (42/42, 7 forged MIR), collection iteration (14 programs, 4 forged MIR), collection programs (6), `Array<Int>` return (13 forged), array parameter, builtin rows, mutable declaration, coverage, runtime helper, default-route native runtime parity, `Array<String>` cleanup, gate subject declaration, component contract, SoT edge | PASS |
| `tests/self_hosted/parity/one_mir/*.sh`, `tests/self_hosted/parity/direct_mir/*.sh` | 13 and 51 PASS; every other gate also fails on the `bfc4280e3` driver. `direct_mir_owned_string_reachable_proof.sh` (manual parity workflow only) stops at its 300 s native probe build on both drivers in this run, after passing in the run of the previous change: the probe's native build took 329 s and 521 s on this tree and 278 s and 325 s on `9c4ac12af` in alternating runs on a loaded machine, so the limit, not the change, decides it here |
| Default-LLVM sweep against the `6ea9e53a8` driver | equal 526, unchanged from the previous change; nothing moved |

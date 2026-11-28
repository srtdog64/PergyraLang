# Self-host anchored-handle release flow

Observed 2026-11-28 KST on `8ba5fef25` plus this change, in WSL
`Ubuntu-C-WSL` tree `/root/ts4`. Drivers (test profile, `-O0`):
`ts4-drv-sr1` for the change, `ts4-drv-base` (the `driver_lf2` build of
`6e10a73cd`'s compiler change) as the baseline. The user asked on
2026-11-28 for SoT work while two subagents take performance and leaks.

## Why this row

A pass over the open registry rows found the default route (the installed
self-host driver behind `bin/pgy`) admitting programs native refuses with a
Slot lifecycle error. Of the 85 sweep programs native refuses and default C
runs, five are `tests/cases/slot_contract/reject/*`: a second `Release`, a
`Read`/`Write` after `Release`, a second `ReleaseDeviceSlot`, a `DeviceRead`
after it. Probes found the same for a release on one branch, in a loop body
reached twice, before a `break`, in a `match` arm, or under a short circuit.
Those programs abort at run time on the runtime's double-release guard
(exit 134) or read a released slot. The fact behind native's refusal is the
per-path slot state of `semantic.resource_flow_universe`; the self-host
front end had no owner for it, so the row cannot close by a scope decision.

## Native rule, measured with probes

- A local `Slot`/`SecureSlot` (native `SYMBOL_SLOT`) is released by
  `Release` or by a move into an `own` parameter. A second release is
  `PGY_SEM_SLOT_DOUBLE_RELEASE`; `Read`/`Write` after it is
  `PGY_SEM_SLOT_RELEASED`; a move after it is `PGY_SEM_MOVE_FROM_RELEASED`;
  a `ref` argument after it is `PGY_SEM_SLOT_RELEASED` ("Cannot borrow
  released slot"). Only the local may `Release` it: a parameter is
  `PGY_SEM_BUILTIN_ARGS_INVALID`.
- A `DeviceSlot` local is released by `ReleaseDeviceSlot` (second release
  `PGY_SEM_BUILTIN_ARGS_INVALID`, `DeviceRead`/`DeviceWrite` after it
  `PGY_SEM_SLOT_RELEASED`) and moved by an `own` parameter (any later use
  `PGY_SEM_MOVE_FROM_RELEASED`). A move after `ReleaseDeviceSlot` is not
  checked: native admits it and the program aborts at run time.
- A join keeps a state from any path; a returning path does not join. A
  loop body runs again unless the loop is a literal range of one
  iteration; `for i in 0..1 { Release(s); }` is admitted. Both operands of
  `false && Read(s)` are checked. A deferred block is checked where it
  stands and its release does not count afterwards. Rebinding does not
  clear the state.

## Change

- `src/self_hosted/semantic/ast_slot_release_admission_owner.pgy` walks each
  function body over the Future lifecycle's lexical binding rows with two
  flags per handle (released, moved) and the rules above, after the Future
  lifecycle admission. A `let` of a handle starts a fresh claim, so a slot
  claimed in each iteration is admitted.
- Eight self-host codes map to native's public code, cause and fix:
  `slot_release_not_owner`, `slot_double_release`,
  `device_slot_double_release`, `slot_read_after_release`,
  `slot_write_after_release`, `device_slot_use_after_release`,
  `slot_borrow_released`, `slot_move_from_released`.
- New gate `tests/self_hosted/parity/semantic/slot_release_flow_owner.sh`
  (self-host gates shard).

## Results

- The gate PASSes on `ts4-drv-sr1` and FAILs on `ts4-drv-base` (it admits
  `double_release_slot`). Fifteen programs are refused on native C and both
  default routes with the same first public code. The default routes name
  the owned code. Two release-path programs print the same values on native
  C, native LLVM and default C.
- Default LLVM refuses both release-path programs after semantic admission
  (`MIR machine-layer projection is invalid`; `Slot<Int>` definition value
  plan), on the baseline driver too. The gate requires only that no
  semantic diagnostic refused them there.
- Sweep of the 212 sweep programs naming a slot type, C and LLVM, new against
  baseline: the five `slot_contract/reject` programs on C are newly
  refused, and native refuses each; no program is newly admitted and no
  output changed. A slot code appears only on those five programs (both
  backends); none on a program native admits.
- Codegen bootstrap gen2 == gen3 (126,675 lines); native oracle C emission
  of the driver source rc 0 (1,078 s with other WSL jobs running); component
  contract and owner size caps, language registry (inventory regenerated for
  the new fixtures), SoT edge (79 authorities, 145 carriers, CLOSED=61
  BRIDGE=16 ACTIVE=2) and adequacy, gate reachability PASS.
- Through make with the new driver installed: `slot-contract`,
  `self-host-semantic-parity` (118 fixtures, C and LLVM),
  `layered-diagnostics-contract`, `diagnostics-json`,
  `self-host-mir-resource-flow-identity` and `self-host-mir-loop-flow-summary`
  PASS. `self-host-diagnostic-catalog-parity` (exit-code parity, C 0 against
  Pergyra 1) and `self-host-semantic-selfcheck` (`undefined_function
  CodegenExpressionTypeFromGraph` at file 31 of 1,962) fail the same way on
  the baseline tree and driver; neither runs in push CI.

## Found, not changed

- Native refuses a valid program in which one function claims a slot in a
  loop body and another function uses a slot: `RIR flow enrichment failed:
  RIR resource fact has no matching ResourceFlow stable identity`. Each
  function alone compiles. The positive fixture keeps that function in its
  own program.

Still open on the row: self-host universe rows for these handles, Future,
Qubit and collection locals, and their MIR carriage.

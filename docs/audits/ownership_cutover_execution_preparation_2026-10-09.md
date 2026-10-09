# Ownership cutover execution preparation

Observed: 2026-10-09 KST. Base `main @
a75da80435e0d051f71cfacf76d44ea825ed87d7`, with preserved shared preflight
changes. This is a dirty-tree preparation receipt, not that SHA's frozen P0
result or ownership-clean implementation evidence.

## Implemented scope

`scripts/measure_build_pressure.ps1` now owns a
`pgy.build-pressure.v3` execution receipt as well as its existing pressure
metrics. It records cwd, actual ordered argv, resolved command/probe hashes,
and the before/after bytes of explicitly supplied inputs and executables.
Command resolution is literal: a wildcard is not authority to choose an
executable. A successful command with changed/unavailable declared bytes
returns 89; a failed command retains its original nonzero result and separately
reports the changed binding. Pressure refusal is 88 in both summary and exit.

Input scope is `declared-paths-only`, or `unbound` when none were supplied.
The caller still owns completeness of that input set. Begin/end hashes cannot
prove absence of intermediate edits or cover unnamed dependencies. A frozen
checkpoint, complete input manifest and actual compiler/artifact receipts
remain required for P0. Generated executables need their existing fixed-point
artifact receipts; listing pre-existing files here does not prove all generated
executables were measured or run.

Heap peak live bytes and cumulative allocation bytes remain explicitly null /
UNMEASURED. Sampled process private/working-set maxima are not those values.
The production cap is still 3072 MiB. Only the controlled negative self-test
uses 1 MiB to force the refusal; it is not a smaller compiler performance input.
Root-only self-test observation neither attributes nor kills detached workers.
That test does not establish complete pressure coverage of detached processes
in a full MSYS compiler build.

The existing pressure structural gate invokes its Windows executable consumer
through the repository's path-conversion owner. Its child uses Windows
PowerShell's built-in module environment, not an inherited PowerShell 7 module
path. Parent configuration is unchanged. Non-Windows execution explicitly
reports that the Windows consumer was not run.

## Observed acceptance

- `make LLVM_ENABLED=0 build-source-inventory-test-smoke`, native MSYS2:
  PASS, including source inventory, compiler owner cluster and the pressure
  contract with nine actual Windows execution cases.
- Cases: bound input/argv/Unicode; explicitly unbound inputs; input drift;
  declared-executable drift; command failure; failure plus drift; pressure
  cap; missing input before child launch; wildcard command before child launch.
  Exact stdout payload confirms real argument round-trip, not just JSON presence.
- Final case receipts:
  `.tmp/build-pressure-receipt-selftest/run-75f27fbb321446a8bad159b099ca3fd5/`.
  Each measured case has its summary, samples, stages and stdout/stderr; its
  `case.json` names the invocation. Missing-input/wildcard negatives emit no
  pressure summary and never produce the child's marker.
- Gate reachability initially failed on five existing proof wrappers. Literal
  focused Make entrypoints now reach them. Rerun PASS: 930 scripts, 861
  target-reachable, 69 manual, zero undeclared/stale/dual entries. The permanent
  proof consumers already belong to `coq_kernel_check.sh`; no duplicate
  full-corpus CI loop was added. Make dry-run verified the five commands, not
  fresh execution of those proof wrappers.
- Documentation quality PASS; PowerShell syntax and scoped diff checks PASS.
  These are preparation checks in a mutable tree, not the full P0 matrix.

Final implementation hashes:

| Input | SHA-256 |
|---|---|
| `scripts/measure_build_pressure.ps1` | `bd6838cead384990181d9426c79f3c7df2a6f544d5b0ea58dc1c6aef89ed7961` |
| `tests/build_pressure_contract_smoke.sh` | `85835619026b56b734608623a65b07c04b60fda734b1910786e03e44dcfc7bbf` |
| `tests/build_pressure_execution_receipt_selftest.ps1` | `2b23aadede7ce618dc2ee5081d83719c8302c730f688573d36741262aae1d899` |

## Current handoff boundary

No production compiler/runtime/backend, CI workflow, registry status or
official installed-binary change. No commit/push/GUI message. `gmon.out` is
preserved, excluded. Claude's new `OwnershipCleanCallLowering.v` and
`OwnershipCleanViewScope.v` appeared during the work; no proof-definition
edit or validation of those in-progress models is claimed here. Latest GPT
handoff check: 24 short-status entries, staged 0; the shared tree is not frozen.

The second checkpoint and new shared-tree freeze are pending. Full P0,
allocator counters, multi-inout/core connection, static view evidence and
production call-contract issuance remain OPEN. Follow
`../agent_work_directives/ownership_cutover_execution_2026-10-09.md` and the
cutover plan's dependency gates before P2-P7. A green receipt consumer does
not enable automatic drops, delete manual owners or certify GUI readiness.

# 2026-10-23: exact field detach to releasing `own` handoff

Base revision: `8ce9b9407`, plus the field-handoff change measured here.
Compiler under test: `/root/pgy-root-20261023/bin/pgy-fieldtake-95c6`,
SHA-256 `44d910fe0a43532cae95b4997f7433c9712ae1405049e5a776356b827cc69347`.

## Result

- `array_field_detach_two_own_valid`: admitted; C and LLVM output
  `1\n1\n0\n0\n`; no landing analysis.
- `array_field_detach_caller_alias_landed_valid`: admitted; C and LLVM output
  `1110`; one landing analysis and one `Clone` in `Main`.
- wrong-field, duplicate-alias, partial-branch, opaque-hold, and live-root-
  alias cases: all refused with `PGY_SEM_BORROW_ESCAPE`, no MIR.
- `tests/function_param_flow_summary_smoke.sh`: PASS.
- `tests/aggregate_storage_transfer_smoke.sh`: PASS for C and LLVM.
- `tests/nested_array_copy_smoke.sh`: PASS, including 12 allocation-failure
  and 6 twin-boundary refusals.

The first broad flow run was discarded: its executable was rebuilt while the
gate was running and the process later reported `Permission denied`. The PASS
above used the immutable copied compiler named above.

## Sanitizer limit

During the run the WSL root filesystem became read-only. `/usr/bin/df` returned
`Input/output error`, `/tmp` writes failed, and emitted ASan binaries terminated
inside sanitizer initialization with `BUS` after interceptor failures. Moving
compiler temporary artifacts to the workspace restored ordinary gates, but it
does not turn the sanitizer attempt green. Sanitizer evidence for this slice is
therefore unavailable in this environment; C/LLVM runtime and MIR Clone checks
above are the observed evidence.

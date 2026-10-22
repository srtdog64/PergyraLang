# Gates

Owner: GPT root lane. Observed 2026-10-23 KST at source/census commit
`4450c83fb`.

## Finish line (OPEN)

| Gate | Last result |
|---|---|
| Fixed DRV-2 final-owner census | RED: 319 refusals, owned by [blocker](blocker.md) |
| Full unchanged canonical | RED before MIR; ordinary compile emitted no artifact and first refused escaped `atom_table` storage |
| Current bootstrap and gen2 == gen3 | not reached because canonical admission is red |
| Installed current C/LLVM driver | not built on the current source |
| Required CI, all jobs at one published SHA | old published runs remain red; no candidate push |

Ordinary cascading errors are not the independent final-owner census or a
distinct-defect count.

## Current exact-candidate evidence

The fixed census reused the unchanged instrumented native compiler, SHA-256
`b1a0e7d3c83c50755d93b771dc9ca83d2e7b569598aecf36683edae8838ae31c`,
against an exact snapshot of `4450c83fb`.

| Receipt | Observed result |
|---|---|
| MIR result-storage owner | component contract PASS: 14 unit checks plus line caps and generator drift |
| Documentation quality | PASS: 5 unit checks and truth size caps |
| Fixed final-owner census | RED 319; exactly 13 removals and 0 additions, 0 hard errors; 942.63 s and 3980072 KiB |
| Native unit/integration matrix | not rerun at `4450c83fb`; older receipts do not establish this source |
| Codegen bootstrap and installed driver | not run while the exact fixed census remains red; no current gen2 artifact |

The latest census receipt is
[recorded here](../evidence/2026-10-23_mir_result_storage_ownership_census.md).
V1 still requires a zero canonical census, actual bootstrap and installed-
driver execution, then exact-SHA remote CI. No timeout, input or safety
condition was relaxed.

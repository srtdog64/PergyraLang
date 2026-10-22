# Gates

Owner: GPT root lane. Observed 2026-10-23 KST at source commit `674fb5903`.

## Finish line (OPEN)

| Gate | Last result |
|---|---|
| Fixed DRV-2 final-owner census | RED: 310 semantic refusals, owned by [blocker](blocker.md) |
| Full unchanged canonical | RED before MIR; no artifact; first refusal is escaped HIR `atom_table` storage |
| Current bootstrap and gen2 == gen3 | not reached because canonical admission is red |
| Installed current C/LLVM driver | not built on the current source |
| Required CI, all jobs at one published SHA | old published runs remain red; no candidate push |

Ordinary cascading errors are not the independent final-owner census or a
distinct-defect count.

## Exact-candidate evidence

The fixed census used the unchanged instrumented native checker, SHA-256
`b1a0e7d3c83c50755d93b771dc9ca83d2e7b569598aecf36683edae8838ae31c`,
against the exact source tree committed as `674fb5903`.

| Receipt | Observed result |
|---|---|
| Component contract | PASS: 14 unit checks plus source caps and generator drift |
| Retained-storage summary probe | PASS: DIR transition escape `0x9 -> 0x1`; program-domain escape `0x9 -> 0x1`; hard errors 0 |
| Fixed final-owner census | RED 310; nine semantic removals, zero semantic additions, zero hard errors; 1028.91 s, 3,981,132 KiB |
| Current native compiler build | PASS; SHA-256 `cb0363a4e9889e14fe20b8ff214fcaf8918a2f061fe80e356079ce648e110aba` |
| String copy physical release | PASS on C and LLVM; deletion mutants leak; escaping control remains valid |
| Codegen bootstrap and installed driver | not run while the fixed canonical census remains red; no current gen2 artifact |

The exact receipt is
[recorded here](../evidence/2026-10-23_mir_routine_and_transition_storage_census.md).
V1 still requires a zero canonical census, actual bootstrap and installed-
driver execution, then exact-SHA remote CI. No timeout, input or safety
condition was relaxed.

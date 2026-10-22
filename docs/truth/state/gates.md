# Gates

Owner: GPT root lane. Observed 2026-10-22 KST at executable commit
`57baf8426`.

## Finish line (OPEN)

| Gate | Last result |
|---|---|
| Fixed DRV-2 final-owner census | RED: 332 refusals, owned by [blocker](blocker.md) |
| Full unchanged canonical | RED before MIR; ordinary compile emitted no artifact and first refused escaped `atom_table` storage |
| Current bootstrap and gen2 == gen3 | not reached because canonical admission is red |
| Installed current C/LLVM driver | not built on the current source |
| Required CI, all jobs at one published SHA | old published runs remain red; no candidate push |

Ordinary cascading errors are not the independent final-owner census or a
distinct-defect count.

## Current exact-candidate evidence

The current root candidate compiler is
`/tmp/pgy-root-loan-20261022-b/bin/pgy`, SHA-256
`a7d232ce4652ab797653bed9b33db50cce2725529d3bd1e141bb0f070d649975`.

| Receipt | Observed result |
|---|---|
| Native root-candidate build | PASS in 157.99 s, peak 175980 KiB |
| Function parameter flow integration | PASS in 382.17 s, peak 254588 KiB (over the 5-minute focused budget) |
| Clone focused C/LLVM probes | positive prints `a41`; shallow-copy growth refuses at 26:40 with no artifact |
| Array/member + receiver ABI gate | PASS in 8.9 s on C and LLVM, including target/index/RHS order, bounds and backing-loan controls |
| Semantic/transpiler units | PASS: 3240/0 and 1014/0 |
| Documentation quality | PASS, including truth size caps and generated validation manifest |
| Previous native unit matrix | PASS on `0d77e1562`; not repeated after this semantic-only integration |
| Assignment backing-loan falsifiers | PASS: both refuse on C and LLVM before artifact publication; prior ASan runs establish the rejected failure mode |
| Same-backing read-only control | PASS on C and LLVM, prints `7` |
| Scalar expression-root C/LLVM falsifier | PASS on C and LLVM, both print `20`; missing root is `None` |
| MIR error-storage owner | AST and component gates PASS; C/LLVM probe is wired behind the current rung driver but canonical admission blocks execution |
| Fixed final-owner census | RED 332; exactly 10 intended removals and 0 additions, 0 hard errors; 1154.78 s and 3980968 KiB |
| Codegen bootstrap | not rerun while the exact fixed census remains red; no current gen2 artifact |

The latest census receipt is
[recorded here](../evidence/2026-10-22_mir_program_error_storage_census.md).
V1 still requires a zero canonical census, actual bootstrap and installed-
driver execution, then exact-SHA remote CI. No timeout, input or safety
condition was relaxed.

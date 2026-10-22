# Gates

Owner: GPT root lane. Observed 2026-10-22 KST on local main `b1e2b532d`
plus the root codegen and assignment-backing candidate named in [main](main.md).

## Finish line (OPEN)

| Gate | Last result |
|---|---|
| Fixed DRV-2 final-owner census | RED: 383 refusals, owned by [blocker](blocker.md) |
| Full unchanged canonical | RED before MIR by the fixed census; the ordinary compile was not repeated after the semantic-only integration |
| Current bootstrap and gen2 == gen3 | not reached because canonical admission is red |
| Installed current C/LLVM driver | not built on the current source |
| Required CI, all jobs at one published SHA | old published runs remain red; no candidate push |

Ordinary cascading errors are not the independent final-owner census or a
distinct-defect count.

## Current exact-candidate evidence

The current root candidate compiler is
`/tmp/pgy-root-loan-20261022-a/bin/pgy`, SHA-256
`64c9cac91c53dcb47e873ceb0fd3739146d5778ed6095de82e5bd006845a711b`.

| Receipt | Observed result |
|---|---|
| Native root-candidate build | PASS in 113.61 s, peak 175828 KiB |
| Function parameter flow integration | PASS in 393.30 s, peak 254592 KiB (over the 5-minute focused budget) |
| Clone focused C/LLVM probes | positive prints `a41`; shallow-copy growth refuses at 26:40 with no artifact |
| Array/member + receiver ABI gate | PASS in 8.9 s on C and LLVM, including target/index/RHS order, bounds and backing-loan controls |
| Semantic/transpiler units | PASS: 3240/0 and 1014/0 |
| Documentation quality | PASS, including truth size caps and generated validation manifest |
| Previous native unit matrix | PASS on `0d77e1562`; not repeated after this semantic-only integration |
| Assignment backing-loan falsifiers | PASS: both refuse on C and LLVM before artifact publication; prior ASan runs establish the rejected failure mode |
| Same-backing read-only control | PASS on C and LLVM, prints `7` |
| Codegen bootstrap | not rerun while the exact fixed census remains red; no current gen2 artifact |

The backing-loan receipt is
[recorded here](../evidence/2026-10-22_indexed_assignment_backing_loan.md).
V1 still requires a zero canonical census, actual bootstrap and installed-
driver execution, then exact-SHA remote CI. No timeout, input or safety
condition was relaxed.

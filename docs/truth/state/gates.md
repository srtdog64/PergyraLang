# Gates

Owner: GPT root lane. Observed 2026-10-23 KST at source commit `f90dd7157`.

## Finish line (OPEN)

| Gate | Last result |
|---|---|
| Fixed DRV-2 final-owner census | RED: targeted eight release rows absent, but deeper hard layer makes the result incomparable to the prior 309 |
| Full unchanged canonical | RED before MIR; 33647 errors, 21 warnings; first error is the opaque String non-retention boundary at line 236 |
| Current bootstrap and gen2 == gen3 | not reached because canonical admission is red |
| Installed current C/LLVM driver | not built on the current source |
| Required CI, all jobs at one published SHA | old published runs remain red; no candidate push |

Ordinary cascading errors are not the independent final-owner census or a
distinct-defect count.

## Exact-candidate evidence

The exact-source native product binary has SHA-256
`8ed9f3af3b182abcaf3ab3e954110ad1c662435b0cff005e20a0dcc5a5966e7e`.
The separate reporting-instrumented binary has SHA-256
`4f7edc3ee5486c01c6508801801cdda17cd0ea0f4fb76df82af5452fcf8bc350`.

| Receipt | Observed result |
|---|---|
| Component contract | PASS: 14 unit checks plus source caps and generator drift |
| Inout-member release gate | PASS: two native C/LLVM positives and four pre-MIR negatives |
| Owned-local transition unit | PASS: 15 checks, 0 failures |
| Minimal sanitizer falsifier | PASS: ASan+UBSan and leak detection, no report |
| Exact baseline/candidate canonical diff | only the first diagnostic and timing differ; candidate reaches the next existing boundary |
| Codegen bootstrap and installed driver | not run; canonical is red and no current gen2 artifact exists |

The exact receipt is
[recorded here](../evidence/2026-10-23_inout_member_release_restore.md).
V1 still requires a zero canonical census, actual bootstrap and installed-
driver execution, then exact-SHA remote CI. No timeout, input or safety
condition was relaxed.

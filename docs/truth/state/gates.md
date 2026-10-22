# Gates

Owner: GPT root lane. Observed 2026-10-23 KST at source commit `4716f9539`.

## Finish line (OPEN)

| Gate | Last result |
|---|---|
| Fixed DRV-2 final-owner census | RED: 309 semantic refusals, owned by [blocker](blocker.md) |
| Full unchanged canonical | RED before MIR; no artifact; first final-owner row is an owned-String release refusal |
| Current bootstrap and gen2 == gen3 | not reached because canonical admission is red |
| Installed current C/LLVM driver | not built on the current source |
| Required CI, all jobs at one published SHA | old published runs remain red; no candidate push |

Ordinary cascading errors are not the independent final-owner census or a
distinct-defect count.

## Exact-candidate evidence

The fixed census used the unchanged instrumented native checker, SHA-256
`b1a0e7d3c83c50755d93b771dc9ca83d2e7b569598aecf36683edae8838ae31c`,
against the exact source tree committed as `4716f9539`.

| Receipt | Observed result |
|---|---|
| Component contract | PASS: 14 unit checks plus source caps and generator drift |
| Graph-atom behavior | PASS: native C/LLVM exact output; forbidden late owned growth rejected on both |
| Fixed final-owner census | RED 309; one semantic removal, zero additions, zero hard errors; 1432.40 s, 3,976,804 KiB |
| Exact-source native compiler build | not run while canonical admission remains red |
| Codegen bootstrap and installed driver | not run while the fixed canonical census remains red; no current gen2 artifact |

The exact receipt is
[recorded here](../evidence/2026-10-23_ast_graph_atom_prebind_census.md).
V1 still requires a zero canonical census, actual bootstrap and installed-
driver execution, then exact-SHA remote CI. No timeout, input or safety
condition was relaxed.

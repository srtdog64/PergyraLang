# Gates

Owner: GPT root lane. Checked 2026-10-17.

## Progress gates

Only these show how far the V1 finish line is. A green regression guard
below does not move this table.

| Gate | Last result | Revision |
|---|---|---|
| Native admission of the unchanged canonical program | RED | local `7dda8a0c2` (native `01d…`) |
| `self-host-codegen-bootstrap-linux` (CI) | RED | `39a8fc1db` |
| Installed-driver build | PASS, 203 s | branch `b8e66a387` only |
| Driver fixed point `gen2 == gen3` | PASS, 219995 lines | branch `b8e66a387` only |
| `self-host-mir-round-trip-semantic-test-smoke` | PASS | branch `b8e66a387` only |
| Rest of `self-host-bootstrap-linux` (5 targets + policy corpus) | not run to the end | — |
| Required CI on one published SHA | RED | `39a8fc1db` |

The `b8e66a387` rows predate the merge with the inherited native work.
They do not hold for main until they are run again on main.

## Regression guards

These are guards against unsafe admission and regressions. Last observed on
native `01d…` built from `61a…` plus its identified overlay:
- phase 10995/0;
- MIR 370/0;
- semantic 3230/0;
- transpile 1012/0;
- AIR 156/0;
- safe C/LLVM runs 66/66;
- Map 24 positives and 58 negatives.

## Known red, outside CI

`self-host-collection-focus-recovery-test-smoke` fails at
`c-nominal_bodyful_forward-emit` (`PROBE|recovery-missing`). It was already
red when it arrived with the v16 candidate. No CI job runs it.

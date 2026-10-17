# Measurements

Owner: GPT root lane. Every value names where it was measured.

## Budgets

| Budget | Value | Where it applies |
|---|---|---|
| CI runner memory | 16 GB | `ubuntu-latest` |
| Codegen bootstrap job time | 30 min | `.github/workflows/ci.yml` |
| Self-host bootstrap job time | 60 min | `.github/workflows/ci.yml` |

## Values

| Measurement | Value | Revision and executable |
|---|---|---|
| DRV-2 `--mir-json` consumer peak | 16.3 GB, 3:45 | `b8e66a387`, native-built scratch driver |
| DRV-2 consumer peak after `3cc0ecc47` | not measured | — |
| Installed-driver build peak | 13.7 GB, 203 s | `b8e66a387`, fresh Linux clone |
| Codegen bootstrap time | 905 s | `b8e66a387`, fresh Linux clone |
| Native canonical admission run | 28–31 s, 1.29–1.37 GB | GPT, natives `c807…` and `01d…` |

## Memory finding behind `3cc0ecc47`

On the round trip of one 269-file module (peak 3.2 GB, 2.4 GB requested),
54.7% of the requested bytes were empty focus recovery facts built by every
`SemanticAstCollectionOwnershipVerdictOk()`. The runtime frees little, so
the requested bytes track peak RSS. Record:
[evidence/2026-10-16_b8e66a387_fixed_point.md](../evidence/2026-10-16_b8e66a387_fixed_point.md).

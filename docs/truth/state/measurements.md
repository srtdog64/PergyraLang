# Measurements

Owner: GPT root lane. Checked 2026-11-17 on `5b0d354fb` plus A2 source
hashes in [A2 evidence](../evidence/2026-11-17_selfhost_memory_a2_phi_reuse.md).
Heavy phases ran serially in WSL `/root/mg-main`; times are observations.

| Same fixed MIR consumer | Baseline peak KiB | A2 peak KiB | Output |
|---|---:|---:|---|
| Complete semantic program | 1,058,664 | 797,600 | C bytes equal |
| Complete DRV-2 program | 11,571,840 | 10,741,020 | C bytes equal |

The independently executed CI memory gate reproduced 797,464 KiB against
an 800 MiB ceiling on the same semantic MIR SHA. The existing 14 GiB
address-space and 30 minute full-phase limits remain unchanged.

The full verified DRV-2 MIR producer completed at 8,625,732 KiB. These
split-process results do not meet the one-process source-to-C goal by
themselves. A1 and the owned-value lifetime work remain open.

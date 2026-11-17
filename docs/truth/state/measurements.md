# Measurements

Owner: GPT root lane. Checked 2026-11-17, parent `7ea3c781f` plus the
[A1 source generation and executable hashes](../evidence/2026-11-17_selfhost_memory_a1_flat_projection_and_oom.md).
Heavy measurements ran serially in WSL `/root/mg-main`.

| Same fixed MIR consumer | Baseline peak KiB | A2 peak KiB | Output |
|---|---:|---:|---|
| Complete semantic program | 1,058,664 | 797,600 | C bytes equal |
| Complete DRV-2 program | 11,571,840 | 10,741,020 | C bytes equal |

The existing 14 GiB address-space and 30 minute full-phase limits remain unchanged.

| Final A1 installed-driver phase | Peak KiB | Ceiling KiB |
|---|---:|---:|
| Complete semantic MIR consumer | 797,588 | 819,200 |
| Complete MIR-lower source-to-C | 5,552,288 | 8,388,608 |

The same final source through source-to-C and source-to-MIR-to-C has equal
C bytes. Before the array panic repair, a fixed source study measured
8,746,168 -> 5,555,788 KiB, equal C: combined A2+A1, not A1-only attribution.
No lower wall-time claim is made.

Full DRV-2 one-process source-to-C remains OPEN. The pre-panic-repair run
failed at 14,107,288 KiB RSS inside the 14 GiB address-space boundary;
the actual full-input rerun is pending. The earlier split MIR producer
(8,625,732 KiB) does not establish this goal. Owned-value lifetime work is open.

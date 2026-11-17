# Measurements

Owner: GPT root lane. Checked 2026-11-18; the current declaration-identity
[source/executable hashes](../evidence/2026-11-18_selfhost_ownership_declaration_identity.md) and
the [String seed bridge](../evidence/2026-11-18_selfhost_string_temporary_seed_bridge.md) are distinct.
Heavy measurements ran serially in WSL `/root/mg-main`.

| Same fixed MIR consumer | Baseline peak KiB | A2 peak KiB | Output |
|---|---:|---:|---|
| Complete semantic program | 1,058,664 | 797,600 | C bytes equal |
| Complete DRV-2 program | 11,571,840 | 10,741,020 | C bytes equal |

The existing 14 GiB address-space and 30 minute full-phase limits remain unchanged.

| Current declaration-identity candidate | Peak KiB | Ceiling KiB |
|---|---:|---:|
| Complete semantic MIR consumer | 776,668 | 819,200 |
| Complete MIR-lower source-to-C | 5,249,316 | 8,388,608 |

The same final source through source-to-C and source-to-MIR-to-C has equal
C bytes. Before the array panic repair, a fixed source study measured
8,746,168 -> 5,555,788 KiB, equal C: combined A2+A1, not A1-only attribution.
No lower wall-time claim is made.

The last complete DRV-2 one-process source-to-C, on the String seed bridge, failed at
14,072,968 KiB, then explicit OOM / exit 134 within the unchanged boundary.
No C artifact is published. The earlier split MIR producer (8,625,732 KiB)
does not establish this goal. Owned-value lifetime work is still open.

The current candidate on the same frozen full MIR passes transition 1054
and refuses at transition 1126 (3,543,780 KiB).
This is a longer reached chain, not a successful compile or comparable cost
measurement. Full one-process DRV-2 source-to-C was not remeasured on this
identity-only repair; the last failed source run above remains the observed limit.

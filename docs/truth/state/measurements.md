# Measurements

Owner: GPT root lane. Checked 2026-11-18; the current String seed bridge's
[source/executable hashes](../evidence/2026-11-18_selfhost_string_temporary_seed_bridge.md) and
the [prior role generation](../evidence/2026-11-17_selfhost_memory_role_receiver_migration.md) are distinct.
Heavy measurements ran serially in WSL `/root/mg-main`.

| Same fixed MIR consumer | Baseline peak KiB | A2 peak KiB | Output |
|---|---:|---:|---|
| Complete semantic program | 1,058,664 | 797,600 | C bytes equal |
| Complete DRV-2 program | 11,571,840 | 10,741,020 | C bytes equal |

The existing 14 GiB address-space and 30 minute full-phase limits remain unchanged.

| Current String seed bridge | Peak KiB | Ceiling KiB |
|---|---:|---:|
| Complete semantic MIR consumer | 777,420 | 819,200 |
| Complete MIR-lower source-to-C | 5,248,516 | 8,388,608 |

The same final source through source-to-C and source-to-MIR-to-C has equal
C bytes. Before the array panic repair, a fixed source study measured
8,746,168 -> 5,555,788 KiB, equal C: combined A2+A1, not A1-only attribution.
No lower wall-time claim is made.

The current complete DRV-2 one-process source-to-C failed at
14,072,968 KiB, then explicit OOM / exit 134 within the unchanged boundary.
No C artifact is published. The earlier split MIR producer (8,625,732 KiB)
does not establish this goal. Owned-value lifetime work is still open.

The role generation's frozen full MIR passes role declaration admission
and refuses at collection ownership transition / routine 1054 (3,614,356 KiB).
This is a longer reached chain, not a successful compile or comparable cost
measurement. It is distinct from the current whole-source measurement above.

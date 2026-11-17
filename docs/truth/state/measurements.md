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

The [nested-return candidate](../evidence/2026-11-18_selfhost_nested_return_transfer.md)
on the same frozen full MIR peaks at 3,597,284 KiB before its next refusal
([blocker](blocker.md)); no C artifact is emitted. Its predecessor stopped
earlier at 3,543,780 KiB. Different prefixes are not comparable memory costs.
Full one-process source-to-C was not remeasured after this endpoint repair;
the last failed source run above remains the observed limit.

The array-transform v2 driver on that same full frozen MIR peaks at
3,921,780 KiB, then refuses transition routine 1437 with no C artifact.
Its source generation is `69690e47a07b87df6f7742e6c8498bc7fe286bbe66ff782866961bc0bf9f9b22`;
driver SHA256 `5e9572fcd1a14070f06fa40b33458f0a40eaadc3d6fe4a064d73832441d680d2`.
The 14 GiB/1,800 s boundary is unchanged and the process group drained.
Parent/GNU clocks disagree, so no wall-time comparison is claimed.
This is another admission prefix, not successful full self-compilation.

The v5 Pergyra-built driver on the complete one-process DRV-2 source-to-C
input exits 134 with explicit allocation failure under the same 14 GiB
`RLIMIT_AS` and 1,800 s limit. GNU time reports 14,077,584 KiB peak RSS;
no C artifact exists, and the owned process group drained. This is 4,616 KiB
above the prior failed prefix, not evidence of a memory win. The wrapper and
GNU clocks disagree; no wall-time comparison is claimed. See the
[array-transform run record](../evidence/2026-11-18_selfhost_array_transform_lifetime.md).

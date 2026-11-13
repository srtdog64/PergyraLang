# 2026-11-13: why the identity cell probe takes 300+ s to build natively

Run by the Claude root lane on the WSL tree `/root/cc-nb` (main `6452bad92`
plus the generic result-relation change, committed later as `bbbadc926`),
WSL `Ubuntu-C-WSL`, while another tree built a driver on the same machine.

Contracts step 3 builds
`tests/self_hosted/parity/fixture/identity_cell_receiver_probe.pgy` with
`--native-pipeline --backend=c --opt=dev` under a 120 s limit. The probe
imports the scalar GraphPlan admission, so the build compiles most of the
direct-MIR compiler.

## Time

| Measure | Result |
|---|---|
| Whole build | 414 s, then 381 s |
| `[pipeline timing] semantic` | 334.5 s (HIR, DIR, RIR together under 5 s) |
| `prepare_flow` | 17 runs, 76.3 s |
| `aggregate_landing` | 17 runs, 35.3 s |
| Other named semantic phases | about 24 s |

The named phases add up to about 135 s. The rest of the 334 s is type
checking and the other passes of each analysis.

## Landing rounds

`PGY_DEBUG_LANDING_ROUNDS=1` with `--emit-c`: 33 analyses (rounds 0 to 32),
ending with 289 String stores and 1,812 loan copies landed.

- Stores grew in 12 rounds: 0 to 5, 7, 9, 11, 14, 18 and 24.
- `semantic_analyze_landing_fixed_point` drops the whole copy plan when the
  store plan grows, so every store growth is followed by the same copy chain
  again: 423, then 15, about 207, 284, about 590 copies, one round each.
- After the last store growth (round 24) the chain took rounds 25 to 31
  (423, 15, 207, 284, 590, 275, 18) before round 32 selected nothing.

So the repeated operation is a whole-program analysis per landing round,
and most rounds re-select copies an earlier round already selected. Keeping
the copy plan across store growth would cut about half the rounds, which is
still over 120 s at about 10 s per round; the build needs both fewer rounds
and cheaper ones.

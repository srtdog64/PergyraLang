# 2026-11-14: keeping the loan copy plan across store growth

Run by the Claude root lane on the WSL tree `/root/cc-nb` at `ac0205b71`
plus the change to `src/semantic/semantic_landing_fixed_point_owner.c`, with
no other load. Native C build, `--native-pipeline --backend=c --opt=dev`,
`PGY_DEBUG_LANDING_ROUNDS=1 PGY_DEBUG_PIPELINE_TIMING=1`.

## What the rounds showed

Before the change, every round in which the store plan grew dropped the
copy plan, and the next round selected the same first batch of copies again
(222 copies in the semantic owner probe, 423 in the identity probe), then
continued the same chain. Copies selected in a store-growth round were
discarded too.

## Result

| Program | Rounds | Semantic | Total | Stores | Copies |
|---|---|---|---|---|---|
| semantic_owner_probe, before | 51 | 544.9 s | 594 s | 428 | 1738 |
| semantic_owner_probe, after | 12 | 141.7 s | 183 s | 404 | 1749 |
| identity_cell_receiver_probe, before | 33 | 358.2 s | 406 s | 289 | 1867 |
| identity_cell_receiver_probe, after | 13 | 124.4 s | 169 s | 286 | 1897 |

Peak RSS was 1.43 GB and 1.47 GB before; it was not measured after.

The extra copies (11 and 30) land as explicit `Clone`s of nodes a later
store made unnecessary. Fewer stores were selected after the change.

## CI before the change (run 43541429551, `ac0205b71`)

The contracts job reached its 30 minute limit during step 11. Step 4 took
1179 s, 572 s of it the shared semantic owner probe build (C and LLVM in
parallel); step 3's identity probe build on the public route timed out at
120 s.

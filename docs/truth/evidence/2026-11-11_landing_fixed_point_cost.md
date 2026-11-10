# Landing fixed point cost, measured on 2026-11-11

Measured by the Claude root lane in WSL `Ubuntu-C-WSL` (16 threads, with
other jobs running alongside). Native builds: `/root/cc-all` at `f3dd62ca1`
(reference landing rule) and `/root/cc-land` at `f3dd62ca1` plus an
uncommitted copy-retention experiment in
`src/semantic/semantic_landing_fixed_point_owner.c`. This record is not
edited after it is written.

## Arity probe

Input: `tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy`,
`--native-pipeline --backend=c --opt=dev`. The nominal field identity gate
builds it under a 120 s timeout.

- Reference rule: 25 analyses; semantic 154.9 s of 174 s total; final plan
  174 stores, 655 copies.
- Sampled once a second (`/root/cc-logs/pmp_ref.samples`): an analysis
  that follows a store wave, when the copy plan has just been reset to
  none, takes 11 to 16 s and spends 43% in flow summary demands. An
  analysis that follows a copy merge takes 2 to 4 s and spends none there.
- Copy retention (merge each round's copies even when stores grow, then
  derive the copies once more from none under the final stores): 8 + 6 =
  14 analyses. `PGY_VERIFY_LANDING_FIXED_POINT` then reran the reference
  rule: same stores digest `bdb112b2ac5c7da3` and copies digest
  `fe86c1dc7eab50a9`.

## Identity cell probe

Input: `tests/self_hosted/parity/fixture/identity_cell_receiver_probe.pgy`,
same flags. The identity cell gate builds it under a 120 s timeout.

- Copy retention, first version (it dropped the copies selected while the
  copy plan was empty): 26 analyses, semantic 370 s, total 444 s; 339
  stores, 2,017 copies.

## Rows currency walk

A 40-sample profile of the canonical driver's native emission
(`src/self_hosted/compiler/driver_bootstrap_main.pgy --emit-c`, copy
retention, about 3,950 landed copies):
`function_param_flow_backing_rows_capture_target` 47% inclusive,
`rows_capture_enqueue` 20% self (a linear membership scan of the walk's
queue) and `rows_capture_index` 5% self (a linear scan of its index
cache). With both lookups hashed, the reference rule built the arity probe
in 194 s instead of 260 s under the same load, and the emitted C was
byte-identical (301,170 lines).

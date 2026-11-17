# Self-compile memory A2: phi allocation reduction

Observed 2026-11-17 in WSL `Ubuntu-C-WSL`, `/root/mg-main`, from
`5b0d354fb37b901495e4b9c2d27cfe57ec0197cc` plus the four source changes below.
Windows main in `D:\PergyraLang` has the same source. Heavy phases ran serially.
This is a consumer allocation change, not lifetime-verifier or V1 closure.

## Source and executable identity

SHA-256:

| Artifact | Hash |
|---|---|
| `ssa_identity_owner.pgy` | `ba536d595ee118acfcffe06a7178c90ae200c1c9243d840b38dabd72b1626268` |
| `routine_definition_dominance_fact_owner.pgy` | `ea38a4ef721bfdcc3a866d89a0d7eb9527efa2478f24603d860a39a4a9b6a403` |
| `phi_predecessor_binding_fact_owner.pgy` | `4b2a54e6f78110ab5f2aadb5da913a72495a6ad69f8fc77e0a4e844c60a2e907` |
| `mir_cfg_graph_owner.pgy` | `c3b20410b25d3e0789b24f895f73b9eb3125b9174cb432486b97727299243de3` |
| Clean baseline installed driver | `81497d8b93c80da4127e12314327ee45aec54bb93a94a547cdeb66e3193c171f` |
| A2 installed driver | `e8359400ee8f4eeb5c2782ada0d64dbb082dfd4204c8ba9943747967fcc77f49` |

Both installed drivers were emitted by the Pergyra codegen seed through
`self_host_compiler_build.sh`, then compiled by the host C compiler. The A2
installation completed in 6:10.60, peak 8,624,468 KiB. This is not a native
direct compilation of the replacement driver or a fixed-point claim.

## Fixed-input measurements

| Same MIR input, `--mir-json` | Before RSS KiB / wall | A2 RSS KiB / wall | Result |
|---|---:|---:|---|
| Complete `semantic/main.pgy` | 1,058,664 / 6.50 s | 797,600 / 8.25 s | C bytes equal; RSS -24.7% |
| Complete `driver_rung2_main.pgy` | 11,571,840 / 4:20.97 | 10,741,020 / 4:42.86 | C bytes equal; RSS -7.2% |

Time increased on this run; this slice prioritizes peak memory. The semantic
MIR SHA is `7e8a40eac7ad0915000128e2bf4d4b7e53065988fd04f95703258e36926016f7`;
both C outputs have SHA `95ecc3ddd2f824c3d83292a482c8eaa2f886198ce715da4e8d72d8561748e1fb`.
The full driver MIR SHA is `f8f283e76dfab4de566f75234ca0c227242bdae7f4211a0f4692f50ddf8b4a8d`;
both C outputs have SHA `baec1ed6a86779bc060472ee5f2e56c481d5e4fa7189f6fc492acfc835420a53`.
Its verified producer completed in 2:55.80 at 8,625,732 KiB RSS. Producer and
both full consumers passed the existing 14 GiB address-space / 30 minute
process boundary, including successful process-group drain receipts.

The new `selfhost_compile_memory_owner.sh` independently reproduced the exact
semantic MIR SHA and completed at 797,464 KiB (800 MiB ceiling). It runs in
the Linux push core shard. The ceiling and the existing process limits were
not raised. Neither measurement is the one-process source-to-C target.

## Falsifiers and other checks

- `phi_dominance_reuse_owner.sh` PASS on C and LLVM, 55.32 s: canonical SSA
  spelling, overflow/sign/padding rejection, Unicode names; cyclic/reused
  traversal workspace with independent retained distances; exhaustive
  dominator/block queries for the fixture with every reuse replayed; a
  deliberately corrupted proof rejected; duplicate phi incoming rejected
  before publication; both valid direct backend executables print `2`.
- The same probe compiled and executed through the Pergyra codegen seed,
  including the corrupted-proof refusal (before the extra queue test).
- Component contract gate PASS in 12.00 s; source counts 27/130/171/295;
  shell syntax and `git diff --check` PASS.
- Two existing optional checks remain red on the clean baseline too:
  `mir_cfg_graph_query_owner_smoke.sh` reports two verdict failures with
  both the baseline graph owner and the A2 owner (native C/LLVM also agree);
  `one_mir_scalar_cfg_graph_projection.sh` rejects the baseline
  `nested_if_in_loop` topology. Neither gate was weakened or relabeled green.

Logs and process receipts are under `.tmp/selfhost-memory-20261117/`;
focused run `.tmp/self_hosted/phi-dominance-reuse.QMpzEl`; memory gate
`.tmp/self_hosted/selfhost-memory.H94Mnx`. These are local artifacts, not
semantic authority. Exact-SHA remote CI remains separately recorded in
[gates](../state/gates.md). A1 and the later owned-value verifier remain open.

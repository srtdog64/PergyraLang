# Oracle MIR memory state

Owner: [oracle memory lane](../lanes/oracle_rss.md).
Checked 2026-11-06 KST at HEAD `60fd1e26d` plus preserved dirty work.
Exact run identities and limits:
[frozen evidence](../evidence/2026-11-06_oracle_mir_repeated_clone_rss_bulkhead.md).

The repeated row/receipt copies are repaired in eight shared Pergyra owners.
The candidate native-produced oracle issues the complete original fixed input
with byte-identical MIR: 389,800,975 bytes, SHA-256
`5e59c2e1f521f2a21b7bca2e7aa2e9b19f1fed49d5475258949bdd75d174ca8c`.
RSS is 11,274,212 KiB (10.752 GiB), down 46.95% from 21,250,796 KiB.
It completed within a 14 GiB address-space boundary in 224.44 s, exit 0.
No source reduction, generated-C patch or safety-rule weakening was used.

Existing source admission/refusal matrix: C 80/80, LLVM 80/80, equal outputs.
Row allocation scaling: 3.788->2.008 for doubled input; 512 rows use 629,275
requested bytes. C/LLVM borrowed-row sanitizer and physical-source-free
controls pass. These are bounded claims; whole-language cleanup is not closed.

Linux full-MIR CI phases now have independent process groups, 14 GiB address
space, a 30-minute phase ceiling, cancellation/parent-death cleanup and
validated boundary receipts. Failure/isolation controls pass. Remote CI has
not run for this dirty patch.

Delivery: `.tmp/oracle-rss-20261106/delivery/` in the shared checkout.
Exact candidate executable and frozen roots are named in the evidence.
Preimages and tested source hashes are preserved; no stage/commit/push.

Main owns new-source gen2 == gen3, installed-driver integration, the separate
constructor-query budget, and one published green commit. The prior green
fixed point is not relabeled as validation of this changed source.

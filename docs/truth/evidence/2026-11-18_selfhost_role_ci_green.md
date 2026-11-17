# General role receiver migration: published CI

Observed 2026-11-18 KST using `gh run view`, after the local role receipts.

- Published main: `9049e547eaa9eb72c242def8a86e3f2b6c7d706d`.
- [CI run 44099172008](https://github.com/srtdog64/PergyraLang/actions/runs/44099172008):
  completed, success; all 33 jobs succeeded, no skipped jobs.
- Codegen bootstrap, self-host seed/bootstrap, native oracle, Linux self-host
  contracts and fixed-point join all executed successfully on this SHA.
- Local production-source and artifact identities, ABI/runtime negatives and
  preserved optional reds are in the [role migration record](2026-11-17_selfhost_memory_role_receiver_migration.md).
- This closes the role dependency's publication check, not S1/S2 or the full
  one-process DRV-2 memory target. The latter remains [open](../state/measurements.md).

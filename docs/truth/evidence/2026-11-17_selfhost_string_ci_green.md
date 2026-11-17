# String allocation repair: published CI

Observed on 2026-11-17 after the [phi header correction](2026-11-17_selfhost_string_ci_header_fix.md).

- Published main: `9f5573f18eaef050d5b52880006a380d5fabdfb0`.
- [Required CI run 44073079061](https://github.com/srtdog64/PergyraLang/actions/runs/44073079061):
  completed, success; all 33 jobs succeeded, none skipped.
- Codegen bootstrap, installed self-host driver seed, native oracle and
  fixed-point join actually executed and passed. The Linux core shard that
  previously failed the phi C header lookup passed.
- Production source and local memory limits remain those in the
  [String allocation record](2026-11-17_selfhost_string_allocation_boundary.md).
  Full one-process DRV-2 still fails with explicit OOM at the fixed boundary.
  CI success does not close the overall memory goal or implement owned-value
  retirement. Role receiver work is a subsequent, unpublished dependency.

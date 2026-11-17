# String temporary seed bridge: published CI

Observed 2026-11-18 KST using GitHub job/step results.

- Published source: `6ff288d70013898a21e867e663f97eaf07e642f8`.
- [Run 44123611930](https://github.com/srtdog64/PergyraLang/actions/runs/44123611930),
  attempt 2: completed, success; all 33 jobs succeeded, none skipped.
- Attempt 1 passed 32 jobs. The macOS job acquired no hosted runner; GitHub
  reported macOS arm64 capacity constraints. One bounded rerun of that job
  acquired a runner and executed the C-only push target successfully.
- Codegen bootstrap, the new String retirement ASan/LSan step, native oracle,
  installed self-host contracts, seed/bootstrap and fixed-point join executed.
- [Local bridge scope and identities](2026-11-18_selfhost_string_temporary_seed_bridge.md)
  remain bounded. This is not full S1/S2 or the one-process DRV-2 memory goal.

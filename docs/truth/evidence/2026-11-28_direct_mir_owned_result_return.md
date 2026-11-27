# Owned array call result returned from a branch

Observed 2026-11-28 KST on `e6097ba08096d3ab1f38ea499b373c9149b8fde2`
plus the return-transfer change. D: is the editing checkout. WSL
`/root/drv2-1128/verified` holds the validated compiler source and receipts.
The Pergyra-built installed driver has SHA-256
`230a0bf8285dc8b870610f93425b4211966c27d95f499b5c921c96278d84dcb4`.
Its generated C has SHA-256
`c271059adeaca948c711a7eafb8777fb4896efa2e530e53fd5bb28ce2877ad5a`.
The release-profile install finished under the 14 GiB / 1,800 s process
boundary with exit 0 and the process group drained.

## Executed focused gates

- The general C and LLVM routes reproduced `3`, `ab`, `fixed`, `branch`,
  `1`, `fallback` on MIR produced by the installed driver. Both executables
  passed ASan and LSan. Each backend refused the missing result row, missing
  push receipt, forged origin and wrong callee with zero artifact output.
  The wrong callee has its own valid owned result, but does not match the
  typed definition. LSan detected the C leak control that omits the
  release of a fresh push operand.
- Array<String> cleanup, inout member restoration and Option coalesce passed.
  The ownership carrier gate passed native/self-host production and reader
  checks, including 55 malformed rows. Component contracts passed 2,675
  line-cap checks and generator drift.
- On the first release build below, the separate callee-identity gate
  stopped at `collection_ownership_transition` ordinal 29 on `real_join`.
  The base driver also stops there. The owned-string pushes into the
  formal-string-transfer local have no receipts in either producer.

## Complete DRV-2 boundaries

- Frozen MIR (418,865,243 bytes, SHA-256
  `e7767d3059972797baf41654ececd912bfb91a0755547afea6336e6af8bbdf19`): the
  base driver `c1687a7d720c26b1936c20b139eb76c847c4c831528aac659c7aefa2f6ff964a`
  refused LLVM at collection transition ordinal 1438, exit 1, 4,086,356 KiB
  peak RSS, with no output. Receipt: `base-llvm.receipt.json` under
  `/root/mg-main/.tmp/selfhost-memory-20261117/drv2-1128/`.
- The first release build `8298c3990756b9d75e43a1d796f22ff6c8031cc0db3a05dfdbc5586db6a4cba0`
  generated MIR for the full current `driver_rung2_main.pgy` (357,038,615
  bytes, SHA-256 `f0b420ecf94ba21ef3d5eff33534b5bc0c7437b19440ee1631f98562a4f0ca7e`).
  Production exited 0 at 4,546,884 KiB peak RSS. Both general backends
  refused the same callable at ordinal 6996, parameter 1:
  `DirectMirScalarProgramRoleImplsJoined`,
  `readonly-ref Array<DirectMirScalarProgramRoleMethodDeclaration>`.
  Both exited 1 without output. Peak RSS was 806,056 KiB for C and
  805,844 KiB for LLVM. Receipts: `full-{producer,c,llvm}.receipt.json` under
  `/root/drv2-1128/final3/.tmp/drv2-review/`. The final focused build keeps
  the receipt policy under existing file caps and removes a duplicate
  unreachable refusal. These complete-input measurements use the first build.

Full general MIR admission, the complete refusal census and publication
remain open at this checkpoint.

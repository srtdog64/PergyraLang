# A2 CI publication checks

Observed on `f9fc19162116270a43b34e1548bd86d95fc2a7ae`, GitHub CI run
`44017658521`. The Linux push core shard executed all 67 steps and failed
two publication checks: the new memory script hardcoded the time executable
path, and the generated word inventory had not been regenerated after the
new phi fixture. No compiler safety rule or resource ceiling was weakened.

The phi C/LLVM replay and negative gate passed on the runner. The memory
gate passed at 796,420 KiB against 800 MiB. All frontend tests in that shard
completed; its two failing checks do not establish an overall green run.

The follow-up resolves the time executable through PATH and regenerates
the inventory using the official generator on an exact archive of f9fc19162
(`src`, `tests`, `examples`, generator and editor inputs), excluding the
uncommitted A1 source. The archived generator's `--check`, the live source
inventory gate and shell syntax check all passed locally. This record does
not claim the follow-up CI result; that is owned by its published run.

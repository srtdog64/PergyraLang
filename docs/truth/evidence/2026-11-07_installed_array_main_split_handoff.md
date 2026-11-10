# Array patch adaptation to main's C emitter split

This follows the frozen [r16 execution receipt](2026-11-07_installed_array_retirement_r16.md).
Its eight-input timing, physical release and safety results are unchanged.

While final packaging was in progress, main split
`direct_mir_scalar_cfg_program_c_emission_owner.pgy` into the routine and
artifact emission owners. The original 60-file pending patch therefore stopped
applying at that file after its earlier successful check. This supersedes the
old receipt's shared-tree application status, not its r16 execution evidence.

The final integration patch is
`.tmp/owner-production-20261107/final-handoff/installed-array-main-split.patch`.
It has 61 file deltas and SHA-256
`33aafe82988d3406dcf530f1d40433edd04554421590e2dcceda4f83f3d2f773`.
The two new owners' pre-change function bodies exactly matched the original
baseline. Their patched bodies exactly match the tested r16 bodies; required
retirement imports were added to both. No shared source was overwritten.

The other 59 file deltas are unchanged. Main's wrapper and new owner boundaries
are preserved. `git apply --check` passed on current main. Actual replay into a
new isolated directory also passed, with all 61 resulting hashes matching.
`main-split-migration.json` and `main-split-replay.json` record the exact scope.

This is a verified patch migration, not a new installed compiler execution.
Main still owns the cap migration, integrated rebuild, canonical input,
gen2 == gen3 and required CI. Use this integration patch rather than the older
60-file patch. The full r7-to-r16 patch remains the fixed execution reference.

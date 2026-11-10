# Verified Array integration patch after main's C emitter split

This is the authoritative final patch-migration receipt. It supersedes the
application status in the [r16 execution receipt](2026-11-07_installed_array_retirement_r16.md)
and the prematurely written [first split note](2026-11-07_installed_array_main_split_handoff.md).
The fixed r16 compiler, eight-input results and safety receipts are unchanged.

Use `.tmp/owner-production-20261107/final-handoff/installed-array-main-split-v2.patch`.
SHA-256: `f97a53c648b9bbe19e06f1780692443b102a61a74024277698483862a5114d9a`.

Main split the C program emission owner into routine and artifact owners while
packaging was in progress. The original pending patch stopped applying at the
old file. Both moved function bodies exactly matched their prior baseline;
the v2 patch puts the tested r16 bodies into those two owners and adds their
retirement imports. The other 59 file deltas remain unchanged.

Observed verification:

- Current shared-tree `git apply --check`: PASS.
- Check and actual application in a new isolated directory: PASS.
- All 61 resulting file hashes match the intended outputs.
- Both relocated function bodies equal the tested r16 bodies.
- Shared source and index were not changed by this verification.

The first split patch omitted a terminating newline between two diff sections.
Its syntax check alone did not reveal that problem. The replay's file inventory
check caught it before application; the first note's replay-success sentence
was premature and must not be used. V2 fixes the patch serialization and passed
the actual replay/hash checks above. Neither compiler source nor benchmark
results changed. Keep the old split patch only as a rejected packaging record.

`main-split-v2-migration.json` and `main-split-v2-replay.json` own exact patch
identities and results. This proves patch migration, not execution of a newly
integrated compiler. Main retains the owner-cap migration, integrated rebuild,
canonical input, gen2 == gen3 and required CI obligations.

# Main and CI

Owner: GPT root integration for the user's 2026-11-17 memory-plan request.
Other paused work and recovery branches remain preserved. Checked 2026-11-17.

| Item | Observed value |
|---|---|
| Published main before A2 | `5b0d354fb37b901495e4b9c2d27cfe57ec0197cc`; CI run 44010239826 pending only self-host bootstrap at last check |
| Work branch | `main`, `D:\PergyraLang`; tracked A2 edits named in its evidence, plus unrelated untracked folder preserved |
| Active work | one memory-plan edit scope; local A2 verified, commit/push and exact-SHA CI pending, then A1 |
| Pre-takeover snapshots | branches `snapshot/all-20261110` (whole tree) and `snapshot/v001-index-20261110` (staged index only) |
| Linux validation | WSL `Ubuntu-C-WSL`: `/root/mg-main` builds the integration head |
| Last completed required CI observed | `ea0783975` green, run 44001627208; bootstrap and installed-driver jobs executed |

The gate results are in [gates](gates.md) and the open items in
[blocker](blocker.md).

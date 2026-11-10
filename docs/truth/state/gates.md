# Gates

Owner: Claude root lane. Checked 2026-11-10 23:30 KST. Local results are from
`/root/cc-dbg-1110` at the named commit unless a line says otherwise.

| Gate | Last observed result |
|---|---|
| Native compile of the canonical driver | PASS at `e6ca0e3f3`: `0 error(s), 26 warning(s)` |
| Codegen bootstrap (gen2 == gen3, 119,545 lines) | PASS on CI at `e6ca0e3f3` |
| Driver fixed point (gen2 == gen3) | OPEN: not rerun since `658a9fcc9`; CI job cancelled at 60 minutes |
| Function parameter flow summary smoke | PASS at `f48930b65` |
| Backend compare, the 23 cases red at `e6ca0e3f3` | 17 PASS in the real harness at `8a970305b`; 6 native refusals |
| Installed driver on 11 fixtures refused with `CODEGEN ERROR` | PASS with the driver built at `658a9fcc9` |
| build-linux contracts shard, local | RED at `6fb4b2345`: 8 of 11 steps |
| build-linux core shard, local | RED at `e6ca0e3f3`: 27 of 55 steps; not completed since |
| Windows `test-all` | PASS on CI at `e6ca0e3f3` |
| Required CI at the published SHA | RED at `e6ca0e3f3`; `e875e9e90` running |

A local PASS on a scratch tree is not CI evidence. The finish line is every
required job green on one published SHA.

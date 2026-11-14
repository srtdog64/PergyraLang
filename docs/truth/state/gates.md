# Gates

Owner: Claude root lane. Checked 2026-11-14. Local results are from WSL
trees (`/root/cc-phi` side tree with the working changes, `/root/cc-nb` for
native trials); a local PASS is not CI evidence.

| Gate | Last observed result |
|---|---|
| Required CI at the published SHA | GREEN at `bcdf042c4` (run 43588624419): all 33 jobs succeeded, no step skipped in the bootstrap, oracle, join or contracts jobs; before it GREEN at `edbb4575b` (run 43583259892, the folder reorganization) and `cdb65d798` (run 43554588799) |
| Driver fixed point (gen2 == gen3), oracle/seed/join | PASS on CI at `bcdf042c4`, with the bootstrap policy corpus in the join job |
| Codegen bootstrap | PASS on CI at `bcdf042c4` |
| Bootstrap installed-driver jobs, CLI mode | PASS on CI at `bcdf042c4` |
| build-linux core shard | PASS on CI at `bcdf042c4`, including the builtin-effect and language-keyword registry checks (steps 38 and 39); the Markdown contract step runs only on markdown-only changes |
| Contracts shard (identity probe in the shared probe build) | PASS on CI at `bcdf042c4` (job 25 min of the 30 min limit) |
| Backend compare (20 shards), sanitizers, native oracle | PASS on CI at `bcdf042c4` |
| macOS, Windows, formal proofs, tsan | PASS on CI at `bcdf042c4` |
| LSP hover registry (`make self-host-lsp` first; not in a CI shard) | PASS locally on WSL at `751e5dd05` (language=27, builtin=7) |

Job times at `bcdf042c4`: seed part 29 min (limit 60), native oracle
14 min, build-linux 17 min, join 11 min, codegen bootstrap 14 min,
contracts 25 min.

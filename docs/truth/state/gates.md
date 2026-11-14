# Gates

Owner: Claude root lane. Checked 2026-11-15. Local results are from WSL
trees (`/root/cc-phi` side tree with the working changes, `/root/cc-nb` for
native trials); a local PASS is not CI evidence.

| Gate | Last observed result |
|---|---|
| Required CI at the published SHA | GREEN at `a7d7655e1` (run 43630062521, the native checker fixes): all 33 jobs succeeded, no step skipped in the bootstrap, oracle, join or contracts jobs; before it GREEN at `65724e27d` (run 43596693746), `edbb4575b` (run 43583259892, the folder reorganization) and `cdb65d798` (run 43554588799) |
| Driver fixed point (gen2 == gen3), oracle/seed/join | PASS on CI at `a7d7655e1`, with the bootstrap policy corpus in the join job |
| Codegen bootstrap | PASS on CI at `a7d7655e1` |
| Bootstrap installed-driver jobs, CLI mode | PASS on CI at `a7d7655e1` |
| build-linux core shard | PASS on CI at `a7d7655e1`, including the builtin-effect and language-keyword registry checks and the native unsafe-negative refusal smoke; the Markdown contract step runs only on markdown-only changes |
| Contracts shard (identity probe in the shared probe build) | PASS on CI at `a7d7655e1` (job 16 min of the 30 min limit) |
| Backend compare (20 shards, 989 cases), sanitizers, native oracle | PASS on CI at `a7d7655e1` |
| macOS, Windows, formal proofs, tsan | PASS on CI at `a7d7655e1` |
| Self-host lifetime fixture sweep (868, ASan; not a CI gate) | Locally on WSL on `fix/ref-formal-value-argument`: 0 memory errors in admitted negatives, 1 in non-negatives (endless recursion), see [evidence](../evidence/2026-11-15_selfhost_lifetime_fixture_admissions.md) |
| LSP hover registry (`make self-host-lsp` first; not in a CI shard) | PASS locally on WSL at `751e5dd05` (language=27, builtin=7) |

Job times at `a7d7655e1`: seed part 22 min (limit 60), native oracle
13 min, build-linux 18 min, join 11 min, codegen bootstrap 19 min,
contracts 16 min.

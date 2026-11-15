# Gates

Owner: Claude root lane. Checked 2026-11-15. Local results are from WSL
trees (`/root/cc-phi` side tree with the working changes, `/root/cc-nb` for
native trials); a local PASS is not CI evidence.

| Gate | Last observed result |
|---|---|
| Required CI at the published SHA | GREEN at `20325cad5` (run 43653186068, the self-host cleanup policy and ref-argument fixes): all 33 jobs succeeded; before it GREEN at `a7d7655e1` (run 43630062521), `65724e27d` (run 43596693746) and `edbb4575b` (run 43583259892) |
| SoT registry gate (`scripts/sot_registry_gate.py`) | PASS on `fix/llvm-leg-memory`: 95 authorities, 213 derived carriers; CLOSED=67 BRIDGE=25 ACTIVE=3 |
| Driver fixed point (gen2 == gen3), oracle/seed/join | PASS on CI at `a7d7655e1`, with the bootstrap policy corpus in the join job |
| Codegen bootstrap | PASS on CI at `a7d7655e1` |
| Bootstrap installed-driver jobs, CLI mode | PASS on CI at `a7d7655e1` |
| build-linux core shard | PASS on CI at `a7d7655e1`, including the builtin-effect and language-keyword registry checks and the native unsafe-negative refusal smoke; the Markdown contract step runs only on markdown-only changes |
| Contracts shard (identity probe in the shared probe build) | PASS on CI at `a7d7655e1` (job 16 min of the 30 min limit) |
| Backend compare (20 shards, 989 cases), sanitizers, native oracle | PASS on CI at `a7d7655e1` |
| macOS, Windows, formal proofs, tsan | PASS on CI at `a7d7655e1` |
| Lifetime fixture sweeps (868, ASan, C and LLVM legs of both front ends; not a CI gate) | Locally on WSL on `fix/llvm-leg-memory`: the only memory errors left are endless recursion (`inout_cycle_*`); C-leg [evidence](../evidence/2026-11-15_selfhost_lifetime_fixture_admissions.md); LLVM IR is linked through `/root/fx_llvm.sh` with a use-after-free witness |
| LSP hover registry (`make self-host-lsp` first; not in a CI shard) | PASS locally on WSL at `751e5dd05` (language=27, builtin=7) |

Job times at `a7d7655e1`: seed part 22 min (limit 60), native oracle
13 min, build-linux 18 min, join 11 min, codegen bootstrap 19 min,
contracts 16 min.

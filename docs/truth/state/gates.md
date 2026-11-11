# Gates

Owner: Claude root lane. Checked 2026-11-12. Local results are from the WSL
tree `/root/cc-tip` at the commit a line names; a run without `wslpath` on
`PATH` stands for CI's Linux runner.

| Gate | Last observed result |
|---|---|
| Driver fixed point (gen2 == gen3) | PASS on CI at `1acc31c5b`; split into oracle/seed/join parts at `61ebf046b`: locally oracle 2,565 s, seed 1,439 s (237,176 lines), join 291 s; not yet run on CI |
| Codegen bootstrap | PASS on CI at `51fbdca00`; at `891a1f23a` it failed installing apt packages (runner DNS) |
| Contracts step 4 (local, `34c3a4aed` + `912788ef9`) | merged probe built in 497 s of 1,200; PASS: hashmap admission, public array drop, nominal constructor identity, binding move prefix, owned result, member identity, aggregate entry, aggregate release source, constructor field input, native storage escape. FAIL: formal key input 0 (blocker 3), member permission after the publication refusals (not yet rerun), focus recovery (blocker 4), inout effect (blocker 2) |
| Contracts step 3 | identity cell probe build 310 s locally against a 120 s limit |
| build-linux core shard | CI at `51fbdca00`: 6 of 55 (5, 6, 24, 32, 34, 43); 32, 34 and 43 pass locally |
| Aggregate storage transfer smoke | PASS locally with the `3cd211b94` compiler; the `ad62986ef` compiler fails exactly the three new cases |
| Backend compare | CI at `51fbdca00`: 16 of 20 shards green; 14 and 16 pass locally |
| macOS, Windows, formal proofs, tsan | PASS on CI at `891a1f23a` |
| Required CI at the published SHA | RED |

A local PASS on a scratch tree is not CI evidence. The finish line is every
required job green on one published SHA.

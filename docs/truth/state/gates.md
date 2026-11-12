# Gates

Owner: Claude root lane. Checked 2026-11-12. Local results are from WSL
trees (`/root/cc-phi` side tree with the working changes, `/root/cc-nb` for
native trials); a local PASS is not CI evidence.

| Gate | Last observed result |
|---|---|
| Driver fixed point (gen2 == gen3) | PASS on CI at `1acc31c5b`; split into oracle/seed/join parts at `61ebf046b`; not yet green on CI since |
| Codegen bootstrap | PASS on CI at `c7e599f58` (20 min) and at `128b644c8`; the two codegen gates now run once, on gen2 |
| Contracts step 4 | PASS locally at `128b644c8`: formal key, binding move prefix, member permission, focus recovery, inout effect (649 source admissions per backend), owned result, member identity, aggregate entry/release, constructor field input, native storage escape |
| Ownership semantic (installed driver) | local driver from `128b644c8`: the self-host LLVM route refuses `collection_field_restore_valid` at a direct-MIR route (also with the 2026-11-11 driver) |
| Contracts step 3 | identity cell probe build 310 s locally against a 120 s limit |
| Contracts step 11 | line caps PASS locally at `128b644c8` |
| build-linux core shard | CI at `c7e599f58`: steps 5, 6, 24 red (as at `1acc31c5b`), 35 fixed in `128b644c8` |
| Bootstrap installed-driver CLI mode | `self-host-array-index-type`: the self-host LLVM route refuses a `Slice` ref parameter at a direct-MIR route |
| Backend compare | CI at `c7e599f58`: shards 4 and 7 red, as at `1acc31c5b` |
| macOS, Windows, formal proofs, tsan | PASS on CI at `c7e599f58` |
| Required CI at the published SHA | RED |

The finish line is every required job green on one published SHA.

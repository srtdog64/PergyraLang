# Current blocker

Owner: Claude root lane. Checked 2026-11-14 on `reorg/folders` at
`360ff8cf0`, the folder reorganization on top of the green `cdb65d798`.

## Objective card

- Objective: keep required CI green on one published commit, with the
  bootstrap and installed-driver jobs executed (met at `cdb65d798`; the
  reorganization must hold it).
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Required CI on the published reorganization commit.
2. Generator drift no required job checks (all older than the
   reorganization): `builtin_effect_projection_owner.pgy` lacks the
   registry's `ArraySetOwnedString` row; the hover projection says `var`
   where its renderer emits `let`; the language-word implementation
   inventory counts are stale.
3. Native admits 28 negative fixtures that fail under ASan
   ([record](../evidence/2026-11-12_native_admits_unsafe_negative_fixtures.md)).
4. Locally, core step 5 fails only on WSL (`wslpath -w` gives a UNC repo
   root); a pass-through `wslpath` shim makes it pass.
5. The self-host MIR route carries no compiler `Clone`; native lands one for
   `collection_field_use_after_move`, the self-host refuses it.

## Done since `cdb65d798`

- Folder reorganization (user decision 2026-11-13): `editors/` holds both
  extensions; parity scripts in twelve family directories; loose `tests/`
  gates in subject directories; loose `docs/` documents in their facade
  areas; large flat `src/` directories split by file prefix (codegen `c/`
  and `llvm/`, compiler IR families, `type_checker/`, `direct_mir/`,
  `driver/`, `ast_collection/`, `ast_expression/`).
- Gates kept their coverage across the moves: depth-limited scans
  (`semantic_tu_size`, `test_inc_size`, numbered-doc uniqueness, inc
  sentinel) read the new directories, and the semantic 599-line cap covers
  `ast_collection/` and `ast_expression/`.
- The codegen seed prebuild key hashed three shared legs at paths that no
  longer existed into empty fields; it now refuses a missing input.
- Locally at `360ff8cf0` (WSL): native build, semantic tests 3394/3394,
  seed, driver, seed-side intent gates, CLI mode, core shard, contracts
  shard. Branch CI at `d29fda1bb` (run 43578035229): 32 of 33 jobs green,
  fixed point included; contracts failed only on two line caps that
  `360ff8cf0` fits.

## Falsifiers kept

- A drop on one branch still refuses the read after the join; own
  transfers of storage a constructor kept stay refused natively.
- A field restore whose MIR row names another field, and a binding move
  whose destination row names itself, are refused with no artifact.
- A 600-line unregistered owner in `src/self_hosted/semantic/ast_collection/`
  is refused; a missing shared leg refuses the seed prebuild key.

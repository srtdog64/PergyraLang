# Current blocker

Owner: Claude root lane. Checked 2026-11-14 on main `bcdf042c4` (CI
green, run 43588624419).

## Objective card

- Objective: keep required CI green on one published commit, with the
  bootstrap and installed-driver jobs executed (met at `cdb65d798`,
  `edbb4575b` and `bcdf042c4`).
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Native admits 36 negative fixtures that fail under ASan
   ([remeasured 2026-11-14](../evidence/2026-11-14_native_unsafe_negative_admissions_remeasured.md)):
   13 owned-String transfers (`ArrayDropOwnedStrings` admits UNKNOWN
   element provenance on purpose, `collection_ownership_fact.c`), 11 callee
   growths of a by-value formal's field copy, 12 others.
2. Locally, core step 5 fails only on WSL (`wslpath -w` gives a UNC repo
   root); a pass-through `wslpath` shim makes it pass.
3. The self-host MIR route carries no compiler `Clone`; native lands one for
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
- A nested Array element store (`slots[i] = row`) emitted an invalid C
  kernel name and was refused by LLVM; both legs now store it
  (`backend_compare/nested_array_element_assign`).
- Three drifted projections regenerated (`751e5dd05`): the builtin effect
  table (`ArraySetOwnedString`), the LSP hover renderer (`var` locals) and
  the language-word inventory. The core shard now checks the builtin-effect
  and language-keyword registries, so the next drift fails CI.

## Falsifiers kept

- A drop on one branch still refuses the read after the join; own
  transfers of storage a constructor kept stay refused natively.
- A field restore whose MIR row names another field, and a binding move
  whose destination row names itself, are refused with no artifact.
- A 600-line unregistered owner in `src/self_hosted/semantic/ast_collection/`
  is refused; a missing shared leg refuses the seed prebuild key.

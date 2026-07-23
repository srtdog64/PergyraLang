# Current Work Handoff

Updated: 2026-07-23 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. On resume, verify this
checkpoint with `git status --short --branch`, the named owner documents, and
the named focused gate. Current source, registries, and executable evidence win
when they disagree with this note.

## Resume checkpoint

- Latest self-host executable checkpoint: `9a438da4` (`Close StringConcat alias
  self-host rung`). At capture time `main` was one commit ahead of
  `origin/main` at `4053192c`; the only dirty path was this handoff refresh.
- Latest native backend checkpoint: `246682fe` (`Close C ArrayMap and
  ArrayFilter result type SoT`), with handoff refresh `4053192c`. It is
  independent of the self-host alias rung.

## Latest native backend SoT closure

- `mir_source_local_expr_call_facts` owns the active-MIR `ArrayMap` and
  `ArrayFilter` result shape as `Array<T>`: the map callback must be a named
  top-level routine with a concrete non-`Void` return, and filter derives its
  element from the source collection.
- The C backend call-type consumer now reads that owner through
  `mir_source_local_call_expr_type_name` while an active MIR routine exists.
  Missing callback/collection facts return `Unknown`; no C-local call-name
  result guess is used as a fallback. LLVM already consumed the same owner in
  `a1678e8d`.
- Focused evidence observed on 2026-07-23: static
  `tests/backend_fail_closed_smoke.sh`, shell syntax, and diff checks passed;
  the separate LLVM-enabled build produced
  `.tmp/verify_c_array_bin/pgy.exe`; the direct C and LLVM probe both ran with
  output `6` and `4`; a `Void` ArrayMap callback failed closed with
  `C index access requires an Array<T> or Slice<T> receiver, got 'Unknown'`.

## Last closed executable rung

The counted DRV-2 executable frontier is fixture 249,
`tests/cases/backend_compare/string_utility_aliases/main.pgy`.

Objective card:

- Objective: project stable source alias
  `StringConcat(String, String) -> String` through the same runtime concat ABI
  as `Concat`.
- Priority: preserve builtin row identity, one runtime symbol owner,
  fail-closed argument types, then patch size.
- Fact owners: `SemanticBuiltinSignatureRows` for the source signature and
  `RuntimeCallCName` for its runtime-symbol projection.
- Last consumers: semantic function-table seeding and semantic direct-call
  emission.
- Forbidden fallback: a `StringConcat` branch in semantic call emission, a
  second C helper, source-text substitution, or shifting existing builtin row
  identities.
- Falsifier: mutate the second argument from the owned `String` local to `Int`;
  both compilers must reject it and the Pergyra driver must report
  `call_arg_type_mismatch`.

Observed before the fix:

- A freshly built Pergyra DRV-2 driver rejected `StringConcat` in both
  `string_utility_aliases` and `nested_array_string` as `undefined_function`.

Closed design:

- `StringConcat` is append-only builtin row 101, the 102nd row. All previous
  builtin identities remain stable.
- `Concat` and `StringConcat` both project through `StringRuntimeCConcatFn`.
  Emitted C contains `pgy_concat` and no second `StringConcat` symbol.
- The manifest now has 257 DRV-2 MIR rows. Rung 249 is counted because the old
  Pergyra driver failed and Pergyra semantic/runtime projection owners changed.
- Prior rung 248 remains closed: contextual `Result<T>` values enter declared
  `Result<T, E>` nominal fields through `ResultTypeAssignableTo`, with payload
  mismatch rejection and no constructor-name policy.

Primary files:

- `src/self_hosted/semantic/builtin_signature_owner.pgy`
- `src/self_hosted/semantic/ast_expression_environment_owner.pgy`
- `src/self_hosted/codegen/emission/runtime_call_rewrite_owner.pgy`
- `src/self_hosted/compiler/driver_rung2_owner.pgy`
- `tests/self_hosted/parity/driver_rung2_string_concat_alias_parity_owner.sh`
- `src/self_hosted/PROGRESS.md`
- `src/self_hosted/OWNERS.md`

## Last observed verification

Green:

- C-built Pergyra driver, filtered producer-first source/MIR parity for
  `string_utility_aliases`: `backends=1 body_fixtures=20 mir_fixtures=1`.
- LLVM-built Pergyra driver, same filtered parity and counts.
- Both paths projected the carried `StringConcat` target to `pgy_concat`,
  compiled emitted C, ran it, and matched native output `HELLO`, `world`,
  `ok:Hello World`.
- `tests/self_hosted_component_contract_smoke.sh`.
- `make self-host-hard-contract-test-smoke`.
- `make self-host-substitution-velocity-test-smoke`.
- `make sot-authority-edge-test-smoke`: 43 authorities, 38 derived carriers,
  `CLOSED=23 BRIDGE=20 ACTIVE=0`.
- `PGY_ALLOW_MISSING_COQ=1 make sot-authority-adequacy-test-smoke`: live
  owner/consumer and negative-mutation checks passed; the proof model was a
  declared skip and was not checked.
- `tests/documentation_quality_smoke.sh`.
- Shell syntax for the modified parity owners and cached-diff whitespace checks.

Not run or not available:

- The full unfiltered 257-row DRV-2 matrix was not run.
- Coq/Rocq is not installed. Never report the formal model as checked on this
  workstation.
- The aggregate documentation Make target was not used for this checkpoint;
  the direct documentation quality gate is the observed evidence.

## Next executable work

The next smaller observed synchronous failure is:

- `tests/cases/backend_compare/fieldless_class_method/main.pgy`
- Current Pergyra driver result after the StringConcat closure:
  `CODEGEN ERROR: method owner field inventory is missing`.
- Audit the nominal declaration/environment owner for a valid zero-field
  inventory. Do not fake a field, emit a class-name exception, or treat missing
  inventory as an empty inventory without owner evidence.

The broader known async failure remains:

- `tests/cases/backend_compare/await_inline_spawn/main.pgy`
- Current Pergyra driver result: `initializer_type_unresolved` for
  `await spawn Inc(4)`.
- Observed cause: `await` is graph-typed, while `spawn` is still represented as
  a leaf and lacks an owned async expression/type/codegen fact.

The async case is still a multi-owner rung. Do not implement `spawn` by erasing
concurrency or routing through a sequential call. Work the observed fieldless
class inventory failure first unless a narrower failing gate disproves it.

## Workstation and repository recovery

- Global Codex baseline: `C:/Users/user/.codex/AGENTS.md`. It was reviewed and
  reduced to a compact cross-project policy; repository `AGENTS.md` remains more
  specific. Read both on resume.
- Git for Windows, Git LFS, GitHub CLI, MSYS2 UCRT64 GCC/Make/Python/LLVM, Node,
  npm, and ripgrep are installed. GitHub CLI authentication remains user-owned;
  verify with `gh auth status` rather than assuming it.
- Repository safe-directory configuration exists for both Windows Git and the
  MSYS gate user. Repository `core.autocrlf=false` preserves the LF policy.
- For Windows `bin/pgy.exe` in tests, source
  `tests/pgy_binary_path_helpers.sh` and call
  `pgy_prepend_windows_runtime_paths` before execution.
- Use serial `make` on this workstation unless the Windows jobserver behavior
  has been revalidated.
- `.gitignore` owns root temporary builders with `/.tmp_*`. Earlier recovery
  recycled 87 root temporary files and reduced `.tmp` from about 9.49 GB to an
  active ignored cache. All `codex_*` directories used for Result, alias, and
  next-rung discovery were moved to the Windows recycle bin after verification.
- Credential hygiene: a prior local process inspection exposed configured
  Figma, GitHub, Render, and Tavily credentials in command-line output. Do not
  copy values into docs or logs. Rotate/revoke the affected credentials before
  relying on them again.

## Resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, and the
   `selfhost.expression_surface` row in
   `docs/semantics/sot_owner_spine_registry.md`.
2. Run `git status --short --branch` and preserve any concurrent dirty paths.
3. Confirm fixture 249 with the filtered C parity gate before broadening.
4. Select the next rung only from an observed failure and write its objective
   card before changing structure.
5. Run the narrow negative first, then component/hard/substitution gates.
6. Refresh this handoff with the exact implementation revision, dirty state,
   last green gate, next falsifier, and blockers.

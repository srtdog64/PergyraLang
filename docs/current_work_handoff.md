# Current Work Handoff

Updated: 2026-07-23 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. On resume, verify this
checkpoint with `git status --short --branch`, the named owner documents, and
the named focused gate. Current source, registries, and executable evidence win
when they disagree with this note.

## Resume checkpoint

- Executable implementation checkpoint: `5c2ba93b` (`Close contextual Result
  field assignment rung`).
- After concurrent work completed, capture-time HEAD and `origin/main` were
  both `a1678e8d` (`Close LLVM ArrayMap element type SoT`). The only dirty path
  was this handoff refresh. Establish the final handoff commit with `git log -3`
  because a file cannot contain its own commit hash.
- Concurrent `ArrayFilter`/`ArrayMap` native work is committed after the Result
  rung. It was not implemented as part of `5c2ba93b`; use its own owner facts
  and gates before changing it.
- No push was performed by this Result-rung session. A concurrent process
  pushed `main`, including `5c2ba93b` as an ancestor.

## Last closed executable rung

The counted DRV-2 executable frontier is fixture 248,
`tests/cases/backend_compare/result_as_class_field/main.pgy`.

Objective card:

- Objective: admit context-incomplete `Ok(T)`/`Err(E)` values into a nominal
  constructor field declared as `Result<T, E>`.
- Priority: declared field/parameter type, canonical wrapper assignability,
  fail-closed mismatch, then patch size.
- Fact owner: `ResultTypeAssignableTo` in
  `src/self_hosted/semantic/wrapper_type_owner.pgy`.
- Last consumers reached by the rung:
  `SemanticExpressionGraphFieldValueAssignableTo` and `CompareCallArgs`.
- Forbidden fallback: class/constructor/variant name branches, accepting every
  Result pair, guessing `E`, or keeping strict type-string equality as a second
  assignability authority.
- Falsifier: mutate `Ok(100)` to `Ok("bad")`; the Pergyra driver must reject it
  with `call_arg_type_mismatch` and the native compiler must also reject it.

Observed before the fix:

- A freshly built Pergyra DRV-2 driver rejected
  `Wallet(Ok(100))` as expected `Result<Int, CardErr>`, actual `Result<Int>`.
- It independently rejected `Cell(10, Ok(5))` as expected
  `Result<Int, FlagErr>`, actual `Result<Int>`.

Closed design:

- The field graph consumer delegates Result pairs to
  `ResultTypeAssignableTo` instead of owning a `Result<Unknown>` exception.
- The remaining non-generic call consumer delegates to
  `ExpressionAssignableTo`, which in turn consumes the same Result owner,
  instead of applying strict string equality.
- No class, constructor, variant, or error-enum name is present in the semantic
  implementation.
- The manifest now has 256 DRV-2 MIR rows. Rows 246 and 247 remain historical
  coverage-only ratchets; rung 248 is counted because the old Pergyra driver
  failed and Pergyra semantic implementation changed to replace that path.

Primary files:

- `src/self_hosted/semantic/wrapper_type_owner.pgy`
- `src/self_hosted/semantic/ast_expression_graph_field_type_owner.pgy`
- `src/self_hosted/semantic/call_check_owner.pgy`
- `src/self_hosted/compiler/driver_rung2_owner.pgy`
- `tests/self_hosted/parity/driver_rung2_result_field_parity_owner.sh`
- `src/self_hosted/PROGRESS.md`
- `docs/semantics/sot_owner_spine_registry.md`

## Last observed verification

Green:

- C-built Pergyra driver, filtered producer-first source/MIR parity for
  `result_as_class_field`: `backends=1 body_fixtures=20 mir_fixtures=1`.
- LLVM-built Pergyra driver, same filtered parity and counts.
- Both paths preserved the typed `Result<Int, CardErr>` ABI, compiled emitted C,
  ran it, and matched the native oracle output.
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

- The full unfiltered 256-row DRV-2 matrix was not run.
- Coq/Rocq is not installed. Never report the formal model as checked on this
  workstation.
- The aggregate documentation Make target was not used for this checkpoint;
  the direct documentation quality gate is the observed evidence.

## Next executable work

No next synchronous fixture is selected by inference. The next known real
failure is:

- `tests/cases/backend_compare/await_inline_spawn/main.pgy`
- Current Pergyra driver result: `initializer_type_unresolved` for
  `await spawn Inc(4)`.
- Observed cause: `await` is graph-typed, while `spawn` is still represented as
  a leaf and lacks an owned async expression/type/codegen fact.

This is a multi-owner async rung. Before implementation, write a fresh objective
card naming the async fact owner, the last legitimate semantic/codegen consumer,
the forbidden sequential fallback, and a graph mutation that must fail closed.
Do not implement `spawn` by erasing concurrency or routing through a sequential
call. If that rung is too broad for one bounded edit, probe for a smaller
observed synchronous failure and record the actual diagnostic before selecting
it.

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
  active ignored cache. The four `codex_*` directories used for rung discovery
  and parity were moved to the Windows recycle bin after verification.
- Credential hygiene: a prior local process inspection exposed configured
  Figma, GitHub, Render, and Tavily credentials in command-line output. Do not
  copy values into docs or logs. Rotate/revoke the affected credentials before
  relying on them again.

## Resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, and the
   `selfhost.expression_surface` row in
   `docs/semantics/sot_owner_spine_registry.md`.
2. Run `git status --short --branch` and preserve any concurrent dirty paths.
3. Confirm fixture 248 with the filtered C parity gate before broadening.
4. Select the next rung only from an observed failure and write its objective
   card before changing structure.
5. Run the narrow negative first, then component/hard/substitution gates.
6. Refresh this handoff with the exact implementation revision, dirty state,
   last green gate, next falsifier, and blockers.

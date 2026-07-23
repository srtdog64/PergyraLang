# Current Work Handoff

Updated: 2026-07-23 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. On resume, verify it
with `git status --short --branch`, the named owners, and the focused gate.
Current source, registries, and executable evidence win when they disagree.

## Resume checkpoint

- Latest self-host executable implementation: `8cc9ad68` (`Close graph-owned
  foreach iterable rung`). Its negative contract ratchet is `54bed08e`
  (`Ratchet foreach graph contract`).
- Latest prior fieldless checkpoint: `8afd9160`, with handoff refresh
  `89625851`.
- Latest native backend checkpoint: `246682fe` (`Close C ArrayMap and
  ArrayFilter result type SoT`), with handoff refresh `4053192c` and matching
  LLVM owner consumption in `a1678e8d`.
- At this capture, a separate uncommitted `spawn` lane exists in the main
  worktree. It was deliberately excluded from the foreach commits and has not
  passed the full gate set. Preserve it and verify its exact status before any
  staging or cleanup.

## Last closed executable rungs

The counted DRV-2 frontier adds fixtures 251 and 252:

- `tests/cases/backend_compare/for_in_array_literal_iterable/main.pgy`
- `tests/cases/backend_compare/for_in_member_iterable/main.pgy`

The manifest contains 260 MIR rows.

Objective card:

- Objective: derive non-identifier foreach iterable types from parser-owned
  expression graph facts and carry them through iteration facts, the one-time
  synthetic hoist, MIR, and codegen.
- Priority: graph identity, homogeneous array-literal typing, nominal member
  typing, fail-closed mismatch, then patch size.
- Fact owners: `SemanticExpressionGraphArrayLiteralTypeName` for recursive
  literal topology/type and the existing nominal receiver/member type owner for
  `b.items`.
- Last semantic consumer: `SemanticAstIterationTypeFactsFromArtifact`; MIR and
  codegen consume its explicit iterable/binding/hoist rows.
- Forbidden fallback: reparsing loop payload text, assuming `Array<Int>`, a
  fixture/member-name exception, or codegen-side iterable type recovery.
- Falsifiers: `[10, "bad", 30]` must fail as `actual: Unknown`; replacing
  `b.items` with `b` must fail as `actual: Bag`.

Observed before the fix:

- A Pergyra-built driver rejected both fixtures with
  `statement_type_unresolved`; the array literal and member expression were
  both reported as `actual: Unknown`.

Closed design and evidence:

- Iteration typing consumes the carried value and auxiliary expression graph
  roots. It no longer calls `SemanticAstExpressionVerdictFromPayload`.
- Non-empty homogeneous literals derive recursive `Array<T>` evidence; empty
  or heterogeneous literals do not acquire a guessed element type.
- Both fixtures carry `binding_type: Int`, `iterable_type: Array<Int>`, and
  `collection_hoisted: true`, plus one typed `__pgy_forin_0` local.
- C-built filtered producer-first parity passed:
  `backends=1 body_fixtures=20 mir_fixtures=2`.
- LLVM-built filtered producer-first parity passed with the same counts.
- Runtime output matched native: `60` for the literal loop and `15` for the
  member loop. Both negative mutations were rejected by both compilers.
- `tests/self_hosted_component_contract_smoke.sh` and shell syntax passed in an
  isolated worktree containing exactly the foreach commits.

Primary files:

- `src/self_hosted/semantic/ast_expression_graph_array_literal_owner.pgy`
- `src/self_hosted/semantic/ast_expression_verdict_owner.pgy`
- `src/self_hosted/semantic/ast_iteration_type_fact_owner.pgy`
- `src/self_hosted/compiler/driver_rung2_owner.pgy`
- `tests/self_hosted/parity/driver_rung2_iteration_expression_parity_owner.sh`
- `tests/self_hosted_component_contract_smoke.sh`

## Previous closed rungs retained

- Fixture 250, `fieldless_class_method`: a present empty nominal field
  inventory is distinct from a missing row. C uses ABI-owned reserved storage
  and `(Calc){ 0 }`; `Calc(1)` fails with `call_arity_mismatch`. Commit:
  `8afd9160`.
- Fixture 249, `string_utility_aliases`: append-only builtin row 101 maps
  `StringConcat` and `Concat` to the one `pgy_concat` ABI owner. Commit:
  `9a438da4`.
- Fixture 248, `result_as_class_field`: contextual `Result<T>` enters a
  declared `Result<T, E>` field through `ResultTypeAssignableTo`, with payload
  mismatch rejection. Commit: `5c2ba93b`.
- Native ArrayMap/ArrayFilter: the active-MIR call fact owns `Array<T>` result
  shape; C and LLVM consume it without local call-name guessing. Commits:
  `246682fe` and `a1678e8d`.

## Verification state

Green for the latest foreach rung:

- C and LLVM filtered producer-first source/MIR parity, two MIR fixtures each.
- Component contract and modified shell syntax in the isolated commit tree.
- Direct self-host negative mutations and native-oracle rejection.
- `git diff --check` for the implementation commits.

Green at the preceding fieldless/StringConcat checkpoint and to be rerun after
the latest docs refresh:

- `make self-host-hard-contract-test-smoke`.
- `make self-host-substitution-velocity-test-smoke`.
- `make sot-authority-edge-test-smoke`: 43 authorities, 38 derived carriers,
  `CLOSED=23 BRIDGE=20 ACTIVE=0`.
- `PGY_ALLOW_MISSING_COQ=1 make sot-authority-adequacy-test-smoke`: live
  owner/consumer and negative mutation checks passed; proof was a declared
  skip, not a checked model.
- `tests/documentation_quality_smoke.sh`.

Not run or unavailable:

- The full unfiltered 260-row DRV-2 matrix was not run.
- Coq/Rocq is not installed. Never report the formal model as checked here.

## Next executable work

The next observed failure is
`tests/cases/backend_compare/await_inline_spawn/main.pgy`:

- Last clean driver result: `initializer_type_unresolved` for
  `await spawn Inc(4)`.
- `spawn` needs one carried expression kind, an owned semantic result type,
  runtime ABI/lifetime, and codegen consumption.
- Preserve actual concurrency. A sequential call, a class/test-name branch,
  or a detached capture of local storage is forbidden.
- The dirty main-worktree spawn lane is only a proposal until its owner docs,
  focused positive/negative/runtime parity, and broader gates pass.

## Workstation and repository recovery

- Global Codex baseline: `C:/Users/user/.codex/AGENTS.md`; the repository
  `AGENTS.md` is more specific. Read both on resume.
- Git for Windows, Git LFS, GitHub CLI, MSYS2 UCRT64 GCC/Make/Python/LLVM,
  Node, npm, and ripgrep are installed. GitHub CLI auth remains user-owned.
- Repository `core.autocrlf=false`; Windows and MSYS safe-directory settings
  are configured.
- For Windows `bin/pgy.exe`, source `tests/pgy_binary_path_helpers.sh` and call
  `pgy_prepend_windows_runtime_paths` before execution.
- Use serial `make` unless Windows jobserver behavior is revalidated.
- `.gitignore` owns root builders with `/.tmp_*`. Recovery recycled 87 root
  temporary files and reduced `.tmp` from about 9.49 GB to an active cache.
- Credential hygiene: prior process inspection exposed configured Figma,
  GitHub, Render, and Tavily credentials in command-line output. Never copy
  values into docs/logs; rotate or revoke the affected credentials.

## Resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, `src/self_hosted/OWNERS.md`,
   and `selfhost.expression_surface` in the SoT registry.
2. Run `git status --short --branch`; preserve the dirty spawn lane unless it
   has since been committed or explicitly abandoned.
3. Reconfirm fixtures 251-252 with the filtered C parity gate.
4. Audit the spawn objective card and its negative/runtime evidence before
   accepting or extending that lane.
5. Refresh this snapshot with exact HEAD, dirty state, last green gate, next
   falsifier, and blockers.

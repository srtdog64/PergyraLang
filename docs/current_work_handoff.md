# Current Work Handoff

Updated: 2026-07-23 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. On resume, verify it
with `git status --short --branch`, the named owners, and the focused gate.
Current source, registries, and executable evidence win when they disagree.

## Resume checkpoint

- Latest self-host executable implementation: `11367f33` (`Close self-host
  named Future spawn await SoT`), with negative contract ratchet `3f2ba459`.
  The preceding inline spawn closure is `e98ba4ac`, with handoff refresh
  `7709890c`; foreach handoff is `c98d0a42`.
- Latest prior fieldless checkpoint: `8afd9160`, with handoff refresh
  `89625851`.
- Latest native backend checkpoint: `246682fe` (`Close C ArrayMap and
  ArrayFilter result type SoT`), with handoff refresh `4053192c` and matching
  LLVM owner consumption in `a1678e8d`.
- VS Code workspace/extension setup is closed in `720928c5` (`Configure VS
  Code Pergyra workspace`). At capture, `HEAD=origin/main=3f2ba459`; unrelated
  VS Code/editor packaging changes and `README.md` remain dirty and must not
  be staged by the next SoT rung.

## Last closed executable rungs

The counted DRV-2 frontier adds fixture 262:

- `tests/cases/backend_compare/async_spawn_await/main.pgy`

The manifest contains 262 MIR rows.

Objective card:

- Objective: carry named `Future<Int>` handle materialization and `await task`
  through one spawn/runtime owner after inline spawn closure.
- Priority: handle identity, ABI type ownership, real worker-pool lifetime,
  fail-closed payloads, then patch size.
- Fact owners: `FuturePayloadTypeOpt`, `spawn_runtime_owner.pgy`, ABI layout,
  and the semantic graph await branch; let codegen consumes those facts.
- Last semantic consumer: `EmitLet` binds `Future<Int>` as `PgyTaskHandle`, and
  named await projects the owned payload type into `pgy_await_take`.
- Forbidden fallback: treating `Future<Int>` as a scalar, a synchronous call,
  a fixture-name branch, or a detached local-storage capture.
- Falsifier: `generic_future_spawn_int` must fail at its actual generic AST
  artifact seam rather than being admitted by a Future-specific exception.

Observed before the fix:

- The self-host driver rejected `let task: Future<Int> = spawn Inc(4)` with
  `unsupported let type ... Future<Int>` because Future handle materialization
  was not an owned C ABI branch.

Closed design and evidence:

- `spawn_runtime_owner.pgy` now owns the bounded `Future<Int> -> PgyTaskHandle`
  ABI type; `EmitLet` registers the typed handle before `await task` consumes it.
- Named await validates the owned Future payload and emits
  `pgy_await_take(task, long long)` through the runtime owner.
- C producer-first parity passed:
  `backends=1 body_fixtures=20 mir_fixtures=1`; runtime output is `5`.
- MIR graph facts include `spawn Inc(4)` and `await task`; emitted C carries the
  handle and worker dispatch without a sequential fallback.
- The `Future<Int> -> Int` scalar mutation is rejected by self-host and native;
  emitted C is also ratcheted against a scalar/sequential fallback.
- Component contract and all modified shell syntax passed; `git diff --check`
  passed before the implementation and ratchet commits.

Primary files:

- `src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy`
- `src/self_hosted/codegen/emission/stmt_emit.pgy`
- `src/self_hosted/codegen/runtime_abi/spawn_runtime_owner.pgy`
- `src/self_hosted/codegen/emission/expr_semantic_graph_emit_owner.pgy`
- `src/self_hosted/compiler/driver_rung2_owner.pgy`
- `tests/self_hosted/parity/driver_rung2_spawn_await_parity_owner.sh`
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

Green for the latest named-Future rung:

- C filtered producer-first source/MIR parity for `async_spawn_await`.
- Named-Future scalar negative mutation parity and fallback prevention.
- Component contract and all modified shell syntax.
- Native C compile/run parity with output `5`.
- `git diff --check` for the implementation commit.

Green for the VS Code workstation surface:

- Workspace JSON parse, language-extension `node --check`, semantic-client
  TypeScript compile, and both npm audits (`0 vulnerabilities`).
- Pergyra language VSIX `0.3.1` and semantic-squiggle VSIX `0.0.1` packaged
  and installed; C/C++, Makefile Tools, and ShellCheck are also installed.
- The extension-equivalent Windows environment compiled the sample through
  `bin/pgy.exe --backend=c`, ran with output `42`, and received an empty
  diagnostic set from `bin/pgy-lsp.exe`.

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

- The full unfiltered 262-row DRV-2 matrix was not run; LLVM was not run for
  this async rung.
- Coq/Rocq is not installed. Never report the formal model as checked here.

## Next executable work

The next observed failure is
`tests/cases/backend_compare/generic_future_spawn_int/main.pgy`:

- Last probe result: `ast_artifact_invalid`, owned by
  `SemanticAstInitializerTypeFacts` (`node_count: 17`). The next seam is the
  generic Future expression/initializer artifact, not another local C type
  spelling branch.
- Preserve actual concurrency. A sequential call, a class/test-name branch,
  or a detached capture of local storage is forbidden.
- The next falsifier is `generic_future_spawn_int`; repair the parser-owned AST
  artifact or fail closed, without a generic-name exception or synchronous
  fallback.

## Workstation and repository recovery

- Global Codex baseline: `C:/Users/user/.codex/AGENTS.md`; the repository
  `AGENTS.md` is more specific. Read both on resume.
- Git for Windows, Git LFS, GitHub CLI, MSYS2 UCRT64 GCC/Make/Python/LLVM,
  Node, npm, and ripgrep are installed. GitHub CLI auth remains user-owned.
- Repository `core.autocrlf=false`; Windows and MSYS safe-directory settings
  are configured.
- For Windows `bin/pgy.exe`, source `tests/pgy_binary_path_helpers.sh` and call
  `pgy_prepend_windows_runtime_paths` before execution.
- Tracked `.vscode` settings select `bin/pgy.exe`, `bin/pgy-lsp.exe`, the
  MSYS2 UCRT64 terminal, focused build/self-host/doc tasks, and F5 Extension
  Host launchers. Both Pergyra extensions prepend discovered MSYS2/LLVM DLL
  directories themselves, so VS Code does not depend on a globally mutated
  user `PATH`.
- Use serial `make` unless Windows jobserver behavior is revalidated.
- `.gitignore` owns root builders with `/.tmp_*`. Recovery recycled 87 root
  temporary files and reduced `.tmp` from about 9.49 GB to an active cache.
- Credential hygiene: prior process inspection exposed configured Figma,
  GitHub, Render, and Tavily credentials in command-line output. Never copy
  values into docs/logs; rotate or revoke the affected credentials.

## Resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, `src/self_hosted/OWNERS.md`,
   and `selfhost.expression_surface` in the SoT registry.
2. Run `git status --short --branch`; the tree should be clean after this
   handoff refresh unless a newer task owns explicit dirty paths.
3. Reconfirm fixture 261 with the filtered C parity gate before changing the
   next async seam.
4. Work `async_spawn_await` from its named `Future<Int>` binding owner; do not
   turn it into a scalar or sequential-call fallback.
5. Refresh this snapshot with exact HEAD, dirty state, last green gate, next
   falsifier, and blockers.

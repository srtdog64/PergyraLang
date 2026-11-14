# 2026-11-15: an Int returned from `Main` is the process exit status

Answers [the OPEN question](2026-11-15_open_main_int_return_exit_status.md).
The user asked for the rule that is better in the long run and left the
choice to the Claude lane. It replaces the lane's earlier recommendation to
drop the value.

## The rule

The entrypoint is `Main`, or `main` when there is no `Main`.

| Entrypoint result | Exit status |
|---|---|
| `Void` | 0 when Main returns |
| `Int` | the returned value, handed to the host after Main returns |
| `Never` | whatever `Exit(code)` sets |
| anything else | refused by both front ends |

- The host takes the value as it takes `Exit(code)`'s argument. Linux keeps
  the low 8 bits, so `return 300` exits 44 on every leg, as `Exit(300)` does.
- The refusal is `PGY_SEM_TYPE_MISMATCH` with cause
  `semantic:entrypoint:result_type` and fix `return-int-or-void-from-main`.
  Both front ends report it before checking function bodies, so it is the
  first diagnostic on both.
- Fixtures whose `Main` returned another type only to carry a value keep
  that function as `Probe` and call it from a `Void` Main; their recorded
  diagnostics and spans do not change.

## Why not drop the value

- Dropping hides a value the program declared. `return 1;` meant as a
  failure exits 0, and a script that reads the status sees success.
- `Exit(code)` calls the C `exit` at once and skips Main's defers and
  ownership cleanup. Returning the status from Main is the only way to end
  with a non-zero status after that cleanup.
- Requiring `Void` would rewrite about 740 test and source files that
  declare `Main() -> Int`, for no gain over this rule.

## Owners

- Native: `mir_program_main_exit_kind` (`src/compiler/mir/mir_program_inventory.c`)
  reads Main's MIR routine result. The C wrapper (`src/codegen/transpiler.c`)
  and the LLVM wrapper (`src/codegen/llvm/llvm_main_wrapper.c`) return it,
  and fail closed on any other result.
- Self-host: the scalar CFG program C and LLVM emitters return the entry
  routine's Int (they dropped it); the literal Log route takes only a `Void`
  entrypoint (it died on an `Int` one). The AST C route already returned it.
- Refusal: `program_check_entrypoint_result` in
  `src/semantic/type_checker/type_checker_program.c` and
  `SemanticAstArtifactEntrypointVerdict` in
  `src/self_hosted/semantic/ast_artifact_verdict_owner.pgy`.
- Gate: `tests/self_hosted/parity/codegen/entrypoint_exit_status_leg_owner.sh`
  in the core push shard.

# Semantic Substitution Intent

## Intent

Provide the first Pergyra-written semantic checker slice for compiler-internal
substitution. The slice is deliberately bounded: function return types, typed
`let` bindings, scoped `if` / `while` bodies, branch conditions, simple
assignment, call arity and argument types, simple/compound undefined identifier
use, and literal/identifier expression typing for `Int`, `Long`, `Float`,
`String`, `Bool`, and `Void`.

## Input Contract

The tool reads one root source path from `Args()[0]`. `source_bundle_owner.pgy`
expands recursive `import "PATH.pgy";` declarations relative to the importing
file before `program_check_owner.pgy` consumes the source bundle. The accepted
subset is one or more `func` declarations with typed parameters, typed `let`
declarations, `return` statements, scoped `if` / `while` bodies, simple local
assignment, and direct calls to known functions.

`semantic_run_owner.pgy` owns the process boundary for this contract: missing
input is reported as a structured `input_missing` diagnostic, then the selected
root source bundle is checked by `program_check_owner.pgy`.
Expression type answers are owned by `expr_type_owner.pgy`; expression
diagnostics are owned by `expr_validation_owner.pgy` and must consume those type
answers rather than re-owning the type rules.

## Output Contract

The tool prints one deterministic diagnostic verdict:

- `Diagnostic: pgy.selfhost.semantic.v1`
- `Status: ok` or `Status: error`
- error diagnostics additionally include `Stage`, `Severity`, stable `Code`,
  `Reason`, `Fix`, `Span`, and structured `Facts`.

Only the first semantic mismatch is reported. Diagnostics carry `stage`,
`severity`, stable `code`, human `reason`, human `fix`, `span` (currently
`none` until source spans are captured), and structured `facts`. Unsupported
expressions are classified as `Unknown` and do not fail the subset checker,
except for simple identifier tokens in expression position where the name is
absent from the current local environment; those report `undefined_symbol`.
Logical and arithmetic/comparison operand diagnostics are selected by
`expr_validation_owner.pgy` after consuming `ExprType(...)` facts.
Same-type `Int`/`Long`/`Float` arithmetic is owned by
`expr_type_owner.pgy`; contextual integer literal assignment to `Long` is the
only widening rule in this rung, so mixed `Long`/`Int` arithmetic remains out
of subset instead of being guessed.

The renderer lives in `src/self_hosted/lib/diagnostic.pgy`; the semantic checker
only owns semantic codes, reasons, fixes, and facts. Do not rebuild the
diagnostic output shape inside `semantic/main.pgy`.

## Oracle

`src/self_hosted/parity/semantic_parity.sh` compiles this tool through the
available C/LLVM backends, runs it on committed fixtures, and checks the same
fixtures against the C compiler accept/reject oracle.

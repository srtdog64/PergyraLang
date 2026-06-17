# Semantic Substitution Intent

## Intent

Provide the first Pergyra-written semantic checker slice for compiler-internal
substitution. The slice is deliberately bounded: function return types, typed
`let` bindings, scoped `if` / `while` bodies, branch conditions, simple
assignment, call arity and argument types, simple/compound undefined identifier
use, and literal/identifier expression typing for `Int`, `String`, `Bool`, and
`Void`.

## Input Contract

The tool reads one source path from `Args()[0]`. The accepted subset is one or
more `func` declarations with typed parameters, typed `let` declarations,
`return` statements, scoped `if` / `while` bodies, simple local assignment, and
direct calls to known functions.

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

The renderer lives in `src/self_hosted/lib/diagnostic.pgy`; the semantic checker
only owns semantic codes, reasons, fixes, and facts. Do not rebuild the
diagnostic output shape inside `semantic/main.pgy`.

## Oracle

`src/self_hosted/parity/semantic_parity.sh` compiles this tool through the
available C/LLVM backends, runs it on committed fixtures, and checks the same
fixtures against the C compiler accept/reject oracle.

# Semantic Substitution Intent

## Intent

Provide the first Pergyra-written semantic checker slice for compiler-internal
substitution. The slice is deliberately small: function return types, typed
`let` bindings, and literal/identifier expression typing for `Int`, `String`,
`Bool`, and `Void`.

## Input Contract

The tool reads one source path from `Args()[0]`. The accepted subset is one or
more `func` declarations with typed parameters, typed `let` declarations, and
`return` statements.

## Output Contract

The tool prints one deterministic verdict:

- `SEMANTIC OK`
- `SEMANTIC ERROR <kind> expected=<type> actual=<type>`

Only the first semantic mismatch is reported. Unsupported expressions are
classified as `Unknown` and do not fail the subset checker.

## Oracle

`src/self_hosted/parity/semantic_parity.sh` compiles this tool through the
available C/LLVM backends, runs it on committed fixtures, and checks the same
fixtures against the C compiler accept/reject oracle.

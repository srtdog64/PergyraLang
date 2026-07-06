# Semantic Substitution Intent

## Intent

Provide the first Pergyra-written semantic checker slice for compiler-internal
substitution. The slice is deliberately bounded: function return types, typed
`let` bindings, scoped `if` / `while` bodies, branch conditions, simple
assignment, call arity and argument types, simple/compound undefined identifier
use, and literal/identifier expression typing for `Int`, `Long`, `Float`,
`String`, `Bool`, and `Void`.

## Compiler World Binding

- **world_zone**: `SemanticVerdictZone`
- **stage_actor**: `SemanticStage`
- **stage_intent**: `CheckProgramSemantics`
- **intent_cluster**: `MiddleEndPipeline`
- **payload_contract**: `SemanticVerdictPayloadContractReady`
- **manifest_binding**: `semantic|SemanticVerdictZone|SemanticStage|CheckProgramSemantics|SemanticVerdictPayloadContractReady`

## Input Contract

The tool reads one root source path from `Args()[0]`. `source_bundle_owner.pgy`
expands recursive `import "PATH.pgy";` declarations relative to the importing
file before `program_check_owner.pgy` consumes the source bundle. It directly
imports the path and text-scan owners it consumes. The entrypoint imports only
`semantic_run_owner.pgy`; stage owner dependencies are declared by the owners
that consume those facts, not by `main.pgy`. The accepted
subset is one or more `func` declarations with typed parameters, typed `let`
and `let mut` declarations, `return` statements, scoped `if` / `while` bodies,
simple local assignment, and direct calls to known functions.

`semantic_run_owner.pgy` owns the process boundary for this contract: missing
input is reported as a structured `input_missing` diagnostic, then the selected
root source bundle is checked by `program_check_owner.pgy`. It imports the
source-bundle, diagnostic, and program-check owners directly.
Expression type answers are owned by `expr_type_owner.pgy`; expression
diagnostics are owned by `expr_validation_owner.pgy` and must consume those type
answers rather than re-owning the type rules.
Literal-token position checks are owned by `text_scan_owner.pgy` as
`Option<Int>` facts. Consumers must use `IsSome` / `UnwrapOption` instead of
`-1` sentinel control flow when a literal is absent.

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
diagnostic output shape inside `semantic/main.pgy`. Stable semantic diagnostic
codes are owned by `diagnostic_code_owner.pgy`; expected fixture `Code:` fields
and `SemanticError...("code")` call sites must be registered there. The owner
also records the current C oracle JSON root code for each fixture-emitted
self-hosted code; the parity harness rejects an invalid fixture when the C
oracle falls through to a backend-native error or reports a different root code.
`diagnostic_owner.pgy` imports both the shared renderer and code vocabulary;
the entrypoint must not import either implementation detail directly.
`semantic_run_owner.pgy --diagnostic-vocabulary` emits the compiled vocabulary
rows consumed by the parity harness, so shell code compares an artifact from
the self-hosted owner rather than re-parsing owner source text.

## Oracle

`tests/self_hosted/parity/semantic_parity.sh` compiles this tool through the
available C/LLVM backends, runs it on committed fixtures, and checks the same
fixtures against the C compiler accept/reject oracle. The same gate also checks
the diagnostic-code vocabulary emitted by `--diagnostic-vocabulary` so new
self-hosted semantic codes cannot appear as fixture-only or call-site-only
aliases, and it checks that invalid fixtures are rejected with the mapped C
oracle JSON diagnostic code.

Fixture inventory is owned by the semantic tool, not by the shell runner.
`diagnostic_owner.pgy` walks `src/self_hosted/semantic/fixture`, reads the paired
`expected/*.diag` status, and `semantic_run_owner.pgy --fixture-manifest` emits
the `name:ok|error` rows consumed by the parity script.

`tests/self_hosted/parity/selfcheck_sources.sh` is the real-source rung. It
compiles this checker through C and LLVM and requires 56 curated self-host
owner/source files to produce `Status: ok`, including the parser entrypoint
through its real import bundle, the compiler path manifest owner, semantic
run/program/body/call/expression owner files, and the deterministic backend
fuzz generator. Files stay out of that manifest until the checker can consume
their imports, local bindings, and call surface without semantic fallbacks.

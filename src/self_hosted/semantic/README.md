# Semantic Substitution Track

Pergyra-written semantic checks live here, mirroring C-side `src/semantic/`.
The first rung is intentionally tiny: function signatures, local typed `let`,
basic literal/identifier types, and return typing.

`ast_artifact_verdict_owner.pgy` is the first semantic owner on the integrated
bootstrap path. It consumes the parser-owned `AstTreeArtifact` directly and
owns executable `Main` cardinality. Codegen consumes that verdict and is
ratcheted against recounting `Main`. The broader rung-2 checker below still
uses the source bundle scanner; it is not wired into the driver and does not
count as artifact-based semantic substitution.

`semantic_run_owner.pgy` owns the CLI/run boundary and directly imports the
source bundle, diagnostic, and program-check owners it consumes. `main.pgy`
must import only the run owner.
`source_bundle_owner.pgy` owns the root-source/import graph bundle consumed by
`program_check_owner.pgy` and directly imports `../lib/path.pgy` plus
`text_scan_owner.pgy` because import expansion consumes path and lexical-scan
facts. `main.pgy` must not import those source-bundle internals directly.
`try_expression_fact_owner.pgy` owns canonical try-expression shape and
operand bounds. `expr_type_owner.pgy` owns expression type queries and consumes
that shape owner;
`expr_validation_owner.pgy` owns expression diagnostics that consume those type
facts, including undefined identifiers and operator operand checks.
`diagnostic_code_owner.pgy` owns the stable lower-case diagnostic code
vocabulary consumed by `diagnostic_owner.pgy`; `diagnostic_owner.pgy` imports
that vocabulary and the shared diagnostic renderer directly, while `main.pgy`
must not import those internals. Call sites must not invent diagnostic code
strings outside that owner. The same owner also maps those
self-hosted codes to the current C oracle JSON root code for parity; if the C
oracle changes code ownership, this owner and the parity harness must change
together.
`program_check_owner.pgy`, `body_check_owner.pgy`, `call_check_owner.pgy`,
`expr_validation_owner.pgy`, and `expr_type_owner.pgy` declare their direct
fact-owner imports instead of relying on `main.pgy` as a dependency aggregator.
The run owner emits deterministic diagnostic verdicts plus owner-projected
manifests for committed fixtures, diagnostic vocabulary, and diagnostic surface
audit. The parity harness keeps the C compiler as the accept/reject oracle, but
it consumes the compiled semantic tool's `--diagnostic-vocabulary` and
`--diagnostic-surface-audit` artifacts instead of re-parsing
`diagnostic_code_owner.pgy`, expected `.diag` files, or semantic source call
sites. It also asks the compiled semantic tool's `--oracle-json-code-match`
mode to parse C oracle JSON diagnostics, so shell code does not own the
`"code"` extraction rule. Do not broaden this into declaration-heavy semantic
owners until expression-operator and diagnostic-code parity are gated. The
diagnostic-code gate checks committed fixture `Code:` fields and self-hosted
semantic call sites inside the emitted surface audit, then checks invalid
fixtures against the C oracle JSON code carried by the fixture manifest.
`selfcheck_sources.sh` is the real-source gate: it compiles this checker
through C and LLVM and runs it on 157 accepted self-host owner/source files,
including lexer/parser/
mir_lower/codegen/compiler-world, the compiler path manifest owner, semantic
run/program/body/call/expression owner files, the deterministic backend fuzz
generator, and audit-tool slices that are inside the current semantic subset.

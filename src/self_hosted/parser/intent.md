# Parser -- Intent / Contract

**Status:** *rung-1 expanding* (2026-06-18). Pergyra-written parser
substitute for a subset of the C-side `src/parser/`. It lexes + parses a
growing subset of Pergyra source and emits the same compact text tree
`pgy --ast` produces, byte-for-byte.

## Intent

After the lexer (`src/self_hosted/lexer/`) proved the committed token parity
rung and the broader historical 97% token surface, the parser is the next
compiler-internal substitute. This tool takes a Pergyra source file, lexes it,
recognises a growing declaration/statement/expression surface, and emits the
same `pgy --ast` tree. Coverage is measured in two ways: committed fixture
parity and the examples scale probe.

## Compiler World Binding

- **world_zone**: `AstTreeZone`
- **stage_actor**: `ParserStage`
- **stage_intent**: `ParseTokens`
- **intent_cluster**: `FrontendPipeline`
- **payload_contract**: `ParserAstTreePayloadContractReady`
- **manifest_binding**: `parser|AstTreeZone|ParserStage|ParseTokens|ParserAstTreePayloadContractReady`

## Input Contract

- **source_owner**: `source_path_owner.pgy`; `examples/hello.pgy` is the
  default path. The parity harness and
  scale probe pass the source path through `Args()[0]`; there is no side-channel
  source override.
- **run_owner**: `run_owner.pgy` owns parser CLI mode selection. `main.pgy` is
  entrypoint-only and calls `RunParserFromArgs(Args())`.
- **fixture_manifest_owner**: `fixture_manifest_owner.pgy` owns the committed
  source/fixture row inventory. `parser_parity.sh` consumes
  `--fixture-manifest` output and must not carry its own row list.
- **program_owner**: `program_parse_owner.pgy` owns the root source read, root
  cursor initialization, top-level declaration parse invocation, and final
  compact AST `Program:` assembly.
- **declaration_owner**: `decl_dispatch_owner.pgy` is the public boundary for
  top-level declarations. It owns declaration dispatch, import graph
  materialization, script-body collection, and branch owner imports. `main.pgy`
  must not own declaration import order.
- **statement_owner**: `stmt_owner.pgy` is the public boundary for the
  mutually recursive statement grammar. It owns statement dispatch and block
  recursion, and imports branch participants as one cluster because native
  imports reject circular imports. Branch-specific statement syntax is split by
  SoT owner: `stmt_loop_owner.pgy` owns `while`/`loop`/`for`, alongside
  `stmt_if_owner.pgy`, `stmt_parallel_owner.pgy`, and `stmt_match_owner.pgy`.
- **expression_owner**: `expr_owner.pgy` is the public boundary for the
  mutually recursive expression grammar. `expr_primary_owner.pgy` owns primary
  expression roots, `expr_postfix_owner.pgy` owns the postfix chain for calls,
  indexes, member access, object-init syntax, postfix try, and call-only
  turbofish consumption, `expr_precedence_owner.pgy` owns expression precedence,
  and `expr_string_owner.pgy` owns string interpolation desugaring. The split
  files are internal participants of one grammar owner because native imports
  reject circular imports.
- Current committed grammar surface:
  - top-level `[async]? [export]? func<T,U>`, `subject`, `class`, `vessel`,
    `struct`, `object`, `tobject`, `type` aliases/record aliases, `enum`,
    `namespace`, `event`, `ability`, `role`/`impl`, `party`, `roster`, `world`,
    `zone`, and `intent ... with retry(n)` metadata.
  - imports with source-relative recursive parse and import graph de-duplication
    through `source_path_owner.pgy`, plus common declaration methods/actions and
    nested generic type names, including simple `impl T`, `any T`,
    intersection, unit, and function type spelling.
  - statements: typed/inferred `let`, destructure-like let shapes, assignment,
    `+=`, `-=`, `<-`, `return`, `if`/`else if`/`else`, `while`, `for`,
    `loop`, `break`, `continue`, `defer`, `match`, `if let Some(...)`,
    `parallel`, `parallel on`/`every`, `continuous`, `transaction`, `fail`, and
    `with slot<TYPE> as VAR { ... }`.
  - expressions: unary `!`, `-`, `<-`, `spawn`, `spawn blocking`, `await`;
    binary precedence through arithmetic, pipe, comparison, `&&`, `||`;
    literals, identifiers, grouped/list primaries, tuple erasure, lambdas,
    postfix object init, calls, indexing, member access, postfix `?`,
    turbofish, `async {}`
    / `parallel (...) join with all {}` expression blocks, dollar string
    interpolation, and common duration suffixes.

## Output Contract

Plain text matching `pgy --ast <source>` byte-for-byte. Exit code `0` on
success, `1` if input cannot be read or parse fails.

## Oracle

The C-side reference is `pgy --ast <source>`. The parity rung asserts
byte-equal stdout against committed `fixture/<base>_ast.txt` files and runs a
live-drift guard against `pgy --ast` (graceful-skip if the sandboxed shell
cannot launch the pgy subprocess).

Current measured coverage:

- `parser_parity.sh`: 188 source/fixture rows byte-equal on both generated C
  and LLVM parser binaries. The compiled parser owner emits the manifest,
  including external `examples/hello.pgy` and duplicate
  `generic_class` coverage.
- `parser_scale_probe.sh --failing`: 120 of 121 `examples/*.pgy` byte-equal
  against live `pgy --ast`; zero byte-drift, zero self-host parser exits, and 1
  C-oracle skip (`secure_slots`).

## Not In Scope (yet)

- The remaining C-oracle skip (`secure_slots`) and full replacement of the
  text-mirror parser with structured AST ownership.
- Full parser recovery/error reporting parity.
- Replacing the C parser. This remains a side-by-side substitute rung with
  `pgy --ast` as oracle.

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

## Input Contract

- **source_owner**: `examples/hello.pgy` by default. The parity harness and
  scale probe pass the source path through `Args()[0]`; there is no side-channel
  source override.
- **program_owner**: `program_parse_owner.pgy` owns the root source read, root
  cursor initialization, top-level declaration parse invocation, and final
  compact AST `Program:` assembly. `main.pgy` only wires the selected source
  path into this owner.
- **statement_owner**: `stmt_owner.pgy` owns statement dispatch and block
  recursion. Branch-specific statement syntax is split by SoT owner:
  `stmt_loop_owner.pgy` owns `while`/`loop`/`for`, alongside the existing
  `stmt_if_owner.pgy`, `stmt_parallel_owner.pgy`, and `stmt_match_owner.pgy`
  branches.
- Current committed grammar surface:
  - top-level `[async]? [export]? func<T,U>`, `subject`, `class`, `vessel`,
    `struct`, `object`, `tobject`, `type` aliases/record aliases, `enum`,
    `namespace`, `event`, `ability`, `role`/`impl`, `party`, `roster`, `world`,
    `zone`, and `intent ... with retry(n)` metadata.
  - imports with source-relative recursive parse, plus common declaration
    methods/actions and nested generic type names, including simple `impl T`,
    `any T`, intersection, unit, and function type spelling.
  - statements: typed/inferred `let`, destructure-like let shapes, assignment,
    `+=`, `-=`, `<-`, `return`, `if`/`else if`/`else`, `while`, `for`,
    `loop`, `break`, `continue`, `defer`, `match`, `if let Some(...)`,
    `parallel`, `parallel on`/`every`, `continuous`, `transaction`, `fail`, and
    `with slot<TYPE> as VAR { ... }`.
  - expressions: unary `!`, `-`, `<-`, `spawn`, `spawn blocking`, `await`;
    binary precedence through arithmetic, pipe, comparison, `&&`, `||`;
    literals, identifiers, grouped/list/object-init primaries, tuple erasure,
    lambdas, calls, indexing, member access, postfix `?`, turbofish, `async {}`
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

- `parser_parity.sh`: 188 committed sources byte-equal on both generated C and
  LLVM parser binaries.
- `parser_scale_probe.sh --failing`: 120 of 121 `examples/*.pgy` byte-equal
  against live `pgy --ast`; zero byte-drift, zero self-host parser exits, and 1
  C-oracle skip (`secure_slots`).

## Not In Scope (yet)

- The remaining C-oracle skip (`secure_slots`) and full replacement of the
  text-mirror parser with structured AST ownership.
- Full parser recovery/error reporting parity.
- Replacing the C parser. This remains a side-by-side substitute rung with
  `pgy --ast` as oracle.

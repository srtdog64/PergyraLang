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

- **source_owner**: `examples/hello.pgy` by default. The parity harness passes
  the source path through `Args()[0]`; `fixture/source.txt` remains only as a
  local fallback for older probes.
- Current committed grammar surface:
  - top-level `[async]? [export]? func<T,U>`, `subject`, `class`, `vessel`,
    `struct`, `object`, `tobject`, `enum`, `namespace`, `event`, `ability`,
    `role`/`impl`, `zone`, and `intent ... with retry(n)` metadata.
  - imports with source-relative recursive parse, plus common declaration
    methods/actions and nested generic type names.
  - statements: typed/inferred `let`, destructure-like let shapes, assignment,
    `+=`, `-=`, `<-`, `return`, `if`/`else if`/`else`, `while`, `for`,
    `break`, `continue`, `defer`, `match`, `parallel`, and
    `with slot<TYPE> as VAR { ... }`.
  - expressions: unary `!`, `-`, `<-`, `spawn`, `spawn blocking`, `await`;
    binary precedence through arithmetic, pipe, comparison, `&&`, `||`;
    literals, identifiers, grouped/list primaries, lambdas, calls, indexing,
    member access, postfix `?`, and turbofish.

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
- `parser_scale_probe.sh --failing`: 107 of 119 `examples/*.pgy` byte-equal
  against live `pgy --ast`; 4 byte-drift, 7 self-host parser exits, and 1
  C-oracle skip (`secure_slots`).

## Not In Scope (yet)

- The remaining example scale-probe drift and self-host parser exit list.
- Full parser recovery/error reporting parity.
- Replacing the C parser. This remains a side-by-side substitute rung with
  `pgy --ast` as oracle.

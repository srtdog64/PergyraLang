# Parser -- Intent / Contract

**Status:** *rung-1 expanding* (2026-05-28). Pergyra-written parser
substitute for a subset of the C-side `src/parser/`. Lexes + parses a
growing subset of Pergyra source and emits the same text-tree
`pgy --ast` produces, byte-equal.

## Intent

After the lexer (`src/self_hosted/lexer/`) covered 97% of example
sources, the next roadmap step is the parser. This tool is the *first*
Pergyra-origin parser: it takes a Pergyra source file, lexes it,
recognises a growing subset of declarations and statements, and emits
the same `pgy --ast` text-tree. Coverage grows tick by tick.

## Input Contract

- **source_owner**: `examples/hello.pgy` by default. The parity harness
  may write a single-line override into `fixture/source.txt` to point
  the binary at a different source; see `src/self_hosted/parity/parser_parity.sh`.
- Current grammar surface (extends across ticks):
  - `(func IDENT((IDENT: IDENT,)*) -> IDENT { stmt* })+` — multi-function
  - `stmt = let IDENT: IDENT = expr ;`
  - `stmt = return expr? ;`
  - `stmt = IDENT((STRING|IDENT|));`
  - `expr = STRING | NUMBER | IDENT`

## Output Contract

Plain text matching `pgy --ast <source>` byte-for-byte. Exit code `0`
on success, `1` if input cannot be read or parse fails.

## Oracle

The C-side reference is `pgy --ast <source>`. The parity rung asserts
byte-equal stdout against committed `fixture/<base>_ast.txt` files and
runs a live-drift guard against `pgy --ast` (graceful-skip if the
sandboxed shell cannot launch the pgy subprocess).

## Not In Scope (yet)

- Arithmetic expressions in `let`/`return` initializers.
- `if`/`while`/`for` statements.
- Generics, lambdas, classes, enums, traits.
- AST nodes for declarations beyond `func`.
- Error recovery; performance.

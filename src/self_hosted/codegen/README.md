# Codegen Substitution Track

Pergyra-written C emitters. This is the **first hard compiler-core substitution
track** opened after the 2026-06-17 BDFL decision that lifted the
`docs/self_hosted/README.md` (2026-06-13) "hard compiler-core migration is not
open" freeze. Before that date this folder was a reserved stub.

## Rung-0/1/2 (2026-06-17) — active

`main.pgy` consumes `pgy --ast` text for a `func Main() -> Void` subset and emits
a self-contained C program whose **run-stdout** matches the C/LLVM oracle.
Supported body statements:

- `Log(<strexpr>)` — `<strexpr>` is a string literal or `Concat(<strexpr>, <strexpr>)`,
  folded to a single literal at emit time (rung-0).
- `Log(ToString(<intexpr>))` — emits `printf("%lld\n", ...)` (rung-1).
- `Let: <name> : Int = <intexpr>` — emits a `long long` declaration (rung-1).
- `Assign: <name> = <intexpr>` — emits a C assignment (rung-2).
- `While: (<cond>) { ... }` and `If: (<cond>) Then { ... } [Else { ... }]` —
  lowered structurally from the AST's indentation-nested `Block`/`Then`/`Else`
  nodes, with a recursive statement emitter (rung-2).

`<intexpr>` / `<cond>` is the C-compatible parenthesized infix the AST printer
produces (identifiers, integer literals, `+ - * / %`, comparisons, `&& ||`);
`Int` is modelled as C `long long`. Integer `/` and `%` match the oracle
including negative operands (truncation toward zero). Anything outside the subset
is an observable `Exit(1)` failure — no silent fallback.

Parity gate: `src/self_hosted/parity/codegen_parity.sh` builds `main.pgy` through
**both C and LLVM backends**, runs it on each of the 11 committed fixtures'
`pgy --ast` output, gcc-compiles the emitted C, runs it, and compares run-stdout
against the committed expected (which a live-drift guard re-derives from the
C-backend oracle exe). `make self-host-codegen-parity-test-smoke`.

The run-output equivalence criterion (not byte-equal C) follows
`src/self_hosted/PROGRESS.md` roadmap step 6: the oracle emits MIR-lowered C with
runtime headers; the Pergyra emitter produces standalone C. They are judged equal
only by observable program behavior.

## Next rungs (not yet implemented)

1. multiple user functions + calls (function definitions, `return`, args)
2. `String`-typed `let`, `Assign`, and string-valued expressions beyond `Concat`
3. `for` loops and `break` / `continue`
4. round-trip: Pergyra emitter compiles a Pergyra-written tool, output runs.

LLVM emission substitution is later than C emission.

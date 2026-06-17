# Codegen Substitution Track

Pergyra-written C emitters. This is the **first hard compiler-core substitution
track** opened after the 2026-06-17 BDFL decision that lifted the
`docs/self_hosted/README.md` (2026-06-13) "hard compiler-core migration is not
open" freeze. Before that date this folder was a reserved stub.

## Rung-0..5 (2026-06-17) — active

`main.pgy` consumes `pgy --ast` text for an `Int`/`String`/`Void` function subset
and emits a self-contained C program whose **run-stdout** matches the C/LLVM
oracle.

**Functions:** one or more functions, exactly one named `Main`. Each emits a C
function; non-`Main` functions get forward declarations (call order and recursion
are free); `Main` lowers to `int main(void)`.

- `Int` param → `long long`, return → `long long`
- `String` param → `const char*`, return → `char*`

**Body statements:**

- `Log(<strexpr>)` → `printf("%s\n", ...)`; `Log(ToString(<intexpr>))` → `printf("%lld\n", ...)`.
- `Let: <name> : Int|String = <expr>` — typed C declaration.
- `Assign: <name> = <expr>` — routed by the variable's recorded type.
- `While: (<cond>) { ... }`, `If: (<cond>) Then { ... } [Else { ... }]`,
  `For: <var> in <a>..<b> { ... }` (exclusive upper bound → C `for`), and
  `Break` / `Continue` — structural lowering of the AST's indentation-nested
  `Block`/`Then`/`Else` via a recursive emitter.
- `Return: <expr>` / bare `Return:` — routed by the function's return type.

`<intexpr>` / `<cond>` is the C-compatible parenthesized infix the AST printer
produces (`+ - * / %`, comparisons, `&& ||`, identifiers, `Name(args)` calls);
`Int` = C `long long`, with `/` and `%` matching the oracle including negatives.
`<strexpr>` is a string literal, `Concat(<strexpr>, <strexpr>)` (lowered to a
runtime `pgy_concat` that mallocs + memcpys), a string-typed identifier, or a
string-returning call.

**Type routing:** `Assign`/`Log`/`Return` need to know whether an operand is a
string or an integer. The emitter threads a per-function variable type
environment (seeded with parameters and extended by `Let`) plus a global
function return-type table built in a pre-scan. There is no block scoping or
string freeing yet — the guarantee is run-stdout parity, not memory correctness.

Anything outside the subset is an observable `Exit(1)` failure — no silent
fallback.

Parity gate: `src/self_hosted/parity/codegen_parity.sh` builds `main.pgy` through
**both C and LLVM backends**, runs it on each of the 18 committed fixtures'
`pgy --ast` output, gcc-compiles the emitted C, runs it, and compares run-stdout
against the committed expected (which a live-drift guard re-derives from the
C-backend oracle exe). `make self-host-codegen-parity-test-smoke`.

The run-output equivalence criterion (not byte-equal C) follows
`src/self_hosted/PROGRESS.md` roadmap step 6: the oracle emits MIR-lowered C with
runtime headers; the Pergyra emitter produces standalone C. They are judged equal
only by observable program behavior.

## Next rungs (not yet implemented)

1. `Bool` type and boolean-valued variables
2. string freeing / arena ownership and block scoping (memory correctness, not
   just run-stdout parity)
3. a larger expression/builtin surface (`StringLength`, `Substring`, array ops…)
4. round-trip: Pergyra emitter compiles a Pergyra-written tool, output runs.

LLVM emission substitution is later than C emission.

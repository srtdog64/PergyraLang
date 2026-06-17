# Codegen Substitution Track

Pergyra-written C emitters. This is the **first hard compiler-core substitution
track** opened after the 2026-06-17 BDFL decision that lifted the
`docs/self_hosted/README.md` (2026-06-13) "hard compiler-core migration is not
open" freeze. Before that date this folder was a reserved stub.

## Rung-0..10 (2026-06-17) - active

`main.pgy` consumes `pgy --ast` text for an `Int` / `Bool` / `String` /
`Array<Int>` / `Array<String>` / `Void` function subset and emits a
self-contained C program whose **run-stdout** matches the C/LLVM oracle.

**Functions:** one or more functions, exactly one named `Main`. Each emits a C
function; non-`Main` functions get forward declarations, so call order and
recursion are free. `Main` lowers to `int main(void)`.

- `Int` param -> `long long`, return -> `long long`
- `Bool` param/return -> `bool` (`<stdbool.h>`); `true` / `false` / `!` pass through
- `String` param -> `const char*`, return -> `char*`

**Body statements:**

- `Log(<strexpr>)` -> `printf("%s\n", ...)`; `Log(ToString(<intexpr>))` -> `printf("%lld\n", ...)`.
- `Let: <name> : Int|Bool|String|Array<Int>|Array<String> = <expr>` -> typed C declaration.
- `Assign: <name> = <expr>` -> routed by the variable's recorded type.
- `While: (<cond>) { ... }`, `If: (<cond>) Then { ... } [Else { ... }]`,
  `For: <var> in <a>..<b> { ... }` (exclusive upper bound -> C `for`), and
  `Break` / `Continue` -> structural lowering of the AST's indentation-nested
  `Block` / `Then` / `Else` via a recursive emitter.
- `Return: <expr>` / bare `Return:` -> routed by the function's return type.
- `Let: <name> : Array<Int>|Array<String> = [a, b, c]` -> growable struct
  (`pgy_ai` / `pgy_as`, a `{data,len,cap}`); the literal lowers to `new()` plus
  one `push` per element (`[]` is just `new()`). `ArrayPush(xs, v)`,
  `ArraySet(xs, i, v)`, `ArrayLength(xs)`, and index reads `xs[i]` lower to
  `pgy_*_push/set/len/get`. Index reads are rewritten env-aware in arbitrary
  expressions (`total + xs[j]` -> `total + pgy_ai_get(xs, j)`), with string
  literals copied verbatim so a `[` inside a string is never touched (rung-7/8/10).

`<intexpr>` / `<cond>` is the C-compatible parenthesized infix the AST printer
produces (`+ - * / %`, comparisons, `&& ||`, `!`, identifiers, `Name(args)`
calls). `Int` is C `long long`, with `/` and `%` matching the oracle including
negatives. Conditions are re-parenthesized so bare predicates (`if flag`) emit
valid C. `<strexpr>` is a string literal, `Concat(<strexpr>, <strexpr>)`, a
string-typed identifier, or a string-returning call. The builtins
`StringLength(x)`, `Substring(s, start, len)`, and `StringIndexOf(hay, needle)`
are rewritten to runtime helpers `pgy_strlen` / `pgy_substr` / `pgy_strindexof`
(`pgy_concat` for `Concat`; `pgy_strindexof` is `strstr`-based, returns -1 when
absent). `Exit(n)` lowers to `exit(n);`.

**Type routing:** `Assign` / `Log` / `Return` need to know whether an operand is
a string or an integer. The emitter threads a per-function variable type
environment, seeded with parameters and extended by `Let`, plus a global
function return-type table built in a pre-scan. There is no block scoping or
string freeing yet; the guarantee is run-stdout parity, not memory correctness.

Anything outside the subset is an observable `Exit(1)` failure; no silent
fallback. The intent contract is pinned in `intent.md`; this README is
explanatory.

Parity gate: `src/self_hosted/parity/codegen_parity.sh` builds `main.pgy` through
**both C and LLVM backends**, runs it on each of the 28 committed fixtures'
`pgy --ast` output, gcc-compiles the emitted C, runs it, and compares run-stdout
against the committed expected output. A live-drift guard re-derives that
expected output from the C-backend oracle executable.
Run `make self-host-codegen-parity-test-smoke`.

The run-output equivalence criterion, not byte-equal C, follows
`src/self_hosted/PROGRESS.md` roadmap step 6: the oracle emits MIR-lowered C with
runtime headers; the Pergyra emitter produces standalone C. They are judged equal
only by observable program behavior.

## Next Rungs

1. file / `Args` I/O builtins (`ReadFile`, `FileExists`, `Args`) - the surface
   the self-host tools need to run standalone
2. string freeing / arena ownership and block scoping (memory correctness, not
   just run-stdout parity)
3. arbitrary user struct / record types
4. round-trip self-compilation (the codegen tool compiling a Pergyra tool)

LLVM emission substitution is later than C emission.

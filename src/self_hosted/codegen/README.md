# Codegen Substitution Track

Pergyra-written C emitters. This is the **first hard compiler-core substitution
track** opened after the 2026-06-17 BDFL decision that lifted the
`docs/self_hosted/README.md` (2026-06-13) "hard compiler-core migration is not
open" freeze. Before that date this folder was a reserved stub.

## Rung-0..16 (2026-06-23) - active

`main.pgy` is the thin CLI entrypoint. It imports owner modules for AST input,
text scanning, type environment lookup, expression rewriting, statement
emission, function/declaration scanning, and program assembly. Together they consume
`pgy --ast` text for an `Int` / `Bool` / `String` / `Array<Int>` /
`Array<String>` / `Option<Int>` / `Void` function subset and emit a
self-contained C program
whose **run-stdout** matches the C/LLVM oracle.
String builtins: `Concat`, `ToString`, `StringLength`, `Substring`,
`StringIndexOf`, `StringTrim`, `StringJoin`, `Join`. Scalar conversion:
`ToFloat`. Array combinators: `ArraySort`, `ArrayMap`, `ArrayFilter`
for `Array<Int>` plus unary `Int -> Int` / `Int -> Bool` function references.
Result values: `Result<Int>` with `Ok`, `Err`, `IsOk`, `IsErr`, `Unwrap`,
`UnwrapOr`, and postfix `?` early-return lowering for `Int` payload lets inside
`Result<Int>` functions.
Option values: `Option<Int>` with `Some`, `None`, `IsSome`, and `UnwrapOption`.
Defer: block-local `Defer: / Block:` scope-exit statements with LIFO ordering
for the supported statement subset.
Tool I/O: `FileExists`, `ReadFile`, `Args`.
Arrays: growable `Array<Int>` / `Array<String>` locals plus `Array<Int>`
parameters and returns.
User structs: top-level `struct` declarations with `Int` fields, struct
literals, member reads, struct parameters, and struct returns.

**Functions:** one or more functions, exactly one named `Main`. Each emits a C
function; non-`Main` functions get forward declarations, so call order and
recursion are free. `Main` lowers to `int main(void)`, or to
`int main(int argc, char **argv)` when the fixture uses `Args()`.

- `Int` param -> `long long`, return -> `long long`
- `Bool` param/return -> `bool` (`<stdbool.h>`); `true` / `false` / `!` pass through
- `String` param -> `const char*`, return -> `char*`
- `Array<Int>` param/return -> `pgy_ai`; `Array<String>` param/return -> `pgy_as`
- struct param/return -> value-passed C typedef for the generated struct
- `Result<Int>` param/return -> value-passed `pgy_result_int`
- `Option<Int>` param/return -> value-passed `pgy_option_int`

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
  `pgy_*_push/set/len/get`. `ArraySort(xs)` sorts the shared `Array<Int>`
  buffer and returns the value, matching the oracle's value-with-shared-buffer
  behavior; `ArrayMap(xs, F)` / `ArrayFilter(xs, P)` are supported for unary
  `Int -> Int` / `Int -> Bool` functions. Index reads are rewritten env-aware in arbitrary
  expressions (`total + xs[j]` -> `total + pgy_ai_get(xs, j)`), with string
  literals copied verbatim so a `[` inside a string is never touched (rung-7/8/10).
- `Let: <name> : StructName = StructName { field: value, ... }` -> an Int-field
  C compound literal. Struct-returning calls can initialize struct locals, and
  member reads (`p.x`) pass through as C field access.
- `Let: <name> : Result<Int> = Ok(v)|Err(s)|Call(...)` -> value-passed
  `pgy_result_int`; `IsOk` / `IsErr` branch conditions and `Unwrap` /
  `UnwrapOr` integer expressions lower through local helpers. `Let: <name> :
  Int = Call(...)?` inside a `Result<Int>` function lowers to a temporary
  `pgy_result_int`, propagates `Err` with the active defer stack emitted before
  return, and binds the unwrapped `Int` payload on the success path.
- `Let: <name> : Option<Int> = Some(v)|None|Call(...)` -> value-passed
  `pgy_option_int`; `IsSome` branch conditions and `UnwrapOption` integer
  expressions lower through local helpers. Match-case `Some(v)` binding is not
  reconstructed from source text here; the MIR JSON path must provide
  `match_variant` and `match_bindings` facts before this emitter sees the
  lowered `Let: v : Int = UnwrapOption(...)` line.
- `Defer:` -> local block-exit emission in reverse registration order. Return
  paths emit the currently active defer stack before returning; broader
  resource/defer semantics remain owned by the native backend path.

`<intexpr>` / `<cond>` is the C-compatible parenthesized infix the AST printer
produces (`+ - * / %`, comparisons, `&& ||`, `!`, identifiers, `Name(args)`
calls). `Int` is C `long long`, with `/` and `%` matching the oracle including
negatives. Conditions are re-parenthesized so bare predicates (`if flag`) emit
valid C. `<strexpr>` is a string literal, `Concat(<strexpr>, <strexpr>)`, a
string-typed identifier, or a string-returning call. The builtins
`StringLength(x)`, `Substring(s, start, len)`, `StringIndexOf(hay, needle)`, `StringTrim(s)`,
and `StringJoin(xs, sep)` / `Join(xs, sep)` are rewritten to runtime helpers
`pgy_strlen` / `pgy_substr` / `pgy_strindexof` / `pgy_strtrim` / `pgy_strjoin`
(`pgy_concat` for `Concat`; `pgy_strindexof` is `strstr`-based, returns -1 when
absent). `ToFloat(s)` lowers to `atof(s)`. `Exit(n)` lowers to `exit(n);`. `Args()` lowers to a stable user-argv
snapshot (`argv[1..]`) stored in the same growable string-array representation.

**Type routing:** `Assign` / `Log` / `Return` need to know whether an operand is
a string or an integer. The emitter threads a per-function variable type
environment, seeded with parameters and extended by `Let`, plus a global
function return-type table built in a pre-scan. There is no block scoping or
string freeing yet; the guarantee is run-stdout parity, not memory correctness.

Anything outside the subset is an observable `Exit(1)` failure; no silent
fallback. The intent contract is pinned in `intent.md`; this README is
explanatory.

`ast_input_owner.pgy` owns the AST path policy (`Args()[0]` or the no-argument
`hello_ast.txt` fixture), the missing-file diagnostic, and the `ReadFile`
boundary. `main.pgy` only wires that owned AST text into `GenerateC`.

Parity gate: `src/self_hosted/parity/codegen_parity.sh` builds `main.pgy` through
the requested backend set, runs it on each of the 55 committed fixtures'
`pgy --ast` output, gcc-compiles the emitted C, runs it, and compares run-stdout
against the committed expected output. A live-drift guard re-derives that
expected output from the C-backend oracle executable. LLVM is mandatory when the
compiler build supports LLVM and explicitly skipped for C-only platform CI.
Run `make self-host-codegen-parity-test-smoke`.

The run-output equivalence criterion, not byte-equal C, follows
`src/self_hosted/PROGRESS.md` roadmap step 6: the oracle emits MIR-lowered C with
runtime headers; the Pergyra emitter produces standalone C. They are judged equal
only by observable program behavior.

Golden/platform contract: `hello` is the no-argument fixture. It must remain in
the active `FIXTURES` list and must pass when `PGY_SELFHOST_CODEGEN_BACKENDS=c`
is the only requested backend. The parity runner therefore builds command arrays
with the executable as element 0 instead of expanding a possibly empty argument
array; this keeps macOS bash 3.2, Git Bash, and Linux bash behavior aligned under
`set -u`.

## Next Rungs

1. string freeing / arena ownership and block scoping (memory correctness, not
   just run-stdout parity)
2. richer struct fields / nested AST-node shapes
3. round-trip self-compilation (the codegen tool compiling a Pergyra tool)

LLVM emission substitution is later than C emission.

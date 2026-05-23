# Syntax Pattern Matrix: Pergyra vs Common Languages

Last updated: 2026-05-12

Status: `beta-planning-reference`

Related source-of-truth documents:

- `docs/107_beta_stable_subset.md` for the beta-stable surface.
- `docs/42_keyword_orthogonality.md` for keyword axes.
- `docs/119_pergyra_lineage_positioning.md` for language lineage.
- `docs/121_types_as_domain_medium.md` for the type-system mandate.
- `docs/125_source_of_truth_spine.md` for compiler fact ownership.

## 0. Purpose

This document is the canonical crosswalk between familiar language patterns and
Pergyra's surface. It exists to prevent parser-first feature copying.

Before adding syntax because C#, Rust, TypeScript, Python, Go, Swift, Zig, or
Java has it, check this matrix:

1. Is there already a direct Pergyra equivalent?
2. Is there a Pergyra-native modeling answer that should be preferred?
3. Is the absence intentional, post-beta, or a real gap?

Stable means `syntax -> semantic -> runtime/ABI -> C -> LLVM -> diagnostics ->
regression -> docs` all agree. Parser acceptance alone is not stability.

## 1. Reference Languages

| Tier | Language | Why included |
| --- | --- | --- |
| Father | C# | Main surface ancestor: multi-paradigm, industrial, async, LINQ-style composition, attributes, tooling expectations. |
| Close contrast | Rust | Static ownership/MIR/borrow-checking reference, but not the target user model. |
| Systems substrate | C / C++ | ABI, FFI, native compilation, and systems baseline. |
| Industry surface | TypeScript | Wide industrial syntax, structural typing pressure, async/API ergonomics. |
| Industry surface | Python | High-ergonomics contrast and scripting expectations. |
| Concurrency sibling | Go | Channel and simple concurrency reference. |
| Resource sibling | Swift | ARC/value-reference split, Result/Option, async isolation pressure. |
| Compile-time contrast | Zig | Comptime and systems minimalism; mostly an explicit non-goal for core. |
| Compiler-engineering reference | D | Multi-paradigm systems language with C interop, UFCS, contracts, in-source `unittest`, and strong compile-speed lessons. CTFE/templates/mixins/GC are mostly non-goals for Pergyra core. |

## 1a. D Language Comparison Notes

D is not a direct surface parent, but it is useful as a compiler-engineering and
multi-paradigm reference. The correct reading is selective:

| D feature | Pergyra judgement | Notes |
| --- | --- | --- |
| `nothrow` | `native-different` | Pergyra's default is Result/failure-contract first, not exception-first. |
| `@safe` / `@trusted` / `@system` | `native-different` | Similar pressure exists, but Pergyra should express most of it through zone/authority/resource boundaries. |
| `pure` | `native-different` | Zone/effect absence already covers the *no observable side-effect* axis; no new core annotation needed. Recorded so the question does not get re-asked. |
| Multi-paradigm surface | `aligned` | Consistent with the C# lineage: procedural, OO-ish, generic, functional, and domain-oriented surfaces can coexist if the source-of-truth spine is clear. |
| Trivial C interop | `aligned` | Pergyra's C backend and ABI contract should preserve this direction. |
| In-source `unittest` blocks | `out-of-beta` | Useful for dogfood/self-host, but not a beta blocker. |
| UFCS (`x.f(y)` <-> `f(x, y)`) | `candidate` | Strong candidate for Intent-Compress / ergonomic method-free-function unification. Must not bypass authority/effect checks. |
| CTFE / `static if` | `reject` | Off-axis with `docs/121_types_as_domain_medium.md`; Pergyra types carry domain coordinates, not arbitrary compile-time programs. |
| String mixins / template metaprogramming | `reject` | Parser/source-trust risk and not aligned with the beta core. |
| `__traits` reflection | `out-of-beta` / `reject for core` | Reflection belongs behind stable ABI/schema/tooling, not as a core escape hatch. |
| Default GC | `reject` | Conflicts with systems-language identity and Slot/ABI ownership model. |
| Ranges | `candidate` | Consider as an `ability Iterable<T>` direction after dogfood, not as D-shaped syntax cloning. |
| Design-by-contract (`in` / `out` / invariant) | `native-different` | Pergyra's `requires`, `causes`, `intent`, and zone contracts are the native answer; avoid duplicate contract syntax before orthogonality audit. |

Decision for beta: do not import D's metaprogramming axis. Track UFCS,
compile-speed engineering, and in-source dogfood tests as post-beta candidates.
The most valuable D lesson for Pergyra is not a keyword; it is treating compile
speed and order-independent declarations as first-class compiler constraints.

## 2. Status Legend

| Status | Meaning |
| --- | --- |
| `stable` | Beta-stable or intended beta-stable surface. |
| `partial` | Implemented or accepted in part, but not closed across all owners. |
| `native-different` | Pergyra has a different first-class modeling answer. |
| `out-of-beta` | Useful later, but not a beta blocker. |
| `reject` / `explicit reject` | Deliberately not part of the core language direction. |
| `gap` | No satisfactory current answer; track before parser work. |
| `aligned` | External-language feature whose underlying direction matches Pergyra's design without 1:1 keyword adoption. Used in cross-language comparison tables. |
| `candidate` | Tracked as a strong post-beta surface candidate. Not committed; recorded so the matrix shows it was considered. |

## 3. Program, Module, And Visibility Patterns

| Pattern | Common shape | Pergyra mapping | Status | Notes |
| --- | --- | --- | --- | --- |
| Program entry | `main`, `static void Main`, `fn main` | `func main() -> Int` / current driver entry path | `partial` | Entry behavior must stay ABI-stable across C and LLVM. |
| Top-level function | `fn`, `def`, `function`, method | `func name(args) -> T` | `stable` | Baseline behavior. |
| Namespace/module block | `namespace`, `mod`, ES module | `namespace name { ... }` | `stable` | Namespaces are stable; package resolver is separate. |
| File import | `import`, `use`, `using` | `import "path.pgy";` | `stable` | Resolver subset is stable enough for beta examples. |
| Export public API | `public`, `pub`, `export` | `export func`, exported declarations | `stable` | Visibility propagation must remain DAG-owned. |
| Internal/protected visibility | `internal`, `protected`, `pub(crate)` | none | `out-of-beta` | Do not widen before module contract is frozen. |
| Import alias/specific import | `using X = Y`, `use a::{b}` | none | `out-of-beta` | Module ergonomics, not beta blocker. |
| Package dependency | NuGet, Cargo, npm, pip | none | `out-of-beta` | Ecosystem work after beta dogfood. |

## 4. Data And Type Declaration Patterns

| Pattern | Common shape | Pergyra mapping | Status | Notes |
| --- | --- | --- | --- | --- |
| Plain record/value object | `struct`, `record`, dataclass | `struct` | `stable` | Data-first shape. |
| Passive utility class | `class` | `class` | `stable` | Avoid using `class` for domain authority. |
| Active domain aggregate | C# class + DDD aggregate root | `subject` | `native-different` | Pergyra splits active domain identity from passive classes. |
| Internal state container | private state holder | `vessel` | `partial` | Keep out of broad claims until examples are closed. |
| Read projection DTO | DTO/read model | `object` | `stable` | Projection target surface. |
| Transfer DTO | transport DTO / serialized shape | `tobject` | `stable` | Transport-boundary shape. |
| Enum / ADT | `enum`, Rust enum, tagged union | `enum` | `stable` | Frozen subset only. |
| Type alias | `using`, `type` | `type Name = T` | `partial` | DAG ownership must remain source of truth. |
| Interface / trait | C# interface, Rust trait | `ability` | `stable` | Exact and multi-bound baseline. |
| Trait implementation | `impl Trait for T` | `role Name for Subject` | `native-different` | Domain role, not a Rust trait clone. |
| Tuple | `(T, U)` | tuple type/literal surface | `partial` | Active surface; close ABI, diagnostics, and C/LLVM parity before promoting. |
| Constructor / initializer | `new T(...)`, object initializer | declaration-specific constructors and calls; initializer sugar rejects explicitly | `partial` | Keep construction ordinary-call shaped; object-initializer sugar is not frozen. |
| Anonymous record | TS object literal / C# anonymous type | none | `out-of-beta` | Not a core beta surface. |

## 5. Binding, Mutation, And Resource Access

| Pattern | Common shape | Pergyra mapping | Status | Notes |
| --- | --- | --- | --- | --- |
| Immutable local | `let`, `const`, `val` | `let x = ...;` | `stable` | Normal local binding. |
| Mutable local | `var`, `let mut`, `ref` mutation | assignment / subject state updates | `partial` | No broad public `mut` baseline. |
| Destructuring | tuple/object destructure | `let (a, b) = ...;` | `partial` | Keep CFG/dataflow-owned. |
| Reference / borrow | `&T`, `ref T`, pointer/ref | `ref` over supported boundaries; slot handles | `native-different` | Pergyra is not a Rust lifetime language. |
| Move/ownership transfer | move-only value, `unique_ptr` | `own`, `MoveToken<T>`, anchored slot boundaries | `partial` | Stable only for anchored subset. |
| RAII/drop/finally | Rust `Drop`, C# `using`, `finally` | `defer`, MIR cleanup, pin cleanup | `partial` | Cleanup source of truth must be MIR facts. |
| Managed object reference | GC reference / handle | `Slot<T>` / registry / handle | `native-different` | Runtime-validated handle plus static boundary verifier. |
| Raw pointer escape | `unsafe`, `*T`, `void*` | explicit unsafe/raw escape policy | `out-of-beta` | Systems baseline item, not default surface. |
| Lock/mutex | `lock`, `Mutex<T>` | Slot pin/view/resource boundaries | `native-different` | Do not import lock syntax as core by default. |

## 6. Functions, Methods, And Behavior

| Pattern | Common shape | Pergyra mapping | Status | Notes |
| --- | --- | --- | --- | --- |
| Free function | `fn`, `def`, `function` | `func f(...) -> T` | `stable` | Baseline function. |
| Hosted method | `this`/`self` method | hosted `func` with `self` context | `stable` | Declaration inventory must be MIR/DIR/RIR-owned. |
| Domain behavior | method with preconditions/effects | `action ... requires / within / causes` | `stable` | Action contracts stay explicit in IR. |
| Member access / method chaining | `x.y`, `x.f()` | `AST_MEMBER_ACCESS` + call chaining | `stable` | Must stay ordinary expression lowering; do not overload into authority semantics. |
| Pipeline composition | `x |> f` | pipe operator parser surface | `partial` | Useful for readable transforms; keep type inference and diagnostics explicit. |
| Lambda / closure literal | `x => x + 1`, `|x| x+1` | `=>` lambda AST/parser surface | `partial` | Standalone lambda callables are active. Lambda-local `let`/destructure/for bindings are not captures; outer local/resource capture is semantically rejected until closure environments and boundary lifetime facts are frozen. |
| Function reference | method group / function pointer | function name as value | `partial` | Enough for narrow callbacks. |
| Higher-order function | `Func<T,R>`, `Fn` | callable parameters | `partial` | No HKT/functor hierarchy. |
| Named arguments | `f(x: 1)` | parser/AST reserved; semantic reject | `partial` | Source spelling and AST print are reserved; call ABI/dispatch is not implemented. |
| Default arguments | `f(x = 1)` | parser-rejected reserved surface | `out-of-beta` | Value defaults are explicit parser rejects until ABI/call dispatch policy exists. |
| Variadic function | `params`, varargs | none | `out-of-beta` | Not beta closure. |
| Cast / type test | `as`, `is`, `dynamic_cast` | reserved statement-expression surface / conversion helpers | `out-of-beta` | Needed for interop eventually; beta prefers explicit conversion/predicate helpers and now rejects `let`/`return`/expression-statement cast spelling explicitly. |

## 7. Control Flow And Pattern Matching

| Pattern | Common shape | Pergyra mapping | Status | Notes |
| --- | --- | --- | --- | --- |
| Conditional | `if / else` | `if / else` | `stable` | CFG must own body safety. |
| Else-if shorthand | `else if` | nested `if` / accepted surface where present | `partial` | Do not expand syntax until parser contract is checked. |
| While loop | `while` | `while` | `stable` | CFG reachability and cleanup facts required. |
| For range | `for i in 0..n` | `for` over ranges | `stable` | Keep simple. |
| For collection | `foreach`, `for v in xs` | `for` over collections | `stable` | Frozen collection subset only. |
| Break / continue | `break`, `continue` | `break`, `continue` | `stable` | Cleanup edges must be validated. |
| Infinite loop | `loop`, `while true` | `loop` or equivalent | `partial` | Verify against all-path return. |
| Match expression | Rust `match`, C# switch expression | `match` | `stable` | Frozen subject/enum subset only. |
| Enum / Result / Option destructure | `Ok(v)`, `Some(v)` | `case .Ok(v)`, `case .Some(v)` | `stable` | Diagnostics must stay explicit. |
| Struct/list patterns | `Struct { x }`, `[a, b]` | none | `out-of-beta` | Useful later, not beta blocker. |
| Guarded match case | `case p if cond` | active match-guard surface | `partial` | Keep CFG-owned; do not let guards bypass exhaustiveness diagnostics. |
| Or-pattern | `case A | B` | active pattern-or surface | `partial` | Backend parity and diagnostics decide promotion. |
| Goto | `goto` | none | `reject` | Not compatible with CFG safety contract. |
| Switch fallthrough | C/C# fallthrough style | none | `reject` | Use explicit match/branching. |

## 8. Error And Failure Handling

| Pattern | Common shape | Pergyra mapping | Status | Notes |
| --- | --- | --- | --- | --- |
| Optional value | `T?`, `Option<T>` | `Option<T>` | `stable` | Prefer over nullable control flow. |
| Fallible result | `Result<T,E>`, `Either`, `Try<T>` | `Result<T, E>` / current Result surface | `stable` | Recoverable failure surface. |
| Error propagation | `?`, `try`, `await` failure | Result/failure contracts | `partial` | Keep recoverable vs hard-fail boundary explicit. |
| Exception throw/catch | `throw`, `try/catch` | none | `reject` | Pergyra is Result/failure-contract first. |
| Panic/abort | `panic!`, process abort | hard-fail runtime boundary | `stable` | Internal invariant/slot/token violations hard-fail. |
| Finally | `finally`, RAII cleanup | `defer` / MIR cleanup facts | `native-different` | MIR cleanup is source of truth. |
| Custom error hierarchy | exception classes / error traits | limited | `out-of-beta` | Do not block beta on rich error taxonomy. |

## 9. Async, Parallel, And Streaming

| Pattern | Common shape | Pergyra mapping | Status | Notes |
| --- | --- | --- | --- | --- |
| Async function | `async fn`, `async Task<T>` | `async func` | `stable` | Stable subset only. |
| Await | `.await`, `await task` | `await` | `stable` | Pin/borrow boundaries must reject unsafe crossing. |
| Spawn task | `tokio::spawn`, `Task.Run`, goroutine | `spawn` | `stable` | Token/authority transport rules apply. |
| Parallel block | Rayon/Parallel.For/scope | `parallel` / `parallel on` | `stable` | Layered resource/concurrency/domain model. |
| Channel | Go channel / mpsc | `Channel<T>`, send/recv | `stable` | Cross-world transfer stays channel-only. |
| Select | Go `select`, Tokio select | `select` | `partial` | Keep semantics narrow. |
| Cancellation | token/abort/cancel scope | cancel/failure contracts | `partial` | Runtime state and diagnostics need tightening. |
| Atomic | `AtomicI32`, `Interlocked` | runtime/internal only | `out-of-beta` | User-facing atomics are not beta core. |

## 10. Domain-Orchestration Patterns

| Pattern | Common shape | Pergyra mapping | Status | Notes |
| --- | --- | --- | --- | --- |
| Saga/workflow | Camunda/Temporal/Saga lib | `intent` | `stable` | Spine of orchestration. |
| State machine | explicit state enum + transitions | `intent` + `zone` + `effect` | `native-different` | Lifecycle propagation must stay explicit. |
| DDD aggregate boundary | aggregate root class | `subject` + `zone` | `native-different` | Avoid forcing tree-shaped ownership. |
| Authorization guard | middleware/filter/attribute | `authority`, `authorized by`, zone policy | `stable` | Layered authority, not one universal owner. |
| Read model projection | DTO/read model/mapper | `projection`, `object`, `tobject` | `partial` | Freshness/provenance closure remains important. |
| Relationship table | ORM relationship / association | `relation` | `stable` | Authority-resource-effect ordering must stay coherent. |
| Effect attachment | mixin/decorator/state extension | `effect` | `stable` | Domain policy, not full Koka-style effect calculus. |
| Transaction rollback | transaction/saga compensation | `compensate`, rollback, `Result` | `partial` | Intent failure model must remain queryable. |
| Intent compact form | decorators / minimal workflow DSL | AI-fillable intent frame plus compiler verification | `partial` | `who`, action `within`, `requires`, `causes`, action-declared `authorized by`, and `using` derivation are active. `who` is actor/provenance, not approval; missing authority must be explicit or inherited from an explicit action contract. The compiler verifies declared evidence; it must not invent domain policy from a goal sentence. Remaining work is richer conflict diagnostics and edge-case provenance. |

## 11. Type-Level And Generic Patterns

| Pattern | Common shape | Pergyra mapping | Status | Notes |
| --- | --- | --- | --- | --- |
| Generic type/function | `T`, `Foo<T>` | generic declarations | `stable` | Frozen exact/ability/multi-bound subset. |
| Constraint / trait bound | `where T : IFoo`, `where T: Trait` | `where T: Ability + Other` | `stable` | DAG and diagnostics must own enforcement. |
| Default type argument | `T = Default` | default type arg resolution | `partial` | Keep actual resolution evidence visible. |
| Associated type | Rust associated type / C# nested type | none | `out-of-beta` | Not required for beta dogfood. |
| Higher-kinded type / functor hierarchy | Haskell/F# type classes | none | `reject` | FP adapters belong in modules, not core HKT. |
| Zig-style comptime | `comptime`, type-as-value | none | `reject` | Pergyra types carry domain coordinates; they are not compile-time programs. |
| Dependent type | Idris/Agda style | none | `reject` | Use domain coherence checks instead. |
| Phantom type | unused type coordinate | possible later | `out-of-beta` | Needs ownership/generic classifier design. |
| Dynamic dispatch / trait object | Rust `dyn Trait`, C# interface reference | static dispatch only via `ability` constraint | `out-of-beta` | Needs vtable ABI, lifetime rules, and intent/zone interaction policy before any `dyn ability` surface. |
| Const generic | Rust `const N: usize`, C++ NTTP | none | `out-of-beta` | Useful for array sizing; revisit after generic ownership classifier (Option C) is settled. |
| Tuple struct / newtype | Rust `struct Wrapper(T)` | use `struct Wrapper { inner: T }` | `native-different` | Use explicit named field; do not adopt positional struct surface. |
| Implicit numeric conversion | C# `int -> long` implicit | none — explicit cast/conversion helpers only | `native-different` | Prevents silent precision loss; aligned with signed-default decision. |
| Variance / phantom lifetime markers | Rust PhantomData, variance annotations | none | `out-of-beta` | Pergyra does not expose lifetime parameters; variance is not surface-visible. |

## 12. Literals, Collections, And Operators

| Pattern | Common shape | Pergyra mapping | Status | Notes |
| --- | --- | --- | --- | --- |
| Integer / long / float | `1`, `1L`, `1.0` | numeric literals | `stable` | Backend parity required. |
| Hex / binary / octal literal | `0x1F`, `0b1010`, `0o17` | decimal integer literal only | `out-of-beta` | Useful for bit-twiddling code; reserve grammar before parser ambiguity emerges. |
| Negative literal vs unary minus | `-1` parsed as literal vs unary | unary minus over decimal literal | `partial` | Keep unary lowering; literal-negative shorthand reserves a later parser choice. |
| Character literal | `'a'`, `'\n'` | none — single-character `String` only | `out-of-beta` | Decide whether `Char` is a primitive or a `String` alias before stdlib freeze; current path treats characters as 1-length strings. |
| Underscore placeholder | `_` unused-binding / type elision | none | `out-of-beta` | Useful for `let _ = expr`, unused param, and generic elision; reserve before grammar conflicts. |
| String | `"text"` | string literal | `stable` | Unicode policy is separate. |
| String interpolation | `$"x={x}"`, f-string | active lexer/parser lowering | `partial` | Freeze public spelling, escaping, and backend parity before promoting. |
| Array literal | `[1, 2, 3]` | array literal | `stable` | Type inference must avoid poison defaults. |
| Indexing | `xs[i]` | `AST_ARRAY_ACCESS` | `stable` | Index type and target indexability diagnostics are already first-class. |
| Slicing | `xs[a..b]`, `xs[..]` | reserved parser surface / helper functions | `out-of-beta` | Slice ABI types exist internally; expression slicing now rejects explicitly until public slicing grammar/policy is frozen. |
| List literal | `vec![...]`, `[]`, collection initializer | array literal only; collection literal sugar out-of-beta | `out-of-beta` | Add typed collection literals only after collection ABI/map policy is frozen. |
| Map/set literal | dict/hashmap/set literal | `{ ... }` object/map literal syntax rejects explicitly | `out-of-beta` | Same as collection literal; use collection APIs for beta. |
| Spread/rest | `...xs`, `*args`, destructure rest | none | `out-of-beta` | Useful with collections/calls later; not beta core. |
| Range | `0..n` | range in loops | `stable` | Keep simple. |
| Arithmetic/comparison/logical operators | `+`, `<`, `&&` | same | `stable` | Backend parity required. |
| Compound assignment / inc-dec | `+=`, `-=`, `++`, `--` | event subscribe/unsubscribe uses `+=`/`-=`; general compound assignment absent | `out-of-beta` | Do not overload event syntax into general mutation before CFG cleanup facts own it. |
| Operator overload | C# overload, Rust `Add` | none | `out-of-beta` | Avoid before generic/ability model is settled. |
| Custom operator | Scala-like operator surface | none | `reject` | Surface trust risk. |
| Null coalescing / optional chaining | `??`, `?.` | `??` lowers to Option coalescing; `?.` remains reserved/rejected | `partial` | `??` is active for `Option<T> ?? T -> T`; optional chaining remains out-of-beta. |

## 13. Metaprogramming, Reflection, And Tooling

| Pattern | Common shape | Pergyra mapping | Status | Notes |
| --- | --- | --- | --- | --- |
| Block / line comment | `//`, `/* */` | `//`, `/* */` | `stable` | Standard lexer surface. |
| Macro | Rust macro, C preprocessor | none | `reject` | Parser/source trust risk for beta. |
| Source generator | C# source generator, build.rs | none | `out-of-beta` | Package/tooling track. |
| Attribute/annotation | `[Attr]`, `#[attr]`, decorators | structured comments; `@` reserved/rejected | `out-of-beta` | Do not use as escape hatch for core semantics. |
| Doc comments | `///`, `/** */`, XML docs | `///` doc-comment tokens and structured tags | `partial` | Keep as documentation metadata, not a semantic annotation escape hatch. |
| Reflection | `typeof`, runtime metadata | limited runtime registries | `out-of-beta` | Needs ABI/schema discipline. |
| LSP diagnostics | editor diagnostics | `pgy-lsp` path | `partial` | Conformance subset must be explicit. |
| Formatter | `fmt` | formatter path | `partial` | Idempotence/round-trip gate required. |
| Debugger | GDB/LLDB/debugger | AST-walking debugger only | `gap` | DWARF/CodeView needed before serious self-host. |

## 14. FFI, Systems, Backend, And Dogfood

| Pattern | Common shape | Pergyra mapping | Status | Notes |
| --- | --- | --- | --- | --- |
| C FFI | `extern "C"` / PInvoke | `extern` / ABI spec | `partial` | ABI non-leakage contract must remain enforced. |
| No-runtime mode | `no_std`, freestanding | runtime-none profile | `partial` | Needed for systems identity, but not full beta blocker. |
| Manual allocation | malloc/new/custom arena | runtime/slot APIs | `partial` | Do not expose broad raw memory by default. |
| Inline assembly | `asm`, `unsafe asm` | none | `out-of-beta` | Systems-tier feature, not dogfood blocker. |
| WebAssembly | wasm backend | C backend -> Emscripten bridge | `out-of-beta` | WebGL dogfood is module/bridge track, not core syntax. |
| Native GPU/WebGL/Skia | framework/API | `pgy.render.*`, `pgy.accel.spray` modules later | `out-of-beta` | Keep out of core language surface. |
| Self-host compiler | compiler in same language | post-beta trajectory | `out-of-beta` | Requires module/std/debugger/bootstrap story. |

## 15. Pergyra-Unique Surfaces To Preserve

| Surface | Core value claim | Closest mainstream analogy |
| --- | --- | --- |
| `subject` | Active domain identity with behavior and state. | C# class + DDD aggregate root, but first-class. |
| `object` / `tobject` | Read vs transport shape split. | DTO/read model pattern, but language-visible. |
| `ability` | Domain contract coordinate. | C# interface / Rust trait. |
| `role` | Domain role binding for a subject. | Trait impl / mixin, but domain-owned. |
| `party` / `roster` | Participation and composition units. | No direct mainstream primitive. |
| `zone` | Authority/resource boundary. | Actor/capability scope. |
| `world` | Outer trust/failure/handoff boundary. | Actor system/process boundary. |
| `relation` | Pairwise domain linkage. | ORM association, but not library-only. |
| `effect` | State attachment / lifecycle influence. | Mixin/decorator pattern, but semantic. |
| `intent` | Observable orchestration spine. | Saga/workflow/state machine library. |
| `Slot<T>` | Runtime-validated resource handle. | Generational handle + capability model. |
| `pin` / `ReadView<T>` / `WriteView<T>` | Scoped typed view and amortized access. | Rust borrow/pin ideas, but no lifetime annotation model. |

## 16. Essential Common Patterns To Track

These are not unique Pergyra surfaces. They are common patterns that appear in
most practical languages because ordinary programs repeatedly need them. If one
of these remains absent, the absence must be intentional and documented; otherwise
it becomes a language ergonomics problem rather than a beta-scope discipline.

| Pattern | Why it is essential | Current Pergyra answer | Required direction |
| --- | --- | --- | --- |
| Cheap local binding + inference | Every program starts here. | `let x = expr` | Keep stable; do not burden with domain clauses. |
| Function/method declaration | Basic abstraction unit. | `func`, hosted `func`, `action` | Keep `func` simple; use `action` only when domain contracts matter. |
| Composite data declaration | Real programs need records. | `struct`, `class`, `subject`, `object`, `tobject` | Preserve the split, but keep plain `struct` easy. |
| Generic abstraction | Libraries cannot scale without it. | `T`, `where T: Ability + Other` | Keep as core; avoid HKT/comptime expansion. |
| Module/import/export | Codebases cannot grow in one file. | `namespace`, `import`, `export` | Resolver subset must stay stable; package manager can wait. |
| Option/result failure shape | Null/exception alternatives are required. | `Option<T>`, `Result<T, E>` | Keep recoverable failure first-class and explicit. |
| Pattern matching / branch destructure | Enums/results/options need readable handling. | `match`, enum/result/option cases | Extend only after CFG/body safety is stable. |
| Collection construction | Data-heavy code uses collections constantly. | Arrays stable; `List`/`Set`/`HashMap` APIs | Literal sugar is a real post-beta gap, not a thesis feature. |
| Element access | Arrays, lists, strings, maps all need indexed/keyed access. | `xs[i]` array access is active; richer slicing/keyed indexing is not frozen. | Keep index diagnostics stable; reserve slicing/keyed policies after collection ABI closure. |
| String formatting/interpolation | Logs, diagnostics, CLI, examples all need it. | string literal + functions | Treat interpolation as high-value ergonomics after string policy. |
| Object construction | Real programs need predictable initialization. | Constructors/calls and declaration initializers | Avoid object-initializer sugar until ownership and cleanup facts can own partial construction. |
| Callable/callback shape | UI, async, streams, and tooling need callbacks. | named functions / limited callable params | Closure literals require capture/resource-boundary design. |
| Async and parallel tasks | Modern code needs concurrency. | `async`, `await`, `spawn`, `parallel`, channel | Keep layer boundaries visible; do not collapse into one async keyword. |
| Cleanup/finalization | Early returns and failures need cleanup. | `defer`, MIR cleanup facts | MIR cleanup must be the source of truth. |
| FFI/host bridge | Dogfood and systems work need host calls. | `extern` / ABI spec | Freeze minimal C ABI path before richer package ecosystem. |
| Diagnostics with reason/fix | A new language lives or dies on errors. | diagnostic registry + `Reason:` / `Fix:` policy | All user-facing errors should converge on source span, reason, fix, severity. |
| Debuggable build output | Self-host needs debugging. | AST-walking debugger only | DWARF/CodeView is post-beta but pre-serious-self-host. |

## 17. Highest-Value Missing Patterns

| Missing pattern | Why it matters | Preferred direction | Beta position |
| --- | --- | --- | --- |
| Intent clause inference | Intent is the signature surface; verbosity hides the thesis. | `on:` inference is active for common receiver/action contracts; finish conflict diagnostics and make compact form the documented default. | Beta-adjacent ergonomics; not a new keyword. |
| String interpolation | Every log/output example wants it. | Parser surface exists; freeze one public spelling and backend parity after string/Unicode policy. | Post-beta unless dogfood proves blocker. |
| Collection literals | Every collection init wants it. | Add after collection ABI and HashMap key policy freeze. | Post-beta unless dogfood proves blocker. |
| Slicing and keyed indexing | Data-heavy code quickly wants `xs[a..b]` and map-like `m[k]`. | Keep `xs[i]` stable; design slicing/keyed indexing after collection ABI and Option failure policy. | Post-beta unless dogfood proves blocker. |
| Named/default args | API and intent ergonomics. | Named call spelling is reserved and semantically rejected; value default args are parser-rejected in function/async/lambda parameter lists. | Post-beta. |
| Closure literals | Callbacks, UI, async, streams. | Parser/AST surface exists; local/resource capture is rejected instead of silently lowering without an environment. | Full capture semantics remain post-beta. |
| Cast/type-test syntax | Interop and generic-heavy code need explicit conversion/testing. | Prefer named conversion helpers for beta; broad `as`/`is` syntax is not frozen and statement-expression cast spelling rejects explicitly. | Post-beta design gap. |
| Object initializer / builder sugar | Industrial APIs need ergonomic construction. | Use constructors/factory functions for beta. | Post-beta; avoid partial-init unsoundness. |
| Binary debug info | Self-host debugging. | LLVM DIBuilder / CodeView Phase 1. | Post-beta before serious self-host. |
| Module/package ecosystem | Ecosystem and self-host. | Resolver subset first; package manager later. | Beta+ / post-beta. |
| Raw pointer escape | Systems baseline. | Explicit unsafe/raw contract. | Design before self-host systems work. |
| Render/GPU/WebGL APIs | Dogfood/web game target. | Module ecosystem track. | Not core syntax. |

## 18. Core Syntax Patterns To Reserve Now

These are language-syntax patterns, not package-manager or ecosystem work. They
should be designed now because adding them after beta freeze would either break
surface compatibility or force awkward sugar around the wrong AST shape.

| Syntax pattern | Current code state | Required next step |
| --- | --- | --- |
| Intent compact step | Active partial surface: `on:` receiver/action inference covers `who`, action `within`, action `requires`, action `causes`, action-declared `authorized by`, and derived `using` when unambiguous. Local `who` never substitutes for approval. | Keep compact step as the default documented form, then close richer conflict diagnostics and edge-case provenance. |
| String interpolation | Active lexer/parser surface exists: normal strings with `${...}` and `f"..."` route to interpolation parsing; escaped `\${` / `\{` openers and malformed fragments remain literal. | Freeze one public spelling and keep C/LLVM output parity gated. |
| Collection literals | Array literals exist; `List`/`Set`/`HashMap` literals are not frozen; `{ ... }` literal syntax rejects explicitly. | Reserve typed collection literal grammar only after collection ABI/key policy is stable. |
| Closure literal | Active AST surface exists as `AST_LAMBDA_EXPR`; parser recognizes lambda starts and `=>`. | Current beta contract: parameter-only/standalone callable bodies are allowed; lambda-local bindings stay local, while outer local/resource captures reject with borrow-escape diagnostics. |
| Named arguments | `Call(name: value)` parses, preserves names in AST/AST print, and semantic rejects before type checking. | Keep semantic reject until call ABI, overload/dispatch, default-arg interaction, and diagnostics are designed. |
| Default arguments | Generic default type parameters exist; function/lambda default value args now reject explicitly when `=` appears in parameter lists. | Decide whether value defaults belong in function signatures or library wrappers before stdlib freeze. |
| Tuple / multi-return | Active tuple type/literal/codegen support exists in several owners. | Reclassify as `partial`, not missing; close diagnostics, ABI shape, and C/LLVM parity before promoting. |
| Struct/named destructuring | Positional `let (a, b) = ...` exists; `let {x} = ...` now rejects explicitly. | Keep positional destructuring; implement named destructuring only if CFG/dataflow facts can own it. |
| Member/index access | `x.y`, `x.f()`, and `xs[i]` are active expression surfaces. | Keep member/index lowering ordinary and stable; add slicing/keyed indexing only after collection ABI policy is fixed. |
| Pipeline operator | `|>` is an active parser surface. | Treat as expression composition sugar; do not use it as an implicit effect/intent boundary. |
| Cast/type-test syntax | Broad `as`/`is` conversion and type testing are not frozen; statement-expression sites reject explicitly. | Keep explicit conversion helpers for beta; reserve syntax only after interop/generic diagnostics are designed. |
| Compound assignment | `+=`/`-=` are event subscribe/unsubscribe tokens, not general mutation sugar. | Do not add general compound assignment until CFG cleanup/mutation facts own it. |
| Object initializer syntax | No C#/TS-style object initializer baseline; `Type { ... }` rejects explicitly in statement-expression contexts. | Prefer constructors/factory functions; revisit after partial-construction ownership policy exists. |
| Slicing/spread/rest | Slice syntax and `...` spread/rest are reserved with explicit parser diagnostics; no public slice expression, spread argument, or rest destructure baseline. | Keep out of beta; these require collection ABI, failure, call ABI, and ownership policy. |
| Optional chaining / coalescing | Postfix `?` propagation exists; `??` is active for `Option<T> ?? T -> T`; `?.` remains a reserved lexer/parser token with explicit parser diagnostics. | Keep optional chaining out of beta until Option/member provenance diagnostics are strong. |
| Match guards and or-patterns | Active parser/AST/semantic/codegen support exists for `case ... if cond:` and `case a | b:`. | Reclassify as stable/partial based on backend parity gates, not as missing syntax. |
| Block expression policy | Blocks are statements; expression-position `{ ... }` rejects explicitly as reserved object/map literal syntax. | Keep statement-only for beta; expression blocks require CFG/type-inference/cleanup ownership before any future promotion. |
| Visibility modifiers beyond export | `public` and `private` tokens/parser paths exist in declaration owners. | Freeze whether `export` remains the public API contract or whether `public/private` become stable source syntax. |
| Unsafe/raw block | Active `unsafe { ... }` AST/HIR/MIR/AIR boundary exists. Raw escape remains mostly rejected/reserved. | Treat `unsafe` as a boundary marker, not permission to bypass Slot/authority. Raw pointer escape needs a scoped capability contract such as `unsafe(raw) { ... }`, not a universal unsafe mode. |
| Attribute/annotation syntax | Structured comments exist; `@` is now a reserved lexer/parser token that rejects with an explicit parser diagnostic. | Implement only after metadata ownership is clear; otherwise keep structured comments as the stable metadata path. |
| Doc-comment metadata | `///` and structured doc tags are active lexer/parser surfaces. | Keep docs separate from semantics; do not let doc tags become hidden attributes. |
| Generic shorthand / type argument elision | Generic defaults and actual type args exist; `_` in type/generic positions now rejects explicitly instead of becoming a type named `_`. | Keep DAG evidence as source of truth; only add elision where ambiguity diagnostics are strong. |

Reservation does not mean immediate implementation. It means the grammar,
precedence, and semantic owner are chosen before beta freeze, so later
implementation does not require incompatible syntax.

## 19. Human/AI Intent Authoring Loop

Pergyra intent is designed to be human-readable and AI-fillable. Humans should
be able to write or review the compact frame without spelling every low-level
resource/runtime detail, and AI agents should be able to expand the same frame
into explicit contracts. The compiler is not responsible for turning natural
language into policy. Its job is to verify the explicit frame that the human or
AI produced.

Minimal human or AI proposal:

```pergyra
intent Login for User in AuthZone {
    on: user.Validate(credentials);
    on: session.Issue(user);
    expect: session.active;
}
```

Compiler response if the proposal is incomplete:

```text
NO
Reason:
- step `session.Issue(user)` writes SecureSlot<SessionToken>
- AuthZone does not provide TokenIssuer authority for that write
Fix:
- add `authorized by TokenIssuer`
- or move token issuing into a zone that owns that authority
```

Expanded frame after human or AI patching:

```pergyra
intent Login for User in AuthZone
    authorized by TokenIssuer
{
    on: user.Validate(credentials);
    on: session.Issue(user);
    expect: session.active;
}
```

Intended loop:

```text
human goal/review -> AI-filled intent/code -> compiler YES/NO -> Reason/Fix -> patch
```

Rules:

1. Compact authoring may omit clauses only when declared action/zone/authority
   evidence exists.
2. AI-generated clauses are not trusted just because they are plausible; they
   must pass semantic, AIR, CFG/MIR, runtime, C, LLVM, diagnostic, and
   regression gates.
3. Derived facts must be explainable. Hidden inference is not stable surface.

## 20. Patterns Pergyra Should Not Copy

| Pattern | Reason |
| --- | --- |
| Rust lifetime annotations | Pergyra should not make business graph modeling depend on lexical lifetime puzzles. |
| Full Rust-grade borrow checker as beta goal | Wrong target; Pergyra uses layered Slot + boundary verification. |
| HKT/functor hierarchy as core | FP adapters belong in modules, not the core language spine. |
| Zig comptime as core | Pergyra types carry domain coordinates; they are not imperative compile-time programs. |
| Exception-first error model | Recoverable failure should remain `Result`/contract/queryable state. |
| Core WebGL/GPU keywords | Rendering and GPU are module ecosystem surfaces. |
| Switch fallthrough / goto | Conflicts with CFG safety and diagnostic clarity. |

## 21. Operational Rule

Before adding or copying a familiar syntax pattern:

1. Check whether a Pergyra-native answer already exists.
2. If yes, improve that answer instead of adding parallel syntax.
3. If no, decide whether dogfood or self-host actually needs it.
4. If it is not needed for dogfood/self-host, mark it `out-of-beta`.
5. If it is needed, identify the compiler source-of-truth owner before parser work.

Parser-first additions are forbidden for beta closure.

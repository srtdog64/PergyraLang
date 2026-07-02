# Lightweight Ergonomics and Polymorphism: Audit and Design

Status: draft (design). Scope: the "lightweight feel" (경량감) deficit and the
polymorphism-coherence question. This document is part of the living language
reference; sections are tagged `stable` / `unstable` as they settle.

## 1. Purpose

The recurring complaint is that the language feels heavy and "a script can
hardly exist." The earlier conclusion held: the lever is *surfaces*
(ergonomics), not more domain core. This document audits the concrete
mechanisms behind that feel, against three axes pulled from mainstream
languages, then narrows the work to the parts that are actually missing.

The three axes audited:

1. The lightweight trio: type-inference depth, `?`-style error propagation,
   optional chaining (`?.`).
2. Polymorphism coherence: do `ability` / `role` give bound + coherence the way
   Rust traits / Haskell typeclasses / Swift protocols do.
3. Exhaustive pattern matching plus payload-carrying sum types (ADTs).

## 2. Method

Findings below are grounded in the current sources (read directly, not
inferred). File and line references are given so each claim is checkable. The
sandbox build mount is unreliable for recently edited files, so verification is
static (source reading) plus the user's CI.

## 3. Audit findings

Legend: Present (works end-to-end) / Partial (some layers) / Reserved (stubbed
on purpose) / Absent.

### 3.1 Error propagation `?` — Partial (`Result` full compiler, `Option` self-host subset)

Implemented end-to-end for `Result`:

- Lexer: `TOKEN_QUESTION` exists, commented `? (try/propagate)`
  (`src/lexer/lexer.h:160`, `src/lexer/lexer.c:408`).
- Parser: postfix `expr?` is parsed (`src/parser/parser_expr_postfix.c:226`),
  currently as an `AST_UNARY` node carrying the `?` operator (no dedicated
  `AST_TRY` node).
- Semantic: `src/semantic/type_checker_expr_ops.c:349` requires the operand to
  be `Result<T>` or `Result<T, E>`, and yields the unwrapped first type
  argument. Non-Result operands are a typed error.
- Codegen: handled on both backends and in let-emission paths
  (`src/codegen/llvm_expr_unary_core.c:58`,
  `src/codegen/transpiler_expr_unary_emit.c:24`,
  `src/codegen/transpiler_let_emit.c:383`,
  `src/codegen/transpiler_mir_preserved_let_emit.c:280`).

Gaps and current self-host status:

- The production compiler path works for `Result`. Rust's `?` also threads
  `Option`; the full C/LLVM language surface still needs that promotion.
- The self-host semantic/codegen subset now accepts `Option<T>?` in a
  let-binding and lowers the missing branch to `None` from an enclosing
  `Option`-returning function. This is pinned by
  `valid_option_try_payload` in the self-host semantic parity suite and
  `option_try` in the self-host codegen parity suite.
- Modeled as a generic unary operator rather than a first-class try node, so
  its semantics piggyback on unary handling. The exact failure path
  (early-return honoring the enclosing function's `Result` return type vs a
  trap/unwrap) must be confirmed and frozen.

### 3.2 Optional chaining `?.` — Reserved, not implemented

- `TOKEN_OPTIONAL_CHAIN` is lexed and the parser recognizes `?.`, but it is
  explicitly rejected (`src/parser/parser_expr_postfix.c:183`): "Optional
  chaining '?.' is reserved but not implemented. Reason: optional member
  provenance is not frozen across semantic, AIR, MIR, and diagnostics."

This is an intentional stub. The blocker is provenance threading across the IR
layers, not syntax.

### 3.3 Inference depth — Partial (measured)

Measured per construct, with evidence:

- `let` bindings — Present and mature. The annotation is optional
  (`let_decl.type` is "Optional", `src/parser/ast.h:99`); the type is inferred
  from the initializer (`src/semantic/type_checker_ownership_let.c:84,153`),
  and the un-inferable cases are rejected loudly with actionable diagnostics:
  `None` without annotation, empty array `[]`, bare `ClaimDeviceSlot`
  (`type_checker_ownership_let.c:156,173,190`). Not a friction site.

- Function return type — Absent. `type_check_func_resolve_return_type`
  defaults to `TYPE_VOID` whenever there is no `-> Type` annotation
  (`src/semantic/type_checker_func_types.c:33-34`); the body is never consulted.
  Consequence: every value-returning function is forced to annotate its return
  type. This is friction site #1 — the clearest contributor to the heavy feel,
  because helper functions (the bulk of script code) all require `-> Type`.

- Function parameters — Required (normal), except implicit `self` inside a
  class/nominal scope, which is inferred (`type_checker_func_decl.c:91-124`).
  Parameter annotations are expected in mainstream languages too; not a target.

- Closure / lambda parameters — Likely Absent. No expected-type ("bidirectional")
  inference for lambda parameters was found; un-annotated lambda params appear to
  fall to unknown rather than being filled from the expected callable type. This
  is friction site #2 (smaller: lambdas are less frequent than function defs).

- Generic arguments at call sites — Largely Present. A substantial apparatus
  exists (`type_checker_generic_support.c`, `_validation.c`, `_contracts.c`)
  and monomorphization resolves type args; treat as mostly working, lower
  priority pending a confirming probe.

Ranked friction: (1) function return type defaults to Void; (2) closure
parameter types; (3) generic call-site corner cases. `let` is done.

### 3.4 Polymorphism coherence — Bound present, coherence Absent

- `ability` / `role` provide method bounds (the trait-like surface exists).
- No coherence machinery was found: no overlapping-impl detection, no orphan
  rule, no duplicate-impl conflict diagnostics. Searches for
  coherence/orphan/overlapping/duplicate-impl in the ability path returned
  nothing relevant.

This is the real polymorphism gap. Bounds without coherence means generic code
can compose ambiguously (two impls for the same ability/type pair) with no
loud rejection. This is the one axis here that is genuinely missing.

### 3.5 Exhaustive match + payload ADT — Present (mature)

This was the surprise: it is largely done, not a gap.

- Tagged-union sum types with per-variant payloads exist in the AST:
  `enum Shape { Circle(Int), Rect(Int, Int), None }` with `variant_params[i]`
  holding each variant's parameter type nodes (`src/parser/ast.h:250`), and
  full parser machinery (`src/parser/parser_enum.c`).
- Exhaustiveness/coverage checking exists
  (`src/semantic/type_checker_flow_match_coverage.c`) for `Option`
  (Some/None), `Result` (Ok/Err), and enums, with per-variant arg-count
  awareness.
- Pattern destructuring of payloads is supported
  (`match_pattern_is_named_variant` returns the bound args).

Remaining to confirm (minor): that all-variant exhaustiveness is enforced for
user-defined enums (not only Option/Result), and the depth of nested/guard
patterns. Treat as polish, not a core gap.

## 4. Revised conclusion: the real gaps are narrow

Two of the three axes the deficit was attributed to are already largely in
place. The ADT/matching machinery is mature; `?` already works for `Result`.
The "경량감 부족" therefore reduces to a short, specific list:

1. Inference depth (the primary felt-heaviness lever).
2. `?` for `Option` in the full compiler path, after the self-host subset
   proof stays green (and freezing the `?` failure-path semantics).
3. `?.` optional chaining (unfreeze the reserved feature by threading
   provenance through the IR).
4. Ability coherence (the one true polymorphism gap).

Everything else on these axes is present.

## 5. Design

Priority order is by felt lightness per unit of risk. Each item lists current
state, the design, and the surfaces it touches.

### 5.1 Inference depth (priority 1)

Goal: make annotations the exception, not the rule, so script-register code
reads light. Friction is now measured (section 3.3); target #1 is return-type
inference.

#### 5.1.1 Return-type inference (target #1)

Current behavior: with no `-> Type`, the return type is `TYPE_VOID`
(`type_checker_func_types.c:33-34`); the body is never consulted.

Constraint discovered: the function's `Type` and symbol are built
(`type_checker_func_decl.c:135-146`) *before* the body is checked
(`:257-259`). Parameters only enter scope at `:208-255`. So the return type
cannot be read off the body at symbol-creation time. This is the classic
declare-symbol-first (for recursion) vs check-body-first (for inference)
tension, and it is why this is a multi-touch change, not a one-liner.

Design (post-body update, C++ `auto`-style recursion rule):

1. When `ast_func_return_type(node) == NULL`, set the working return type to an
   "inferring" sentinel (reuse `TYPE_UNKNOWN`) instead of `TYPE_VOID`, and
   declare the symbol with it. The missing-return check already skips
   `TYPE_UNKNOWN` (`:260-261`), so no false missing-return error fires.
2. Add a `ctx` accumulator (e.g. `Type *inferred_return; bool inferring_return;`).
   During body checking, `return <expr>` in inferring mode records the expr type
   into the accumulator and unifies with prior records instead of checking
   against a fixed type. Incompatible records are a loud error
   ("returns disagree; add an explicit `-> Type`").
3. After the body, finalize: no recorded returns -> `Void`; one unified type ->
   that type. Update `func_sym->type`'s return so later callers see it.
4. Recursion rule: a recursive call while the accumulator is still empty yields
   `TYPE_UNKNOWN` -> loud error asking for an explicit return type (mirrors
   C++ `auto` and keeps inference strictly local; no fixpoint, no whole-program
   pass).

Touch points: `type_checker_func_decl.c` (sentinel + post-body finalize),
`type_checker_func_types.c` (only when callers want the inferred type),
return-statement checking (`type_check_return_stmt` / `_flow`) for the record-vs-
check branch, and one `ctx` field. All additive: behavior changes only when an
annotation is absent.

Risk: medium. This is core type-checker surgery with corpus-wide blast radius,
and it cannot be locally verified (sandbox build is unusable). It must go
through the build-loop (apply -> user CI -> fix from errors), one touch point at
a time, starting with the sentinel + accumulator before the recursion rule.

Status: IMPLEMENTED (pending CI). Touch points applied:
- `type_checker.h`: ctx fields `inferring_return`, `inferred_return`,
  `inferred_return_conflict`.
- `type_system.c` / `.h`: `type_function_set_return_type` setter.
- `type_checker_func_decl.c`: when `ast_func_return_type(node) == NULL`, work
  with `TYPE_UNKNOWN` during body checking; after the body, finalize (no
  value-returns -> Void; unified type otherwise; unresolved/recursive -> loud
  "add -> Type"); save/restore the ctx fields across nesting; update the
  function symbol via the setter.
- `type_checker_ownership_return.c`: in inferring mode, accumulate/unify each
  return's type (`ownership_return_record_inferred`) instead of asserting against
  a fixed expected type; disagreeing returns raise a loud conflict.
Behavior is additive: it changes only when a `-> Type` annotation is absent.

Positive fixture: `examples/infer_return.pgy` (inferred `add` and `label`),
registered in `tests/example_contract_smoke.sh` expecting `INFER RETURN sum=5`.

Negative fixtures to wire into the semantic-error harness (kept out of
`examples/` so the example runner does not try to run-and-pass them):
- Conflict: `func f(b: Bool) { if b { return 1 } return "x" }` -> "Return types
  disagree across paths".
- Recursion without annotation:
  `func f(n: Int) { if n <= 0 { return f(n) } return f(n) }` -> "Cannot infer the
  return type ... add an explicit '-> Type'".

Known v1 limitations (follow-ups): (a) an inferred non-Void function that may
fall through is not re-checked for missing-return after finalize; (b)
`type_check_func_resolve_return_type` direct callers (some method-return paths)
still see Void for inferred methods until they read the symbol type - inferred
return is most reliable for free functions in v1.

#### 5.1.2 Closure parameter inference (target #2)

When a lambda with un-annotated parameters is supplied where an expected callable
type is known (e.g. a higher-order builtin), fill the parameter types from the
expected signature (bidirectional). Smaller blast radius than 5.1.1; contained to
lambda checking plus the expected-callable-type already threaded in `ctx`.

#### 5.1.3 Discipline

Infer only where the result is unique and locally determined. No global/whole-
program inference: it harms error locality and compile speed, which the dual-
backend + parity model depends on.

Surfaces: semantic only (`type_checker_*`); no new syntax.

### 5.2 `?` extended to Option, and frozen semantics (priority 2)

Status: PARTIAL. The self-host subset has landed the low-risk slice:
`Option<T>?` inside `let` statements, with early `None` propagation when the
enclosing function returns `Option`. The full language path still needs C/LLVM
compiler promotion and a dedicated spec/golden row before this section can be
marked stable.

- Allow `expr?` where `expr : Option<T>`, yielding `T`, with the failure path
  returning `None` from an enclosing `Option`-returning function (symmetric to
  the existing `Result` behavior returning `Err`).
- Decide and freeze the failure path: `?` early-returns the enclosing
  function's `Result`/`Option`, threading `E`. Confirm the current codegen
  honors the enclosing return type rather than trapping; if it traps, fix it.
- Consider promoting `?` from `AST_UNARY` to a dedicated `AST_TRY` node so its
  semantics are explicit and not entangled with generic unary handling. This is
  the cleaner long-term shape and mirrors how `transaction`/`fail` were given
  first-class nodes.

Surfaces: semantic + both backends. Moderate; the Result path already exists to
mirror.

### 5.3 `?.` optional chaining (priority 3)

The blocker is provenance, per the parser's own rejection message. Design:

- Model `a?.b` as sugar that lowers to an `Option` match: evaluate `a`; if
  `Some(x)`, project `x.b`; if `None`, the whole chain is `None`. The result
  type is `Option<typeof(a.b)>`.
- Thread "optional member provenance" as an explicit flag on the projection so
  semantic, MIR, AIR, and diagnostics agree on where the `None` short-circuit
  originates (this is exactly what the parser said is not yet frozen).
- Lower to the same machinery as a hand-written `match`, so codegen reuses
  existing Option handling rather than inventing a new path.

Surfaces: parser (unfreeze), semantic (provenance), IR threading, both
backends. Higher risk; do after `?`-on-Option so the Option plumbing is settled.

### 5.4 Ability coherence (priority 4, parallel track)

Goal: bounds that compose unambiguously.

- Add a coherence pass: for each (ability, implementing type) pair, reject more
  than one impl (loud diagnostic, never silent pick). Mirror the existing
  "loud, never silent" stance used for duplicate `Main` and `fail`-outside-
  transaction.
- Decide an orphan rule: an impl must live in the module that defines either the
  ability or the type, to keep coherence checkable without whole-program
  analysis. Document the rule even if initially permissive.
- This is independent of the ergonomics trio and can proceed in parallel.

Surfaces: semantic (a new coherence pass over ability/impl tables) + a diag
code (string `#define`, no enum/switch hazard).

### 5.5 ADT/match polish (priority 5)

- Confirm all-variant exhaustiveness is enforced for user enums (not only
  Option/Result); if not, extend `type_checker_flow_match_coverage.c`.
- Assess nested and guard patterns; fill only proven gaps.

## 6. Sequencing

1. Inference friction audit, then inference depth (5.1) — biggest lightness win,
   lowest risk, no syntax change.
2. `?` on Option + freeze semantics (5.2). The self-host let-binding slice is
   landed; full compiler promotion remains.
3. Ability coherence (5.4) — parallelizable, independent track.
4. `?.` optional chaining (5.3) — after Option plumbing settles.
5. ADT/match polish (5.5) — confirm and fill.

## 7. Relation to the spec and the valence principle

Each item above is also a spec section waiting to be written: deciding the
canonical form of error propagation, optional access, and coherence *is* the
act of absorbing the mainstream lesson. Per the valence principle, none of
these is a new domain atom; they are surface refinements that make the existing
atoms (Result, Option, ability, enum) compose lightly. That is the correct
shape for closing the "script can hardly exist" gap without adding weight.

Recommended: freeze each section here as `stable` only once its IR provenance is
threaded and its tests pass, exactly as the `?.` rejection message demands.

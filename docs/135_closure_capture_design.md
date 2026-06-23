# Closure Capture Design

Status: `beta-adjacent, in-progress`. Owner decision doc for filling the one
real expressiveness gap recorded in `docs/124` §17 (closure literals: capture).

Related:

- `docs/42_keyword_orthogonality.md` — capture lives on the **Resource axis**.
- `docs/124_syntax_pattern_matrix.md` §6/§17/§18 — lambda/closure surface status.
- `docs/134_language_surface_hygiene.md` — mutability surface (`let` vs `let mut`).
- `src/semantic/type_checker_lambda_capture.c` — the current rejection site.

## 0. Problem

Today a lambda lowers to a **standalone hoisted function with no environment**
(`pgy_lambda_N` in C; an LLVM module function in LLVM). Both backends emit it
directly from the AST — lambdas do **not** flow through MIR. Because there is no
environment, capturing any outer **local** binding is rejected
(`PGY_CODE_SEM_BORROW_ESCAPE`) so that lifetime/cleanup/authority ownership never
becomes implicit. Top-level declarations (`func`/`class`/`zone`/`intent`/...) are
already capturable because they have static storage and no lifetime.

The gap is real but narrow: a lambda cannot close over a local counter, config
value, or name. This blocks ordinary callbacks.

## 1. Framing — Capture Is the Resource Axis, Not Rust Lifetimes

Capture is **not a new concept**. It is another boundary crossing, expressed with
the same `own`/`ref`/copy classification already used at function boundaries and
Slot boundaries. Pergyra does **not** import Rust's `Fn`/`FnMut`/`FnOnce` +
lifetime-parameter model (`docs/124` §20 forbids it).

| Capture mode | Meaning | Reuses | May the closure escape its scope? |
| --- | --- | --- | --- |
| **copy** | snapshot a value-type binding | nothing (no link, no lifetime) | yes — free |
| **ref** | borrow the outer binding | function `ref` boundary rules + escape-summary masks | no — non-escaping only |
| **own** | move the binding into the env | `own` / `MoveToken` / MIR cleanup | yes — closure becomes the new owner |

The escape question is the existing boundedness question (`zone`/scope). The
authority question reuses the capability refinement (effect→capability mask,
tasks #28/#30). No new axis is introduced; orthogonality is preserved.

## 2. Closure Environment ABI (shared by all stages)

A lambda value stops being a bare function pointer and becomes a **closure
pair**, emitted symmetrically in both backends (C/LLVM parity is mandatory):

```
closure_N = { fn_ptr; env_N }
```

- `env_N` is a per-lambda struct holding one field per captured binding.
- The hoisted body takes the environment as a hidden **leading parameter**
  (`env_N *__env`); inside the body each captured name `c` reads as `__env->c`.
- At the lambda expression site, the env fields are populated from the current
  values of the captured bindings according to each binding's capture mode.
- A call through a closure value passes `&closure.env` as the hidden first
  argument: `closure.fn(&closure.env, args...)`.
- Zero-capture lambdas keep a uniform shape (empty env) so call dispatch has one
  path, not a "is-it-a-closure" branch.

This ABI is the foundation Stages A–D all share. Stages differ only in **what a
field may hold** and **whether the closure may escape**.

## 3. Staging

### Stage A — copy-capture of value-type locals (foundation)

Allow capturing outer **local** bindings of value types (`Int`, `Long`, `Bool`,
`Float`, `String`) **by copy** (snapshot at closure creation). No aliasing, no
lifetime link → always sound, escape-independent. Builds the env ABI end to end.
Copy is inferred (no annotation): a snapshot can never be wrong. Any non-value /
slot / token / authority / non-copyable capture **still rejects** (Stage D).

Semantic: `type_checker_lambda_capture.c` stops rejecting copy-value locals and
instead **records** them as captures (mode = copy) on the lambda AST node.

### Stage B — ref-capture, non-escaping closures

Allow `ref` capture when the compiler proves the closure value does not leave its
creating scope (not stored in a field, not returned, not spawned). Reuses/inverts
`function.param_escape_summary_masks`. Enables `map`/`filter`/`forEach` over outer
state.

### Stage C — own-capture, escaping closures

Escaping closures move their captures via `own` (consume `MoveToken`); the outer
binding becomes unusable, the heap-allocated env becomes the new owner, and
cleanup is the env's MIR/cleanup responsibility.

### Stage D — slot / token / authority / capability capture

Hardest residual (likely post-beta). Only via `own` move, the closure inherits a
**capability mask** (closure used-caps ⊆ captured/declared caps), and zone/world
boundary checks prevent authority from being smuggled across a boundary.
Fail-closed.

## 4. Source-of-Truth Owners

- **Capture set + modes**: computed in semantic analysis, stored on the lambda
  AST node (`lambda_expr.captures`), mirroring the existing
  `semantic_return_type_name` annotation pattern (`docs/134`).
- **Env layout / call ABI**: emitted by each backend from the capture set. This
  keeps the fact on the AST node (single owner) rather than re-derived; it is a
  known, accepted exception that lambdas bypass MIR (see `docs/42` §6 Rule 4) —
  promoting closures to MIR routines is tracked separately with architecture
  target #4 (unified MIR).

## 5. Non-Goals

- No `Fn`/`FnMut`/`FnOnce` trait hierarchy, no lifetime parameters.
- No implicit capture of resources/authority (Stage D is explicit `own` only).
- No silent capture-by-reference that could outlive its owner.

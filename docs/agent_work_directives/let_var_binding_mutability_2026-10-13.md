# Binding mutability: Swift-style `let` / `var`

Status: PLANNED. Not started. Decision recorded 2026-10-13; the user delegated the
choice ("네 판단에 맡길게").
Base: `e2240cb2` with the GPT lane's DRV-2 work uncommitted in the shared tree.
Outputs: implementation candidate. This directive does not own language semantics;
the grammar documents and the executable gates do once the slices land.

## Decision

- `let` is an immutable binding. It cannot be reassigned, and the value it holds
  cannot be changed through it: no field or element writes, no mutating builtins
  (`ArrayPush`, `MapSet`, ...), no passing as an `inout` argument.
- `var` is a mutable binding. A `var` that is never changed gets a warning that
  points at `let`.
- `let mut` and the contextual word `mut` are retired. `var` replaces them, so the
  registry keeps the same word count. There is no period in which both spellings
  are accepted.
- Fields keep their surface: `name: Type;` is a mutable field and `let name: Type;`
  is immutable after construction. Because structs are values, a struct held by a
  `let` cannot be written even through a mutable field, as in Swift.

Rejected: `let` mutable with `let mut` immutable (the word `mut` would mean its
opposite); keeping a mutable-by-default `let` and dropping `mut` (no default
protection and no visible mutation intent); keeping `let mut` with enforcement
(see the ceremony reason below).

## Why

1. Safety default. Today a local `let` protects nothing, while a `let` field is
   enforced; the same word means two things. Immutable-by-default is the only
   default that matches "fail closed".
2. One vocabulary for value semantics. Pergyra's value model is Swift's:
   collections are values and caller-visible mutation is copy-in/copy-out
   `inout`. `docs/mut_borrow_parameters.md` already removed Rust's `&mut` for that
   reason. `let`/`var` finishes the same move for bindings.
3. Ceremony. About 11,000 reassigned locals must be marked either way. With `var`
   the marker replaces `let` (zero added tokens). With `let mut` every one of them
   gains a token, and the 500 existing `let mut` lose one when they become `var`.
4. Ownership facts. A `let` that holds a collection can never grow, so a view into
   it can never be invalidated by growth (the HashMap growth counterexample of
   `docs/audits/valen_group_borrowing_comparison_2026-10-11.md`), and a `let`
   never rebinds. Expected to simplify exclusive-value last-use and view
   admission; not measured, so it is a hypothesis, not a reason to ship.

## Measured current behavior (2026-10-13, `.tmp/claude-bin/pgy.exe`, self-host driver)

| Program | Result today | Target |
| --- | --- | --- |
| `let x: Int = 1; x = 2;` | accepted, no diagnostic | error: reassigning `let` |
| `let p: Point` then `p.a = 2` with bare field `a: Int;` | accepted | error: `p` is `let` |
| `let xs: Array<Int>` then `ArrayPush(xs, 2)` | accepted | error: mutating a `let` value |
| `let a: Int;` field, `p.a = 2` | `immutable_field_write` | unchanged |
| `let mut y` never changed | accepted, no diagnostic | `var` never mutated: warning |

Size estimate (regex over tracked `.pgy`, function-chunked; an upper-bound sketch,
not a migration list):

| Root | `let` | `let mut` | plain `let` reassigned | `let mut` never reassigned |
| --- | --- | --- | --- | --- |
| src/self_hosted | 38,150 | 37 | ~9,287 | ~22 |
| tests | 10,510 | 410 | ~1,813 | ~234 |
| examples | 1,551 | 62 | ~165 | ~49 |
| stdlib | 124 | 0 | ~1 | 0 |

Mutating builtins and `inout` arguments on `let` values are not in this count; the
first enforcement slice measures them exactly.

## Objective card

- Objective: one binding-mutability rule, enforced identically by both front ends,
  with `let`/`var` as the only spellings.
- Priority: semantic identity (one rule, one fact) > fail-closed enforcement >
  old-spelling deletion > migration ceremony > patch size.
- Fact owner: the parser's binding-mutability flag. Native: `let_decl.is_mutable`
  set in `src/parser/parser.c` (around line 449). Self-host: `mutable_binding` in
  `src/self_hosted/parser/stmt_owner.pgy` (around line 282) and the field mode in
  `src/self_hosted/parser/decl_nominal_owner.pgy` (around line 154).
- Consumers: native assignment and mutation checks in
  `src/semantic/type_checker_assignment.c` and
  `src/semantic/type_checker_collection_mutation_contract.c`; the self-host member
  write verdict in `src/self_hosted/semantic/ast_member_write_verdict_owner.pgy`;
  the inout argument boundary on both front ends.
- Last legitimate consumer: the semantic verdict that admits a write. Codegen never
  re-decides mutability.
- Forbidden fallback: inferring mutability from later writes; accepting `let mut` as
  an alias; downgrading the error to a warning for "compatibility"; a per-file opt-out.
- Gates: new positive and negative fixtures on both front ends with byte-equal
  diagnostics, the keyword registry gate, the VS Code grammar gate,
  `documentation_quality_smoke`, and the full self-host bootstrap after the
  rewrite.

## Slices, in dependency order

1. **Spelling.** Add `var` to `src/lexer/language_keyword_registry.def` as the
   mutable-binding introducer with exactly today's `let mut` meaning. Teach both
   parsers. Rewrite every `let mut` to `var` mechanically (sources, tests, gate
   strings such as `tests/abi_ownership_shape_smoke.sh` and docs/136). Delete
   `mut` from the registry and both parsers; `let mut` becomes a parse error whose
   fix says "use var". The language-word docs (docs/199, the cheatsheet and the
   keyword registry doc) change in the same commit.
2. **Enforcement and migration, landed together.** Add the semantic rule on both
   front ends: reassignment, compound assignment, field and element writes,
   mutating builtins and `inout` passing on a `let` binding are errors. Run the
   compiler over every tracked `.pgy` to list the offending declarations by span,
   rewrite exactly those `let` to `var` with a script that reads the diagnostic
   spans, and land the rule and the rewrite in one commit so no gate is red in
   between. Hand edits are not part of the migration.
3. **Never-mutated warning.** Warn on a `var` that nothing changes; rewrite those to
   `let` with the same span-driven script.
4. **Close.** Remove the "plain local reassignment compatibility" sentence from
   `docs/grammar/02_grammar.md`, replace the decision banners in
   `docs/grammar/01_syntax.md` and `02_grammar.md` with the rule itself, and update
   the VS Code TextMate grammar.

## Blockers and timing

- The GPT lane holds the DRV-2 ownership work uncommitted in about 377 files, many
  of them `src/self_hosted` sources that slice 2 rewrites. Start after that work
  lands; rebasing a ten-thousand-line mechanical rewrite over it is not acceptable.
- AGENTS.md keeps one active executable rung. Run this as its own rung once the
  DRV-2 baseline lands, not interleaved with it.

## Falsifiers

- A `let` binding that is written by any route (assignment, field, element,
  builtin, `inout`, destructuring target, loop variable) and still compiles.
- The two front ends disagree on any fixture.
- A `let mut` that still parses after slice 1.
- A `var` warning that fires on a binding that is written.

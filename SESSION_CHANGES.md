# Session Changes — Language Surface + Source-of-Truth Closure

C backend verified (LLVM_ENABLED=0). Every change below was built clean under
`-Wall -Wextra -Werror` and re-verified against the regression suites with zero
failures:

- `test_transpile`: 898 passed, 0 failed
- `test_semantic`: 2672 passed, 0 failed
- `test_parser`: all tests completed
- example emit-c sweep: 110/117 (no regression at any step)

Note: the environment filesystem repeatedly corrupted source files (NUL
injection / truncation) and `.git/packed-refs` during this session. git was
recovered: `refs/heads/main` was restored to commit `b90e2a71` (full 559-commit
history intact in the object store; only the branch pointer was lost). The
generics work below was built and verified in a corruption-free native copy
(`git archive` of HEAD + these changes) since the mount could not be built on
reliably.

## 1. Generic class monomorphization (source-of-truth)

`generic_class.pgy` now runs: `3 / 7 / hello / world`.

Three wiring gaps in the C backend, no new feature code:

- `src/codegen/transpiler_expr_call_member_emit.c` — member-call metadata
  lookup now falls back from the monomorphized name (`Pair_Int`) to the base
  generic decl (`Pair`) via `transpiler_generic_class_spec_base_decl`.
- `src/codegen/transpiler_mir_func_emit.c` — monomorphized method body renders
  the `self` parameter as the specialization type, not the base, through
  `resolve_generic_class_self_type_name` plus the active-spec hint fields on
  `TranspilerCtx` (`active_generic_class_base_name` / `..._spec_name`).
- `src/codegen/transpiler_call_constructor_result_emit.c` — the constructor
  compound literal consumes `ctx->expected_type` (the SSA LHS type) so
  `Pair(3,7)` emits `(Pair_Int){...}` instead of the incomplete base `Pair`.

Supporting: `transpiler_generic_class_spec_base_decl` added in
`transpiler_generic_class_specialization_emit.c` (+ header), two hint fields in
`transpiler.h`.

## 2. Runtime dyn dispatch (semantic + codegen source-of-truth)

`dyn_test.pgy` now runs: `10 / 20` (runtime vtable swap Warrior -> Archer).

The vtable codegen already existed end-to-end. Two gaps closed:

- `src/semantic/type_checker_expr.c` — value-position member access on a party
  resolves a `dyn role slot` to its ability type.
- `src/semantic/type_checker_host_helpers.c` — `semantic_host_decl_for_type`
  and `semantic_host_decl_methods` now resolve ability decls and their methods,
  so `team.fighter.Attack()` type-checks.
- `src/parser/ast_role_type_accessors.c` (+ `ast_domain_api.h`) — added
  `ast_ability_methods` array accessor (no raw-field reads in semantic).
- `src/codegen/transpiler_expr_call_member_emit.c` — the dyn dispatch receiver
  is rendered through `emit_expression` so it uses the SSA name
  (`_pgy_ssa_team_1`) instead of the raw source name (`team`).

`bind` semantic checking was already complete and was left unchanged.

## 3. `&self` / `&mut self` borrow receivers (parser surface)

`func Get(&self)` / `func Bump(&mut self)` parse, check, and run (`5 / 6`).

- `src/parser/parser_decl.c`, `src/parser/parser_async.c` — accept a leading
  `&` (and optional `mut`) on a parameter, mapping the receiver to
  `PARAM_MODE_REF` (borrow). Codegen reuses existing pointer-self handling.

## 4. Body-less ability method signatures (parser surface)

`func IsAlive(&self) -> Bool` (no `;`, no body) parses inside an ability.

- `src/parser/parser.h` — `in_abstract_method_context` flag on `Parser`.
- `src/parser/parser_domain.c` — set/reset the flag around ability method
  parsing.
- `src/parser/parser_decl.c` — in that context, a missing body without a
  trailing `;` is treated as a declaration-only signature. Outside the context,
  `{` or `;` is still required.

## 5. Comma-separated struct/class fields (parser surface)

`struct Entity { health: Int, attackPower: Int, name: String }` runs.

- `src/parser/parser_decl.c` — a field is terminated by `;`, `,`, or a trailing
  field directly before `}`. Two adjacent fields with no separator are still an
  error.

## 6. `type X = { record }` anonymous record alias (parser surface)

`type Matrix = { rows: Int, cols: Int, name: String }` runs (`M / 3`).

- `src/parser/parser_decl.c` — `parse_record_type_alias_struct` desugars the
  record into a `struct X` so it flows through the existing nominal struct path
  (semantic + codegen unchanged).
- `src/parser/parser_internal.h` — declaration.
- `src/parser/parser_type.c` — `parse_type_alias_declaration` routes a `{`
  target to the record desugar; named aliases (`type MyInt = Int;`) unchanged.

## 7. Optional colon in event signatures (parser surface)

`event OnDamageTaken: (target: Int, damage: Int);` parses, as does the
colon-less `event OnTick(count: Int);` form.

- `src/parser/parser_domain_event.c` -- accept an optional `:` between the
  event name and the parameter list.

## 8. Object/struct literal `Type { field: value }` (parser + semantic + codegen)

`Matrix { rows: 3, cols: 4, name: "M" }` runs, including reordered fields
(`Matrix { name: "Z", cols: 9, rows: 7 }` binds correctly by name).

No new AST node: the literal desugars to a named-argument `AST_CALL`,
reusing the existing `arg_names` infrastructure.

- `src/parser/parser_expr_postfix.c` -- parse `Identifier { f: v, ... }` in
  postfix position into a named-arg call. A two-token lookahead
  (`{` then `identifier :`) distinguishes a literal from a statement block
  (`parallel { ... }`, `spawn { ... }`, control bodies), so those are
  untouched.
- `src/parser/parser.h`, `src/parser/parser_stmt.c` -- `no_struct_literal`
  flag is set while parsing if/while/for/match condition expressions so a
  bare `if Foo {` is never mistaken for a literal; it is cleared inside call
  argument lists and literal bodies.
- `src/parser/ast_expr_control_accessors.c` (+ `ast_api.h`) -- added
  `ast_call_has_named_arguments` and `ast_call_find_named_argument`.
- `src/semantic/type_checker_expr_call.c` -- allow named arguments when the
  callee is a nominal constructor (class/party); still reserved elsewhere.
- `src/semantic/type_checker_call_constructor.c` -- validate each named
  argument against the field of that name (`constructor_decl_field_type_by_name`).
- `src/codegen/transpiler_class_constructor_emit.c` -- emit designated
  initializers keyed by field name when names are present, so reordered or
  partial literals lower correctly.
- `src/tests/parser/test_parser_special_part_c.cases.h` -- the obsolete
  "object initializer is reserved" diagnostic test now asserts the literal
  parses.

## 9. Full generics — combinatorial substitution (semantic + codegen)

Generics are the language's spine; this closes them past the single happy-path
example into the full combination space. All cases below run end-to-end on the
C backend, and the type checker rejects wrong arguments (including nested):

- single `<T>`; two params `<T, U>` (`Pair<Int, String>`)
- method parameter `T` (`Box<Int>.Swap(9)`)
- method parameter `Array<T>` (`Holder<Int>.Take(Array<Int>)` -> 3)
- method return `T` used in an expression (`Box<Int>.Get() + 5` -> 15)
- nested instantiation `Wrap<Wrap<Int>>` (`o.Get().Get()` -> 7)
- rejection: `Box<Int>.Add("s")` -> `expects 'Int', got 'String'`;
  `Holder<Int>.Take(Array<String>)` -> `expects 'Array<Int>', got 'Array<String>'`

Semantic substitution:
- `src/semantic/type_checker_expr_host.c` -- `expr_host_subst_generics`
  recursively substitutes the host's generic parameters with the receiver's
  instantiation arguments (matched by declaration order) through constructed
  types, applied to method argument checking AND the method return type.
  `expr_host_type_contains_generic` skips the assignability check when a param
  remains generic (self-calls inside a generic body).
- `src/semantic/type_checker_expr_call.c`, `type_checker_internal.h` -- thread
  the receiver type into `expr_type_check_host_method_call_on_host`.

Codegen substitution:
- `src/codegen/transpiler_type_render.c` -- the binding-aware renderer now also
  substitutes generic argument names *inside* `<...>` (`Array<T>` -> `Array<Int>`).
- `src/codegen/transpiler_type_require.c` -- `transpiler_subst_generics_in_type_name`
  token-substitutes active bindings inside a type-name string, so the MIR
  type-name path (forward declarations) handles `Array<T>` too.
- `src/codegen/transpiler_mir_func_emit.c` -- monomorphized method parameter
  types render through the binding-aware AST path.
- `src/codegen/transpiler_func_forward_metadata.c` -- forward-decl param fix
  plus a missing `string_compat.h` include that had been silently failing the
  `-Werror` build (so earlier forward-decl fixes never linked in).
- `src/codegen/transpiler_type_mapping.c` -- `pergyra_type_to_c_copy` now mangles
  *user* generic instantiations `Base<Args>` -> `Base_Args` (e.g. `Wrap<Int>` ->
  `Wrap_Int`), which closed nested monomorphization.

Verified: `test_transpile` 898/0, `test_semantic` 2672/0 (native build).
Known edge: a generic class whose base name collides with a single-letter or
runtime C symbol (e.g. a class literally named `H`) still emits a spurious base
`typedef struct H H;`; use non-colliding names. Tracked as a follow-up.

## 10. Data-race isolation — parallel shared-mutable detection (semantic)

Closes the concurrency gap noted earlier: `parallel { counter = counter + 1; }`
previously compiled with zero diagnostics. The slot/resource model already
rejects multiple tasks mutating the same *slot*; this extends isolation to
plain (non-slot) captured variables, matching the design rule "shared mutable
goes through a channel only."

- `src/semantic/type_checker_flow_parallel.c` -- for each parallel task, walk
  the body (`parallel_task_assigns_name`) for an assignment whose target root
  identifier names a variable captured from an enclosing scope, and emit a
  race-risk diagnostic. Channel-typed symbols are excluded (the channel send
  `ch <- v` is `AST_CHANNEL_SEND`, not an assignment, and is the sanctioned
  sharing path).

Behavior:
- `parallel { counter = counter + 1; }` -> `Parallel task writes shared
  captured variable 'counter'; ... data race. Send updates through a channel`
- `parallel { ch <- 3; ch <- 5; }` -> no diagnostic (channel-mediated)

Emitted as a warning (consistent with the existing parallel race-risk warning)
to avoid breaking code the suite covers; it can be promoted to a hard error
once the rule is adopted. Reads of captured variables remain allowed (shared
immutable read). Verified: `test_semantic` 2672/0, `test_transpile` 898/0.

This is the first increment of the isolation track: memory-unsafe wants an
effect-DAG plus region containment; data-race-unsafe wants isolation
(channel-only sharing + zone/world ownership), which is what this enforces.

## Remaining surface gaps (next increments)

Each remaining aspirational example stacks several features. Observed next
blockers after this session:

- `parallel.pgy` — struct-literal call form / `with slot<T>` blocks.
- `party_system_demo.pgy` — multi-arg slot generics (`Slot<Int, any Model>`).
- `role_ability_demo.pgy` — event declarations.
- `world_roster_city.pgy` — further field/receiver forms + combinators.
- `secure_slots.pgy`, `structured_comments.pgy` — class-body destructuring
  (`let (a, b) = expr`), currently an explicit parser reject.
- `vessel_action_design.pgy` — expression-position form not yet accepted.

These each require parser + semantic + codegen work (like items 1-2), not just
parser surface.

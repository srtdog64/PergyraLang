# reflect: Compile-Time Reflection Operator

Status: rung-2b implemented (pending a build on a real machine; the sandbox
fuse mount corrupts header reads so it could not be compiled here). The
operator is lexed and parsed; `reflect TypeName` yields a `projection` value
type, and `projection.name` resolves to the type name as a String. Richer
targets (non-identifier) are still gated with a diagnostic. This document is
the decision record so the design survives long breaks: it pins what was
chosen, why, what substrate is missing, and the rung ladder to a working
feature.

Representation note: `projection` is registered as a primitive value type
whose runtime representation reuses `String` (C `char*`, LLVM `i8*`), so no new
backend object was introduced. `reflect TypeName` folds in the type checker to
the name String (the projection's representation), and `projection.name` folds
to the projection value itself. So a projection carries exactly its name today.
This means projection is not yet a zero-runtime erased value; a fully erased
compile-time-only representation is a later optimization, not a blocker.

## What it is

`reflect` is a compile-time prefix operator. It takes a reflection target (a
type, or a domain entity such as an intent, zone, role) and yields a
`projection` describing that target. It is evaluated entirely at compile time;
it emits no runtime code and carries no runtime cost.

    let p: projection = reflect ChargeCard;
    let fx: EffectSet  = p.effects;     // {io, alloc}
    let auth: Authority = p.authority;  // PaymentAuthority

    let t: projection = reflect Account;
    let n: String = t.name;             // "Account"

## Decisions (locked)

Spelling is the keyword `reflect`. The C++26 reflection operator `^^` was
rejected on aesthetics: the double caret reads as exponent or xor and clashes
with the keyword-shaped surface (`func`, `let`, `inout`, `zone`, `intent`).
C++ chose a sigil because template metaprogramming needs terseness; Pergyra is
keyword-flavored, so a keyword is the consistent choice. The token is a single
lexer keyword, so the spelling can be changed in one line if ever desired.

Evaluation time is compile-time only. This matches the language determinism
posture: no runtime metadata tables, no runtime cost, C and LLVM stay at
parity because the operator is resolved before any code is emitted.

The result type is `projection`. Pergyra already owns the word `projection`
for a borrowed, read-only view of domain state, so reflection reuses it rather
than minting a C++-style opaque `std::meta::info`. Reflecting a compile-time
entity is conceptually projecting it into an inspectable view, so the
vocabulary closes on itself.

Reflection target is layered: nominal type reflection first (name, kind,
fields, methods), then domain reflection (effect set, required authority,
resilience policy, role slots) on top. Type reflection is the C++-parity floor;
domain reflection is the part C++ cannot express and is the reason this
operator is worth building.

## Why this is not "C++ reflection ported"

C++ `^^` reflects the type system: it answers structural questions about types,
members, and functions, because structure is all C++ has. Pergyra carries an
ontology C++ does not: intent, zone, authority, effect, role, projection. So
`reflect` is aimed at a different axis.

C++ `reflect` answers: what is the shape of this type?
Pergyra `reflect` answers: what does this intent do, what effects does it
discharge, what authority does it require, what is its resilience policy?

That axis is the engine for the language-that-emits-languages / MPaC /
blackboard thesis: if a compile-time value can describe an intent's effects and
authority, that value can drive code generation. The information already exists
in the AIR graph and MIR declaration headers, so domain reflection is exposing
facts the compiler already computes, not deriving new ones. That keeps the
implementation risk on the plumbing, not on new analysis.

## Substrate that does not yet exist

Two foundations are missing, and they are the real cost of the feature. Naming
them here so a future session does not mistake `reflect` for a one-evening
unary-operator add.

There is no `projection` value type in the type system. `projection` today is
a domain construct (see `parser_domain_projection.c`,
`type_checker_builtins_projection.c`), not a first-class value type in
`type_system.h`. For `reflect X` to yield a `projection` value usable in
`let p: projection = ...`, a projection value type (or a dedicated `Meta`
result type) must be introduced, most cheaply as a constructed type analogous
to `Array<T>` rather than a new kind variant.

There is no compile-time evaluation path. A search for consteval, constant
folding, comptime, or constant evaluation finds nothing. `reflect` is
compile-time only, so it needs a fold step that resolves the operator to a
constant before lowering. The smallest first form (type-name reflection) can be
folded in the semantic phase into a string constant, which does not require a
general comptime engine; richer forms will.

## Integration map (where it plugs in)

Verified against the tree. Existing prefix operators (`!`, unary `-`, `&`) and
the `?` try operator are the models.

Lexer. `TOKEN_REFLECT` added to the enum in `src/lexer/lexer.h` and to the
keyword table in `src/lexer/lexer_keywords.c` (between `ref` and `relation`).

Parser. `parse_unary` in `src/parser/parser_expr.c` matches `TOKEN_REFLECT`
alongside `!`, `-`, `&` and builds an `AST_UNARY` node via `ast_create_unary`.
No new AST node type: the operator token lives in `unary.op`.

Semantic. `type_check_unary` in `src/semantic/type_checker_expr_ops.c` handles
the unary token switch. This is where the result type (projection) and the
compile-time fold will live.

Lowering and backends. `transpiler_expr_unary_emit.c` (`emit_unary`) for C and
`llvm_expr_unary_core.c` (`llvm_emit_unary`) for LLVM. For a compile-time-only
operator these should never see a `reflect` node once folding lands; until
then the semantic gate prevents reaching them.

Tests. A backend-compare case under `tests/` plus a self-host parity gate in
the same shape as the AIR graph consumer tools.

## Rung ladder

Rung 1 (done): lex and parse `reflect X`; gate it in `type_check_unary` with a
diagnostic, so writing `reflect` yields a clean honest error instead of a
confusing cascade. The gate reuses the existing unary-operator diagnostic code
with the lowering-not-implemented fix hint, exactly as the retry gate did, so
the diagnostic catalog is untouched. No backend changes.

Rung 2a (done, pending build verification): semantic fold of
`reflect TypeName` into the type name as a String literal. In `type_check_unary`,
when the `reflect` operand is a simple identifier, the `AST_UNARY` node is
rewritten in place into an `AST_STRING` node holding the identifier name, and
the expression types as `TYPE_STRING`. Because the node becomes a plain string
literal before any lowering, MIR and both backends see a constant and `reflect`
leaves no runtime trace; no backend code changed. Non-identifier operands keep
the rung-1 gate. Covered by the `reflect_type_name` backend-compare parity case.
The result type here is `String`, not yet `projection`: with a single fact
(the name) a projection object would be premature. The projection-valued result
is rung 2b, taken when a second queryable field exists.

Rung 2b (done, pending build verification): introduce the `projection` value
type so the result of `reflect` is a `projection` exposing `.name`, rather than
a bare String. `projection` is a registered primitive (`type_system.c`,
resolution table, C and LLVM type maps all route it to `String`'s
representation). `reflect TypeName` types as `projection` and folds its node to
the name String; `projection.name` member access folds to that String. The
`reflect_type_name` backend-compare case exercises
`let p: projection = reflect Account; let n: String = p.name;`. Still open:
mint a dedicated diagnostic code `PGY_SEM_REFLECT_NOT_LOWERED` to replace the
reused unary code, and add `.kind`.

Rung 3 (done, pending build): type structure reflection for `.name` and
`.kind`, in both the direct `(reflect T).name`/`.kind` and the
`let p: projection = reflect T; p.name`/`p.kind` forms. `.kind` folds to the
declared kind (`"class"`/`"enum"`/`"primitive"`; struct and class are both
`"class"` since the type system does not distinguish them). The projection
field logic lives in `expr_ops_projection_member` (`type_checker_expr_ops.c`),
called from `type_check_member_access` (which also kept `type_checker_expr.c`
under the 600-LOC semantic-core-shape cap). The `let`-bound form is supported by
carrying the reflected type name on the binding: `Symbol.reflect_target_name`
is set in `type_check_let_decl` (`type_checker_ownership_let.c`) when a
projection let initializes from a folded `reflect`, and the member helper reads
it via `lookup_identifier_symbol`. Covered by the `reflect_type_name`
backend-compare case (`Account` / `class`). Still open: `.fields` and
`.methods` from MIR declaration headers, then rung 4 (domain reflection).

Rung 3 implementation plan (turnkey, do on a machine with a build loop). The
String-representation shortcut from rung 2b cannot carry a second fact, so
rung 3 needs projection to carry the reflected type's identity, not collapse to
its name. The principled, backend-free design is to fold every field access at
compile time and never let a projection reach the backend:

- Stop pre-folding `reflect Ident` to a String in `type_check_unary`. Instead
  keep the `AST_UNARY` REFLECT node, typed `TYPE_PROJECTION`, with the operand
  identifier (which holds the type name) intact. Validate the operand names a
  declared type (struct/class/enum); error otherwise.
- In `type_check_member_access`, when the object types as `projection`, fold the
  field at compile time from the reflected type name: `.name` to the name
  String, `.kind` to a String like `"struct"`/`"class"`/`"enum"` looked up from
  the type declaration. Replace the member node with the folded String literal.
- Propagate through `let` bindings so `let p: projection = reflect X; p.kind`
  works: the projection-typed local must carry the reflected type name. Add a
  `char *reflect_target_name` field to `Symbol` (`symbol_table.h`), set it where
  a `let` initializer is a REFLECT node, and read it at the member-access fold.
  Open item: the exact `let`-statement symbol-creation site was not pinned in
  the sandbox (it is not `type_checker_flow_statement_kinds.c:31`, which is the
  `with`-slot site). Find the `AST_LET_DECL` handler that calls
  `scope_declare` for an ordinary local and set the field there.
- Because every field folds away, projection never needs a runtime struct and
  the backends stay untouched. The rung 2b `String` type map entries for
  `projection` can then be removed once nothing produces a runtime projection.

This is a representation change on top of rung 2b, so it should land only after
rung 2b is built and green, to keep failures bisectable.

Rung 4 (partial, pending build): domain reflection. `.effects` is implemented:
it resolves the reflected declaration's symbol and folds
`type_function_effects` through the shared `effect_mask_to_string` renderer to a
String like `"io,alloc,authority"` (empty for a non-function such as a plain
struct), in both the direct and `let`-bound forms via the same
`expr_ops_projection_member` helper. Because reflection folds in the semantic
phase, `.effects` reads the live effect computation and does NOT depend on
#126 (which captures effects into MIR for backend/runtime consumption, a
separate axis).

`.fields` and `.methods` are also implemented: both resolve the class
declaration via `semantic_find_class_decl_by_name`. `.fields` folds the
comma-joined `name:Type` list (`projection_source_field_count`/`_at` plus
`ast_type_name` for the field type), and `.methods` folds the comma-joined
method names (`ast_class_methods` plus `ast_declaration_name`). The
`reflect_type_name` backend-compare case exercises
`.name`/`.kind`/`.effects`/`.fields`/`.methods`.

Still open, and each needs machinery beyond a simple accessor (so left for a
build-loop session rather than a blind change):

- `.authority` as the specific named authority. Today authority is only the
  `EFFECT_AUTHORITY` flag (already surfaced inside `.effects`). There is no
  per-declaration authority-name accessor: authority is modeled at the zone
  level (`ASTZoneAuthorityData`), so a named `.authority` is not a simple field
  fold -- it belongs to the same zone-authority + AIR-graph domain layer as
  intent reflection below, and should be designed with it rather than bolted on.
- Resilience policy and AIR-graph-sourced intent reflection (effects/authority
  per intent boundary) are the richer domain-reflection layer and the MPaC hook
  proper.

`.kind` is now precise: `projection_kind_label` reads the declaration's
`NominalDeclKind` via `ast_class_nominal_kind`, returning `"struct"`,
`"class"`, `"subject"`, `"vessel"`, `"object"`, or `"tobject"`, and falls back
to `"enum"`/`"primitive"`/`"unknown"` from the type kind.

Rung 5: a splice or generation surface that consumes a compile-time
`projection` to emit declarations, closing the language-that-emits-languages
loop.

## Open questions for a later session

Whether domain authority (runtime capability) and reflected authority
(compile-time fact) should share one representation, or stay distinct.

Whether the result should be a reusable `projection` value or a reflection-only
`Meta` type; the projection choice is locked for vocabulary economy but should
be revisited if the two concepts diverge in practice.

The splicer/generation syntax for rung 5 is unspecified and should get its own
decision record when rung 4 is green.

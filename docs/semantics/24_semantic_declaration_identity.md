# Semantic Declaration Identity Contract

Status: `partial-gate-backed`

## Owner

The semantic symbol table owns declaration identity. A declaration-bearing
`Symbol` stores its nonzero `decl_syntax_id` and whether it is an unfinished
forward placeholder. Source line and column remain diagnostic provenance; they
are not declaration identity.

Pass 1 creates a placeholder from a finalized AST declaration and records its
`SyntaxNodeId`. Pass 2 may complete that symbol only when all three predicates
hold:

1. the symbol kind is the expected placeholder kind;
2. `is_forward_placeholder` is true;
3. `decl_syntax_id` equals the current declaration's `SyntaxNodeId`.

A same-name declaration with a different ID is a redeclaration. It cannot
update the first symbol even when both declarations have identical line and
column coordinates in different modules.

Parser and import-merge boundaries normally finalize `SyntaxNodeId` first.
The public semantic program entry also finalizes a programmatic AST whose root
still has ID zero, using the same AST-owned assignment routine before any
semantic fact is created. This is input normalization, not a name or coordinate
fallback.

## Evidence

`make semantic-declaration-identity-test-smoke` locks the owner fields and
consumer calls, forbids `existing->decl_line/decl_col` identity reads, and
compiles a positive multi-import case. Two negative fixtures export same-name
functions or classes from different modules at the same source coordinates.
They must fail in the semantic stage with stable redeclaration diagnostics;
reaching the MIR duplicate-header verifier is considered too late and fails
the gate.

## Remaining Obligation

This closes Pass 1 to Pass 2 declaration placeholder identity for the current
top-level declaration set. It is not yet a full `SymbolId` or `EntityId`:

- symbol-table lookup remains name-indexed;
- namespace flattening is not injective;
- HIR callgraph linkage now lowers this identity into `RoutineId` edges under
  `25_hir_routine_identity.md`; receiver dispatch and MIR hosted-method joins
  remain open;
- lexical bindings and SSA values still need `BindingId` and `ValueId`;
- revision/foreign-ID rejection remains open.

Those later handles must derive from this declaration owner, not recover
identity from names, prefixes, AST pointers, or source coordinates.

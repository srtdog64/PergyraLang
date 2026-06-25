# 144. Pre-Semantic Declaration-Field Metadata Layer (F2 design sketch)

Status: **design sketch** (2026-06-26). Not implemented. This closes the
SoT-spine residue named in `docs/125` rows 612/609/L731-734: semantic still reads
`ast_class_fields(...)` because class fields are AST-carried, so field shape has
two de-facto owners (AST-for-semantic + `MIRDeclField`-for-codegen).

This document is grounded in the current code, and one finding **downgrades** the
earlier "multi-week architecture rewrite" estimate to a **methodical multi-phase
hoist+migrate** — see §3.

## 1. The residue (one fact, two owners)

Field shape (`name / type / access / mutable / vessel / default`) is owned by the
AST `ClassField` struct (`src/parser/ast_types.h:295`) and re-read in **two**
later places:

1. **Semantic** reads it directly — 7+ sites:
   - `type_checker_class_decl.c` (×2, declaration validation)
   - `type_checker_resolution_graph_decl.c`, `type_checker_resolution_stage_nominal.c` (graph collection)
   - `type_checker_projection_path.c` (×2, projection field owner)
   - `type_checker_assignment.c`, `type_checker_helpers_resources.c`
2. **MIR lowering** re-derives it into `MIRDeclField` (`mir_decl.h:83`) from the
   AST again, via `mir_decl_header_fields.c`.

So the same field-shape fact is rediscovered twice. The SoT spine wants one
owner. The codegen side is already closed (it consumes `MIRDeclField`); the open
side is **semantic**, which has no pre-MIR field model to consume.

## 2. Why the obvious fix doesn't work

The obvious fix — "let semantic consume `MIRDeclField`" — is **impossible by
pipeline order**. `MIRDeclField` is built during MIR lowering, which runs *after*
semantic:

```
parse → AST → semantic_analyze(ast)   ← driver_app.c:164, reads ast_class_fields
            → HIR → RIR → MIR lower    ← builds MIRDeclField (too late for semantic)
```

`MIRDeclField` cannot feed a pass that runs before it exists. So closing the
residue requires a field-metadata model that exists **before** semantic — i.e.,
right after parse.

## 3. Key finding: field shape is **syntactic** (no ordering hazard)

The a-priori worry was that some field facts (e.g. `is_subject_like`,
`is_tobject_like`) require *type resolution* and are therefore inherently
semantic — which would forbid a single pre-semantic owner and force a two-tier
(syntactic + resolved) split. **The code says otherwise:**

- `mir_decl_header_fields.c:103` — `meta->is_subject_like = field->is_vessel_field;`
  → driven by the **`vessel` keyword** (a syntactic `ClassField` flag), not by
  resolving whether the field's type is a Subject.
- `:193-194` — domain-slot `is_subject_like` / `is_tobject_like` come from
  `ast_domain_slot_is_subject(...)` / `ast_domain_slot_is_tobject(...)` — AST
  accessors, syntactic.
- `MIRDeclFieldKind` (8 kinds: class / shared / role-slot / roster-slot /
  world-roster / world-zone / domain-slot / zone-layer) is chosen by **dispatch
  on the owning declaration's AST node kind** — syntactic.

Every `MIRDeclField` shape fact is derivable from the AST alone, with **no
semantic type-resolution dependency**. It is built during MIR lowering only by
historical placement, not by necessity.

**Consequence:** a *single* pre-semantic model can own field shape. There is no
syntactic/semantic ownership seam to split, and therefore **no semantic-ordering
hazard** — the migration is deterministic hoisting, which is low-risk. This is
the finding that downgrades the scope from "architecture rewrite" to "methodical
multi-phase migration."

## 4. The one genuine deferral: the type carrier (shared with F1)

`MIRDeclField` still carries `ASTNode *type` and `ASTNode *initializer`
(`mir_decl.h:80-81`) plus a rendered `type_name`. The rendered `type_name` is
lossless **except** for callable (`EventHandler`) and tuple types — the exact gap
that **F1** (docs/125 row 607 inherent tail) addresses: `mir_render_type_name`
returns NULL for `AST_EVENT_HANDLER_TYPE`, so callable field types fall back to
the retained AST node.

Therefore the pre-semantic field model can own everything **except** a fully
AST-free *type carrier* for callable/tuple field types. That residue is **shared
with F1** and stays AST-referenced until F1's lossless routine/type-signature
payload lands. F2 does not need to solve it; F2 owns field *shape* and references
the type carrier (AST today, F1 payload later).

## 5. Proposed model

A new pre-semantic value `PgyDeclFieldModel`, populated by a pass that runs
immediately after parse and before `semantic_analyze`:

```c
/* Owned by the new pre-semantic decl-field pass. One per declaration that
   carries fields (class / party / roster / world / domain / zone). */
typedef struct {
    const char       *owner_name;
    const char       *name;
    PgyDeclFieldKind  kind;          /* == MIRDeclFieldKind, hoisted earlier */
    AccessModifier    access;
    bool              has_explicit_access;
    bool              is_mutable;
    bool              is_vessel_field;   /* drives is_subject_like */
    bool              is_dynamic;
    bool              is_tobject_like;
    bool              is_relation_layer;
    bool              is_pool_layer;
    int               pool_capacity;
    bool              has_default;
    /* Type carrier: lossless string where possible (F1), AST ref for
       callable/tuple until F1 lands. NOT owned here beyond the reference. */
    const char       *type_name;     /* NULL ⇒ consult type_ast (callable/tuple) */
    ASTNode          *type_ast;      /* retained until F1 lossless payload */
    ASTNode          *default_ast;
} PgyDeclField;
```

Ownership relationships after the change:

- `PgyDeclFieldModel` is the **single owner** of field shape, built once after
  parse.
- **Semantic** consumes `PgyDeclField` instead of `ast_class_fields(...)`.
- **MIR lowering** builds `MIRDeclField` by **copying/referencing**
  `PgyDeclField` instead of re-deriving from AST. `MIRDeclField` keeps only the
  codegen-specific extras (e.g. `required_ability_refs`, claim rows) layered on
  top.
- AST `ClassField` becomes raw parse provenance only — read by the new pass, not
  by semantic or MIR.

## 6. Migration phases (each gated, parity-preserving)

The migration must be incremental because there are ~10 consumers (7 semantic +
the MIR builder + codegen claim paths). No atomic flip.

1. **Build the model, dual-run.** Add the pass + `PgyDeclFieldModel`, populate it
   after parse, but keep all existing readers on AST. Assert
   `PgyDeclField == ast_class_fields` shape for every decl (a drift check, like
   the `ast_compat_count` pattern that closed row 616). Gate: model-vs-AST parity.
2. **Migrate semantic consumers one file at a time.** Each `ast_class_fields(...)`
   site → `pgy_decl_field_*` accessor. Run `test-semantic` + the resolution-graph
   tests after each. Gate: per-site, plus a smoke that forbids new
   `ast_class_fields` in migrated files (allowlist shrinks each step).
3. **Make MIR lowering consume the model.** `mir_decl_header_fields.c` references
   `PgyDeclField` instead of re-reading AST. Verify `test-transpile` 914/0 +
   `mir-declaration-inventory-test-smoke`. Gate: no field-shape AST reads in MIR
   builder.
4. **Flip to authoritative + lock.** Remove the dual-run drift check; forbid
   `ast_class_fields` in `src/semantic` and `src/compiler` (except the new pass
   and `host_decl_compat.c`) via `mir-declaration-inventory-test-smoke`. This is
   the point docs/125 row 612/L731-734 calls "dedicated declaration field
   metadata replaces AST-carried class fields."
5. **Type-carrier residue → F1.** The `type_name == NULL ⇒ type_ast` fallback for
   callable/tuple field types is removed only when F1's lossless type payload
   lands. Until then it is the *one* documented AST reference left, shared with
   row 607's inherent tail.

## 7. Risks & gates

| Risk | Mitigation |
|---|---|
| Hidden semantic dependency in a field fact I classified as syntactic | Phase 1 drift check (`PgyDeclField` vs `ast_class_fields`) runs on the full test corpus before any consumer migrates — a mismatch surfaces a non-syntactic fact before it can cause divergence |
| 8 field kinds × multiple host decls = wide surface | Migrate per-kind, not per-fact; `test-mir` already preserves each kind |
| Parity drift during consumer migration | Per-phase `test-semantic` / `test-transpile` (914/0) gates; never flip authority before all readers migrate |
| Re-introduction of AST field reads | Shrinking allowlist in `mir-declaration-inventory-test-smoke` |

## 8. Honest scope estimate (revised)

Earlier framing: "multi-week architecture change, session-impossible." The §3
finding revises this: it is **not** an architecture rewrite (no
syntactic/semantic ownership seam to split, no ordering hazard) — it is a
**deterministic hoist of an existing syntactic computation earlier in the
pipeline, plus ~10 consumer migrations, each independently gated.**

It is still **not a single session**: a new IR-adjacent layer, ~10 consumer
cutovers, 8 field kinds, and per-phase parity make it a multi-session methodical
migration. But the risk profile is *low* (syntactic determinism), and it
decomposes cleanly into the 5 phases above — each phase is independently
landable and gate-verified, so it can proceed incrementally without a long-lived
branch. Phase 1 (model + drift check) is the natural first session and the
correctness foundation for everything after.

## Related
- `docs/125` rows 612 / 609 / L731-734 — the residue this closes
- `docs/36` — IR minimality (this layer is pre-IR declaration metadata, not a new codegen IR)
- F1 (row 607 inherent tail) — shares the callable/tuple type-carrier residue

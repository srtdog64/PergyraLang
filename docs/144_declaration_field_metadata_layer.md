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

## 9. Progress log

- **2026-06-26 — Phase 1 landed** (`47992aed`): `src/compiler/decl_field_model.{h,c}`
  (`PgyDeclField`, class-field subset) + opt-in dual-run drift check
  (`PGY_F2_DRIFT_CHECK`). Verified: `test-semantic` 2786/0 with the check on,
  **zero `[f2-drift]`** across the corpus — the model is a faithful, total carrier
  of class-field shape.
- **2026-06-26 — Phase 2 complete (simple read consumers)** (`5c1fc7e7`,
  `14c40d1f`): the three pure name/type/mutability readers migrated off
  `ast_class_fields` to the model and added to the smoke allowlist —
  `type_checker_resolution_graph_decl.c` (graph collection),
  `type_checker_resolution_stage_nominal.c` (nominal-stage resolution),
  `type_checker_assignment.c` (field-mutability check). Each verified 2786/0. This
  closes the "graph collection" item of the docs/125 L731 sanctioned-residue list.

- **2026-06-26 — Phase 3 complete (interface cutover)** (`987065b4`): the two
  `ClassField*`-returning accessors (`subject_host_field_at`,
  `projection_source_field_at`) + `projection_source_field_count` now read the
  model and return `PgyDeclField` by value; ~16 caller sites across 11 files
  migrated to value semantics; `expr_host_resolve_class_field_type` takes the
  type node directly. `intent_role_fields.c` contained (2 `ClassField*`-finders
  kept on AST). Verified 2786/0. This closes the "projection field owner" item
  of the docs/125 L731 list. **The semantic side is now down to 4
  `ast_class_fields` sites in 2 files**, all in L731's sanctioned categories:
  `type_checker_class_decl.c` (declaration validation at :222 — cascades via
  `class_declare_field_symbol`; and the generic-shell AST *writer* at :64 which
  stays), and `type_checker_intent_role_fields.c` (the 2 contained finders —
  cascade via `intent_role_resolve_field_type`). Those + Phases 4-5 (MIR builder
  consumes the model; authority flip + lock) and the F1-shared type carrier
  remain.

- **2026-06-26 — Phase 3b complete; SEMANTIC READER SIDE DONE** (`17723ac2`,
  `49a8cee8`): the declaration-validation reader (`type_checker_class_decl.c`;
  `class_declare_field_symbol` now takes `const char*`) and the
  `intent_role_fields.c` finder cluster (`find_nominal_field_by_name` /
  `find_subject_surface_field_by_name` return `PgyDeclField` by value;
  `intent_role_resolve_field_type` takes the type node) are migrated. **Every
  class-field shape read in `src/semantic` now goes through the model.** The only
  remaining `ast_class_fields` call in all of `src/semantic` is the generic-shell
  AST *writer* at `class_decl.c:64` (parse-completion mutation — not a shape
  read, stays by design). Gate: `mir-declaration-inventory` smoke now asserts the
  `src/semantic` `ast_class_fields` count stays at exactly 1 (the writer) — any
  new reader fails. Verified 2786/0. **This closes the docs/125 row 612/L731
  semantic residue** ("semantic rediscovers class-field arrays"): semantic no
  longer rediscovers field shape, it consumes the owned model.

### Scope boundary — what F2 (the docs/125 L731 residue) actually was

docs/125 row 612 / L731 named exactly one residue: **semantic re-reading
`ast_class_fields(...)`** ("declaration validation, graph collection, and the
projection field owner ... until dedicated declaration field metadata replaces
AST-carried class fields"). **That residue is now closed and gate-locked**
(semantic `ast_class_fields` count == 1, the writer) + the MIR class-field
builder consumes the model.

What it did NOT name, and what is therefore **a separate, larger generalization
(call it "F2-general"), not the L731 residue**:
- the ~20 semantic readers of *other* field kinds — `ast_*_shared_fields`
  (party/roster/world/domain), `ast_zone_layer_slots` — and the ~10 MIR-builder
  sites for the same. These are different field *shapes* (shared fields, role
  slots, zone-layer slots), not class fields; modelling them means `PgyDeclField`
  becomes multi-kind (or grows siblings), touching ~30 sites.
- The codegen side of those kinds is already internally consistent (row 616 /
  hosted-view fail-closed), so F2-general is an *ownership-unification* effort,
  not a correctness gap. It should be a deliberately-scoped decision, not folded
  silently into "the F2 residue".

So `mir_decl_header_fields.c` is intentionally in a **mixed** state: class fields
build from the model; the other kinds still build from their own AST shapes
(unchanged, row-616-sanctioned). That is correct, not a wart — the model owns
the one shape L731 was about.

### Remaining after the semantic reader side (F2-general, opt-in scope)
- **Phase 4** — the MIR builder (`mir_decl_header_fields.c`) still re-derives
  `MIRDeclField` from AST. Making it consume `PgyDeclField` would make the model
  the single owner for *both* semantic and codegen (today they derive the same
  syntactic facts independently). The codegen side is already internally
  consistent (row 616), so this is an ownership-unification step, not a
  correctness gap.
- **Phase 5 / F1 carrier** — the model's `type_ast` reference becomes a fully
  AST-free carrier only when F1's lossless callable/tuple payload lands; and the
  generic-shell AST writer at `class_decl.c:64` is the last AST-carried-field
  dependency (relocating it into parse/normalization is the final step toward
  "AST no longer carries class fields").

## 10. Phase 3 plan (interface cutover) — DONE (see §9); kept for reference

The remaining `ast_class_fields` readers are **not** simple reads; they are the
`ClassField*`-returning accessors that the whole semantic layer funnels through —
the "projection field owner" docs/125 L731 names as the sanctioned residue. The
right move is to migrate the *accessor's source* (AST → model), not to delete it.

**Accessors to migrate** (`type_checker_internal.h`):
- `subject_host_field_at(decl, index) -> ClassField*` (owner: `type_checker_helpers_resources.c`)
- `projection_source_field_at(decl, index) -> ClassField*` (owner: `type_checker_projection_path.c`)
- `projection_source_field_count(decl) -> size_t` (same owner; trivial — model count)

**Design:** change the `_at` accessors to return `PgyDeclField` **by value** (a
small struct of AST-lifetime pointers + scalars — no allocation lifetime to
manage; "not found" is signalled by `name == NULL`, matching the current NULL
return). The owner builds the model and indexes it.

**Caller cutover** (~16 sites; each changes `field->X` → `field.X`,
`field->type` → `field.type_ast`, and `field == NULL` → `field.name == NULL`):
`type_checker_builtins_projection.c`, `type_checker_builtins_query_domain.c` (×2),
`type_checker_call_constructor.c` (subject + projection), `type_checker_domain_projection_fields.c`,
`type_checker_expr.c`, `type_checker_expr_host.c`, `type_checker_intent_helpers.c`,
`type_checker_intent_role_fields.c` (×2), `type_checker_projection_path.c` (×2),
`type_checker_reflect.c`.

**Watch-outs:**
- *O(n²):* building the model inside `_at(index)` per call is O(fields) per call
  → O(fields²) in the typical `for i<count { at(i) }` loop. Acceptable for
  semantic (not hot; correctness > perf per CLAUDE.md §9), but the cleaner end
  state is to give callers a `build-once + iterate` accessor in a later pass.
- *Parity:* `test-semantic` (2786/0) after the cutover; never partial-land (all
  callers in one slice, since the accessor signature changes).
- *Gate:* once the two owners read the model, add them to the F2 allowlist; the
  only remaining sanctioned AST reader is then `type_checker_class_decl.c`
  (declaration validation + the generic-shell AST *writer* at :63, which stays).

**Phases 4-5** (MIR builder consumes the model; flip authority + lock the gate)
and the F1-shared type-carrier residue follow, per §6.

## Related
- `docs/125` rows 612 / 609 / L731-734 — the residue this closes
- `docs/36` — IR minimality (this layer is pre-IR declaration metadata, not a new codegen IR)
- F1 (row 607 inherent tail) — shares the callable/tuple type-carrier residue

# reflect: Domain Reflection (intents, authority, resilience)

Status: design. This is the decision record for the next reflect layer, the one
that makes the operator the MPaC hook: reflecting domain entities (intents,
their boundaries, authority, resilience policy), not just types and functions.
The current `reflect` (see `reflect_operator_design.md`) folds type and function
fields at compile time. This document pins what domain reflection should expose,
the one hard architectural constraint that splits it into two rungs, and the
build order.

## Why this is a separate layer

Everything reflect does today folds in the semantic phase: the operator rewrites
its node into a String constant before any lowering, reading facts that exist at
semantic time (type kinds, fields, function params, declared effects). Domain
reflection wants facts that live at two different phases:

- Semantic-time intent facts: an intent's name, its steps, its declared
  resilience modifier (`with retry(n)`), its declared effect set, and its
  involved participants. These exist on the AST/semantic side and can fold the
  same way the current fields do.
- AIR-graph intent facts: per-boundary provenance, `authority_required`,
  sync class, evidence linkage. These are produced by the AIR back-end IR
  (`src/compiler/air_dump_json.c`, schema `pgy.air.graph.v1`), which does not
  exist yet when the semantic fold runs.

That phase gap is the whole reason this is not just "more helper branches." A
compile-time `reflect MyIntent.authority` that wants the AIR boundary's
`authority_required` cannot fold in semantics, because the AIR graph is built
after semantic analysis. Pretending otherwise would either read a half-built
graph or silently return empty.

## Evidence: what each source exposes

Intent declarations (`src/parser/ast_domain_api.h`):

- `ast_intent_decl_name`
- `ast_intent_decl_steps` / `ast_intent_decl_step_count`
- `ast_intent_decl_involves` / `ast_intent_decl_involve_count`
- `ast_intent_decl_values`, `ast_intent_decl_bindings`
- Resilience: `with retry(n)` is parsed and carried as
  `MIRDeclHeader.intent_retry_count` (see `reflect_operator_design.md` and the
  retry gate). At pure AST level the retry attribute is on the intent node.

Effects (already wired): `type_function_effects` + `effect_mask_to_string`,
including the `authority` flag (`EFFECT_AUTHORITY`).

AIR graph (`src/compiler/air_dump_json.c`, `pgy.air.graph.v1`):

- summary: `intent_count`, `boundary_count`, `evidence_count`, `drift_count`,
  `rir_boundary_evidence_count`, `rir_authority_evidence_count`.
- boundaries[]: `kind`, `owner_name`, `source_name`, `intent_index`,
  `step_index`, `sync_class`, `authority_required`,
  `source_from_intent_default`, `source_from_action`.
- evidence[] back-references boundaries; boundaries back-reference intents.

Named authority: authority is modeled at the zone level
(`ASTZoneAuthorityData`, referenced from `src/parser/ast.h`); there is still no
per-declaration authority-name accessor. The AIR boundary carries
`authority_required` (a yes/no), not the authority's name. So a named
`.authority` needs either a zone-authority-name accessor at AST level or the
AIR boundary to carry the authority name. This is an open prerequisite, recorded
here rather than guessed at.

## Proposed surface

Reflecting an intent yields a `projection` (same value type as today). Fields,
split by phase:

Semantic-foldable (rung 4a):

- `.name` -- intent name (already works through the generic name fold).
- `.steps` -- comma-joined step labels, or step count as a string.
- `.retry` -- the declared retry count (`"0"` when none).
- `.effects` -- the intent's effect set (already works via `.effects`).
- `.involves` -- comma-joined participant names.

AIR-sourced (rung 4b):

- `.authority` -- the required authority (named once the prerequisite lands).
- `.boundaries` -- boundary kinds / provenance per step.
- `.evidence` -- evidence linkage counts.

## The two rungs

Rung 4a (semantic-foldable intent fields) -- STARTED, pending build. Implemented
in `expr_ops_projection_member`, mirroring `.fields`/`.params`:

- Added `semantic_find_intent_decl_by_name` (`type_checker_host_lookup.c`),
  a one-line wrapper over the generic host-index finder, since `AST_INTENT_DECL`
  is already indexed.
- `.steps` folds the comma-joined step names (`ast_intent_decl_steps` +
  `ast_intent_step_name`).
- `.retry` folds the declared retry count (`ast_intent_decl_retry_count`,
  formatted with a stack `snprintf` -- allowed; only `sprintf`/`strncat` are
  banned -- then `ast_morph_to_string`).

`.involves` is also done: it folds the comma-joined `alias:Subject` participant
list (`ast_intent_decl_involves` + `ast_intent_involves_alias` +
`ast_intent_involves_subject_type`). Rung 4a is therefore complete for the
semantic-foldable intent fields (`.name`/`.steps`/`.retry`/`.involves`).

Still open in 4a: a backend-compare parity case using a real intent (deferred
until the type/function reflection bundle is CI-green, since a non-compiling
case would turn CI red). `.effects` on an intent currently returns empty because
`type_function_effects` only reads function types; surfacing an intent's effect
set is a small follow-up. Everything past this is rung 4b, which the design
above gates on a concrete MPaC use case -- it should not be built speculatively.

Rung 4b (AIR-sourced reflection). This is the genuinely new machinery and the
MPaC hook. Two candidate designs, to be decided when 4a is green:

1. Deferred reflection. `reflect MyIntent.authority` records, at semantic time,
   a deferred query keyed by intent name + field; an AIR post-pass resolves it
   against the built graph and back-patches the constant. This keeps the
   in-language surface uniform but introduces a deferred-constant mechanism the
   compiler does not have today.
2. Tool-side reflection only. AIR-sourced facts stay in the build-time AIR
   consumer tools (the existing `air_graph_*` self-hosted tools already read the
   live dump), and the in-language `reflect` is restricted to semantic-foldable
   fields. Simpler, but the in-language operator never sees AIR facts.

Recommendation: 4a now (real, foldable, low-risk). For 4b, prefer design (1)
(deferred reflection) only if a concrete MPaC generation use case needs AIR
facts inside the language; otherwise keep AIR facts in tooling (design 2) and
do not add the deferred mechanism speculatively.

## MPaC hook (rung 5, separate doc when reached)

The payoff: a compile-time `projection` describing an intent's effects,
authority, and resilience can drive code generation -- the language-that-emits-
languages thesis. That generation surface (a splice/emit form consuming a
projection) is its own decision record and should be opened only after rung 4a
is green and at least one real generation use case is written down, so the
generation primitive is shaped by a consumer instead of invented first.

## Build order

1. Add/confirm `semantic_find_intent_decl_by_name` and an AST-level intent
   retry accessor.
2. Rung 4a: `.steps`, `.retry`, `.involves` folds in
   `expr_ops_projection_member`, using the safe `ast_morph_to_string` +
   `pergyra_str_append` patterns (the two CI gates, semantic-core-shape and
   memory-string-safety, forbid raw `data.string.` writes and unsafe string
   APIs respectively).
3. Backend-compare parity case reflecting an intent.
4. Resolve the named-authority prerequisite (zone-authority-name accessor).
5. Decide rung 4b design against a concrete MPaC use case.

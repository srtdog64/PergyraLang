# Judgment -> Diagnostic Code Map

A compact, machine-checked table mapping each domain **judgment** (the `|-` rules
written in prose in `00_proof_contract.md`, `01_intent_world_zone.md`,
`02_relation_effect_projection.md`) to the **compiler diagnostic code** emitted
when that judgment is violated, and to the checker that emits it.

This exists to close one specific gap: the judgments were stated as prose and
the diagnostic codes lived in the compiler, with no checked correspondence
between them. The manifest below is verified by
[`../../tests/judgment_diagnostic_adequacy_smoke.sh`](../../tests/judgment_diagnostic_adequacy_smoke.sh):
every code must be a real entry in `src/semantic/diag_codes.h`, and the named
checker must actually emit it. Rename a code or move a check and the gate fails.

## What this is NOT

It is **not** a soundness proof, and the 1:1 ideal is **not** yet reached. The
`status` column states the truth: only some judgments have a *dedicated* code;
others are *folded* into a shared code (a known gap), or *split* across several.
The manifest measures that distance rather than hiding it. A genuine 1:1 table
(one dedicated diagnostic per judgment) is the target, not the current state.

## Manifest

Columns: `judgment  diagnostic-code  status  primary-checker`.
`status`: `dedicated` (its own code) | `folded` (shares another judgment's code,
no dedicated one) | `split` (spread across multiple codes/checkers).

<!-- BEGIN judgment-diagnostic-manifest -->
```
intent_ok      PGY_CODE_SEM_INTENT_STEP_INVALID    dedicated  type_checker_intent_decl.c
authorized_by  PGY_CODE_SEM_INTENT_STEP_INVALID    folded     type_checker_intent_authority.c
world_ok       PGY_CODE_SEM_WORLD_CONTRACT_INVALID dedicated  type_checker_world_decl.c
zone_ok        PGY_CODE_SEM_ZONE_CONTRACT_INVALID  dedicated  type_checker_zone_decl_authority.c
relation_ok    PGY_CODE_SEM_ZONE_CONTRACT_INVALID  folded     type_checker_relation_decl.c
effect_ok      PGY_CODE_SEM_EFFECT_CONFLICT        dedicated  type_checker_flow_effects.c
projection_ok  PGY_CODE_SEM_VIEW_KIND_MISMATCH     split      type_checker_builtins_slotops.c
```
<!-- END judgment-diagnostic-manifest -->

## Coverage (honest)

7 judgment families, **4 with a dedicated diagnostic code** (intent, world, zone,
effect) and **3 without** (`authorized_by` folds into the intent-step code;
`relation` folds into the zone code; `projection` is split across view/zone/
builtin codes). So the judgment->code mapping is **4/7 dedicated** today. The
remaining three are the concrete work to reach a real 1:1 rule table: add a
dedicated `AUTHORITY`, `RELATION`, and `PROJECTION` diagnostic code and route the
corresponding checker to it.

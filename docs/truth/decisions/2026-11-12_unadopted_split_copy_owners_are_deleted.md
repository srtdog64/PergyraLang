# The unadopted split exclusive-copy owners and their unit are deleted (2026-11-12)

Recommended by the Claude root lane on the single-source-of-truth reading
below; the user approved it on 2026-11-12 ("작업해라 너의 행동을 믿는다").

- `ast_collection_exclusive_value_copy_owner.pgy` (added 2026-10-11, still
  edited in the 2026-11-10 landing) is the owner the semantic body bundle
  imports. The split owners added on 2026-10-16 (`copy_edge`, `copy_binding`,
  `copy_liveness`, `copy_consumption`, `copy_observation`, `copy_partition`,
  `field_label`) redeclare its functions with other signatures, and no
  production module imports them.
- `tests/self_hosted/fixtures/collection_copy_field_identity_unit.pgy`
  imported that split chain beside the body bundle and stopped building at
  HEAD; its 29 checks exercised only the unadopted copy.
- Two definitions of one fact family is dual authority. The split owners,
  the unit, and the unit's checks in `collection_inout_effect_owner.sh` and
  `collection_copy_field_owner.sh` are removed. The copy-field source
  fixtures those gates run through the arity probe stay.
- If field-identity copy edges are wanted again, they return as a
  migration of the monolith's consumers that deletes the monolith, not as a
  parallel owner. The deleted files are recoverable from `d01c2388a`.

This narrows the "every negative is kept" line of the merged-probe decision
of [2026-11-11](2026-11-11_semantic_owner_probes_merge_into_one_build.md) for
these 29 checks only.

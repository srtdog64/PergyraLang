# Bootstrap Artifact Comparison Review

Status: AUDIT COMPLETE; primary transport integration locally verified.

Base revision: `b4d22cf2c6e68fcbd42a1ce4a44444de0e7899fa` plus the primary's
uncommitted enum/receiver/CI/documentation work. This is a bounded review of
the reached bootstrap storage failure, not a second compiler implementation
rung. Findings are observations or proposals until the primary validates them.

## Shared objective card

- Objective: eliminate normalized copies when the existing bootstrap artifact
  inputs are byte-identical, without changing the Pergyra parity verdict.
- Priority: semantic equality and the existing verdict owner, explicit input
  and normalization failure, then materialization cost.
- Fact owners: `pgy_selfhost_normalize_text_artifact` owns the existing CR,
  `source_module_path` and trailing-empty-line policy; the Pergyra backend-output
  comparator owns artifact-kind/projection admission and final line comparison.
- Last consumer: `pgy_selfhost_compare_expected_text_artifact_file_with_owner`,
  reached by the unchanged file-pair calls in `driver_bootstrap.sh`.
- Forbidden fallback: shell-only success from `cmp`, changed normalization,
  native retry, ignored I/O failure, fixture routing, a new cache, or deletion
  of old artifacts. No compiler source, installed binary or semantic registry
  changes belong to this review.
- Integration gate: existing `backend_output_comparator_parity.sh`, including
  the new `text_artifact_file_comparison_owner.sh`; the primary also checks the
  existing full-source MIR pair once without materializing normalized copies.

## Independent lanes and edit scopes

- Primary: the existing file-comparison function and explicit normalization
  preparation failure guards in `llvm_leg_helpers.sh`, the focused gate,
  its existing parity parent, this directive and navigation evidence. The
  reached blanket cmp ban in the component inventory is narrowed to the named
  transport function, with focused checker-mechanics negatives; compiler
  behavior remains owned by the executable parity gate. First observe the old
  identical-input copy behavior and the blanket-ban false positive as RED gates.
- `ci_assertion_review`: inspect verdict preservation, explicit error paths,
  normalization equivalence and immutable-input assumptions. Read production
  and test sources; edit only
  `docs/audits/2026-09-05_bootstrap_artifact_transport_review.md`.
- `semantic_index_review`: independently assess the focused gate's ability to
  detect bypassed owner verdicts, wrong normalization and swallowed failures.
  After the primary announces the patch, run the focused gate with the supplied
  existing real comparator. Edit only
  `docs/audits/2026-09-05_bootstrap_artifact_gate_review.md`.

Agents must not edit primary files, each other's reports, active handoff,
progress percentages or SoT status. Report blockers promptly; do not open
another implementation lane or delegate further.

## Allowed checks and budgets

- Read-only `rg`, source reads, scoped Git diffs and shell syntax checks.
- Static checks: 60 seconds; focused execution: 300 seconds. No full matrix,
  full MIR producer, full bootstrap, compiler rebuild or remote publication.
- Use the real comparator already at
  `.tmp/self_hosted/driver/enum_receiver_integration_20260905/backend_output_comparator_38116.exe`
  (SHA-256 `8F11050ADBDB56C9E14552369617E7BFDFB50A47ED4FD89231E4E29B2DDB21FC`).
  Preserve old artifacts. A focused gate may create its own small run directory;
  D: currently has only about 93 MiB free.
- Do not inspect or edit `examples/raid_graph_fsm/results.txt`,
  `docs/compiler_architectures/`, `pgy-80135c2c/`, or `pgy-91d769ec/`.
- If normal execution fails during environment initialization, an approved,
  narrowly scoped execution request may be used. Do not treat that as deletion,
  Git write or publication authority. All text edits use `apply_patch`.

## Integration responsibility

The primary accepts or rejects findings against current source and executable
evidence, records observed RED/PASS results separately, and updates this status
only after both reports and the shared gate have been checked. Review completion
does not imply self-host substitution progress or a green remote dirty-tree CI.

## Observed completion

- [Transport review](../audits/2026-09-05_bootstrap_artifact_transport_review.md):
  normalization preparation failure propagation, metadata and input-lifetime
  qualifications, then the scoped component placement ratchet and six
  independent checker-mechanics negatives.
- [Gate review](../audits/2026-09-05_bootstrap_artifact_gate_review.md): final
  22-case focused run `75625` PASS (`run.MbqSpI`), including the strengthened
  top-level verdict and both actual path-conversion refusals.
- Primary observed old identical-input copy RED (`run.FEbn0y`), final C/LLVM
  comparator parent `15801` PASS, and the existing full-MIR pair through the
  same function PASS (`full-mir.cee6AL`), with no normalized copies and unchanged
  input hashes. This is not a full bootstrap rerun or constant-memory claim.
- Complete component integration `59600` PASS in 188 seconds after its blanket
  cmp ban was observed RED and narrowed to the existing transport function.
  The 60-second static latency target remains unmet. No compiler source,
  installed artifact, SoT status, old artifact or Git index was changed by this
  coordination slice. Source/test/docs edits remain unpublished.

# ArrayString named value-boundary semantic rejection

Status: `PUBLISHED — REMOTE GREEN`

Exact base: `7fbe86c2c44b65c1330a34585ca5db0096595c1c` on
`origin/main`.

## Objective card

- Objective: make the production self-host source-to-MIR entrypoint reject an
  unnamed `Array<String>` value passed through an `own`, `ref`, or `inout`
  parameter boundary before a MIR artifact is published, matching the native
  semantic boundary.
- Priority order: preserve the native semantic distinction; consume carried
  call-target, signature, and expression-place facts; fail before MIR; retain
  named-local ArrayString and copy-only String behavior; keep the change
  bounded to the reached executable fixture.
- Fact owner: a semantic named-value-boundary verdict derived after body call
  targets and expression places are resolved. The existing signature owner
  remains authoritative for parameter type/mode, and the expression-place
  owner remains authoritative for binding versus value identity.
- Last legitimate consumer: `SemanticAstBodyTypeBundle` admission in the
  production `DriverSourceMirProjectionFromAdmittedRequest` path. MIR lowering
  and direct C/LLVM backends are not semantic recovery owners.
- Forbidden fallback: accepting the source and relying on direct-backend error
  19, rescanning AST/source text, inferring the boundary from a backend type
  name, rejecting copy-only String call results, accepting an unnamed
  ArrayString literal/call result, or publishing a partial MIR artifact.
- Verification gate: one focused parity gate must observe native and self-host
  rejection with no artifacts, while the existing named-local ArrayString
  owner-handle fixture and fresh String owner-handle fixture remain green.
  The component contract must ratchet the semantic owner and forbid a backend
  reconstruction path.

## Edit scope

- Allowed implementation: `src/self_hosted/semantic/` and the minimum import
  edge needed by its existing body bundle.
- Allowed tests: one permanent negative fixture and one focused parity script
  under `tests/self_hosted/`, plus literal structural ratchets.
- Allowed coordination/evidence: this directive,
  `docs/current_work_collaboration.md`, `docs/current_work_handoff.md`, the SoT
  registry, one dated audit/result, and existing progress/owner inventories
  only when their current contracts require it.
- Forbidden overlap: direct-MIR C/LLVM error-19 widening, caller cleanup/move
  policy, ArrayString ABI projection, generic query/cache architecture, and
  unrelated BRIDGE rows.

## Validation budget and integration

- Static owner gates: 60 seconds each.
- Focused native/self-host parity: five minutes.
- Full component/production bootstrap: only after the focused slice is green;
  integration shards remain within the repository 30-minute local budget.
- Integration owner: the primary task at this exact base.
- Integration gate:
  current consolidated successor
  `tests/self_hosted/parity/direct_mir_array_named_value_boundary_owner.sh`
  supersedes the published String-only filename while retaining its cases.

The focused native/self-host gate, existing ownership controls, current-source
DRV-2 build, and native production-bootstrap emission are locally green.
Implementation `5f77be4fe3601487e5e5b7f6e7b67fb2df5a18b9` and generated
inventory correction `a6f530ff153f6cf9047bcc4d889418a08198f781` are on
`origin/main`; exact-head full CI run `33258296309` completed GREEN 30/30.
This directive does not own semantic status, SoT census, project progress, or
a successor rung.

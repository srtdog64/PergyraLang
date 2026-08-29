# Array type named value-boundary parity

Status: `IMPLEMENTATION COMPLETE — LOCAL GREEN, PUBLICATION PENDING`

Exact base: `965d4399e46795f583dcbb8b881cd78e626f13a2` on
`origin/main`.

## Objective card

- Objective: make the production self-host source-to-MIR entrypoint reject an
  unnamed value of any canonical `Array<T>` type passed through an `own`,
  `ref`, or `inout` parameter boundary before MIR publication, matching the
  native borrow-tracked array rule.
- Priority order: preserve native semantic identity; consume the existing
  canonical Array shape owner; reject before MIR; keep named Array arguments,
  default-mode Array values, and copy-only non-Array values admitted; avoid an
  element-type allowlist.
- Fact owner: `SemanticArrayElementType` remains the canonical structural
  `Array<T>` shape fact. `SemanticAstNamedValueBoundaryVerdict` consumes that
  fact together with admitted parameter mode and expression place.
- Last legitimate consumer: `SemanticAstBodyTypeBundle` admission in
  `DriverSourceMirProjectionFromAdmittedRequest`, before
  `SelfMirProgramFactsBeforeCanonicalIdsObserved` can publish MIR.
- Forbidden fallback: accepting an unnamed Array value and relying on a direct
  backend rejection, exact `Array<String>`/`Array<Int>` allowlists, substring
  matching, source/AST rescans, element-type-specific duplicate verdicts,
  rejecting default-mode Array parameters, or rejecting copy-only String call
  results.
- Verification gate: one consolidated Array named-boundary gate observes
  native/self-host rejection without C/MIR artifacts for String and Int call
  results and populated literals, while named `own Array<String>`, named
  `own Array<Int>`, default-mode Array values, and copy-only String remain
  admitted. A malformed Array spelling must not classify as tracked.

## Reproduced executable gap

- Native rejects `ReleaseOwnedInts(BuildOwnedInts())` with the named-variable
  ownership diagnostic and publishes no C.
- The installed self-host at this base exits zero and publishes a 9,682-byte
  verified MIR artifact.
- The direct C backend rejects only afterward with
  `direct MIR Array argument entrypoint identity is invalid`.

## Edit scope and integration

- Allowed implementation: the existing semantic named-boundary verdict owner
  and its canonical Array shape import only.
- Allowed tests: consolidate the existing named-boundary parity script, add
  permanent Int negative/named fixtures, update the minimum Make target and
  literal structural ratchets.
- Allowed coordination/evidence: this directive, current collaboration and
  handoff cards, the SoT registry, owner inventory, one dated audit, and
  generated language-word inventory only if the official gate requires it.
- Forbidden overlap: backend Array parameter admission, Array ABI/layout,
  caller cleanup/move policy, non-Array ownership families, cache/query
  architecture, and unrelated BRIDGE rows.
- Integration owner: the primary task at this exact base.
- Integration gate:
  `tests/self_hosted/parity/direct_mir_array_named_value_boundary_owner.sh`.

The current-source installed driver, consolidated Array family gate, existing
Array ABI/owner-transfer controls, component contract, and native production
bootstrap are locally green. This directive does not own semantic status,
registry census, project percentage, remote publication, or a successor rung.

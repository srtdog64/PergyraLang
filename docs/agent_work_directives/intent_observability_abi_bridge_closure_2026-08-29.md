# Intent observability ABI BRIDGE closure — 2026-08-29

Status: `WAITING` (candidate base `01afa785794f461367b91d37ecea47168a09e55e`;
runs `33209375173` and `33213561874` exposed successive integration failures;
release only after the follow-up bootstrap repair's replacement exact-head CI
is GREEN)

This directive coordinates the next candidate rung. It is not semantic
authority, a registry verdict, progress evidence, or permission to overlap the
active intent-declaration publication lease. The current source, canonical ABI
definition, SoT registry, and executable gates remain authoritative.

## Shared objective card

- Objective: migrate every reached self-host emission consumer of the existing
  `abi.intent_observability_rows` authority from source-name reconstruction to
  the carried `RuntimeCallAbiId`, delete the old reads, and change that same
  registry row from `BRIDGE` to `CLOSED`. No new authority row is allowed. A
  valid closure reduces the census from `CLOSED=53 BRIDGE=34 ACTIVE=1` to
  `CLOSED=54 BRIDGE=33 ACTIVE=1`.
- Priority: canonical ID carriage, source/ID cross-seal at admission, exact
  consumer migration, missing/crossed-ID failure before artifact publication,
  emitter-side source lookup deletion, static ratchet, then patch size.
- Fact owner: `src/common/intent_observability_abi.def` owns the append-only ABI
  identity and row fields. The generated Pergyra projection owns `RowForId`;
  semantic expression identity and admitted MIR expression nodes own the
  carried `RuntimeCallAbiId`. Emitters may project a carried ID through the row
  owner but may not rediscover identity from source spelling.
- Last legitimate consumers: semantic source-C direct-call emission, nested
  intent C/LLVM projection, and composite intent LLVM projection. All three
  reached consumer families must migrate before the registry row can close.
- Direct bypasses to delete:
  `RuntimeCallCName -> IntentObservabilityAbiRowForSource(source_name)` in the
  semantic C emitter; the four fixed `RowForSource` lookups in nested intent
  C/LLVM emitters; and the nine fixed lookups in the composite intent LLVM
  emitter. Raw-text runtime-call rewriting may not act as an observability ABI
  compatibility path.
- Forbidden fallback: emitter-side `RowForSource`, a fixed observability-name
  array, deriving ABI identity from arity or sorted position, accepting a
  missing/foreign valid ID, retrying through raw expression text, or closing
  only one of the three reached consumer families.
- Verification gate: the integration owner extends
  `tests/self_hosted/parity/intent_observability_installed_self_host_owner.sh`
  and the existing MIR identity, nested C/LLVM, and composite LLVM gates. The
  decisive falsifiers hold source spelling and shape fixed while crossing the
  carried ID to another valid observability row, and require nonzero exit with
  no C, LLVM, object, or executable artifact.

## Independent work scopes

- Semantic C carriage scope: consume
  `SemanticExpressionGraphRuntimeCallAbiId` at the semantic direct-call leaf,
  cross-seal it against admitted source identity, and remove observability from
  raw-text/name-based runtime rewriting. It must not edit nested/composite
  planners or emitters.
- Nested intent scope: seal the four actual MIR call-node ABI IDs into the
  existing nested plan/receipt and make both C and LLVM emitters resolve only
  those IDs. It must not edit semantic C or composite intent consumers.
- Composite intent scope: seal the actual Main observability call-node ABI IDs
  into the composite plan and remove its nine fixed source-name rows. It must
  not edit semantic C or nested intent consumers.
- Integration scope: the primary task alone owns shared row-projection changes,
  common gates and ratchets, registry status, progress, handoff/collaboration
  documents, staging, commit, push, and CI interpretation. Parallel agents may
  not publish or change registry/progress state.

## Allowed commands and budgets

- Read-only audits may use `rg`, `git diff`, and focused source inspection.
- After the current publication lease is released, each implementation scope
  runs its narrow syntax/static check first. The integration owner builds one
  fresh installed driver and reuses it across the focused gates.
- Static owner/ratchet checks target 60 seconds. Focused executable parity has
  a five-minute budget per semantic execution target. No new CI job, duplicate
  self-host build, full matrix, cache, shard, or timeout increase is admitted.

## Integration and output classification

- Integration owner: the primary task holding
  `docs/current_work_collaboration.md`'s sole active lease.
- One integration gate:
  `tests/self_hosted/parity/intent_observability_installed_self_host_owner.sh`,
  expanded to invoke the existing nested/composite identity negatives rather
  than rebuilding the compiler per fixture.
- The three 2026-08-29 agent audits are `OBSERVATIONS` and this file is an
  `IMPLEMENTATION CANDIDATE`. Neither proves readiness, closure, or progress.
  The primary task must recheck the exact CI-released tree and first demonstrate
  a red valid-ID crosswire before changing production code.

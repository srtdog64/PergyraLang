# Nested Collection-Call Arity Semantic Admission

Status: PUBLISHED — EXACT CI GREEN

Exact base revision: `1478c418049cdc39b32bede3c74085ef52f6de02`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, fuzz priority, or completion
claim.

## Shared objective card

- Objective: make the existing Pergyra expression-graph owner reject a nested
  collection call with the wrong arity before MIR publication, preserving the
  exact `call_arity_mismatch` facts already owned by the call protocol.
- Priority order: keep collection protocol identity and arity authoritative;
  retain graph traversal and exact diagnostic facts; reject before MIR; relay
  the already published public JSON identity; preserve valid nested scalar
  calls; add one negative ratchet for the reached shape.
- Fact owners: `ast_expression_graph_collection_call_protocol_owner.pgy` owns
  `SetSize` arity one; `ast_expression_graph_set_call_owner.pgy` already owns
  the exact arity verdict; the concrete-scalar graph verdict owner owns nested
  traversal and must not turn an owned invalid call into “not applicable”.
- Production entrypoints: installed public MIR and C JSON requests plus the
  direct Pergyra DRV-2 diagnostic modes.
- Direct bypass to delete: `SemanticExpressionGraphConcreteScalarValueOwned`
  asks successful resolved-call scalar typing before it admits the child call
  to `SemanticExpressionGraphConcreteScalarValueError`; invalid arity therefore
  becomes false ownership, the error traversal is skipped, and MIR publishes
  `SetSize(0, 0)` inside a binary expression.
- Last legitimate consumer: `SemanticAstExpressionVerdictForGraph` must consume
  the existing graph-owned arity error before its MIR-stage caller publishes a
  statement.
- Forbidden fallback: expression-text or identifier rescanning, native retry,
  C-side semantic mapping, a second arity table, changing the public identity,
  rejecting all nested calls, backend-only repair, or treating malformed JSON
  as the semantic result.
- Focused gate:
  `tests/self_hosted/parity/nested_collection_call_arity_semantic_admission_owner.sh`.
- Falsifying case: `let x=SetSize(0,0)-0;` must remain accepted by both AST
  paths, fail before public MIR/C artifact publication with exact function,
  expected-one, and actual-two Pergyra facts, match explicit native public
  identity, and retain a valid nested collection-call success control.

## Opening evidence

- At exact published HEAD, public and explicit-native AST both accept the
  21-byte source. Public MIR and both direct DRV-2 diagnostic modes exit zero
  and publish a one-instruction MIR containing `(SetSize(0, 0) - 0)`.
- Explicit-native MIR and C both reject with
  `PGY_SEM_BUILTIN_ARGS_INVALID`, cause
  `semantic:builtin:signature_mismatch`, fix
  `match-builtin-signature`, line 1 column 7, and the exact one-versus-two
  message. Public C rejects only with a malformed-receipt boundary.
- The collection protocol already declares `SetSize` arity one, and the set
  call fact owner already constructs exact Pergyra `call_arity_mismatch`
  facts. The missing fact is not a new signature; it is ownership of the
  invalid nested call during concrete-scalar traversal.
- SoT opens unchanged at `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. This rung targets the reached semantic execution seam; counts and
  progress do not change until executable substitution is proven.

## Local integration evidence

- The concrete-scalar owner now classifies a resolved call with invalid arity
  as owned before successful return typing, allowing its existing recursive
  error traversal to emit the protocol-owned verdict. It adds no function-name
  table, message scan, backend mapping, or retry.
- A fresh Pergyra-built DRV-2 was installed at SHA-256
  `DDB7C126CBC5A483C4E45DE33CF91D08CCFCC8125AAD64D8411FCC8A9C3FF189`.
- The focused gate passes exact direct text/JSON, public MIR/C receipt relay,
  native identity comparison, no-artifact/no-native-retry negatives, and a
  valid nested `SetSize` C execution control.
- The existing public call-arity receipt, language-word registry, language
  contract golden spine, and full installed CLI aggregate are green. Static
  diagnostic, SoT edge, single-owner, protocol, build-source, substitution,
  hard-contract, and progress gates are green.
- The general standalone `self-host-semantic-parity` gate is outside this
  production artifact-graph rung and remains pre-existing red: its fixed
  114-row manifest ratchet does not admit the already published 115th
  `bad_value_param_array_index_assign` golden, whose separate text checker
  reports `ok`. This rung neither changes that gate nor treats it as success.
- Local green evidence preceded publication and did not by itself close the
  active lease.

## Publication evidence

- Exact implementation revision
  `687d7c20bd63d12d7d5afdc9bc12ac47cca00f3a` is published on `origin/main`.
- Exact CI run `33587272803` completed `30/30` success in 35.7 minutes.
  `build-linux` completed in 17.6 minutes, Windows in 8.2, sanitizer in 12.5,
  and self-host codegen bootstrap in 8.9.
- The full self-host job completed in 35.4 minutes with exactly one
  `gen2 == gen3 (173295 lines)`, installed a Pergyra-built DRV-2, observed the
  focused nested-arity marker, and completed the installed CLI aggregate.
- Linux observed the 146-row language-word inventory and component structural
  source/removed-path ratchets green. The focused executable gate owns behavior.
- This production artifact-graph rung is published. SoT registry counts remain
  `88/183`, `55/32/1`; the existing CLOSED collection-call protocol row now
  names its concrete-scalar consumer and executable evidence.

## Coordination bounds

- Independent edit scope: the concrete-scalar graph verdict owner, one exact
  production parity fixture/expected receipt, a valid control, its focused
  execution gate, Make/installed/component wiring, the reached SoT evidence
  row if applicable, and current handoff snapshots.
- Forbidden overlap: no other task may edit or publish this rung. Public wire
  ownership, C transports, collection protocol arities, unrelated diagnostic
  families, and protected untracked paths are read-only.
- Integration owner and gate: the primary task owns implementation and
  integration; the focused gate above is the falsifier and the installed CLI
  aggregate is the single local integration boundary.
- Outputs are candidates until a fresh Pergyra-built DRV-2, focused gate,
  aggregate, exact commit/push, and exact CI are observed.

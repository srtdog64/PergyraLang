# Contextual collection-constructor call-argument parity

Status: `DONE — PUBLISHED`

Exact base: `c098b2e1bb02b0e73d34909da41dacdef0e82a08` on
`origin/main`.

## Objective card

- Objective: make the production self-host source-to-MIR entrypoint admit a
  direct zero-argument `ListNew()`, `QueueNew()`, or `SetNew()` call when the
  enclosing function parameter supplies the matching concrete collection type,
  matching native contextual typing before verified MIR publication.
- Priority order: preserve exact native type identity; consume the existing
  contextual builtin owner; cover its complete List/Queue/Set constructor
  family; retain named-boundary policy; reject constructor-family mismatch;
  avoid a call-checker-local allowlist or generic Unknown coercion; do not copy
  the native pipeline's observed wrong-family acceptance defect.
- Fact owner: `ast_contextual_builtin_type_owner.pgy` joins a direct call-spine
  identity and the carried expected type through
  `SemanticContextualBuiltinReturnTypeOpt`. Parameter type identity remains
  owned by the admitted signature facts.
- Last legitimate consumer: `SemanticAstExpressionVerdict` selects the
  graph-owned contextual call-argument capability, then
  `SemanticExpressionGraphConcreteScalarValueError` performs assignability
  against the same carried parameter type before the body bundle can admit MIR
  publication.
- Forbidden fallback: treating `List<Unknown>`, `Queue<Unknown>`, or
  `Set<Unknown>` as assignable to any expected type; exact constructor-name or
  collection-name checks in the call checker; copying initializer-only policy;
  using argument source text as final identity; widening nested/member/computed
  calls; accepting a wrong constructor family; or relying on backend rejection.
- Verification gate: native and installed self-host admit matching fresh
  List/Queue/Set call arguments and publish C/MIR; List/Queue `own` controls
  cross the existing named-boundary exception; self-host keeps a mismatched
  collection family fail-closed; both pipelines reject nonzero constructor
  arity without artifacts; structural ratchets require the contextual owner and
  prohibit an Unknown/expected equality fallback.

## Reproduced executable gap

- Native admits fresh `ListNew()` and `QueueNew()` at matching `own`
  parameters and publishes C. Installed self-host rejects before MIR with
  `call_arg_type_mismatch`, observing `List<Unknown>` / `Queue<Unknown>` instead
  of the carried `List<Int>` / `Queue<Int>` parameter types.
- Native admits `SetNew()` at a matching default `Set<Int>` parameter.
  Installed self-host rejects before MIR with the same diagnostic and
  `Set<Unknown>` actual type.
- Native currently also admits `QueueNew()` at a `List<Int>` parameter. The
  canonical self-host contextual owner correctly rejects that family mismatch;
  this rung keeps the self-host fail-closed result instead of treating native
  unsoundness as the parity oracle. Native and self-host both reject a nonzero-
  arity `ListNew(7)` call without artifacts.
- The contextual builtin owner already validates zero arity, exact nominal
  family, one concrete type argument, and absence of unresolved Unknown. This
  rung migrates the call-argument consumer to that owner; it does not invent a
  second constructor policy.

## Edit scope and integration

- Allowed implementation: the existing contextual builtin type owner, its
  initializer consumer rename, and the existing concrete-scalar call-argument
  verdict consumer.
- Allowed tests: matching List/Queue/Set fixtures, one wrong-family negative,
  one arity negative, one consolidated parity gate, and minimum
  Make/owner/registry references.
- Allowed coordination/evidence: this directive, current collaboration and
  handoff cards, one dated audit, and official generated language inventory
  only through its generator.
- Forbidden overlap: named-value boundary classification, Slice, nested
  contextual propagation, member/computed constructor calls, general generic
  inference, collection ABI/lowering, the pre-existing source-LLVM intent
  Option gap, and unrelated BRIDGE rows.
- Integration owner: the primary task at this exact base.
- Integration gate:
  `tests/self_hosted/parity/direct_mir_contextual_collection_constructor_argument_owner.sh`.

## Local closure evidence

- A current-source DRV-2 was rebuilt and installed through the Pergyra typed-
  source bootstrap path. The consolidated gate passes matching List, Queue,
  and Set cases and retains wrong-family and nonzero-arity rejection without
  artifacts.
- The changed concrete-call and expression-verdict owners emit native C and
  LLVM IR with zero diagnostics. The component source inventory and removed-
  path ratchets pass after replacing their two obsolete concrete-only selector
  strings. SoT edge remains green at `88` authorities, `183` carriers,
  `CLOSED=55 BRIDGE=32 ACTIVE=1`.
- The local initializer projection gate fails at
  `identity-bound callable C binding is missing`. Reverting all four semantic
  source files byte-for-byte to the exact base reproduces the same failure with
  the freshly rebuilt GCC 16 native compiler, so it is not recorded as a
  regression, pass, or skip for this slice. Exact implementation-head Windows
  CI passes and therefore supplies the cross-toolchain falsifier. Local
  Coq/Rocq is also unavailable; no missing-prover bypass was used, and the Rocq
  9 CI job passes.

## Publication evidence

- Implementation `e3905894d4800d47ec87f2c008ebac1b37014aeb` and generated
  inventory correction `33701ca8937ee325aadf2f7840e1ac77000b0ad9` are on
  `origin/main`.
- Implementation run `33273587010` exposed only the generated language-word
  inventory drift in Linux step 14. Windows, TSan, sanitizers, macOS C-only,
  Rocq 9, codegen bootstrap, backend toolchain, and comparison shards 20/20
  passed. Its initially cancelled full self-host job was rerun alone in attempt
  2 and passed in about 41 minutes.
- Corrected exact-head run `33274558821` completed green in 54 seconds through
  the docs-only classification path. The implementation lease is released;
  neither the broader expression-surface `BRIDGE` row nor the 83% project
  forecast changes, and no successor rung is inferred.

This directive is a temporary coordination artifact. It does not own semantic
status, registry census, project percentage, remote publication, or a successor
implementation rung.

# Value-Parameter Collection-Mutation Public JSON Diagnostic Receipt

Status: DONE — PUBLISHED; EXACT CI GREEN

Exact base revision: `f7800aed4b710890aca3df13e2b38cc280badddd`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, or completion claim.

## Shared objective card

- Objective: make both language-contract mutation paths—collection builtin and
  direct array index assignment—reach the existing Pergyra-owned
  `value_param_collection_mutation` verdict, then publish its exact public JSON
  identity through installed MIR, C, and LLVM requests instead of letting the
  direct assignment bypass policy and the owned verdict produce an empty
  private receipt plus generic C transport failure.
- Priority order: preserve the Pergyra ownership/effect code and exact
  function/parameter/mode facts; add the missing direct-ArraySet executable
  fixture; carry the native-agreed public identity for this exact code; reuse
  shared wire/process owners; keep other same-identity codes fail-closed.
- Fact owners: `src/self_hosted/semantic/collection_mutation_policy_owner.pgy`
  continues to own value-parameter mutation policy and
  `src/self_hosted/semantic/diagnostic_code_owner.pgy` owns the
  `PGY_SEM_BUILTIN_ARGS_INVALID` projection. The semantic public-receipt owner
  will own cause `semantic:builtin:signature_mismatch` and fix
  `match-builtin-signature` for exactly `value_param_collection_mutation`.
  The shared public wire renderer remains the sole serialization owner.
- Production entrypoints: public `pgy --mir --error-format=json SOURCE`, public
  C compile/artifact JSON requests, and public LLVM compile/artifact JSON
  requests.
- Direct bypass to delete: `ast_assignment_type_fact_owner.pgy` currently
  verifies indexed assignment without consulting the existing collection-
  mutation policy, and the owned builtin-path verdict emits an empty private
  JSON receipt followed by a generic C-side malformed/missing diagnostic.
  Native compilation is not an acceptable replacement.
- Last legitimate consumers: the assignment type owner and statement path
  consume the existing mutation-policy result; the installed Pergyra semantic
  verdict is then consumed by the existing MIR/artifact diagnostic process
  owner and public stderr boundary.
- Forbidden fallback: C semantic mapping, native retry or preflight, message
  parsing, grouping by `PGY_SEM_BUILTIN_ARGS_INVALID`, co-admitting
  `call_arity_mismatch`, co-admitting `option_concrete_type_required`, changing
  Pergyra text codes or facts, weakening the `inout` language contract, a
  second semantic pass, invented source location, partial wire output, or
  another serializer.
- Focused gate:
  `tests/self_hosted/parity/public_value_param_collection_mutation_json_diagnostic_receipt_owner.sh`.
- Falsifying cases: existing `bad_value_param_arraypush.pgy` and a new direct
  index-assignment fixture must retain exact normalized Pergyra diagnostics
  with `ArrayPush`/`ArraySet`, parameter `xs`, and mode `default`; installed
  MIR/C/LLVM requests must relay exact receipts on stderr without stdout,
  artifact, or native timing; wording drift must not change identity; all
  three `call_arity_mismatch` fixtures must remain unadmitted despite sharing
  the same native public identity.

## Opening evidence

- `bad_value_param_arraypush.pgy` exits nonzero with exact Pergyra code
  `value_param_collection_mutation` and facts `func: ArrayPush`, `param: xs`,
  `mode: default`. Its private JSON response is whitespace-only and public MIR
  JSON stops at `pgy: self-host JSON diagnostic receipt is malformed`.
- Explicit native JSON fixes code `PGY_SEM_BUILTIN_ARGS_INVALID`, stage
  `semantic`, layer `type`, cause `semantic:builtin:signature_mismatch`, and
  fix `match-builtin-signature` for this production fixture.
- `docs/semantics/16_language_contract_golden_spine.md` declares builtin
  mutation and direct array index assignment as the same default-value-
  parameter prohibition. A new direct-index fixture proves the current
  Pergyra assignment path incorrectly succeeds and emits MIR, while explicit
  native JSON rejects it with the same builtin-signature identity and a source
  location. This is an executable semantic bypass, not merely missing fixture
  coverage.
- Builtin and user-call arity fixtures retain the distinct Pergyra code
  `call_arity_mismatch` while explicit native JSON gives all three the same
  public identity. They are the exact-code negative ratchet, not an admission
  group.
- SoT opens unchanged at `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. `diagnostic.catalog` remains `BRIDGE`; project forecast remains
  83%.

## Local implementation evidence

- `ast_assignment_type_fact_owner.pgy` now sends indexed assignment through
  the existing collection-mutation policy as `ArraySet`; it does not recreate
  parameter-mode or collection policy. The old direct-assignment success path
  is therefore gone for the measured fixture.
- A fresh Pergyra-built DRV-2 produces the exact
  `value_param_collection_mutation` verdict for both `ArrayPush` and direct
  `ArraySet`, including `xs` and `default`, and the focused gate relays the
  exact public identity through MIR, C, and LLVM while all three arity
  siblings remain fail-closed.
- Rebaselined return, logical, let, assignment, call-argument, and builtin-
  argument receipt gates pass independently. The complete installed-driver
  aggregate also passes through artifact, parser, tokens, AST, LLVM IR,
  native opt-in, REPL, formatter, and DeviceSlot boundaries.
- Diagnostic, SoT, Gate, protocol, source-inventory, substitution-velocity,
  hard-contract, and progress ratchets pass. SoT remains `88/183`,
  `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9 blockers; implementation volume is
  21.23%. The broad component inventory and exact 30-job CI remain publication
  evidence and are not claimed locally.

## Publication evidence

- The implementation sequence is published through
  `4b5e9c8fbc9cf273b0439a21ceac9ab1f3ec97d5`; the bounded fuzz audit is the
  separate read-only commit `fb85644d` and owns no part of this rung.
- Exact-head CI run `33578028783` is green 30/30 with backend shards 20/20.
  `build-linux` passed in 24m21s, including the generated language-word
  inventory and structural component contract.
- Full self-host passed in 33m33s and records exactly one 173286-line
  `gen2 == gen3` fixed point, receipt-bound driver adoption, Pergyra-built
  DRV-2 installation, the focused ArrayPush/ArraySet receipt marker, and the
  complete installed-driver CLI aggregate.
- Exact CI first exposed two supporting-inventory omissions: the generated
  language-word implementation census did not include the new fixture, and
  the semantic fixture frontier retained independent 114-row structural and
  progress counters. The owner-generated inventory and all three frontier
  ratchets now agree on 115; no semantic fallback was added to repair CI.
- This directive is DONE and authorizes no successor rung. A successor requires
  a fresh production executable falsifier; delegated parser fuzz findings
  remain read-only audit evidence until separately leased.

## Coordination bounds

- Independent edit scope: the assignment-type consumer of the existing
  mutation policy, one direct-ArraySet semantic fixture and expected
  diagnostic, the semantic public-receipt and contract owners, one focused
  parity gate, its Make/installed-aggregate/component wiring, affected
  admitted-family negative fixtures, the `diagnostic.catalog` evidence row,
  the language-contract golden wiring if required, and current coordination
  snapshots.
- Forbidden overlap: no other task may edit or publish this executable rung.
  C diagnostic transports, the mutation-policy decision itself, native parser
  fuzz repair, stable public wire schema, and protected untracked paths are
  read-only. The assignment consumer may call the existing policy but may not
  recreate its mode/type decision.
- Allowed budgets: focused source/static checks within 60 seconds, focused
  parity within 5 minutes after one required DRV-2 rebuild, and one installed
  CLI integration shard within 30 minutes. Full matrices run only after
  publication.
- Integration owner and gate: the primary task owns integration; the focused
  gate above is the falsifier and
  `tests/self_hosted/parity/installed_driver_cli_mode_owner.sh` is the single
  local integration boundary.
- Outputs are implementation candidates until the primary task observes the
  focused and integration gates. They do not change SoT or progress status by
  themselves.

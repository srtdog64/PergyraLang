# Vessel Method Argument-Type Semantic Admission

Status: ACTIVE — LOCALLY GREEN, PUBLICATION/EXACT CI PENDING

Exact base revision: `6d849b168e616eab63640ac4ca5eefbe97b1e929`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, fuzz priority, or completion
claim.

## Shared objective card

- Objective: make the installed Pergyra semantic graph reject a resolved
  vessel method call whose argument is not assignable to the method signature,
  before MIR or C/LLVM artifact publication, while preserving a valid
  same-type call.
- Priority order: consume the existing resolved member target and signature
  facts; validate arguments independently of the call return shape; preserve
  one exact member-call diagnostic identity without changing the already
  published general-call identity; retain valid MIR/C behavior and fail-closed
  missing facts.
- Fact owners: `ast_expression_call_target_fact_owner.pgy` retains the resolved
  `V.F` target and receiver `parameter_offset=1`; the existing function
  signature facts retain `self` and `x: V`; the resolved-call argument verdict
  in `ast_expression_graph_concrete_scalar_verdict_owner.pgy` owns typed
  argument checking; `ast_expression_verdict_owner.pgy` is the last
  orchestration consumer before MIR admission. The existing diagnostic code
  and public-receipt owners retain the distinction between general and member
  call identity; the shared public wire renderer remains the serializer.
- Production entrypoints: installed public/self-driver MIR diagnostic, public
  C and LLVM artifact requests, and their explicit-native parity oracle.
- Direct bypass to delete: graph argument checking currently executes only
  when a call's return value is concrete scalar or has a contextual builtin
  argument. A `Void` vessel method therefore falls through to the legacy text
  checker, which does not own member target spelling and silently returns
  success for `v.F(0)`.
- Last legitimate consumer: `SemanticAstExpressionVerdictFromGraph` consumes
  the existing target/signature facts and must reject the call before the MIR
  builder or artifact process can observe it.
- Forbidden fallback: text/member-name reconstruction, AST/program-root
  rescanning, a second vessel signature table, C semantic mapping, native retry
  or preflight, message parsing, backend-only rejection, changing receiver
  carriage/lifecycle semantics, weakening valid nominal calls, or admitting a
  broad unsupported argument shape merely to reach this fixture.
- Focused gate:
  `tests/self_hosted/parity/vessel_method_argument_type_admission_owner.sh`.
- Falsifying cases: a vessel `V` with `F(self, x: V) -> Void` must reject
  `v.F(0)` as `member_call_arg_type_mismatch` with expected `V` and actual
  `Int` in direct self-driver and public MIR/C/LLVM requests, with no artifact
  or native timing. Its public identity must exactly match native
  `semantic:call:arg_type_mismatch` / `align-arg-type`, while the existing
  general `call_arg_type_mismatch` keeps assignability identity. `v.F(x)` where
  `x: V` must remain valid in public/native MIR and public C. A non-graph-owned
  composite argument must retain its existing fallback rather than being
  guessed into this owner.

## Opening evidence

- At exact base, public and native AST both accept the same vessel source.
  Public MIR and direct text/JSON DRV-2 modes exit zero and publish `v.F(0)`;
  public C exits zero and publishes an artifact.
- Explicit-native MIR and C both reject before artifact with exact identity
  `PGY_SEM_TYPE_MISMATCH`, cause `semantic:call:arg_type_mismatch`, and fix
  `align-arg-type`; the message fixes expected `V` and actual `Int`.
- The same source with `let x: V = V(); v.F(x);` succeeds through public/native
  MIR and public C. Parsing, target resolution, receiver carriage, and valid
  emission are therefore controls rather than repair targets.
- Source inspection confirms the resolved target already returns `V.F` with
  parameter offset one. The existing typed graph error path validates arity
  and argument assignability, but its caller gates it on concrete-scalar return
  ownership. The text fallback cannot recognize the member source name.
- A fresh general-call measurement fixes an important identity boundary:
  `bad_user_arg.pgy` uses public/native cause `semantic:assignability_check`
  and fix `annotate-or-convert`, while this vessel method uses call-specific
  cause/fix. Reusing the same owned code would create dual public authority, so
  the existing diagnostic vocabulary now names the member case separately;
  neither message wording nor callee spelling chooses the public identity.
- SoT opens unchanged at `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. This is one executable consumer migration inside the active
  semantic/self-host rows; no registry row is declared closed. Implementation
  volume opens at 21.25% and project forecast remains 83%.

## Coordination bounds

- Independent edit scope: the existing resolved-call argument verdict and
  expression verdict consumer, one bad/control parity fixture pair, one focused
  production gate, Make/installed-aggregate/component wiring, the applicable
  SoT evidence row, and current coordination snapshots.
- Fuzz scope is independent and read-only. The delegated agent must exclude
  this vessel method axis plus prior SetSize/binop/parser-enum axes, and may
  write one uncommitted `docs/audits/` report only. It owns no implementation,
  SoT, registry, lease, CI, publication, or priority decision.
- Forbidden overlap: no other task may edit or publish this executable rung.
  C diagnostic transports, native semantic owners, receiver ABI, protected
  untracked paths, and the published diagnostic receipt schema are read-only.
- Allowed budgets: focused source/static checks within 60 seconds, focused
  parity within 5 minutes after one required DRV-2 rebuild, and one installed
  CLI integration shard within 30 minutes. Full matrices run only after
  publication.
- Integration owner and gate: the primary task owns integration; the focused
  gate above is the falsifier and
  `tests/self_hosted/parity/installed_driver_cli_mode_owner.sh` is the single
  local integration boundary.
- Outputs are implementation candidates until the primary task observes the
  focused and integration gates. Fuzz observations do not reorder this active
  rung and neither output changes SoT or progress status by itself.

## Local implementation evidence

- `SemanticExpressionGraphResolvedMemberCallArgumentsOwned` admits only a
  resolved member target with an exact arity match and graph-owned arguments.
  It consumes the existing target/signature/receiver-offset facts and invokes
  the existing recursive argument error traversal even for a `Void` return.
- A member assignability failure emits the separately registered
  `member_call_arg_type_mismatch`. The semantic public-receipt owner maps that
  exact code to the native call-specific cause/fix; the pre-existing general
  `call_arg_type_mismatch` mapping and all thirteen regression contexts remain
  assignability-owned.
- Adding the vocabulary row exposed a stale executable count ratchet: the code
  owner already contained 38 rows while the contract still asserted 36 after
  earlier zone-code additions. The executable contract and its two structural
  consumers now assert the observed 39-row owner. The rebuilt driver exercises
  that contract instead of preserving a false historical count.
- A fresh Pergyra-built DRV-2 was installed at SHA-256
  `45D3EF8DFC97031317DF106F7E14A17B5A383961F761746131CF7FA12BE26014`.
  The focused vessel gate passed in 4.1 seconds. The existing thirteen-context
  general call-argument receipt gate, diagnostic registry, compiler-world
  contract, and complete installed CLI aggregate all pass.
- The broad component inventory was stopped after exceeding its 60-second
  local budget; it had no failure output before termination. Its exact static
  inventory remains required in exact-head CI. SoT status and project progress
  counts remain unchanged pending publication evidence. The implementation
  volume ratchet now measures 21.29%; the project forecast remains 83%.

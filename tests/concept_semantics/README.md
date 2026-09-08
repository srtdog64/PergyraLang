# Concept deletion and strengthening tests

These are bounded source rewrites and semantic controls, not removal of a
concept from the compiler and not a proof against every possible encoding.

Run the currently supported contracts with installed binaries:

```sh
bash tests/concept_semantics/run.sh
```

The four lanes compare typed Intent terminal attribution, capability/effect/
participant authority, domain call/binding contracts, and immutable object/
struct behavior. Native C/LLVM execution, self MIR fact carriage, and self
capability-manifest checks have separate scopes. See each runner and the
[integrated audit](../../docs/audits/2026-09-05_language_axes_semantic_integration.md).

Run the **open self-host source-admission claims** separately:

```sh
timeout 300 bash tests/concept_semantics/source_admission_parity.sh
```

This command is a bounded set of claims, not whole-language admission parity.
It checks owned refusals before MIR publication, typed Intent v3 plan carriage,
and valid nominal/lexical-local behavior in native/public C and LLVM. Exact
results and binary identities belong to the current handoff, not this guide.
Every negative first checks a native semantic diagnostic. Invalid MIR is never
executed; missing tools/timeouts are not successful rejection evidence.

`named_enum_match_admission.sh` extends this boundary to named enums: missing
variants, duplicate arms, nested match identity, default scope and payload
arity. Complete/default controls, including payload-free enums and a match
after a loop, execute in all four paths. `enum_match_mir_refusal.py DRIVER`
separately changes return/branch/variant facts in valid MIR, then requires
owned C/LLVM projection refusal with no output. It never compiles or executes
the mutated inputs. These gates do not claim coverage for Option/Result or
all finite-pattern forms.

`resilience_admission.py NATIVE DRIVER` checks that unavailable retry execution
fails before MIR publication, malformed attempt counts are parse errors, and
reserved timeout/backoff modifiers report a diagnostic. A no-retry typed Intent
remains a positive admission control. `intent_retry_admission_fact.pgy` validates
the public receipt and missing/crossed owner facts without running input code.
The keyword registry keeps these spellings for parsing/diagnostics with support
mask zero; this is not an implemented retry policy.

`action_authority_admission.py NATIVE DRIVER` compares action declaration
contracts: self/subject parameters, within-only compatibility, explicit
authority, wrong bindings/types/zones and diagnostic-bearing func-within
refusal. It never executes rejected sources. The source integration gate also
executes the valid uncalled-action control on native/public C/LLVM.
`action_authority_admission_fact.pgy` independently checks the receipt and
crossed callable/authority identities. A declaration may be compatible with
any explicitly authorized slot of its binding's type; an actual participant
still needs exact slot attribution at the call/Intent boundary.
The same owner checks required ability implementations, preserving ordered
generic tuples and declaration-owned concrete defaults. A default that refers
to an ability formal (for example `U = T`) is a native rejection control, not a
supported substitution rule. Role MIR publication resolves the base ability
declaration but retains the exact implementation tuple. This gate does not
claim module-visibility or general receiver-backend closure.

`future_lifecycle_admission.py NATIVE DRIVER` compares source admission for
owned Future/RemoteFuture retirement: scope/return/loop exits, branch and match
joins, short-circuit operands, explicit own-parameter transfer, aliases and
mutable bindings. Cancellation is an observation, not a join; Cancel-then-await
is positive, Cancel-only is negative. Wrong arguments and ownership-bearing
payloads require their own diagnostics. Primitive kind names are mechanically
projected from the native type owner, not another copyability registry.
`future_lifecycle_admission_fact.pgy` runs the same common body validator and
checks the diagnostic receipt. Neither command executes source inputs.
Keep same-spelling nested Future and skipped/taken logical operands as native
comparators too; these exposed stable resource identity and conditional
consumption bugs in native admission. A source-admission pass alone does not
claim native/public asynchronous runtime parity.

`zone_rule_admission.sh` checks exact approving subject-slot identity, including
same-type and same-name slots in other zones, plus effect/relation declaration
kind. It preserves both explicit-authority and authority-free valid sources.
`tests/self_hosted/parity/zone_rule_admission_owner.sh` executes the validator
on synthetic typed facts. Neither command executes invalid source programs or
claims complete domain/lifecycle semantics.

`generic_bound_admission.sh` checks inferred/explicit actuals, conjunctive
bounds, exact role implementations, and forwarding of a caller's formal
bounds. It also executes valid controls in native/public C/LLVM and remains
RED when a backend cannot execute them. The synthetic
`tests/self_hosted/parity/generic_bound_admission_owner.sh` separately checks
binding witnesses, public diagnostic identity and stripped/missing bounds.

The focused `field_write_admission.sh` requires the owned immutable-field
diagnostic, then executes valid mutable/construction/inout/nested controls in
native/public C and LLVM. It does not mark the remaining source-admission
claims green. `tests/self_hosted/fixtures/nominal_field_write_fact.pgy` checks
typed field-mode provenance, missing/invalid rows and artifact drift; its
native C execution passed, while native LLVM requires an identifier receiver
for `ArrayPop` and public C rejects the member `facts.field_write_modes` as
`undefined_symbol`. Those reverse-admission discrepancies remain open rather
than rewriting the probe to hide them. The full focused field gate also keeps
public LLVM's mixed/nested-record refusals RED.

`capability_admission.sh` compares owned declared-capability refusals, valid
source admission, C/LLVM execution and inspection-manifest masks separately.
It checks direct/transitive/forward calls, inferred and broad declarations,
recursion, dynamic FileOpen modes and synthetic graph tails. A manifest may
report masks with an error status; that does not authorize executable output.
The existing capability fact is now produced by the common body bundle, not
recomputed by the manifest renderer. Callable attribution has its own gate below;
runtime emission failures remain open claims, not successful rejection evidence.
`tests/self_hosted/parity/capability_admission_fact_owner.sh`
executes eleven identity/missing-row/diagnostic controls in native C only;
it is not public C/LLVM or full-bootstrap proof.

`intent_capability_admission.sh` checks purpose-local `on`, `expect`,
compensation and nested-Intent capability propagation, plus valid and unused-
purpose isolation controls. Both public backends must reject an invalid bound
before executable publication. Native false acceptance or an unrelated native
refusal remains RED. The separate `intent_capability_fact_owner.sh` runs ten
native-C controls for purpose identity, aggregate masks, crossed/missing rows
and invalid declared masks; it does not prove typed Intent plan publication.
The capability runners require the selected driver's machine-manifest companion;
a missing packaged input is a setup error, not a semantic test outcome.

`callable_capability_admission.sh` asserts valid and invalid callback uses,
forwarding, recursion, higher-order dispatch, call-site isolation and unused
function values. An intermediate function's explicit capability bound must hold
even when its caller permits more. Native and self outcomes are each compared
to that contract; native false acceptance and builtin-shadow typing errors stay
RED. No invalid output is executed. `callable_capability_fact_owner.sh` separately
checks exact masks, deferred templates, unique instantiations and missing/wrong
actual identities in native C. That witness is not public execution evidence.
`callable_capability_execution.sh` executes only valid composition, recursive
forwarding and higher-order dispatch sources in native/public C and LLVM.
It keeps nested callable-signature lowering failures visible even when both
frontends publish MIR. A successful admission gate is not backend completeness.

`effect_admission.sh` checks declared-effect refusals, independent capability/
effect attribution, forward/formal calls, builtin-shadowing and effectful
arguments. Scalar and collection positive controls must also execute equally
in native/public C and LLVM; a fixed effect row alone cannot admit a source or
prove a working backend. `effect_call_fact_owner.sh` tests the Pergyra fact
implementation separately, including unknown targets and missing policy rows.
`effect_builtin_normalized_owner.sh` checks checked-Int and in-place array
transform ABI/type/storage facts, including forged graph IDs and absent facts.
Execution controls also retain returned-array alias observations, empty/single
element transforms and legal i32 boundary arithmetic.
`effect_builtin_execution_owner.sh` independently emits C/LLVM from each
positive source's admitted MIR and executes both artifacts.

`hashmap_admission.sh` checks contextual constructor types, receiver/key/value
arguments, nested calls and mutation modes before publication. Its valid
C/LLVM controls cover all four scalar key types, replacement/removal, sorted
key snapshots that survive mutation, a callable formal named MapNew, and
exactly-once source-order evaluation of effectful key/value operands.
`hashmap_signature_fact_owner.sh` separately executes the Pergyra signature
specialization against the shared collection-call protocol and checked native
key-policy projection. `hashmap_runtime_owner.sh` checks the generated native
key/raw ABI projection and normalized-call mutations (wrong key/value/result,
missing context, forward operand and receiver identity). These fact checks are
not full-bootstrap evidence. Current
binary hashes, admission/execution results and remaining failures belong in the
[single work snapshot](../../docs/current_work_handoff.md), not this guide.

The [full-vocabulary experiment intake](../../docs/audits/2026-09-06_language_word_deletion_intake.md)
reopens every registered language word and each grammatical context, including
previous KEEP-CORE concepts. The four lanes above are not that complete census.
Spelling removal, source rewrite, ordinary library encoding and deletion of a
compiler mechanism are different interventions. A library is not disqualified
merely for retaining the same information in structs, enums or receipts.
Compare static rejection, effects/alias, failure/cleanup phases and boundary
costs as well as output. Six native MIR/admission rechecks in the intake are
not runtime or self-host parity, and the earlier in-memory LSP batches have
not been preserved as the same completed fixtures.

The later [executed word-deletion matrix](../../docs/audits/2026-09-06_language_word_deletion_execution_matrix.md)
adds 35 bounded experiments and 104 durable programs under `word_deletion/`.
Its historical installed-binary native/public C records cover all 104 programs, with 34 outcome
differences. Primary independently reproduced 22 outcomes for the enum-match,
capability-bound and subject-parameter experiments; see the intake follow-up.
`word_deletion/run_matrix.py` is a manual observation collector, not an
expected-result gate: exit zero alone does not validate recorded outcomes,
and its timeout/missing-executable summaries are not semantic rejection or
equivalence evidence. Full LLVM and per-word/context equivalence remain open.

`world_zone_admission.sh` checks live world-zone value escape, real detached
Clone, ordinary member reads and lexical Clone shadowing. Its callable-alias
negative must retain the world claim, not merely any earlier diagnostic.
`typed_intent_publication.sh` sends native/public source MIR through the
existing executable v3 machine-admission probe. It covers a single step and
a predecessor/compensation chain, then removes the plan or crosses actual
terminal/completion instructions without changing the plan digest. These
checks establish publication/cross-seal, not C/LLVM runtime equivalence.

These are manual focused entrypoints, not newly wired CI jobs. The supported
runner's green result must not hide the second command's red result. Fix one
named production owner/consumer seam at a time; do not add a blanket native
retry or weaken the rejection assertions to make this suite green.

`identity_cell_receiver_execution.py NATIVE DRIVER` checks retained/uncalled
actions, repeated receiver mutation, two owners with the same method name,
Int/Bool/String fields, explicit arguments and both branch outcomes. It also
checks frame-owned Zone subject slots (two/three fields, equal-valued distinct
backing, declaration order and ordinary scalar-call composition). The probe
tests slot ownership directly after graph admission, so checksum rejection
alone cannot satisfy the shared/borrowed-backing counterexamples. Readonly Zone
calls and reborrows with mixed scalar/Zone parameters retain the same storage;
crossed carriage/pass/resource/layout facts and returned cells must refuse.
The probe checks value-carriage/return refusal at the lifetime owner itself.
Zone sync and Intent protocol execution remain separate obligations. Native
and public MIR enter the same C/LLVM GraphPlan issuer. Crossed owner, routine,
formal, field and call facts are admission-only controls, never executions.
`--shared-only` skips production public execution during an owner edit loop;
it must not be reported as installed-driver or full source-pipeline parity.

`loop_statement_execution.py NATIVE DRIVER` checks the typed synthetic `loop`
condition, while equivalence, break/continue, nesting, early Void returns,
false conditions, canonical empty Void bodies and the retained String branch. Both source pipelines and
both MIR producers exercise C/LLVM; invalid conditions, body types, unretired
tasks and crossed MIR return facts are refusal-only controls. Its shared-only
mode has the same restricted meaning as the receiver gate above. The separate
String parity script's structural cap/hash checks are not replaced by this gate.
`--probe=PATH` reuses an already-built admission/projection probe and records its
hash. The report separately lists condition mutations that cannot be expressed
after native constant folding; omitted mutations are not passing checks.

`unsafe_block_execution.py NATIVE DRIVER` checks bare unsafe statement effects,
lexical shadowing, loop/return flow, transitive/callable effects, unused bodies,
parallel refusal and Future scope exit. Invalid source stops at admission.
`--admission-only` builds the existing common body-verdict probe, not a full
production driver. Its additional Float literal/body controls are C-only
regressions for the unclaimed emission family, not Float GraphPlan/LLVM claims.
The public identity-required scalar C family consumes shared LocalRefs before
MIR-to-AST could flatten distinct bindings, including formal/local collisions.
Native same-spelling SSA now carries the semantic declaration ID through HIR
and MIR. The gate retains nested, initializer, parameter, branch and mixed-type
shadows, plus unreachable-tail flow. The companion lexical owner smoke is only
a structural residue check. Current production failures remain recorded in the
handoff; this command has no expected-failure waiver.
Primitive inout controls include branch/unchanged/typed shadows, nested calls,
return-call ordering and value input passed to an inout callee. Int, Long, Bool
and String use the admitted formal's copy-in/copy-out storage boundary.

`scalar_value_result_identity.py NATIVE DRIVER [PROBE]` is a focused manual
gate, not a new CI job. Fixed valid String/Int-shadow controls execute through
both source pipelines and both MIR producers' shared C/LLVM projections. MIR
mutations are admission-only: formal/local/foreign-owner identity, entry SSA
uses, carriage, layout and source-local inventory. Matching names and types do
not authorize crossing a formal's storage identity. All artifacts and inputs
are hashed in its report; the optional probe is compiled from the existing
receiver entrypoint when omitted.

`generic_instantiation_execution.sh` checks multi-type forwarding, same-instance
recursion and finite constant-type recursion in native C/LLVM and public C.
Constructor-expanding recursion is a public-C refusal-only control. Public LLVM
is still an execution obligation in `generic_bound_admission.sh`; it is not
silently counted as covered by the forwarding gate.

`binary_evaluation_order.py NATIVE DRIVER [--binary-only]` is a manual
source-execution gate with independent expected observations. It checks the
eager scalar/String and lazy-expression boundary on native/public C/LLVM; the
default also retains the inline and separate-statement Intent observations.
`--binary-only` selects that named expression rung, not an expected-failure
waiver for Intent. No malformed source/MIR executes. Compile failures, timeouts,
runtime errors and incorrect output remain failed rows in its hashed report.

Dynamic Int division retains the existing checked i32 zero/overflow boundary.
`tests/self_hosted/parity/direct_mir_scalar_int_divide_owner.sh` checks shared
C/LLVM execution, runtime linkage and crossed ABI/type admission. Its normalized
validator checks malformed facts without executing a division or emitted program.

`intent_predicates.py NATIVE DRIVER [--c-rung]` checks pre-failure, invariant
pre/post failure, placement-before-check and successful action observations.
Duplicate/non-Bool predicates are admission-only. The named C rung does not
waive the default four-leg LLVM obligation. The separate
`tests/self_hosted/parity/intent_phase_carrier_negative_owner.sh` checks missing
or crossed invariant graph pairs and refuses partial artifact publication.

`generic_call_occurrence.sh` checks the public MIR specialization-to-call join:
routine/block/instruction/lane/node anchors, callee declaration identity and
complete occurrence coverage. It tests missing/crossed/duplicated anchors and
production C/LLVM refusal before backend dispatch. Row order and producer-local
provenance renumbering remain invariant. This is identity-carriage evidence,
not proof that the general backend can instantiate every admitted template.

`PGY_BIN` and `PGY_SELFHOST_PREBUILT_DRIVER` select binaries. `PYTHON_BIN`
selects the Python JSON validator. Git Bash is required on this Windows host.
Each lane has a five-minute outer budget in the supported runner; the open
admission command has a five-minute total budget in the invocation above.
Logs stay under the runner's reported `.tmp/self_hosted/` evidence directory; logs and case
counts are evidence, not semantic owners or self-host replacement progress.

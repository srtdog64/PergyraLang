# Hard Substitution Velocity Process

Status: ACCEPTED / ROUTED  
Date: 2026-07-12

## 1. Current Snapshot

The self-host track has nine ACTIVE blockers in the pre-self-host expansion
ledger. Five are direct substitution blockers and four are process/evidence
blockers.

- Direct substitution blockers: mixed AST-like typed expression rows, stable
  JSON/MIR fact transport, target capability consumption, shared symbol/mangle
  rows, and cross-backend ABI/layout rows.
- Process/evidence blockers: subprocess execution, the Pergyra-owned test
  harness, compatibility evolution consumption, and live AIR evidence
  consumption.
- Implementation inventory is 10.27 percent of the C reference inventory
  (29,514 frontend/backend LOC against 287,395 C-reference LOC, measured
  2026-07-13). This remains an implementation-volume metric, not substitution.
- Released/default replacement is 0 percent. The default `pgy` driver remains
  C-owned.

These numbers have different meanings and must not be collapsed into one
percentage.

## 2. Decision

SoT is a hard-substitution rung condition, not a separate project that must be
globally completed before self-hosting can advance. Actual execution is
expected to expose additional SoT seams. A rung closes only the seam that its
real compiler path reaches; unrelated debt remains in the ledger.

A valid progress unit must do all of the following:

1. Put a Pergyra-written owner on a real compiler path previously owned by C.
2. Consume one owner fact without reconstructing it from text, AST, AIR, or a
   backend-local convention.
3. Fail closed when the required fact is missing.
4. Prove C/LLVM oracle parity for the bounded path.
5. Add a negative ratchet that rejects the removed fallback.

Owner files, gates, documents, and LOC do not increase substitution progress
unless a real compiler path is replaced.

## 3. Work-In-Progress Limit

Only one hard-substitution rung may be ACTIVE at a time. The repository must
not accept more than two consecutive SoT-only commits without either:

- an executable substitution delta on the active rung, or
- an explicit BLOCKED record naming the missing fact, owner, last consumer,
  and falsifying fixture.

The default effort split for this track is:

- 70 percent executable hard substitution;
- 20 percent build and test feedback reduction;
- 10 percent SoT, process, and documentation maintenance.

This ratio is a scheduling guard, not a code-size target.

## 4. Validation Budget

The edit loop uses bounded validation:

| Gate class | Default budget | Action when exceeded |
|---|---:|---|
| Static owner/ratchet gate | 60 seconds | Split the scan or remove redundant work. |
| Focused executable parity | 5 minutes | Reduce the fixture to the active owner seam. |
| Integration shard | 30 minutes | Add impact selection, caching, or bounded parallelism. |
| Full platform/matrix suite | Scheduled or merge gate | Never run after every local edit. |

Raising a timeout is not the first response to a budget violation. The test
must first show why its scope cannot be narrowed or cached.

The integrated driver bootstrap follows this rule: blocking CI compares the
Pergyra-built integrated seed with the native-built same driver on a real
source. Both builds must then produce byte-identical verified MIR for the
TestHarness-owned sample and consume that common fact to byte-identical C. The full-input
stage2/stage3 fixed
point is retained as the explicit
`self-host-driver-bootstrap-full-test-smoke` merge/scheduled proof.
Its prerequisite is the seed-only codegen profile: `gen2` plus the parser AST
producer. The standalone codegen fixed point and breadth run in their own
blocking Linux job rather than being repeated inside the driver job.

## 5. Finite SoT Closure

For an active rung, SoT is closed when:

- one named owner supplies every semantic fact consumed by that rung;
- missing facts fail closed;
- semantic fallback reads are zero;
- provenance-only source reads are explicitly named as provenance; and
- the negative gate prevents the old read path from returning.

This definition is local to the executable rung. It does not claim that every
future compiler feature has already exposed all of its SoT seams.

## 6. Active Rung

The next executable rung is the mixed AST-like expression bridge. Remaining
string-backed expression payloads must move to dedicated typed expression rows
one consumer at a time under C/LLVM oracle parity. The implementation must not
create a second parser, recover expression facts from emitted JSON/text, or
leave `typed ? text` dual-read authority.

The first slice must replace a live expression consumer, fail closed on a
missing typed row, and reject reintroduction of the removed text recovery.

First executable delta, 2026-07-12, retired by the 2026-07-14 cutover:
array-literal bracket recognition and body extraction first moved from codegen
into `SemanticAstLocalBindingFacts`. That intermediate body-string accessor is
now deleted. The parser expression graph owns array roots and ordered element
edges, and hard statement emission consumes only those handles. C-built and
LLVM-built DRV-2 drivers emit byte-identical MIR and C for the focused typed
array fixture; missing or invalid graphs fail closed.

Second executable delta, 2026-07-12: canonical try-expression shape moved to a
semantic owner, local-binding facts capture the operand, and codegen consumes
only that row. The old `ast_text_try_let_owner.pgy` was deleted. Existing
`option_try` and `result_try` fixtures are byte-identical across C-built and
LLVM-built codegen tools and remain run-equal.

Third executable delta, 2026-07-12: `ArrayPush` and `ArraySet` payloads now
flow from `SemanticAstStatementFacts` through the semantic statement codegen
view. The transitional collection AST-text owner was deleted. Four focused
Int/String mutation fixtures are run-equal under C-built and LLVM-built tools,
and their emitted C artifacts are byte-identical.

Fourth executable delta, 2026-07-13: semantic enum names, ordered variants,
and payload arity now travel in `SemanticAstArtifactAnalysis`. `CollectEnums`
consumes only that owner row, and the AST-text enum variant owner is deleted.
The native and self-host parser printers preserve variant parameter types;
188 parser rows are byte-equal on C/LLVM, payload-free enum codegen remains
run-equal, and the codegen parity gate requires both backend-built tools to
reject the TestHarness-owned two-parameter payload enum rather than splitting
its nested comma or erasing arity.

Fifth executable delta, 2026-07-13: nominal names and ordered field rows now
flow from `SemanticAstNominalConstructorFacts` through a fail-closed codegen
view. The mixed declaration owner is deleted, the remaining bridge is explicitly
role-only, and four struct fixtures are run-equal under C/LLVM-built tools.

Sixth executable delta, 2026-07-13: role names, target types, and owned method
`NodeId` rows now flow from `SemanticAstRoleFacts`. Operator binding and
receiver ABI consume those facts; the AST role bridge is deleted and the
TestHarness role fixture is run-equal under C/LLVM-built tools.

Seventh executable delta, 2026-07-13: runtime/header expression usage now
consumes `SemanticAstExpressionSurfaceFacts`. The codegen group vocabulary
remains backend-owned, while atom/value/auxiliary capture and string-safe
call/token queries are semantic-owned. Nine runtime-family fixtures pass under
C/LLVM-built tools.

Eighth executable delta, 2026-07-13: runtime/header type usage now consumes
canonical `SemanticAstTypeSurfaceFacts`; codegen no longer scans arena
type-name rows. C/LLVM parity also locks the explicit LLVM `String` unwrap fact.

Ninth executable delta, 2026-07-13: runtime/header statement-kind usage now
consumes canonical `SemanticAstKindSurfaceFacts`; codegen no longer scans arena
kind rows. The incorrect local `ArrayLiteral` alias for canonical tag 16 was
deleted in favor of `ArrayPopStmt`. The aggregate runtime usage projection now
has no arena/count input, and five kind-driven fixtures pass C/LLVM parity.

Tenth executable delta, 2026-07-13: executable `Main` cardinality and selected
function-node identity now consume `SemanticAstFunctionSignatureFacts`.
Semantic verdict and codegen no longer maintain separate arena scans, and the
selection projection returns `Option<Int>` instead of a `-1` sentinel.
`func_call` and `hello` pass under C/LLVM-built codegen tools.

Eleventh executable delta, 2026-07-13: statement routing now consumes local
binding, assignment, and statement-kind authorities instead of codegen arena
predicates. The statement owner gained `Defer`, `Break`, `Continue`, and
`MatchDefault`; structural `Else`/`Block`/`Then` traversal remains provenance.
Twelve representative fixtures pass under C/LLVM-built tools.

Twelfth executable delta, 2026-07-13: top-level function, nominal, role, and
enum declaration routing now consumes semantic-owned node identity. The four
codegen arena declaration predicates are deleted; seven declaration fixtures,
the payload-enum rejection leg, and the role-operator leg pass under C/LLVM-
built tools.

Thirteenth executable delta, 2026-07-13: ability and event declaration
classification now consumes the canonical semantic node-kind surface. This
widens the existing owner instead of creating routing aliases. The final two
codegen arena declaration predicates are deleted, and event rejection is a
TestHarness-owned negative C/LLVM parity leg.

Fourteenth executable delta, 2026-07-13: `SemanticAstExpressionSurfaceFacts`
now stores normalized atom/value/auxiliary payloads and one compact typed
top-level operator row per lane. `Log` expression emission looks up the atom
row by `SyntaxNodeId`; the role-operator path consumes its additive index and
operator kind, then fails closed if the row does not match the emitted
expression. The migrated function cannot call `FindTopLevelPlus`, and
C-built/LLVM-built codegen tools
remain oracle-equal for the role fixture, including `+` dispatch versus raw
`-`, and both negative declaration legs.
The owner remains `BRIDGE`: value/auxiliary consumers and recursive child
expressions still parse compact text.

Fifteenth executable delta, 2026-07-13: scalar and String value returns now
consume the atom-lane shape row, while ordinary scalar/String local
initializers and assignments consume the value-lane row. The role fixture
proves operator dispatch from both `Log` and `return`; `func_call`,
`string_concat_op`, `c_reserved_binding`, `long_scalar`, and `float_math` pass
under C-built and LLVM-built tools. Indexed collection elements,
Option/Result/struct wrappers, auxiliary lanes, and recursive child expressions
remain the explicit bridge.

Sixteenth executable delta, 2026-07-13: semantic operator rows then distinguished
top-level `||`, `&&`, `==`, and `!=` instead of carrying one ambiguous logical
position. At that intermediate checkpoint `if`/`while` root conditions consumed
those rows through `RewriteBoolWithSemanticShape`; that function could not call
`FindTopLevelOp2`. A shared equality projection kept String and payload-free
enum behavior identical. `bool_logic`, `string_equality`, and
`string_equality_concat` pass C/LLVM oracle parity, including precedence and
String equality cases. The seventeenth delta below superseded this emitter with
the recursive graph owner.

Seventeenth executable delta, 2026-07-13: the semantic expression owner now
materializes stable node handles and child edges for `if`/`while` condition
atoms. `WrapCondWithSemanticGraph` recursively consumes those rows, and its
emitter is ratcheted against `RewriteBool`, `FindTopLevelOp2`, and `Substring`.
The grouped-precedence fixture `(flag || other) && !other` prevents a flattened
graph projection from changing semantics. C-built and LLVM-built codegen tools
emit byte-identical C and run-equal on logical and String-equality fixtures.
This does not close the owner: compact-text-to-graph production must still move
into the parser arena, and non-condition recursive expressions remain bridge.

Eighteenth executable delta, 2026-07-15: scalar `Option<T>`
reassignment now consumes `SemanticAstAssignmentTypeFacts.expected_type_names`
and the semantic expression graph. The assignment emitter is ratcheted against
environment-kind payload recovery, generic text rewriting, and `None` text
inspection. Assignment and statement type rows travel through one borrowed
body-type view so recursive emission does not copy compiler-scale facts.
The language permits only direct synchronous same-parameter `ref` reborrowing;
other helper, parameter, async, return, and channel escape paths remain closed.
The semantic suite passed 2799/2799 and the compiler-scale HIR probe passed.
A focused projection probe now imports the assignment emitter directly and
proves `Option<Int>` and `Option<String>` `Some`/`None` reassignment output under
C-built and LLVM-built binaries. Both binaries also reject a missing semantic
expected type with the owned fail-closed diagnostic. The full 71-fixture
codegen matrix was not rerun, and the released/default replacement percentage
does not change. The broader runner no longer compiles the C codegen owner once
for its fixture manifest and again for the C leg: a dedicated build leg keeps
the manifest-producing binary live and reuses it only under an exact
source-set/tool/compiler fingerprint.

Nineteenth executable active-rung delta, 2026-07-15: fully graph-owned scalar
operator trees now derive their result type from
`SemanticExpressionGraphFacts`. `SemanticAstExpressionVerdictFromGraph`
does not invoke `ExprType(text, ...)` for that capability subset. The focused
initializer-to-MIR probe lowers `seed + 2` identically under C-built and
LLVM-built tools. Its negative preserves the source expression and graph-root
text while replacing only the `2` leaf; both tools reject the canonical graph
identifier fact as `undefined_symbol`, so source text cannot silently restore
the result type. Changing that same leaf to a String produces the canonical
`binop_type_mismatch` from graph child types under both tools. Result typing and
operand diagnostics are therefore graph-owned for this declared scalar
capability subset. Call/member/composite trees remain the explicit text bridge.
This delta does not raise the released/default replacement percentage.

Twentieth executable active-rung delta, 2026-07-15: direct named calls with a
concrete scalar return now derive that return from the graph-owned callee and
canonical callable return table. The initializer-to-MIR probe keeps
`ToIntValue(2)` and its root text unchanged, changes only the graph callee to
the equally shaped `ToTextValue(Int) -> String`, and both C-built and
LLVM-built tools reject the declared `Int` initializer as `let_type_mismatch`.
The positive mode projects `x: Int` into the MIR local row identically under
both backends. Call-argument validation, member/namespace calls, generic and
aggregate return typing, and other composite expressions remain the explicit
text bridge. The expression owner stays `BRIDGE`, and released/default
replacement remains 0%.

Twenty-first executable active-rung delta, 2026-07-15: direct calls whose
return, parameter rows, and argument nodes are concrete scalar leaves now
validate arity and argument types from graph handles. The focused negative
keeps `ToIntValue(2)` and its root text unchanged while replacing only the
graph argument leaf with a String; C-built and LLVM-built tools both emit
`call_arg_type_mismatch`. Unsupported nested, generic, collection,
Option/Result, member, and namespace calls still use the explicit bridge. The
expression owner remains `BRIDGE`, and released/default replacement stays 0%.

Twenty-second executable active-rung delta, 2026-07-15: the same direct-call
consumer now accepts fully graph-owned scalar operator trees as arguments.
Operand diagnostics walk the parser arena's postorder subtree and do not
re-parse source text. The positive `ToIntValue(1 + (2 * 3))` path projects
`x: Int` to MIR identically under C-built and LLVM-built probes; changing only
the graph's `2` leaf to a String fails as `binop_type_mismatch`.

Twenty-third executable active-rung delta, 2026-07-18: self-host call emission
derives `ref` and `inout` argument addressability from the parser expression
node kind plus codegen binding and nominal field facts. The final consumer is
`RewriteSemanticCallArgument`; `IsIdentifier` and dotted-source scans are
forbidden there. Missing node-kind facts fail closed, direct bindings consume
`cbind`/`cref`, and member lvalues require a resolvable nominal field type so
payload-free enum spelling cannot masquerade as storage. Addressability walks
the member receiver graph back to a stable binding; `MakePair().left` therefore
rejects instead of exposing temporary storage. The `ref_param` fixture executes
a direct binding, a forwarded readonly ref, and `pair.left`; the temporary
member reject fixture is the executable negative. The component contract is
the fallback ratchet and codegen parity/bootstrap are the executable gates.
This bounded codegen substitution does not change the released/default
compiler replacement percentage.

Thirtieth executable active-rung delta, 2026-07-18: addressability authority
moved out of codegen. `SemanticAstAnalysisResolveExpressionPlacesFromBody`
classifies each stable expression node after initializer/iteration fixpoint as
value, direct binding, readonly-ref binding, or member place. The classification
is carried in the semantic expression arena. `RewriteSemanticCallArgument`
consumes only that row for `ref`/`inout`; the recursive codegen owner and its
`cbind` lookup are deleted. A missing row mutates the canonical body bundle to
`expression_place_rows` and fails closed. Focused C- and LLVM-built codegen
tools preserve the `ref_param` and `inout_return_forward` outputs and reject
`MakePair().left`. Persisted MIR JSON carriage remains outside this bounded
delta; a consumer that re-runs the semantic body fixpoint receives the same
owner fact, while a future direct-MIR backend must carry the row explicitly.
Parser-canonical root spelling is checked against the surface by
the same compact parser owner instead of being treated as a second semantic
spelling. Arguments containing nested calls, generic/aggregate signatures,
collection or Option/Result policy, and member/namespace calls remain bridged.
The expression owner remains `BRIDGE`, and released/default replacement stays
0%.

Twenty-third executable active-rung delta, 2026-07-15: a concrete direct call
may now consume another concrete direct call as a scalar argument. The
capability and verdict recurse over graph call-spine handles; they do not invoke
the source call parser. `ToIntValue(ToIntValue(2))` projects `x: Int` under
C/LLVM parity. Keeping that source unchanged while changing only the inner
graph callee to `ToTextValue(Int) -> String` fails at the outer call as
`call_arg_type_mismatch` under both backends. Scalar operators containing call
nodes, generic/aggregate signatures, collection or Option/Result policy, and
member/namespace calls remain bridged. The expression owner remains `BRIDGE`,
and released/default replacement stays 0%.

Twenty-fourth executable active-rung delta, 2026-07-15: one concrete scalar
graph capability now composes leaves, scalar operators, and concrete direct
calls. `1 + ToIntValue(2)` projects `x: Int` under C/LLVM parity without
calling source-text call, binary, or result-type recovery. Keeping the source
unchanged while changing only the graph callee to `ToTextValue(Int) -> String`
fails as `binop_type_mismatch` under both backends. The former direct-call-only
verdict owner and names were deleted rather than retained as aliases.
Generic/aggregate signatures, collection or Option/Result policy, and
member/namespace calls remain bridged. The expression owner remains `BRIDGE`,
and released/default replacement stays 0%.

Twenty-fifth executable active-rung delta, 2026-07-15: namespace-qualified
static calls now consume the canonical target already carried on the semantic
call node. `Math.Add(2)` resolves through `Math_Add` and projects `x: Int`
identically under C/LLVM. Keeping source and member graph unchanged while
changing only the carried target to the String-returning `ToTextValue` fails as
`let_type_mismatch` under both backends. The direct-leaf-only type owner and
names were deleted; namespace spelling is not re-flattened by the consumer.
Receiver-bound member calls, generic/aggregate signatures, and collection or
Option/Result policy remain bridged. The expression owner remains `BRIDGE`, and
released/default replacement stays 0%.

Twenty-sixth executable active-rung delta, 2026-07-15: receiver-bound member
calls now resolve their semantic target from the parser-owned member graph,
the lexical receiver type, and owner-qualified callable rows. `box.Get()`
resolves to `Box_Get`; its implicit `self: Box` parameter is represented by
the resolved target's parameter offset rather than counted as a source
argument. Keeping source text unchanged while replacing only the member-node
handle with `Text` resolves `Box_Text() -> String` and fails the declared
`Int` initializer as `let_type_mismatch` under C and LLVM. The callable table
now preserves declaration ownership as `Owner_Method`, and the former
static-call-only type owner/name and codegen-local member view were deleted
instead of retained as aliases. MIR/backend receiver-target carriage,
generic/aggregate signatures, and collection or Option/Result policy remain
bridged. The expression owner remains `BRIDGE`, and released/default
replacement stays 0%.

Twenty-seventh executable active-rung delta, 2026-07-15: the bounded
receiver-member target is now one carried fact from semantic analysis through
self MIR into hard C emission. The expression graph stores target kind and
canonical owner-qualified name together; `box.Get()` carries
`member/Box_Get`, and hard codegen emits `Box_Get(box)` without rebuilding the
name from receiver type plus member spelling. Removing only that carried row
fails at MIR production under C and LLVM. Extending the graph arena exposed an
LLVM-only aggregate `inout` crash, so compact graph construction was split
from fact verification and now threads the six row arrays directly. A static
ratchet forbids `SemanticExpressionGraphArena` or graph-fact aggregates from
returning to that mutable boundary. Generic or chained receivers remain
active, so `selfhost.call_target_identity` is not marked closed. Released and
default replacement stay 0%.

Twenty-eighth executable active-rung delta, 2026-07-15: generic receiver
locals now reuse a typed canonical type-name fact before call-target lookup.
`Box<Int>.Count()` resolves to `member/Box_Count`, reaches self MIR, and hard
codegen emits `Box_Count(box)` without parsing generic spelling or rebuilding
the owner-qualified target. Removing only the carried generic target fails at
MIR production under C and LLVM. The call-target row remains `ACTIVE` because
chained field receivers still require nominal field facts at this consumer;
no string-to-string nominal helper, fallback, or second target owner was added. Released and default replacement
stay 0%.

Twenty-ninth executable active-rung delta, 2026-07-15: chained field receivers
now resolve through expression handles and `SemanticAstNominalConstructorFacts`.
`holder.box.Count()` consumes `Holder.box: Box`, carries
`member/Box_Count` through self MIR, and hard codegen emits
`Box_Count(holder.box)`. Removing only the carried chained target fails before
emission under C and LLVM. Nominal count and field lookup are read-only `ref`
queries, so the aggregate is neither copied nor allowed to escape through a
value helper. Receiver target resolution is now covered for leaf, generic-local,
and chained-field forms. `selfhost.call_target_identity` remains `ACTIVE`
because direct calls still recover final identity from the callee leaf instead
of a carried target row. Released/default replacement stays 0%.

Thirtieth executable active-rung delta, 2026-07-15: direct call targets now
join namespace and receiver targets as mandatory semantic facts. Signature
capture writes `direct/ToIntValue`; constructor and receiver targets are
completed by the semantic body fixpoint after their inventories exist. Self
MIR serializes the target kind as `direct`, the importer validates the same
row, and hard codegen emits from the carried canonical name without reading
the callee leaf. Removing only the direct target fails before emission under C
and LLVM. The focused gate also rejects a `callee_text` recovery in hard
codegen. `selfhost.call_target_identity` is therefore `CLOSED` for the active
self-host expression rung. Generic/aggregate result policy and the wider
expression surface remain bridged; released/default replacement stays 0%.

Thirty-first executable active-rung delta, 2026-07-15: nominal aggregate call
returns now consume the carried target and canonical signature return row.
`MakeBox() -> Box` projects `box: Box` into self MIR and hard codegen emits
`MakeBox()` under C/LLVM parity. Keeping source text unchanged while replacing
only the carried target with `ToTextValue() -> String` fails as
`let_type_mismatch`. The former ambiguous resolved-call type function was
deleted; one broad return owner now feeds an explicit concrete-scalar filter,
and the scalar graph consumer may not index `function_returns` independently.
Generic substitution, collection/Option/Result policy, and composite aggregate
validation remain bridged. Released/default replacement stays 0%.

Thirty-second executable active-rung delta, 2026-07-15: exact-formal generic
return substitution now consumes typed signature rows. `Generic params: <T>`
is a dedicated HIR node kind, and `SemanticAstFunctionSignatureFacts` owns
ordered generic starts, counts, and names. `Identity<T>(value: T) -> T` binds
`T` from the expression-graph argument type and projects `Identity(2)` as
`Int` under C/LLVM parity. A negative keeps source text unchanged and changes
only the carried call target to `ToTextValue(Int) -> String`; both backends
reject the declared `Int` initializer as `let_type_mismatch`. The generic
consumer is gate-forbidden from calling `ExprType` or reparsing provenance.
Nested/composite generic substitution, explicit generic actuals, collections,
and Option/Result remain bridged. Released/default replacement stays 0%.

Thirty-third executable active-rung delta, 2026-07-15: signature capture now
owns a flat parameter/return type-expression arena. The generic call consumer applies
the already inferred exact-formal bindings to that arena, so
`Wrap<T>(value: T) -> Option<T>` projects `Wrap(2)` as `Option<Int>` without
call-site type-text parsing or replacement. The existing exact return and
carried-target negative remain C/LLVM equal. Nested generic parameter
inference, explicit generic actuals, builtin collection/Option/Result policy,
and aggregate validation remain bridged. Released/default replacement stays
0%.

Thirty-fourth executable active-rung delta, 2026-07-15: nested generic
parameter inference now uses the same signature type-expression arena as
return materialization. `First<T>(values: Array<T>) -> T` unifies an
`Array<Int>` graph argument, binds `T=Int`, and projects an `Int` initializer
under C/LLVM parity. The executable negative presents `Int` to the structured
`Array<T>` parameter and requires `call_arg_type_mismatch`. Exact and nested
forms therefore share one owner and one binding path. Explicit generic actual
carriage, builtin collection/Option/Result policy, and aggregate validation
remain bridged. Released/default replacement stays 0%.

Thirty-fifth executable active-rung delta, 2026-07-15: explicit generic call
actuals are parser-owned graph nodes, not skipped punctuation or call-text
payload. The ordered actual list reaches the same signature binding owner used
by inferred exact and nested parameters. A C/LLVM executable pair accepts
`PickSecond<Int, String>(2, "value")` as `String` and rejects
`PickSecond<String, String>(2, "value")` at the argument boundary. The probe
must use `SemanticAstArtifactAnalyzeTyped`; routing through the compact bridge
is a falsifying regression because it drops the generic actual rows. Builtin
collection/Option/Result policy and aggregate validation remain bridged.
Released/default replacement stays 0%.

Thirty-sixth executable active-rung delta, 2026-07-15: scalar Option/Result
builtin calls are graph-policy facts. Initial call-target capture now projects
canonical builtin signatures before initializer typing; the final expression
verdict consumes that carried target, graph argument handles, and the builtin
signature table. The native C oracle and C/LLVM-built Pergyra probes agree for
`Some`, `UnwrapOption`, `Ok`, `UnwrapOr`, `IsSome`, and `IsOk`. Non-concrete
`None`, non-wrapper arguments, and a carried-target mutation fail closed. The
graph owner is gate-forbidden from calling `ExprType` or `CheckCall`.
Collection policy, unknown/aggregate wrapper payloads outside this capability,
and composite aggregate validation remain bridged. Released/default
replacement stays 0%.

Thirty-seventh executable active-rung delta, 2026-07-15: caller-visible
collection mutation admission has one policy owner. Specialized
`ArrayPush`/`ArraySet`/`ArrayPop` statement facts consume it without rebuilding
a call, and general mutator calls consume carried target identity plus the
graph receiver node. The graph call checker is configured not to replay the
source receiver policy. Native C-oracle, C-built, and LLVM-built probes accept
local and `inout` mutation, reject default-parameter mutation, and reject a
carried-target mutation. Collection result/element typing, unknown/aggregate
wrapper payloads, and composite aggregate validation remain bridged.
Released/default replacement stays 0%.

Thirty-eighth executable active-rung delta, 2026-07-15: aggregate field type
validation consumes graph facts. A dedicated field owner projects scalar
operators, direct nominal returns, nested struct values, `Some(struct)`,
wrapper unknowns, and structural `Int`-literal-to-`Long` widening. The struct
verdict no longer calls source `ExprType` or `ExpressionAssignableTo`. Native
C-oracle, C-built, and LLVM-built probes agree on valid and invalid fields and
reject source-preserving leaf type drift plus a missing child fact.
The actual rung-2 driver, built once with C and once with LLVM, also passes all
20 body fixtures and the selected `option_struct_value_flow` MIR fixture. Its
readiness owner names the first failed contract instead of collapsing every
owner failure into one opaque Bool.
Generic/member aggregate field values, object initializers, and special unary
forms remain bridged. Released/default replacement stays 0%.

Thirty-ninth executable active-rung delta, 2026-07-15: explicit top-level
generic calls used as aggregate field values are carried through the hard
DRV-2 path. Native MIR records routine generic formals and canonical call
actuals; the parser graph preserves both `generic_callee` edges and canonical
`Identity<Int>` text. Semantic aggregate validation consumes the graph-owned
generic return fact before legacy text type inference, and codegen emits only
the concrete `Identity_Int` specialization. The specialization producer fills
local row arrays before constructing its immutable fact bundle, preserving C
and LLVM ABI parity. C- and LLVM-built drivers pass all 20 source fixtures plus
the selected generic MIR fixture, including canonical MIR and run-output
parity. Removing the formal or corrupting a generic actual fails closed.
Inferred actuals, member generic calls, nested generic locals, and the other 28
MIR fixtures remain outside this bounded proof. Released/default replacement
stays 0%.

Fortieth executable active-rung delta, 2026-07-15: direct generic calls below
a local initializer root no longer require an explicit `<...>` actual to reach
hard DRV-2. `SemanticAstGenericSpecializationFacts` owns the call-node key,
signature index, and ordered actual type names derived from the typed expression
graph and local semantic environment. Codegen projects only concrete C names
and ABI rows from that fact; it may not rescan the expression graph or infer a
specialization from source spelling. C- and LLVM-built drivers pass all 20
source fixtures plus the selected thirtieth MIR fixture, emit `Identity_Int`,
and reject a graph-only argument-type mutation as `call_arg_type_mismatch`.
Inferred generic calls rooted in returns or assignments, member generic calls,
nested generic locals, and the other 29 MIR fixtures remain outside this
bounded proof. Released/default replacement stays 0%.

Forty-first executable active-rung delta, 2026-07-16: Option try-let lowering
now consumes its operand type from `CodegenExpressionTypeFromGraph` using the
already-carried semantic graph handle. The emitter no longer reads the operand
text or invokes `ExprKind` to reconstruct the wrapper type. Missing graph type
evidence fails closed with the statement provenance, and the component gate
forbids both old reads. The C-built self-host codegen passes all 73 fixtures,
including `option_try`; broader legacy expression-shape result classification
remains bridged. Released/default replacement stays 0%.

Forty-second executable active-rung delta, 2026-07-16: `Array<T>` return
emission consumes the expected-value semantic graph for both array literals and
ordinary array-valued expressions. `EmitReturnValue` no longer trims return
text, tests its first character, or chooses between the legacy array-literal
emitter and `RewriteExpr`. The component gate forbids those four reads, while
`array_return_literal` and the existing `array_param` fixture cover the literal
and variable forms under C-oracle run parity. The C-built self-host codegen
frontier is now 74 fixtures; broader Result and leaf-expression legacy
rewriting remains bridged. Released/default replacement stays 0%.

Forty-third executable active-rung delta, 2026-07-16: `Result<Int>` return
emission now consumes the expected-value semantic graph that already crosses
the hard statement boundary. `Ok(...)`, `Err(...)`, and result-return
expressions no longer enter the legacy `RewriteExpr(rexpr, env)` text scanner.
The component gate forbids that fallback, while `result_int_core` and
`result_try` cover direct wrapper calls and a pipe-bearing `Ok(...)` return
in the C/LLVM oracle parity gate. `result_int_core` is also the thirty-first
DRV-2 MIR producer fixture, so source/MIR emission and missing or invalid graph
mutations exercise the same hard consumer. Other leaf-expression
compatibility rewriting remains bridged. Released/default replacement stays
0%.

Forty-fourth executable active-rung delta, 2026-07-16: arrays whose element is
a declared nominal record now consume a compiler-owned derived ABI fact.
Semantic type-surface usage selects the record element, the type environment
proves that it is declared, and collection emission consumes the resulting C
array type and runtime symbol row. The emitter no longer needs an
`AstExpressionGraphRows` exception. The `nominal_record_array` fixture raises
the C-oracle codegen frontier to 75 fixtures and compiles and runs the emitted
C. Undeclared element types remain fail-closed.

Forty-fifth executable active-rung delta, 2026-07-16: foreach body initializer
typing now consumes the verified loop-binding rows owned by
`SemanticAstIterationTypeFacts`. The base initializer pass still supplies the
collection type needed to prove the loop header; a bounded second pass then
rechecks body initializers with that binding fact. Invalid loop rows are not
injected, preserving the original header diagnostic. This closes the
`diagnostic_catalog_checker` bootstrap failure without rescanning source text.

Forty-sixth executable active-rung delta, 2026-07-17: `Exit(Int)` now carries
its argument from the parser-owned atom graph through statement typing into C
emission. Statement typing rejects a non-`Int` graph with
`call_arg_type_mismatch`; codegen consumes the same graph with an expected
`Int` fact. The statement-payload accessor and `IntEval` recovery path were
deleted, and the component gate rejects their return.

Forty-seventh executable active-rung delta, 2026-07-17: scalar leaf emission
now consumes the parser-owned expression node and the canonical codegen binding
enum, callable-target, or runtime ABI row directly. Bool, String, Int, Long,
bound identifiers, and callable references no longer enter the broad
`RewriteTokens` scanner, including literal-only and higher-order call
arguments. Unknown identifiers and unsupported leaf spellings fail closed
instead of falling back to text recovery. The component gate rejects
`RewriteTokens` from both the graph dispatcher and the bounded leaf projector.
Leaf lexical classification and callable-reference target carriage are still
bridges until the parser and semantic graph carry those dedicated kinds, so
the expression owner remains `BRIDGE`.

Forty-eighth executable active-rung delta, 2026-07-17: equality and inequality
emission now consumes child node handles and
`CodegenExpressionTypeFromGraph`. String comparison selects the runtime ABI
compare row from those facts; scalar and payload-free enum operands use their
already projected graph values. Qualified payload-free variants derive their
owner type only when both the enum declaration row and exact variant row
exist. The graph emitter no longer calls the legacy
`RewriteEqualityProjection`, `ExprKind`, or `RewriteExpr` text path. Existing
String equality and concatenated-String equality fixtures remain the focused
C/LLVM proof; `enum_return` pins the qualified-variant type row in the DRV-2
MIR integration gate. Other expression bridges listed in the owner registry
remain open, so this does not promote the whole expression owner to `CLOSED`.

Forty-ninth executable active-rung delta, 2026-07-18: canonical self-host MIR
now consumes `expr0_graph` through `MirExpressionGraphFactsForArtifact` before
semantic analysis. `--canonicalize-mir-json` no longer reconstructs expression
topology from compact instruction text; missing or invalid graph rows fail
closed. Native C-oracle MIR does not yet carry those rows, so its temporary
compatibility path is isolated behind the explicitly named
`--canonicalize-oracle-mir-json` command. The focused `result_int_core` gate
compares both canonical artifacts and mutates missing and invalid graphs on the
strict self-host path. The broader expression owner remains `BRIDGE`.

Fiftieth executable active-rung delta, 2026-07-19: native residual
`MIR_INST_ASSIGN` now carries its target in `expr0_graph` and value in
`expr1_graph`. The self producer continues to lower the same source assignment
to an SSA `def`, where the value is `expr0_graph` and the target is
`expr1_graph`. The Pergyra MIR consumer owns this physical distinction and
appends both forms in target-before-value semantic order; it does not infer the
order from expression text. `array_index_assign` is now the 35th DRV-2 MIR
fixture and selects strict `--canonicalize-mir-json` for the native artifact,
not `--canonicalize-oracle-mir-json`. The focused C/LLVM gate compared 20 body
fixtures plus this one MIR fixture, directly consumed native MIR to byte-equal
self C, ran it against the C oracle, and rejected removal of either native
graph. This closes the residual-assignment graph bridge for this executable
slice only; the mixed expression owner remains `BRIDGE`.

Fifty-first executable active-rung delta, 2026-07-19: typed array-literal
initializers no longer reparse their payload with bracket trimming and
`CallArgAt`. `ast_expression_graph_array_literal_owner.pgy` now owns the array
spine view and declared-element compatibility walk; codegen and semantic
initializer checking consume that one view. Nested arrays recurse through
graph handles, while scalar and nominal elements consume existing graph type
owners. The initializer owner is statically forbidden from calling
`SemanticProjectionArrayLiteralMatchesDeclaredType`. Focused C/LLVM DRV-2
parity for `ast_node_array_literal` matched canonical MIR, emitted C, and
runtime output, and the existing graph-negative gate still rejects missing or
invalid array roots. Assignment and statement array-literal checks remain on
the legacy projection, so this is an executable slice, not global result-type
closure.

Fifty-second executable active-rung delta, 2026-07-19: `await` is a parser-owned
arity-one expression node rather than an `"await "` leaf convention. Semantic
and codegen result typing consume the child `Future<T>` / `RemoteFuture<T>`
fact, MIR JSON preserves the node kind, and remote-await emission selects the
machine runtime ABI from the typed child binding. The focused machine-layer
gate changes only the MIR node kind to `leaf` and requires fail-closed
rejection. Arbitrary await operands and local executor lowering remain outside
this bounded rung.

Fifty-third executable active-rung delta, 2026-07-19: payload-free enum
expected-value emission no longer calls the root-text enum projection owner.
It consumes the parser-owned receiver/member graph and the exact enum row used
by the general graph emitter. Focused `enum_return` DRV-2 parity passed with C-
and LLVM-built self drivers over 20 body fixtures plus the selected MIR
fixture. The negative leg preserves root `Choice.B` provenance and changes only
the member child to `Missing`; both hard consumers reject it. Text projection
remains in separately named call/match compatibility owners, so the mixed-tree
blocker remains `ACTIVE`.

Fifty-fourth executable active-rung delta, 2026-07-19: index emission now
consumes `CodegenExpressionTypeFromGraph` for its receiver and the collection
runtime ABI row for its getter. The emitter no longer recovers a binding or
member-field type from receiver text. The new `member_array_index` fixture
raises DRV-2 to 41 MIR fixtures and exercises `holder.values[1]`. Focused C/LLVM
parity passes for that fixture. Its negative changes only member-node
provenance from `holder.values` to `stale.provenance`; both drivers still emit
the child-graph-owned getter. A static gate forbids node-text and
`ExprMemberFieldType` reads in `RewriteSemanticIndex`. Other text consumers
remain, so the mixed-tree blocker stays `ACTIVE`.

Fifty-fifth executable active-rung delta, 2026-07-19: DRV-2 grows from 41 to
59 committed MIR fixtures by promoting 18 declaration, arithmetic, call,
branch, reassignment, and loop programs that the Pergyra producer already
handles. Every promoted row matched native-oracle canonical MIR and runtime
output under both C-built and LLVM-built self drivers. The complete 59-row
matrix was not rerun in this slice. The contract explicitly excludes three
falsifying fixtures until their actual owners land: `break_after_stmt`
(canonicalizer fixed point), `nested_if_in_loop` (loop-phi facts), and
`array_destructure` (parser/semantic typed-binding facts). Promotion may not
recover those facts from source text or delegate them to a hidden C fallback.

Fifty-sixth executable active-rung delta, 2026-07-19: `break_after_stmt` becomes
DRV-2 MIR fixture 60. In a cyclic CFG, structural-merge distance could reach a
future iteration's then block and flatten the current then-arm `break` into an
incorrect empty branch. The MIR-to-tree consumer now checks the current
then-arm successor fact and projects a direct loop-exit edge as the abrupt arm;
it does not inspect source text. The official focused harness passed all 20
body fixtures plus this row with C-built and LLVM-built drivers, including
native/self canonical MIR and runtime parity. The remaining excluded fixtures
are `nested_if_in_loop` for loop-phi consistency and `array_destructure` for
typed-binding carriage.

Fifty-seventh executable active-rung delta, 2026-07-19:
`nested_if_in_loop` becomes DRV-2 MIR fixture 61. A loop with only `break`
exits has no header backedge, so its assigned local must not receive an eager
two-input header phi. `loop_reachability_fact_owner.pgy` computes
`reaches_backedge` and `falls_through` from typed control-flow nodes; the MIR
producer compares those facts with completed CFG successors and takes live SSA
versions from its local-version owner. The official C/LLVM focused harness
matched native/self canonical MIR, emitted C, and runtime output. Its negative
injects the retired one-predecessor header phi and both final consumers reject
it. At that checkpoint, `array_destructure` remained the only excluded
executable counterexample in this local expansion.

Fifty-eighth executable active-rung delta, 2026-07-19:
`array_destructure` becomes DRV-2 MIR fixture 62. Parser-owned pattern and
initializer graphs feed semantic ordered binding/type rows and one typed MIR
destructure instruction. The final consumer derives canonical temp/index
graphs from these facts; it does not recognize `Split` text, and its temp name
is binding-derived rather than JSON-offset-derived. The official focused
harness matched native/self canonical MIR, emitted C, and runtime output with
C-built and LLVM-built self drivers. Removing only the element-type fact fails
closed.

Fifty-ninth executable active-rung delta, 2026-07-19:
direct and member generic calls share one self-MIR specialization owner.
`generic_member_inferred_flow` becomes DRV-2 MIR fixture 63, while the existing
inferred `Identity<T>` fixtures consume the same row family. Self identity is
the parser-anchored tuple `(owner SyntaxNodeId, expression lane, local call
ordinal)`, never a transient expression-graph index. The hard MIR-to-C
entrypoint passes the decoded MIR specialization view directly into codegen;
it no longer creates a semantic codegen view and then overwrites it. Recomputed
semantic rows verify identity, target, formal, actual, and canonical symbol.
Missing direct/member rows, identity drift, and symbol drift fail closed. The
nested member fixture carries ordinals 0 and 1, infers `T=Int` without source
`<Int>` actuals, and emits one deduplicated `Box_Echo_Int` body. Native MIR
captures the child specialization before its parent so the exact generic return
binding reaches the outer call. The owner remains `BRIDGE`: native and self
identities are not yet one representation, while non-class hosts and
constructed nested generic return substitution remain outside this bounded
executable delta.

Sixtieth executable active-rung delta, 2026-07-19:
`generic_vessel_member_inferred_flow` becomes DRV-2 MIR fixture 64. It reuses
the same stable member-call identity and MIR specialization rows as the class
fixture while changing the nominal host to Pergyra's passive state-owning
`vessel`. Both nested calls infer `T=Int`, emit one deduplicated
`Cell_Echo_Int` body, and run as `42`. Missing row, identity drift, and symbol
drift remain fail-closed through the shared owner. This proves class/vessel
host orthogonality for the bounded exact-return slice; other nominal hosts and
constructed nested generic return substitution remain outside the rung.

Sixty-first executable active-rung delta, 2026-07-19:
`generic_member_constructed_return_flow` becomes DRV-2 MIR fixture 65. The
inner `Wrapper.Wrap<T> -> Option<T>` call infers `T=Int`; native MIR renders the
structured return-type AST as `Option<Int>` and self MIR carries the same
actual without reparsing call text. That exact result then specializes the
outer `Wrapper.Echo<T> -> T` call as `Wrapper_Echo_Option_Int_`. Source-to-C
and MIR-to-C both run as `43`. The native verifier rejects a formal identifier
left inside an actual type, while the self verifier rejects the corresponding
row mutation. Option emission consumes the semantic direct-call target for
`Some` rather than reclassifying its callee node. The bounded class/vessel and
one-level `Option<T>` slice is executable; multi-formal and deeper constructed
return expressions remain open.

Sixty-second executable active-rung delta, 2026-07-19:
`generic_member_array_return_flow` becomes DRV-2 MIR fixture 66. The same
structured return substitution resolves `ArrayWrapper.Wrap<T> -> Array<T>` as
`Array<Int>` and specializes the outer call as
`ArrayWrapper_Echo_Array_Int_`; hard source/MIR paths both run as `44`.
Program runtime-usage projection now distinguishes the generic declaration
surface `Array<T>` from materialized array types: declared formals come from
semantic signature facts, while concrete array uses come from the MIR-carried
generic specialization view. It does not turn arbitrary unknown element names
into formals. The focused C oracle/self parity and public bounded replacement
gate are green; nominal-record concrete actuals remain the next executable
falsification case.

Sixty-third executable active-rung delta, 2026-07-19:
`generic_member_record_array_return_flow` becomes DRV-2 MIR fixture 67 and
closes that falsification case. `Wrap<Point>` yields `Array<Point>`, the outer
call specializes on that exact constructed type, and native/self source/self
MIR all run as `45`. The emitted-C gate requires the concrete
`pgy_Point_array` definition in addition to both specialized method bodies.
This proves that generic-formal suppression does not erase a materialized
nominal-record array discovered only through specialization actuals.

Sixty-fourth executable active-rung delta, 2026-07-19: five codegen-only paths
become DRV-2 MIR fixtures 68 through 72. `ref_param` and
`inout_return_forward` exercise readonly-ref and value-result carriage;
`option_int_core`, `array_param`, and `bool_logic` exercise wrapper mutation,
collection parameter/return flow, and recursive logical control flow. Their
self-produced MIR and native-oracle MIR canonicalize identically before the
hard MIR consumer emits and runs the same C. This is a bounded breadth
promotion, not proof that every expression or ownership surface is closed.
The focused runner now accepts a comma-separated fixture set, preserving the
five-minute impact-isolation budget without recompiling the same driver five
times. At this checkpoint, `defer_scope` was deliberately not promoted because
its native MIR `defer_body` was string authority rather than a typed cleanup-
body fact.

Sixty-fifth executable active-rung delta, 2026-07-19: `defer_scope` becomes
DRV-2 MIR fixture 73. The bounded one-statement `Log` body is carried by
`AST_DEFER_STMT`, the explicit `Log` body kind, and an expression graph. The
hard MIR consumer no longer reads the legacy `defer_body` string array.
Native MIR keeps that array as compatibility provenance, but strict
canonicalization and the missing-graph negative prove that it cannot recover
execution meaning. C-oracle and self paths agree on LIFO block-exit cleanup and
early-return cleanup. Multi-statement and non-`Log` bodies remain rejected by
the bounded producer until nested typed cleanup rows are added.

Sixty-sixth executable active-rung delta, 2026-07-19: `enum_match` becomes
DRV-2 MIR fixture 74. Bare match identifiers remain syntax facts until the
semantic owner proves the subject enum and a zero-payload declaration row.
The MIR producer carries the pattern with the native-compatible null variant
field, and the hard consumer resolves `Direction.North`, `Direction.East`, and
`Direction.South` only from MIR declaration facts. Deleting the `North`
declaration while retaining the branch pattern is a blocking negative and
cannot fall back to source text. Focused C parity covers source-to-MIR,
canonical oracle comparison, MIR-to-C, and runtime output. Payload-bearing
enum cases and native/self CFG block-order unification remain open; the latter
is isolated behind the named oracle canonicalization bridge.

Sixty-seventh executable active-rung delta, 2026-07-19: `result_try` becomes
DRV-2 MIR fixture 75. The parser-owned postfix-try graph preserves
`?Validate(doubled)` and its call-argument spine through self MIR, oracle/self
canonicalization, and hard MIR-to-C consumption. Runtime parity exercises both
the successful payload path and Err early return. The existing graph mutation
gate rejects a missing required try graph, so the new Result coverage does not
create an operand-text fallback. Option and Result try checks now share a
small parity owner; the producer parity runner remains orchestration-only.

Sixty-eighth executable active-rung delta, 2026-07-19: `for_continue` becomes
DRV-2 MIR fixture 76. The producer emits an explicit continue CFG edge and one
carried for-loop summary. The hard consumer reconstructs `Continue` from the
edge target, not from instruction display text, and validates the summary
before projecting the loop. A mismatched declared summary count is rejected
with the LoopFlowSummary diagnostic. Focused C parity matches native canonical
MIR and runtime output. Wider nested transfer and loop-state combinations
remain outside this bounded row.

Sixty-ninth executable active-rung delta, 2026-07-20: `else_if_chain` becomes
DRV-2 MIR fixture 77. The producer carries `(n < 0)`, `(n == 0)`, and
`(n < 10)` as three distinct typed condition graphs. The hard consumer emits
the nested branch chain from those graphs; it does not recover conditions from
instruction text. Focused C parity matches native/self canonical MIR, emitted
C, and runtime output. The shared graph mutation gate removes and corrupts the
required graph and must fail before emission. This closes the bounded else-if
condition transport seam, not all branch or control-flow state.

Exhaustive-parity SoT closure, 2026-07-20: the assignment projection probe
previously built `Some` and `None` call graphs but left their callable target
rows unclassified. That let a fixture bypass the semantic call-target owner
and fail only at the hard codegen consumer. The probe now derives builtin call
identity through `SemanticExpressionGraphCallTargetsFromSignatures`, the same
owner used by the compiler pipeline. A missing-call-target negative mode skips
that derivation and must fail with the final consumer diagnostic. No probe-
local callee parser or codegen fallback was added.

Post-delta SoT closure, 2026-07-20: the unreferenced
`expr_semantic_shape_emit_owner.pgy` and its two dead codegen shape accessors
are deleted. `expr_semantic_graph_emit_owner.pgy` is now the only expression
emission consumer. Semantic shape rows remain as graph-root consistency
evidence, not a second emission authority. The component contract rejects the
retired file, import, and accessor names.

CI proof ownership, 2026-07-17: the dedicated Linux
`self-host-parity-linux` job owns real-source selfcheck, the four-stage
completeness ledger, and the complete parity surface. The parallel
`self-host-bootstrap-linux` job owns the codegen and integrated-driver fixed
points. The ordinary Linux, Windows, and macOS jobs own native parser,
semantic, codegen, and DRV-2 parity plus the shared contract gates. They must
not repeat the exhaustive source proof. This split follows the
impact-isolation rule: platform jobs prove platform behavior, while two
parallel Linux jobs prove repository-wide parity and bootstrap closure.
`self_host_ci_profile_smoke.sh` rejects routing drift in either direction.
The integrated driver fixed-point runner emits a 60-second heartbeat because a
single real-source MIR consumption phase can exceed the CI no-output interval.
Until full-source self-host MIR production has bounded allocation, the native
oracle produces the fixed-point MIR once and gen2/gen3 consume the same fact.
Bounded DRV-2 producer parity remains blocking. Repeating whole-source analysis
inside the fixed-point leg is a gate design regression, not evidence strength.
The first Windows local platform-profile run completed in 21m49s; the previous
GitHub Windows full-preparation step took about 75 minutes. The first GitHub
platform-profile run completed in 27m32s on Windows and 7m09s on macOS.

Resource-pressure measurements are advisory while the sampler and benchmark
corpus are being stabilized. Memory, elapsed-time, and owner-hash evidence must
not block self-host preparation CI. Functional emission, ABI ownership,
negative fallback, parity, and fixed-point gates remain blocking.

The same DRV-2 expansion exposed and closed an assignment graph transport gap:
plain assignment targets now retain their semantic leaf graph just as indexed
targets retain their index graph. MIR verification rejects either missing
shape, and the MIR JSON consumer reads target-before-RHS to match the semantic
atom/value lane order. The readiness fixture also compares the canonical
`x + 1` spelling rather than the pre-normalization `(x + 1)` spelling. No
target text recovery was added.

The expanded producer frontier also exposed nominal constructors as a missing
call-target owner input. Carried call-target verification now consumes the
typed nominal-constructor inventory alongside function and builtin signatures,
so `Pair(...)` is accepted as a direct constructor call while unknown callees
still fail closed. No callee text recovery was added.

Removing the carried-target shortcut then exposed the existing namespace and
method provenance gap. Initial validation now admits only declared canonical
namespace/member targets. The body fixpoint independently resolves
`Math.Add()` from the qualified callable inventory and `v.LengthPlus()` from
the receiver type plus method signature, then rejects any carried mismatch
instead of trusting or overwriting it. The full C DRV-2 frontier passes 20
source fixtures and 32 MIR producer fixtures with this comparison active.

Mechanized closure delta, 2026-07-12: `SoTAuthority.v` now defines rung closure
as required-owner completeness, authority uniqueness, required consumption,
and zero semantic fallback. It proves that the current array-literal,
try-operand, collection-mutation, enum declaration, nominal/field, and role rows are closed in the
bounded model and that missing facts, duplicate producers, and
owner-plus-fallback bridges are not closed. The source adequacy gate binds only
those modeled rows to live files; future consumers require new bindings rather
than inheriting a global proof claim.

Whole-spine owner declaration, updated 2026-07-15: 36 authority rows have
stable owner identities in `docs/semantics/sot_owner_spine_registry.md`, and 13
self-host fact carriers are explicitly classified as derivatives rather than
alternate authorities. Matching `SpineFact` / `SpineOwner` constructors remain
a checked Coq projection. The current split is
`CLOSED=16 BRIDGE=11 ACTIVE=10`; only executable rung closure may promote a row.
`tests/sot_authority_edge_smoke.sh` consumes the registry without copying its
owner list or status count. `src/self_hosted/OWNERS.md` remains only a physical
module inventory.

Seventy-seventh executable active-rung delta, 2026-07-21:
`branch_defer_scope` and `branch_defer_skipped` become DRV-2 MIR fixtures 191
and 192. The first probe falsified the self emitter's lexical cleanup model:
the taken static branch emitted `1,2`, while the native MIR path registers the
reachable defer for routine exit and emits `2,1`. The self emitter now threads
a routine-level LIFO registration state through nested statement emission.
The skipped static branch registers nothing because its defer instruction is
not reachable in the MIR-derived artifact. Independent C-built and LLVM-built
self drivers pass canonical-MIR, emitted-C, host-compile, and runtime parity.
The bounded proof still depends on the beta rule that dynamic-control defer is
rejected. Native C/LLVM backends retain a separate bridge because their MIR
emitters register the AST body pointer attached to the instruction; removing
that bridge requires a fully typed nested cleanup-body row and backend
consumer migration, not a self-host fallback.

Seventy-eighth executable active-rung delta, 2026-07-21: ten more
backend-compare programs become DRV-2 MIR fixtures 193 through 202, covering
small array sorting/counting, buffer predicates, enum phase progression,
class-chain calls, Caesar decoding, grid accumulation, match factories,
all-positive checks, and class-state loops. Clean committed-source C-built and
LLVM-built self drivers agree with the C oracle on canonical MIR, emitted C,
host compilation, and runtime output. The probe keeps three boundaries honest:
`bubble_sort_basic` is missing the indexed-assignment base-use graph, while
`break_continue_while_slot` and `class_bump_option_match` fail in the bounded
semantic subset. The focused selector also stops spawning external
`dirname`/`basename` processes for every filter comparison and uses Bash path
expansion instead, so Windows probe cost no longer scales as external process
count times the manifest size.

Seventy-ninth executable active-rung delta, 2026-07-21:
bubble_sort_basic becomes DRV-2 MIR fixture 203 after closing assignment
binding-mode carriage. An indexed inout parameter may omit an ordinary SSA
base use because parameter version zero is an input, not a definition; local
targets may not. The semantic owner carries inout_param into MIR, canonical
self-MIR input is checked against that semantic fact, and a local-mode mutation
fails closed. Clean C-built and LLVM-built self drivers match the C oracle
through canonical MIR, MIR-to-C, host compilation, and runtime output. The
explicit oracle compatibility bridge remains separate and is not evidence for
self-MIR fact completeness.

## 7. Fifteen-Day Correction

The previous roughly fifteen-day interval delivered substantial owner, gate,
and bounded-rung work, but released/default replacement remained at 0 percent.
That work is useful infrastructure, but it is not sufficient progress by
itself. From this decision onward, progress reports lead with executable
replacement evidence and report SoT work only as the condition that enabled or
blocked that replacement.

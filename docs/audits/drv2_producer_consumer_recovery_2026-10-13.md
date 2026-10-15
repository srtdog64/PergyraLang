# DRV-2 producer / consumer recovery — B source review

Status: contract proposal, not implementation or independent review approval.
Production/gates are unchanged. Baseline zXdZzt reproduces6898/9; supplied
leaf counts are reached-observer observations, not exhaustive safety evidence.

## One contract, separate facts

The actual successful typed operation must issue its existing publication
snapshot. Consumers distinguish (a) payload -> result dependence, (b) synchronous
input observation, (c) actual independent allocated result, and (d) retaining
store/output. None can be derived from another, a name, COPY_ONLY or a zero mask.
Unknown/incomplete remains explicit. No new top-level owner/family is proposed.

## Actual leaf families and required issuer

| Reached family | Existing source authority | Required relation / remaining boundary |
|---|---|---|
| Some31 | stdlib_variant.c120–143: arity1, actual payload Type, future/storage checks, Option<T>, owned result name | payload -> result, no freshness; String/record bits survive; scalar payload bits empty only from physical evidence |
| Unwrap4 | stdlib_variant.c155–198 | UnwrapOption selects Option payload; Unwrap selects Result payload: NOT the same branch. Census must identify the four actual targets before limiting implementation. Concrete payload required |
| StringTrim3 | stdlib_scalar.c317, runtime string_builtin_inline.h238 / lib_string_exports.h219 | actual unary String check; runtime malloc/copy result, not borrowed input. Input observation and fresh result need separate checked receipts; OOM empty fallback is not exact-copy/allocation-success proof |
| StringSplit1 | stdlib_scalar.c338, runtime lib_string_split_exports.h7 | checked String2 -> Array<String>; new array and duplicated segments. Shared C/LLVM contract must be checked; no generic Array freshness |
| nominal constructors23 | type_checker_call_constructor.c typed field/argument assignment checks | join exact admitted payload fields into aggregate result, not a constructor freshness grant; all children still contribute nested retaining effects |
| String+8 | existing type_checker_expr_ops.c operator dispatch | only actual selected String concatenation may use its allocating result contract; user overloaded + is not the same operation. Need result-name/type snapshot and both original operand edges |
| member Slice | existing member checker + published member/value and backing-place owner | preserve exact source owner/root, view lifetime and current binding; never categorize as fresh or physical |
| destination physical binding | publication physical value + index original LET/assignment identity | physical RHS does not prove a local destination. Exact local binding and current target/RHS are required |
| nonlocal scalar write | backing_store source→output matrix | formal/member/heap write must preserve output dependency even when RHS/result is scalar; do not use local-scalar exemption |

Scalar NumberRows→Some(ArrayLength)→UnwrapOption already passes the root's
2KlaoO probe. String/record retaining probes already reject. Therefore absence
of a retention-registry row is not blanket missing permission. The relevant
gap is a checked operation carrying nonempty backing origins through a result,
plus genuinely incomplete output relations elsewhere.

## Whole producer / consumer change set

1. Typed successful branches above own operation selection and exact actual
   argument/result type evidence. stdlib dispatch stamps stdlib=true only after
   the branch; a branch-issued row is pending until record_checked_call seals
   original target/kind/declaration/value binding/arity/argument ptr+ID.
2. Existing publication c/h owns immutable snapshots, begin invalidation,
   current-generation query and owned-name retirement. Reuse node-specific
   carrier; no retained scope Symbol, second spelling table or raw name grant.
3. **Raw slot_analyzer_escape.c155–235 also changes**: it currently treats an
   unhandled wrapper argument as CALL. Merely fixing backing_origin leaves this
   raw CALL bit intact. It must consume the same checked operation/input/result
   relation, preserve child recursion, and distinguish a returned loan from
   external retention. No global CALL clear.
4. Summary selector, backing_origin and backing_consumers consume that same
   receipt. Result origins join exact payload bits; physical or independently
   materialized result is separate. Child consumers still run, so an Int result
   from Keep does not erase its ref input's retaining effect.
5. Existing source/output matrix and fixedpoint keep distinct pending/complete
   state; opaque/Slot/inout/recursive consumers remain retaining or incomplete.
   Cached claim ledger must include newly consumed operation/type/operand
   evidence and validate drift on both fast paths and full backing reuse.
6. Last consumers remain raw parameter validation, pending ref finalizer,
   HIR/MIR summary projection and full canonical/installed driver. Published
   admission0 for DEFAULT/COPY_ONLY is not raw descriptorCALL/RETURN0.

## P1 and lifetime boundary

Direct ref String callers still skip COPY_ONLY before recording pending calls;
the generic borrowed validator skips it again. This is an existing last-consumer
gap, not fixed by wrapper production. RETURN-only getter/local read must stay
legal while opaque Keep and shallow Slot storage preserve raw CALL and reject
borrowed/dynamic backing at the caller. Literal/static aliases need an actual
binding-origin lifetime receipt; allocated independence alone cannot authorize
retaining storage. Channel<String> duplicates backing in shared C/LLVM runtime,
so CHANNEL32 alone is not UAF or a blanket rejection contract.

## Cap, retirement and falsifiers

Publication is593/599. A generic checked-result-dependence snapshot/query cannot
honestly fit by adding isolated operation APIs. Candidate consolidation INSIDE
the same owner: share pending operation operand/name ownership with the existing
cast/call node carrier, current typed value validation and exact argument seal;
one selected-operation relation query, not one table/owner per builtin. Measure
actual LOC before approval; schema may remain in the already private header,
but producer/query/retirement responsibility cannot move just for line count.
No cap increase or compressed compound-statement line golf is proposed.

Required falsifiers: scalar Option positive; dynamic String/aggregate loan local
read versus returned/Slot/opaque/inout retention; Trim/Split allocated result
versus unknown/user-shadow; checked nominal payload alias with retaining child;
String+ overloaded target; member Slice source release; local vs formal/member
scalar destination; original target/operand/type/binding/generation corruption;
recursive incomplete relation and unrelated output ordinal. Root builds once,
then runs all original native/query/work-cap gates and unchanged canonical.

Remaining unknowns before production: actual Unwrap targets/types, exact nominal
selected identity, String+ backend result allocation selection, member Slice
root issuance, and precise physical destination missing facts. Some/Unwrap alone
cannot close the other observed leaves or the direct String consumer. This audit
does not close C2/drop, SoT, bootstrap, installed-driver or same-SHA CI.

## Current6672/9: real returned String loan refinement (read-only)

Root observed538 RETURN reports after the checked result receipts. This is not
evidence that all538 are independent leaf defects. The first real source is
AstSourceModulePathForNode -> Some(facts.module_paths[index]): the ref owner stays
live and borrowed, so its part cannot be moved out. The new Some receipt correctly
keeps that backing origin; it does not materialize a value copy.

### Adopted contract versus actual implementation

`docs/semantics/27_ownership_clean.md:543-572` adopts D1(B): String and plain
implicit-copy aggregates require actual deep copy facts when the source remains
live; collections require Clone. Slice is a borrowed view, not an owning copy.
`:869-930` requires one type-owned copy/drop descriptor consumed by both C/LLVM,
copy/shallow-path removal BEFORE emitting drops, active-payload glue for Option/
Result/enum and field glue for structs. The same section explicitly says general
per-type drop glue does not yet exist. Category COPY_ONLY is not this glue.

Observed current owners / last consumers:

- Native indexed_string_borrow_owner.c records exact identifier Array<String>
  backing and explicit allocating producers; member-base index remains missing
  exact source identity. Publication result-relation preserves payload dependence
  but has no copy landing/materialization operation or lifetime certificate.
- C transpiler_expr_array_access_emit.c76 generates pgy_array_get_String; its
  strdup_fmt allocates generated source text, NOT program String payload.
  LLVM llvm_expr_array_access.c143-164 emits checked getter or load of element.
  Neither creates the implicit fresh String required at a borrowed return.
- Option C builtin emitter242+ packages the lowered payload; LLVM result_option
  calls489-504 coerces/inserts the existing value into the tagged result.
  Unwrap extracts it. Neither copies String backing because it is Option-shaped.
- Runtime owned String operations exist: StringConcat and Trim allocate bytes;
  StringSplit/Array<String> Clone duplicate payloads; owned-push/deep-array-drop,
  Map String insertion/drop and Channel String transport have specific contracts.
  These are operation-specific, not a type-generated general copy/drop owner.
  Allocation empty/NULL fallbacks are not exact-copy or successful-allocation
  proofs. No source Concat/own/Clone workaround is proposed.
- Region escape fact is allocation_site_id/scope_id/function_syntax_id for bounded
  synchronous concat allocation. It certifies selected allocation scope, not a
  general result-origin, implicit return copy, or loan lifetime ledger.
- MIR ABI rows own type/layout/runtime target/materialization identity. Sealed
  AST call result names and private collection initializer witnesses establish
  typed selection, not String copy/drop ownership. Existing direct MIR array
  String boundary facts carry CallerFrame/BorrowedStaticElements/BorrowedResult
  only for the admitted literal-array/constant-index/immediate-observer pattern.
  They cannot certify ref dynamic member arrays or Some-returned owned labels.
- MIR cleanup/resource operation witnesses handle admitted resource/collection
  edges. They do not provide general per-type String/plainaggregate copy glue
  and automatic active-payload/field drops required by§5.4.

### Necessary bounded return-loan chain, not an implementation decision

The source typed return boundary must choose one actual outcome: (1) admitted
fresh implicit String copy with explicit allocator/failure semantics, (2) proven
dead owned-part move, or (3) checked borrower lifetime through the last caller use.
The current first case is ref storage, excluding outcome2. Neither outcome1 nor3
is currently supplied by a general existing result receipt. Required facts would
bind original indexed/member source identity, typed String/Option payload path,
original return/landing identity and exactly-once evaluated value, allocation/
transfer and cleanup responsibility. Semantic publication -> HIR/DIR/RIR/MIR
projection/validation -> both C and LLVM must consume the SAME selected operation;
backends cannot add a typename/name wrapper or reevaluate the source expression.
Existing retainer/Slot/inout and returned-loan negatives remain necessary. Root
must decide the smallest admitted operation and real failure/cleanup contract
before code; this is not permission to start unrestricted G3/C2 or claim it closed.

### Smallest honest nominal dependent receipt

Initially only exact nongeneric plain struct construction can be considered:
actual selected declaration/type, exact field arity, unique named/positional
field mapping and concrete checked assignability for every argument; no Void,
Unknown, failed future/storage/borrow check. The checker currently continues
after field diagnostics and returns sym->type, so success requires a local latch,
not merely reaching its final return or a global diagnostic-count delta.
Original declaration/field/type/argument identities and actual result name must
reach final checked-call/value seal and cached claims. Result dependency is the
union of supplied payload dependencies, NOT fresh allocation or permission to
retain them outside the result. Child retaining effects remain traversed.

Generic classes currently skip field validation; annotation success is not a
replacement proof. Domain/world/zone/role constructors permit filtered/default
inputs and authority/affine transfer, so missing default/capture effects cannot
be assumed empty. Ordinary non-struct classes with omitted fields/defaults and
named-argument duplicate-field gaps also require separate existing-owner proof.
These forms remain unsupported for a new receipt, not newly rejected source.
Even the exact struct relation cannot admit a genuine borrowed returned String
payload without the separate copy/move/lifetime landing above.

## Approved physical-return eligibility delta

Only function_param_publication_owner.c changed: the two-line loop rejecting
DEFAULT nonphysical input types was removed from
semantic_param_publication_routine_return_dependency_is_scalar. The original
current no-output header (including no own/inout/async), generation/type/body/
formal seals and primitive/Void result whitelist remain. Exhaustive completed
backing analysis still owns CALL/CHANNEL/output effects and RETURN refinement;
String/Option/aggregate results receive no grant. No materialization/glue changed.

Observed static evidence: publication597/599; narrow publication+summary GCC
syntax rc0/no warnings; scoped diff-check rc0. Frozen publication SHA256:
43573BE9DB874CABF0FB2F1C249315DB51C9EAF5FD25FBABF89D4D822948A12D.
Root owns behavioral/original canonical verification; this is source-ready only.

## Direct ref String consumer boundary — read-only R0dhWA survey

Current three bypasses are param_summary.c110 (COPY_ONLY formal skips admission
summary), ownership_call.c498 (COPY_ONLY returns before pending obligation), and
ownership_boundaries.c44 (COPY_ONLY suppresses the final diagnostic). These own
different stages; changing only one leaves either a lost pending obligation or
the second diagnostic bypass. DEFAULT admission0 is likewise not raw backing0.

Facts that can honestly be consumed now:

- Exact original AST String literal has static storage; publication issues its
  independent origin only after actual String typing. An identifier alias is
  NOT yet a static-lifetime receipt: indexed-string source0 conflates static and
  allocated independence, while publication identifiers generally have no such
  origin-ready fact. A name or Symbol zero cannot recover static lifetime.
- Checked allocating producers issue separate independent-result evidence, but
  this only separates their backing from input backing. A local heap allocation
  is not an unbounded lifetime/transfer certificate for opaque Keep or shallow
  Slot publication. Last-use ownership transfer/copy and cleanup remain needed.
- Exact canonical per-ordinal BorrowedForCall and complete current user no-retain
  summaries permit synchronous input reads. Some/Unwrap/MapGet String receipts
  preserve result dependence and permit the operation itself, not a retained
  result. Pure RETURN-only getter/local observer remains distinct from CALL.
- Channel<String> shared C/LLVM runtime duplicates payload before storage and
  fails without storing when duplication fails. Its existing transport-copy
  contract can permit that operation; CHANNEL32 is not a shallow retention
  theorem. Input result freshness or general channel/container permission does
  not follow. Slot Write payload1 and opaque calls have no such copy contract.
- Physical primitives retain the existing exact publication fact and behavior;
  broad String COPY_ONLY classification remains D1 value semantics, not storage.

Smallest coherent existing-owner recovery: preserve category; keep raw retention
and completeness separate from published parameter admission; record an exact
pending ref-call backing obligation before category bypass; finalize against the
sealed callee/raw retention and original argument/backing fact; deliver only an
unproved actual retained loan to the existing ownership diagnostic. Generic
COPY_ONLY diagnostic suppression cannot be the last consumer for that obligation,
but globally deleting it would falsely reject D1 values and actual transport
copies. Actual RETURN materialization/view admission is a separate decision.

Unknowns before implementation: static alias lineage and local heap transfer/
cleanup; exact operation-specific copied transport facet versus raw CHANNEL;
whether a completed user summary returns a loan but does not retain it; missing
member/index source lifetime and caller last-use proof. Existing current facts
do not authorize inventing any of these. A separately owns Type-cache identity.

Paired falsifiers: literal/pure ref String reader; static alias only with actual
issuer proof; dynamic local -> opaque Keep and ref record field -> Slot; returned
loan -> retaining call versus local StringLength; Channel<String> copy positive;
physical scalar unchanged; original call/formal/mode/body/binding/generation
faults. Source-level acceptance is not a demonstrated UAF or cleanup proof.
No source/test/build changes were made for this survey.

## Exact reached nominal survey at current e224 / EDuQVu

The preserved zXdZzt opaque-origin observer has23 nominal call sites and21
distinct constructor names. All21 current source declarations are nongeneric
plain structs; this source match does NOT prove success or a current complete
reason census. The two repeated names are SemanticAstExpressionVerdict and
SemanticAstCollectionOwnershipVerdict. The single-site names are JsonObjectFactTable,
JsonArrayObjectFactTable, AstMatchCasePatternFact, SemanticAstIntentStepActorFact,
SemanticAstIntentVariantBindingRow, SemanticAstDomainQueryProtocol,
SemanticDiagnosticMessageFact, SemanticExpressionGraphIdentifierFact,
SemanticExpressionGraphCollectionCallProtocolFact, SemanticExpressionGraphDomainQueryFact,
SemanticExpressionGraphCollectionMutationFact, SemanticAstGenericSpecializationFacts,
SemanticRoleOperatorTargetIdentity, SemanticExpressionGraphView,
SemanticAstNamedValueBoundaryVerdict, SemanticAstCollectionOwnershipReceiptRows,
SemanticAstBodyVerdict, SemanticStringWindowBinding, SemanticAstZoneValueCarriageVerdict.
GenericSpecializationFacts is its literal struct name, NOT a generic declaration.

Important selected-identity gap: helpers_late.c53 returns constructor_type before
its ordinary function declaration stamp at89. Constructor selection in
call_constructor.c252 resolves an actual nominal declaration, but the call's
existing callable declaration ID is normally0. Reusing this as a routine ID
would invent a callable/body-summary entry. A selected nominal pointer/stableID
must therefore be carried as actual constructor evidence in the existing result
receipt, independently of the unchanged callable identity namespace.

Exact local success latch candidates in the existing checker:

- visibility/actual declaration/kind; nongeneric AST_CLASS_DECL + is_struct;
- provided count == actual field count; all arguments/type nodes present;
- every named/positional argument maps uniquely onto one admitted actual field;
- actual typed-site result is concrete/non-Void and assignable to that field;
- future, channel, array/storage and borrowed constructor-field checks succeed;
- actual result Type and owned call value-type name agree with selected nominal;
- no generic/skipped/default/domain/affine branch issues this first receipt.

The latch must explicitly record failures at their owning decision, not compare
diagnostic_count (dedup/allocation) or equate reaching sym->type with success.
Existing source still diagnoses then continues for wrong arity/field/assignability;
generic classes skip field checks; ordinary classes/domain constructors can omit
inputs; named duplicate-field coverage is not a proved uniqueness certificate.
These established checker behaviors must not be silently tightened merely to
issue an optional relation: unsupported forms simply have no new receipt.

The resulting receipt is the union of original supplied field payload origins,
and says the constructor itself does not externally store them apart from its
result. Raw escape + selector + backing must consume it before opaque-CALL
classification, while child retaining calls and actual output/Slot effects stay.
Cached claims seal selected declaration/field/type/argument identities and count;
owner retirement frees transferred metadata. A result containing a collection
is not implicitly deep-copyable, and a borrowed String field does not become fresh.

This is a prerequisite typed-origin fact for A's return-landing choice, not a
separate nominal closure track or an implementation of String copy/drop glue.
The canonical first module-path return itself uses Some, so adding nominal
facts alone cannot repair that first true returned loan.

Publication599 has no remaining space. A meaningful existing-responsibility
split would keep field selection/assignability and its exact current-field seal
in the existing constructor owner, and keep generic original-call/value/generation
receipt admission plus owned storage/retirement in publication. Both consumers
must use the admitted constructor snapshot, not rescan names or reinterpret raw
rows. The current unary/map payload-name bookkeeping can be generalized into
one owned per-argument vector rather than adding a second nominal-specific
allocation/query path; actual LOC and metadata cost must be measured first.
No extra file, header-hidden policy, line packing or cap increase is proposed.
Root must approve the precise transferred snapshot and current-query boundary
after C's current reason census; this audit is not permission to implement it.

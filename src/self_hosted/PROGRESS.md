# Self-Host Progress

**This is the canonical progress measurement for Pergyra self-hosting.**
The number that matters is *how much of the C/LLVM compiler has been
substituted by Pergyra-written equivalents* -- not how many peripheral
audit tools exist.

Last updated: 2026-07-14

Evidence currency: this file is the canonical progress ledger, but individual
green claims remain dated to the gate runs named in each section. Updating this
ledger or touching an isolated SoT owner does not imply a fresh
`self-host-preparation-test-smoke` run. New validation should follow
`docs/152_validation_isolation_policy.md`: run the owner-scoped self-host rung
gate first, and escalate to the heavy preparation/parity bundle only when a
broader compiler-world artifact changed or broad parity is explicitly requested.
The latest broad parity refresh was `make self-host-preparation-test-smoke`
on 2026-07-09: it completed green with 203 real sources accepted by both
selfcheck backends, codegen bootstrap `gen2 == gen3` at 9816 generated-C lines,
DRV-0/DRV-1 driver parity, LSP parity, backend tri-compare, and MIR JSON rung-0b
parity over 86 fixtures. Later focused refreshes on 2026-07-10
raised the M2 ledger to 219/219 after the incremental fact graph owner and
completeness impact owner split landed; the changed-source impact run proved
the incremental graph, completeness ledger, impact owner, and TestHarness owner
sources through lexer/parser/semantic/codegen;
the MIR JSON fact-only frontier then moved to 96 fixtures. It first added Long
scalar flow, array index assignment, `Option` `?` propagation, and string
equality-plus-concat surfaces, then closed the remaining committed codegen
fixture surfaces: C-reserved binding spelling, payload-free enum match
comparison projection, Float signatures, seeded random flow, and string-array
index return flow.
The next focused slice raised the source inventory to 221 after splitting the
shared driver pipeline owner from DRV parity policy. That bootstrap gate proved
the standalone codegen fixed point and the first integrated parser/codegen
driver fixed point. The older integrated proof covered the bounded
source-to-AST-to-C pipeline, not the later MIR-producing driver and not the
whole compiler. Semantic analysis owns
executable `Main` cardinality, signatures, local declaration/function/scope,
initializer, iteration, assignment, expression-use, return, condition, and
ordered body-verdict rows. The latest standalone codegen bootstrap fixes
`gen2 == gen3` at 14,673 generated-C lines. DRV-2 now integrates bounded MIR
production and consumption. Its current full-source gen2 artifact builds and a
34.5-second `mir_lower` owner preflight is byte-identical between seed and gen2;
the expensive full-source gen3 comparison has not yet been refreshed and must
not be reported green.
The landing Windows run observed about 1.35 GB peak working set in the first
integrated driver seed generation. The fixed point is correct, but self-host
codegen still needs bounded emission storage before this path is production-
cheap.

The current codegen-only owner slice is materially cheaper: single-pass
runtime-call rewriting plus `TextBuilder` measures 956.1-956.5 MB and
15.792-15.877 seconds on the pinned 1,289,598-byte artifact with byte-identical
output. A separate integrated-driver probe moved complete typed-arena row-shape
validation back to the artifact boundary and made readers validate only their
consumed rows. That reduced peak private memory from 223.4 MB to
159.4-161.0 MB with identical generated output, but did not improve the
34-36-second runtime. Parser/semantic/MIR character and substring scans remain
the current CPU boundary; these measurements do not close the expensive
full-source gen3 fixed point.

A later allocation-free comparison slice repointed five remaining hot
`Substring(...) == token` checks in codegen literal/expression scans to the
existing `SubEqualsWithLen` fact. On that same pinned artifact, two control runs
measured 948.4-985.5 MB peak private memory and two candidate runs measured
947.6-951.6 MB; all four emitted the same 1,191,490-byte artifact and canonical
SHA, and an LLVM-built candidate emitted that same artifact. Candidate elapsed
time was 16.286-17.031 seconds versus 15.798-16.942 for
the controls, so this is not recorded as a speed win. Function-level String
reclamation and integrated parser/semantic/MIR stage lifetime measurement
remain open.

Generic type canonicalization now consumes the same nested-comma range owner as
call and parameter facts. The call-only producer name was removed instead of
kept as an alias. Top-level label and closing-delimiter checks use byte facts;
integrated `CharAt` calls fell again from 424,152 to 337,974 (88.1 percent
cumulative from the original profile). Runtime remained neutral at
36.432-36.743 seconds against a 36.528-second same-window control, and C/LLVM
drivers retained the byte-identical artifact.

The first shared source-scan slice now consumes `CharCode` and
`SubEqualsWithLen` facts rather than allocating one-character and keyword
`String` values in trivia, parser-cursor, and semantic-text hot loops. On the
same integrated-driver `mir_lower` input, the measured runtime moved from
37.915-38.071 seconds to 36.891-37.131 seconds with byte-identical output.
Parser 188/188 and semantic 111/111 manifests remain expected-artifact equal
under C and LLVM, and the C/LLVM integrated drivers emit the same 151,762-byte
artifact. This is one source-scan owner closure, not whole parser/semantic text
lifetime closure.

The next semantic slices give top-level operators one shared fact owner,
validate qualified callable names through byte ranges, and capture call
argument/signature partitions as `SemanticDelimitedRangeFacts`. Arity,
builtin, receiver, and type checks now share those ranges by `ref`. Integrated
`CharAt` calls fell from 2,851,682 to 424,152 (85.1 percent), while same-window
runtime remained neutral at 36.854-36.927 seconds against a 37.029-second
control. C/LLVM semantic parity remains 111/111 and both integrated drivers
emit the same 151,762-byte artifact. This closes duplicated semantic text
ownership; it does not close remaining type-name/expression scans or the
full-source gen3 fixed point.

The latest DRV-2 slice carries normalized expression graphs from the parser/HIR
artifact into MIR branch, definition, value-return, Log, and bare-call
instructions as
`expr0_graph`. The precedence/postfix parser emits logical, equality,
relational, additive, multiplicative, index, logical-not, numeric-negate,
member-access, direct-call, and call-argument node kinds and child edges in the
same walk that produces the compact parity projection. Unary and call-root
nodes carry one edge; member-access nodes carry receiver/member edges, and each
call-argument node carries the prior call spine plus one argument. Malformed
arity, a non-leaf member name, or a call-argument whose left edge is not a call
spine is rejected. Array literals carry one zero-arity literal root plus an
ordered element chain; each element remains its full recursive expression
graph. Struct literals carry a braced literal root plus ordered field-name,
field-binding, and field-spine nodes; each field value remains its full
recursive expression graph. An element or field whose left edge is not its
declared spine is rejected. Typed
source compilation binds roots by
`(owner kind, lane)` for `if`, `while`, `let`, assignment, and value return; a
text-created artifact without a required graph fails closed. Direct
`--mir-json` compilation likewise requires the carried graph and does not
reparse `expr0`. C-built and LLVM-built drivers emitted byte-identical MIR JSON
and C across 20 source fixtures and all 24 DRV-2 MIR fixtures. The strengthened
mutable-local fixture covers arithmetic precedence in initializer, assignment,
condition, and return positions, and its generated program exits successfully.
This closes parser production, MIR carriage, and hard consumption for those
migrated operators and statement lanes. Index reads select the collection
runtime ABI from their receiver fact without calling the legacy index scanner.
Logical-not and numeric-negate emit directly from their operand handle rather
than reparsing the unary root text. Direct identifier calls now emit arguments,
parameter modes, runtime ABI aliases, and struct constructors from the carried
call spine without the legacy argument-list scanner. The `class_method` MIR
fixture also emits simple `self.field` access and `v.Method(arg)` dispatch from
the receiver/member handles, method signature rows, and explicit receiver
argument; those consumers cannot call the legacy member, qualified-call,
field-access, or parenthesis scanners. The `nested_member_access` fixture
projects `line.end.x` and `line.start.x` recursively from receiver/member edges
and field/type rows rather than scanning a dotted path. The
`nested_member_call` fixture follows the same recursive type facts for
`line.end.LengthPlus(2)`, then consumes the method signature row and emits the
explicit receiver without a dotted-path or member-call scanner. The named
compact C-oracle canonicalization bridge reuses the Pergyra expression parser
to upgrade legacy native MIR text; it cannot feed the hard consumer, which
still requires `expr0_graph`. Namespace-qualified calls now carry a canonical
callable target in each call-node row; direct hard-MIR consumption validates
that target against semantic signature ownership before codegen. Object-init
internals, `CodegenAstTextNode` array-literal and `ArraySet` collection-value
bridges,
borrow/receive/spawn/await unary forms, and expression result-type
classification beyond graph-owned struct literals remain `BRIDGE` work.

The focused codegen rung now also requires a value-lane expression graph for
`ArrayPush`. A pushed `CodegenAstTextNode(...)` constructor is emitted through
that graph and the expected collection element type; the push consumer cannot
call `EmitStructValue`, `RewriteExpr`, or `StringTrim`. The committed fixture is
run-equal under C-built and LLVM-built self-host codegen. This closes only the
`ArrayPush` consumer: array-literal and `ArraySet` element emission still use
the explicit text bridge and remain the next collection-value boundary.

The Log statement lane now extracts its single argument subtree from the
parser-owned call spine, requires that atom-lane root in semantic and MIR
verification, and emits the value through `RewriteExprFromSemanticGraph`.
`EmitLog` cannot reopen the old `StartsWith("ToString(")`, `Substring`, or
semantic-shape paths. A richer self MIR call graph and the approved compact
C-oracle graph initially selected different ToString optimizations; the parity
gate falsified that topology-dependent output, so both now project the same
runtime-alias C form. Expression result-type classification beyond the
graph-owned nominal struct-literal row remains a separate text-backed seam and
is not claimed closed by this carriage delta.

The bare-call statement lane now classifies direct calls through the canonical
`TypedAstCallStatementKindForCallee` owner, carries the complete parser call
spine at `(TypedAstKindBareCallStmtTag, AstExpressionLaneAtom)`, and emits it
through `RewriteExprFromSemanticGraph`. The retired text payload accessor and
`RewriteExpr(call_expr, env)` fallback are gate-forbidden. On the pinned
`param_carriage` fixture, C-built and LLVM-built DRV-2 drivers emitted
byte-identical MIR JSON and generated C, both projected `Mutate(&value);`, and
the generated program matched the native oracle output `2 / 2 / 42`. Removing
only that instruction's graph produced the same fail-closed diagnostic under
both driver builds. The component contract is green. The broader 20-source /
13-MIR corpus was started but exceeded the five-minute focused-gate budget, so
this entry claims only the pinned falsifying fixture and does not present a new
full-corpus refresh or a released/default replacement increase.

Pipe syntax now canonicalizes in `ParsePipeFact` to the existing direct-call
and call-argument graph instead of collapsing the rendered call to a leaf. The
new `pipe_carriage` fixture proves `5 |> Double |> Add(3)` as a nine-node nested
call spine with root text `Add(Double(5), 3)`. C-built and LLVM-built DRV-2
drivers emitted byte-identical MIR JSON and generated C; the native-oracle and
self-produced canonical MIR JSON were byte-identical, and both executables
printed `13`. Removing the initializer graph failed closed with the same
diagnostic under both driver builds. The DRV-2 manifest therefore contains 14
canonical MIR producer/consumer fixtures. This closes pipe graph production,
carriage, and consumption; `?`/object-init/special-unary and structured leaf
bridges remain open, and the full 20-source/14-MIR matrix is not claimed
refreshed by this focused run.

The 2026-07-11 owner-isolated closure raised the M2 source and stage minima to
250. The preceding unfiltered ledger exposed six codegen gaps; focused reruns
closed those six at 6/6, and the two newly split executable contract owners
passed lexer/parser/semantic/codegen at 2/2. The readonly `ref` parameter row is
now emitted as `const T *`, retained as a readonly binding fact, and consumed by
member/call rewriting without recovering parameter mode from text. `Action:`
rows under `Subject` now project to the canonical function kind and carry the
subject owner/self type through the same signature inventory as ordinary
functions. This is a 250-source completeness ratchet, not a released compiler
replacement claim; released/default replacement remains 0%.

The typed `AstArena` shape now lives at
`src/self_hosted/hir/typed_ast_arena_owner.pgy`, not under codegen. The old
codegen-owned file is rejected by the component contract. This is an owner
closure, not a substitution-percentage increase: parser output is still a
compact AST-text artifact internally, but parser now returns one
`AstTreeArtifact` carrying text provenance, the shared arena, and node count.
The temporary `CodegenAstTextNode` inventory is consumed while constructing the
arena and does not cross the artifact boundary. The integrated driver passes
that same artifact through one `SemanticAstArtifactAnalysis` and into codegen
without rebuilding the arena. Entrypoint cardinality and function owner/name,
parameter name/type/mode, and return-type rows are derived once by semantic and
consumed by function emission, prototype emission, role-operator lookup, and the
codegen type environment. The deleted codegen-owned signature scanner cannot
return. Local declaration name/type/scope/initializer-payload facts now follow
the same path and codegen no longer recovers them from the arena.
The isolated DRV-2 `--emit-c-verified` path joins semantic initializer,
iteration, assignment, expression-use, return, condition, and body facts and
fails closed before codegen. DRV-0/DRV-1 remain the lightweight breadth path.
Wiring the current source scanner into the driver would create a second parser
and does not count.

**Velocity correction (2026-07-12):** the expansion ledger currently has nine
ACTIVE blockers: five direct executable-substitution blockers and four
process/evidence blockers. Despite substantial bounded owner and gate work over
roughly fifteen days, released/default replacement remains 0%. SoT is therefore
enforced as a condition of one active hard-substitution rung, not pursued as an
independent globally complete project. The track uses a 70/20/10 effort split
for executable replacement, build/test feedback, and SoT/process maintenance,
and permits at most two consecutive SoT-only commits before an executable
delta or an explicit blocked record. The accepted process is
`docs/self_hosted/16_hard_substitution_velocity_process.md`.

The first post-correction executable delta moved array-literal body ownership
into `SemanticAstLocalBindingFacts`. The codegen view consumes the typed row,
the old AST-text array-literal owner is deleted, and the component gate forbids
`StringTrim` / `CharAt` structure recovery in the replacement view. The focused
`array_index_assign` fixture produced byte-identical generated C from C-built
and LLVM-built codegen tools (SHA-256
`DD203935F1F28983577975D65F4C3C0E8E679DF3FB45115F5AF9446A9A138756`) and the
generated program matched the committed output. This is one mixed-tree
consumer closure; released/default replacement remains 0%.

The next executable delta first moved try-expression shape into a semantic
local-binding row. That intermediate owner is now superseded: postfix `?` is a
parser-owned `AstExpressionNodeTry` with one operand edge, and hard codegen
consumes only the semantic expression-graph view. The parallel local-binding
operand string and its dedicated codegen view are deleted. A named compact
bridge preserves the same Try graph only while canonicalizing legacy/native
MIR input; it is not a hard-codegen fallback. Focused `option_try` evidence
made C-built and LLVM-built DRV-2 raw MIR byte-identical, made native/self
canonical MIR byte-identical, and made every source/MIR route emit the same C
and committed runtime output. Removing `expr0_graph` fails closed on both
drivers. The full 20-source/15-MIR matrix was not refreshed in this slice, and
released/default replacement remains 0%.

The next executable delta completed recursively nested field reads. The parser
already emitted `MemberAccess(MemberAccess(line, end), x)`; hard codegen now
derives each receiver type recursively from that graph and `LookupFieldType`
rows. It cannot call the text-backed `ExprMemberFieldType` dotted-path scanner.
The `nested_member_access` fixture made C-built and LLVM-built drivers emit
byte-identical MIR and C, executed equal to the native oracle (`3`), and rejected
both a missing graph and an invalid root. The full producer-first gate is green
at 20 source fixtures and 17 MIR fixtures across both driver backends.
Nested-receiver instance method calls are closed in this bounded hard path.

The next executable delta closes namespace-qualified call classification.
`SemanticExpressionCallTargetFact` resolves `Math.Add` through the canonical
callable index, carries `Math_Add` through self MIR JSON, and requires the same
target during direct `--mir-json` consumption. Hard codegen consumes that fact
before method dispatch and no longer rebuilds a namespace symbol from receiver
text. Replacing the carried target with `none` fails closed under C-built and
LLVM-built drivers. The full producer-first gate is green at 20 source fixtures
and 18 MIR fixtures; both drivers emit byte-identical MIR and C, and the new
fixture runs equal to the native oracle (`7`). Widening the hot semantic graph
arena to six growable-array values exposed an LLVM aggregate ABI crash, so the
semantic representation keeps one optional canonical target-name row while the
MIR boundary remains explicitly tagged by `call_target_kind` plus
`call_target_name`. Released/default replacement remains 0%.

The following executable delta closes `for` value/auxiliary graph carriage.
`ParseForStmt` now captures the lower/collection expression in the value lane
and the range upper expression in the auxiliary lane during the canonical
precedence walk. MIR attaches the value graph to `loop-init` and the range-stop
graph to `branch`; direct MIR consumption requires them in that order. Hard
codegen emits range bounds and identifier foreach collections from those node
handles and no longer calls `IntEval(start/end)`, `ExprKind(collection)`, or
`RewriteExpr(collection)` on the statement text. C-built and LLVM-built
drivers emitted byte-identical C for `forloop` and `for_each` (SHA-256
`D39BE785...B57F7D3` and `C17441A...356DD`) and matched outputs `0/1/2` and
`60/abbccc`. Removing only the range-stop or foreach-value graph failed closed
under both drivers. The full 20-source/18-MIR matrix exceeded the five-minute
focused budget and was terminated, so that historical slice claimed only those
two falsifying fixtures. Non-identifier foreach classification was closed by a
later 20-MIR producer run; released/default replacement remains 0%.

The next executable delta deletes the payload-free enum call-argument text
bridge from hard graph emission. A qualified enum argument such as
`IsEast(Direction.East)` is already a parser-owned
`member_access(Direction, East)` subtree, and the type environment owns its
enum projection row. `RewriteSemanticCallArgument` now delegates directly to
that graph instead of reclassifying the source token from the expected
parameter type. The nineteenth DRV-2 MIR fixture emitted byte-identical C from
C-built and LLVM-built drivers (SHA-256
`E4E901D03F43C7429A2E9E033FCC12651D58718897453F0875AE4285D82409A3`) and
both executables printed `east`. Removing only the call graph failed closed on
both drivers while the expected enum parameter and expression text remained.
Array and struct literal arguments remain bounded text bridges; released/
default replacement remains 0%.

The twentieth DRV-2 MIR fixture closes non-identifier foreach normalization.
`SemanticAstIterationTypeFacts` owns the full iterable type and whether the
collection requires a single-evaluation hoist. Self MIR consumes those rows to
emit the same reserved synthetic local and post-order ordinal as native
`forin_desugar`, while direct MIR consumption sees only the normalized local.
The nested/sibling fixture makes native/self canonical MIR byte-identical under
both C-built and LLVM-built drivers and run-equal at `30`; removing the
synthetic source-local type is rejected. The temporary consumer-side callable
return lookup was deleted and is forbidden by the component contract. The
synthetic ordinal lookup reports absence as `Option<Int>`; the likeness gate
forbids reintroducing its former `-1` sentinel.

The twenty-first DRV-2 MIR fixture closes array-literal call-argument
reparsing. `ParsePrimaryFact` now keeps the empty literal root and every element
subgraph instead of collapsing the literal to one leaf. The semantic/MIR graph
and JSON projection preserve that ordered spine, and
`RewriteSemanticCallArgument` emits each element through its node handle; it no
longer recognizes `[` or calls `EmitArrayLiteralValue(source, ...)`. A nested
arithmetic/direct-call fixture made all four C-built/LLVM-built native/self
canonical MIR artifacts byte-identical (SHA-256
`56A2A77CBDCE635ECE29084E378B801C56E5359F31DBDA758DBD391D45A98A13`) and
printed `11`. Reclassifying the literal root as a leaf while retaining the
element chain is rejected as an invalid MIR expression graph. Struct literal
arguments and expression result-type classification remain open; released/
default replacement remains 0%.

The twenty-second DRV-2 MIR fixture closes named struct-literal call-argument
reparsing. The native AST records whether an `AST_CALL` came from braced
initializer syntax, so AST/MIR round trips preserve `Line { ... }` instead of
aliasing it to `Line(...)`. The self-host parser/HIR graph carries explicit
struct-literal, field-name, field-binding, and field-spine nodes. Semantic
identifier checking traverses field values while treating the type and field
labels as declarations, and expected-type codegen recursively emits nested
struct values from those graph edges. The old call-argument struct text
classifier and rewrite fallback are deleted. C-built and LLVM-built DRV-2
drivers are green across 20 source and 22 MIR fixtures; native/self canonical
MIR, emitted C, and execution agree, the nested fixture prints `6`, and
reclassifying a struct-literal spine node as a leaf fails closed. Top-level
struct values outside the migrated call-argument lane and initial compact
bridge graph construction remain open; released/default replacement remains
0%.

The twenty-third DRV-2 MIR fixture closes general named struct-literal value
reparsing for local initialization, assignment, and value return. The semantic
expression graph owns the braced literal and ordered field spines; its nominal
type owner validates the literal type against the canonical constructor row.
Initializer, assignment, and return verdicts consume that graph fact, and
codegen emits through the expected-type graph boundary instead of
`EmitStructValue`. A borrowed expression-surface view yields only the scalar
root handle, so initializer, assignment, statement, and iteration rows no
longer return a graph-bearing view per lookup. C-built and LLVM-built DRV-2
drivers are green across 20
source and 23 MIR fixtures; the value-flow fixture prints `11`, and changing
the carried struct-literal root to a leaf is rejected as invalid MIR. The
legacy text struct emitter remains only on explicit unclosed lanes such as
`Option<struct>` payloads and `CodegenAstTextNode` collection elements; initial
compact graph construction and non-struct result-type classification also
remain open. Released/default replacement remains 0%.

The twenty-fourth DRV-2 MIR fixture closes `Option<struct>` `Some` constructor and
payload reparsing for local initialization, assignment, and value return. It
also carries contextual `None` through local initialization, reassignment, and
value return: the native C assignment consumer obtains the expected option type
from the MIR source-local expression-type fact, the LLVM consumer obtains it
from the MIR local expected-type fact, and both fail closed instead of selecting
an `Option<Int>` fallback. A
shared semantic call-spine view owns the ordered `Some` argument handle, while
the expected `Option<T>` type selects the MIR-owned scalar or struct runtime ABI
row. Codegen emits the payload through the expected-type semantic graph
boundary; it cannot recognize `Some(`, slice payload text, or call the legacy
struct text emitter. C-built and LLVM-built DRV-2 drivers are green across 20
source and 24 MIR fixtures; the fixture prints `7` and `11`, and reclassifying
its `Some(Pair { ... })` call-argument spine as a leaf is rejected as invalid
MIR. The same graph view joins nominal constructor field rows before type
inference: assigning `String` to `Pair.left: Int` is rejected by both the
self-host driver and native oracle. The emitted-C ratchet requires the
`pgy_option_none_Pair()` ABI constructor, and C-built and LLVM-built DRV-2
drivers remain green across 20 source and 24 MIR fixtures.
`CodegenAstTextNode` collection elements, initial compact graph construction,
and non-struct result-type classification remain open.
Released/default replacement remains 0%.

The third executable delta deleted
`codegen/input/ast_text_collection_stmt_owner.pgy`. The parser-owned artifact
was already captured by `SemanticAstStatementFacts`; `ArrayPush` target/value
and `ArraySet` target/index/value now flow through the fail-closed semantic
statement codegen view. The focused `array_push`, `array_sum`,
`str_array_push`, and `str_array` fixtures were run-equal under C-built and
LLVM-built codegen tools, and all four emitted C artifacts were byte-identical.

The fourth executable delta added `SemanticAstEnumFacts` to the integrated
artifact analysis and deleted `codegen/input/ast_text_enum_variant_owner.pgy`.
`CollectEnums` now consumes semantic enum names, ordered variants, and payload
arity through a fail-closed codegen view; enum aux text is no longer parsed in
codegen. Native and self-host AST printers now preserve variant parameter
types; parser parity is 188/188 on C and LLVM with live drift enabled.
`enum_match` remains run-equal and byte-identical across codegen tool backends,
while `codegen_parity.sh` requires both tools to reject the TestHarness-owned
two-parameter payload-enum artifact with the same committed fail-closed
diagnostic. Semantic enum capture uses nested comma ranges, so
`Rect(Int, Int)` is one variant with arity two rather than two rows.

The fifth executable delta reused the already integrated
`SemanticAstNominalConstructorFacts` owner for nominal names and ordered field
name/type rows. `CollectStructs` no longer walks nominal/field AST rows; the old
mixed declaration owner was deleted and the remaining role bridge was renamed
to its exact responsibility. Four struct fixtures are run-equal under C-built
and LLVM-built codegen tools.

The sixth executable delta added `SemanticAstRoleFacts` for role name, target
type, and owned method `NodeId` rows. Operator binding and role receiver ABI now
consume those rows; the role AST bridge and descendant scan are deleted. The
TestHarness-owned role operator prints `123` under C-built and LLVM-built
codegen tools, matching the native C oracle.

The seventh executable delta moved runtime/header expression usage to
`SemanticAstExpressionSurfaceFacts`. Codegen keeps builtin-group policy but no
longer reads arena atom/value/auxiliary rows or parses calls locally. Nine
runtime-family fixtures plus the role/enum hard legs are run-equal under
C-built and LLVM-built tools.

The eighth executable delta moved canonical runtime type usage to
`SemanticAstTypeSurfaceFacts`. Codegen no longer scans arena type-name rows.
The LLVM leg exposed and then closed one missing concrete `String` unwrap fact;
the same nine runtime-family fixtures now pass C/LLVM parity.

The ninth executable delta moved runtime statement-kind usage to
`SemanticAstKindSurfaceFacts`. Codegen no longer scans arena kind rows, and the
old local tag named `ArrayLiteral` was removed because canonical tag 16 is
`ArrayPopStmt`. Five kind-driven fixtures plus the enum/role fail-closed legs
are run-equal under C-built and LLVM-built tools. Runtime usage projection now
accepts only semantic expression, type, and kind facts; its dead arena/count
parameters are gone.

The tenth executable delta moved `Main` cardinality and selected function-node
identity behind `SemanticAstFunctionSignatureFacts`. The semantic verdict now
counts signature names, and codegen consumes an `Option<Int>` projection rather
than rescanning arena function/name rows or using `-1` as hidden control flow.
The helper-before-Main `func_call` fixture and `hello` pass C/LLVM parity.

The eleventh executable delta moved statement dispatch to three semantic
authorities: local-binding identity for `Let`, assignment identity for
`Assign`, and statement kind rows for all remaining emitted statements.
`Defer`, `Break`, `Continue`, and `MatchDefault` were added to the statement
inventory; twenty codegen arena predicates were deleted. Twelve representative
fixtures pass C/LLVM parity while `Else`/`Block`/`Then` remain syntax-structure
traversal rather than semantic fallback.

The twelfth executable delta moved top-level declaration dispatch to semantic
node identity. Function signatures own function nodes; nominal, role, and enum
rows own their declaration nodes. `program_emit.pgy` no longer classifies those
four declarations through codegen arena predicates. Seven focused declaration
fixtures plus payload-enum rejection and role-operator parity pass under both
C-built and LLVM-built self-host codegen tools.

The thirteenth executable delta completed top-level declaration classification:
ability and event nodes now consume canonical `SemanticAstKindSurfaceFacts`.
The earlier runtime-consumer-specific owner label was generalized to node-kind
surface ownership rather than duplicated. Event rejection is now a committed
TestHarness negative leg, and no codegen arena declaration predicate remains.

The fourteenth executable delta moved top-level expression operator positions
into `SemanticAstExpressionSurfaceFacts`. The owner stores normalized
atom/value/auxiliary payloads and compact operator rows. `Log` emission now
looks up its atom row by node identity, and the role-operator path consumes the
stored additive index and operator kind instead of calling `FindTopLevelPlus`.
The fixture proves `+` dispatch without misrouting `-`. The migrated
function is ratcheted against that fallback, and C/LLVM-built tools remain
oracle-equal. Value/auxiliary consumers and recursive child expressions remain
the active mixed-expression bridge.

The fifteenth executable delta extended the same authority without adding a
new expression owner. Scalar/String returns consume atom-lane shape rows;
ordinary scalar/String local initializers and assignments consume value-lane
rows. Five focused fixtures plus the role and negative declaration legs pass
under C-built and LLVM-built tools. Indexed collection values,
Option/Result/struct wrapper internals, auxiliary lanes, and recursive child
expressions remain bridge consumers.

The sixteenth executable delta made logical/comparison root facts precise:
separate `||`, `&&`, equality position, and equality-kind rows now drive
`if`/`while` lowering. The shape-aware condition function cannot call
`FindTopLevelOp2`; both paths share one String/enum equality projection.
Logical precedence and String equality fixtures pass C/LLVM parity. Recursive
child conditions remain the next expression-tree seam.

The seventeenth executable delta added stable semantic expression node handles
and child edges for `if`/`while` condition atoms. Recursive condition emission
now traverses those edges and cannot call `RewriteBool`, `FindTopLevelOp2`, or
`Substring` to rediscover precedence. The grouped `(a || b) && c` fixture
prevents flattening from changing meaning; C-built and LLVM-built codegen tools
emit byte-identical C and remain run-equal on logical and String-equality
fixtures. The owner remains a bridge because graph production still lowers the
compact parser payload instead of consuming parser-arena expression nodes.

The follow-up ratchet deleted the final dead codegen arena payload views:
atom, type, value, auxiliary value, parameter type, and parameter mode. The
remaining mixed-expression blocker is therefore exact: semantic owner rows
still carry expression text into lowering. Codegen no longer has a direct arena
payload recovery API that can bypass those owners.

The same bounded closure is now modeled in
`docs/semantics/proofs/SoTAuthority.v`. Rocq/Coq checks owner completeness,
uniqueness, required consumption, and zero semantic fallback, while
`tests/sot_authority_adequacy_smoke.sh` binds those names to the live semantic
owner and codegen consumer and mutation-tests missing-owner and fallback
reintroduction. The bounded model now covers the array-literal body, try-let
operand, collection-mutation statement, enum declaration, and nominal/field
declaration and role operator consumers; it
does not increase released/default replacement or close the remaining
mixed-expression consumers.

The whole compiler skeleton now has a machine-gated 28-row owner declaration in
`docs/semantics/sot_owner_spine_registry.md`: 15 architectural rows plus thirteen
bounded self-host closure rows, with twelve `CLOSED`, seven `BRIDGE`, and nine
`ACTIVE` rows. Each row names its stable handle, Coq fact/owner,
authority implementation, last consumers, forbidden fallbacks, gate, and open
reason. `tests/sot_owner_spine_contract_smoke.sh` validates the live bindings
and mutation-rejects missing rows, duplicate ids, missing owners, Coq drift,
invalid status, and dishonest `CLOSED` promotion. This defines the owner
outline; it does not raise the released/default replacement percentage.

## Headline Number

### Three-axis scorecard

These numbers must not be collapsed into one percentage:

| Axis | Current evidence | Meaning |
|------|------------------|---------|
| Implementation inventory | 30,720 frontend/backend LOC / 287,406 C-reference LOC = 10.69%; broader Pergyra compiler-core inventory = 48,246 LOC | Pergyra compiler code exists; this is not substitution. The ratio denominator is the C reference, not the Pergyra compiler-core inventory. |
| Bounded executable replacement | DRV-2 has 20 producer-first source semantic fixtures, 24 canonical MIR producer/consumer fixtures, and the standalone fact-only MIR consumer has 96 fixtures | Explicit Pergyra-owned paths run, fail closed, and compare against the C/LLVM oracle. |
| Released/default replacement | 0% | default `pgy` still uses the C-owned native driver; explicit DRV-2 uses the Pergyra MIR producer and consumer. |

The scorecard prevents two false claims: implementation volume must not be
reported as native replacement, and native replacement at 0% must not erase
measured progress in bounded executable rungs.

**Hard self-host contract (2026-06-22):** hard self-host is now gated as
staged substitution rather than tracked as a separate cleanup project. The
contract lives in `docs/self_hosted/10_hard_self_host_contract.md`, and
`tests/self_host_hard_contract_smoke.sh` keeps the docs, Makefile wiring, active
hard rungs, C oracle, LLVM oracle, bridge/fallback split, codegen bootstrap, and
MIR JSON fact-only lowering aligned. The substitution percentage below is
unchanged by that contract gate; future percentage increases require a Pergyra
implementation to replace a real compiler stage/pass beside the C/LLVM oracle.

**Implementation inventory is live-measured, not a substitution percentage.**
Run `make self-host-progress-metric-test-smoke` to measure the current Pergyra
frontend/backend and compiler-core LOC beside the C reference inventory.
Implementation volume only proves that code exists.
**Released/native replacement remains 0%** because native compile and the
default path still use the C compiler. **Explicit bounded replacement: DRV-2 is
live** through `make self-host-compiler` and
`pgy --self-driver <source.pgy>`; unsupported inputs fail closed instead of
falling back to the C pipeline.
The verified component frontiers are the
lexer, parser, a bounded semantic verdict rung, and -- as of 2026-06-17 -- the
**first codegen rungs** (`src/self_hosted/codegen/`, 4,821 LOC; rung-0 string Log,
rung-1 integer let/arithmetic, rung-2 assign + `while`/`if`/`else`, rung-3
multi-function definitions + calls + `return`, rung-4 `String` types with a
variable/function type environment + runtime `pgy_concat`, rung-5 `for` loops +
`break`/`continue`, rung-6 `Bool` type + `StringLength`/`Substring` builtins,
rung-7/8 fixed `Array<Int>`/`Array<String>` literals + indexing +
`ArrayLength`/`ArraySet`, rung-9 `StringIndexOf` builtin + `Exit`, rung-10
**growable arrays** (`ArrayPush`) via a `{data,len,cap}` struct rep with
env-aware index-expression rewriting, rung-11 `StringTrim` builtin, rung-12
`FileExists`/`ReadFile` file I/O, rung-13 `Args()` user-argument snapshots,
rung-14 value-passed Int-field structs, rung-15 `Array<Int>` param/return flow,
rung-19 typed `Int` / `Bool` / `Float` / `String` struct field facts, and rung-20 nested struct-valued field facts).
The codegen entrypoint is now split into thin `main.pgy` orchestration plus
resource-owner folders: `input/` for AST path/read ownership and codegen-only
views over the shared HIR artifact. `hir/` owns compact AST-text inventory,
typed `CodegenAstTextRowFactInput` row facts, marker-node predicates and
function/return/enum/nominal/role/parameter/field payload accessors plus
statement row facts projected into typed arena rows for
`Let`, `Assign`, `Log`, `Return`, `Defer`, `ArrayPop`, `ArraySet`, `ArrayPush`,
`Exit`, `Break`, `Continue`, `For`, `While`, `If`, `Else`/`else if` routing,
and bare call statements for the transitional `pgy --ast` bridge. `run/` owns
the CLI boundary, `text/` owns codegen-specific expression facts, and
`type_facts/` owns type
evidence, compiler-world symbol rows for emitted-symbol spelling including
namespace-qualified call lowering,
`abi_layout/` for self-host C ABI type spelling, `runtime_abi/` for `Array<Int>` /
`Array<String>` plus bootstrap `Array<CodegenAstTextNode>` C collection runtime
helper symbol spelling, supported
math/random helper and target-library symbol spelling, supported host
file/stdin/argv/process helper, C process entrypoint ABI, and target-library symbol spelling, supported
`Option<Int>` / `Option<String>` / `Result<Int>` helper symbol spelling, and supported string/text
helper and conversion target-library symbol spelling, and `emission/`
for C-emission action participants. That keeps
`program_emit`, `function_emit`, `stmt_emit`, `expr_rewrite`, and
`struct_value_emit` out of fake zone folders while still making the real
resource owners visible. Parameter-mode facts (`inout` / `own` / `ref`) now
survive `pgy --ast`; the self-host C codegen consumes `inout` from function-env
`pm` facts and lowers it as value-result copy-in/copy-out instead of guessing
from `ArrayPush` or other statement text. Top-level comma-separated expression
sequences for array literals, call arguments, and struct literal field lists now
route through `text/expr_sequence_owner.pgy` instead of local emission loops,
payload-free enum literal projection routes through
`text/enum_literal_owner.pgy` instead of local enum-key reconstruction,
struct literal call-envelope facts route through
`text/struct_literal_call_owner.pgy`, and typed struct literal field-entry row
facts route through `text/struct_literal_field_owner.pgy`.
The M2 completeness ledger now inventories and ratchets
250 production self-host source files across lexer, parser, semantic, codegen,
and full-pipeline identity. The ledger itself is not a bootstrap-loop proof: it
still runs through the current C/LLVM oracle compiler path and proves source
breadth. A separate fixed-point gate now proves that the Pergyra-built bounded
parser/semantic-entrypoint/codegen driver can rebuild itself; broader semantic
and MIR inclusion remain the next compiler-pipeline bootstrap boundary. The
real-source semantic selfcheck uses the broad
203-source C/LLVM gate from the latest parity preparation refresh over the current accepted semantic subset,
including the codegen run boundary, lexer run/fixture-manifest owners, emission
action owners, type-fact owner, MIR-lower fact owners, and SEA execution-lane
mirror. The
AST-text bridge's root/body/block/then
structural marker checks now consume owner-owned `kind` facts rather than raw
line-text equality, and program/function/statement emission-depth traversal now
consumes typed arena indent/parent facts rather than raw `CodegenAstTextNode`
indent rows. `GenerateCUnit` builds that typed arena projection once and threads
the `AstArena` fact through function and statement emission participants instead
of letting recursive emitters rebuild it. Function signature emission consumes
`SemanticAstFunctionSignatureFacts` from the shared artifact; declaration
emission still consumes typed arena rows for role targets, enum names, and
fields instead of reading `CodegenAstTextNode.name`, `type_name`, or `mode`
directly. Statement emission consumes semantic-owned node/function/scope/
payload rows for `Log`, value `Return`, `ArrayPop`, `Exit`, `While`, `If`,
`Match`, match cases, and bare calls. `Let` and `Assign` consume semantic local,
initializer, target/base/index/RHS, expression-use, and type-verdict rows;
missing facts fail before codegen. `ArrayPush` emission consumes arena atom/value rows
for the receiver and pushed expression; `ArraySet` consumes arena atom/value/
aux-value rows for receiver, index, and assigned value; `For` consumes the
semantic statement/iteration owner projection for loop variable, range
start/end, foreach collection, and binding type. Program/function/statement
routing and marker checks consume arena
kind facts. Runtime/header usage facts now consume lane-specific arena facts:
type/header requirements read typed arena `type_name` rows, builtin-call
requirements scan only expression-bearing arena rows with string-literal-aware
call matching, and statement-only requirements continue to consume arena kind
facts. The deleted raw-node usage bridge cannot return.
The rest of codegen,
runtime and released/native compiler driver/LSP substitution are still 0%.
The compiler driver now has DRV-0/DRV-1 artifact rungs plus a bounded integrated
parser/codegen bootstrap fixed point, and LSP has LSP-0
diagnostic payload, LSP-1 squiggle-policy projection, and LSP-2a..LSP-2i
buffered transport/request/response/session/document-state/feature-shape/session-state/hover-content rungs
(docs/150).
The DRV-0/DRV-1 artifact rungs consume the same 69 committed codegen parity
fixture frontier as `codegen/fixture_manifest_owner.pgy`; this broadens
artifact assembly coverage. The fixed point proves a real Pergyra-built
source-to-C compiler loop, but neither it nor DRV parity counts as
released/native driver replacement until semantic and MIR stages enter that
same executable path.
The separate DRV-2 `--emit-c-verified` entrypoint makes artifact-body semantic
evidence a hard precondition without calling the source-scanning checker. It
joins initializer, iteration, assignment, expression-use, call, return, and condition
verdicts before C emission. Its semantic cost is isolated from DRV-0/DRV-1 and
ordinary codegen checks. It is a bounded hard-semantic rung, not yet the full
CFG/MIR body replacement claim.
The DRV-2 body gate runs twenty manifest-owned positive/negative fixtures
through both C-built and LLVM-built drivers, compares emitted C or diagnostics,
and C-compiles every positive artifact. The builtin signature table, canonical
type-name owner, and shared source character/trivia scanner are single owners;
the integrated driver may not recover these facts from source text.
The same executable owns a bounded typed-artifact MIR producer and accepts
`--mir-json <file>` through the existing MIR fact consumer. Source mode now
follows `artifact -> verified MIR rows -> pgy.mir.v1 -> MIR consumer -> artifact
verifier -> codegen`; it no longer calls the C MIR producer. Twelve intersection
fixtures (linear local/log, range loop, nested CFG with phi, indexed assignment,
payload-free enum return, explicit if/else phi, parameter carriage, recursive
calls, nominal method construction, typed array foreach, and parser-owned array
index read plus parser-owned logical-not)
require C/LLVM-built drivers
to produce stable canonical reconstructions, emit identical C, and run-equal.
This is not a raw native-MIR byte-equality claim: the indexed fixture therefore
also gates its pre-canonical self-MIR target and use rows directly. Indexed
assignment keeps its full semantic target text in the MIR emission payload
while SSA identity continues to use the base local name. The default native
compiler path remains unreplaced.

The foreach producer no longer assumes every loop binding is `Int`. DRV-2
carries the semantic iteration owner rows through `DriverRung2VerifiedFacts`,
projects the verified node/type rows into `SelfMirRoutineInput`, and emits
range or foreach MIR from that owner. The gate directly pins `Int` and `String`
foreach binding types plus collection SSA uses. Non-identifier collections now
carry their verified iterable type and one explicit hoist fact in those same
rows. The MIR producer evaluates the expression once into the reserved
`__pgy_forin_N` local and gives the loop only that local; MIR consumption and
codegen do not recover a callable return type from expression text. A nested
plus sibling call-foreach fixture fixes the native post-order names
`__pgy_forin_0/1/2`, and deleting a required synthetic source-local type fails
closed. C-built and LLVM-built full DRV-2 gates are green at 20 source and 22
MIR fixtures; all four canonical native/self JSON artifacts for this fixture
have SHA-256
`19815C3CD3E5C3B36AA9F70EF9241BC8105CAE5B7FFA739DA36E3B6D7F06FCCB`,
and the native/self executables both print `30`. Self-produced MIR also owns
the `parallel_capture_boundaries` inventory, including the empty bounded-subset
case, so `pgy.mir.v1` output remains consumable by the same self-host path.

The same slice removed two compiler-scale quadratic scans. Typed AST parent and
child rows now use an indentation stack plus CSR-style child offsets, and AST
text inventory uses one `Split(tree_text, "\n")` pass instead of one-character
`Substring` scanning. On the 996,867-byte, 24,340-row DRV-1 artifact, the cached
self-host codegen check measured 53,003 ms before the line-inventory change and
374 ms after it on the same machine and generated artifact.
C LSP also exposes `pgy-lsp --dump-diagnostics <src>` as a live oracle
plumbing path for LSP diagnostics shape checks and fixture-level canonical
event comparison across clean plus logical/undefined/type/condition/unary
diagnostic families, but those are not counted as released driver/LSP
replacement.
The MIR-lowering
substitution has now *started* (see below).

**MIR-lowering substitution started (2026-06-18, path (a) rung-0b):** the C
compiler now emits MIR JSON (`pgy --mir-json`, schema `pgy.mir.v1`) with the CFG
skeleton, explicit expression/source-shape facts (`expr0`, `expr1`,
`source_type`), source-local type facts (`source_locals`), and a transitional
`ast` compatibility text field captured by the MIR source-shape owner. A new
Pergyra owner graph `src/self_hosted/mir_lower/` consumes that JSON and
reconstructs the `--ast` tree, which the existing codegen lowers to C. It is
available both as a standalone tool and inside DRV-2 before the same semantic
artifact verifier. The whole MIR -> C path is Pergyra and run-equivalent to the
C backend on the supported rung-0b CFG
subset (linear code, signatures/return, if/else, nested if, while, and
`for i in a..b`), plus selected codegen fixture surfaces that already lower from
MIR facts (args, arrays, Bool/string/Float builtins, Bool-literal branch
reassignment, straight-line calls, direct integer arithmetic, builtin-name
string literals, directory walking, exit-guard branches, multiple Void routines
with bare-call statements, string concat/equality, `Result<Int>` `?`
early-return flow, recursion, loop-control
`continue`/`break` edge blocks, trailing-newline Log normalization, nested
string concatenation, string array concatenation, string case/index/trim
builtins, `Join`/`ToFloat` string utility flow, array pop, array for-each,
array sort/map/filter/reverse combinators, `Result<Int>` core constructors and
inspection helpers, typed struct field declarations/value flow,
plain class/subject/object/tobject/vessel declarations and class methods through MIR-owned
nominal-kind/field/method/owner facts,
payload-free enum declarations through MIR-owned variant facts,
break edges after non-empty statement blocks, inferred `Random()` Int locals,
match-case integer pattern conditions, runtime-aligned absolute-path I/O policy,
file read/write, Long scalar flow, array index assignment, `Option` `?`
propagation, string equality-plus-concat flow, C-reserved binding spelling,
payload-free enum match comparison projection, Float signatures, seeded random
flow, string-array index return flow, and phi-bearing loop headers classified
by CFG backedges rather than phi presence alone, plus MIR-owned array destructure
binding facts), gated by
`parity/mir_json_parity.sh`
(`make self-host-mir-json-parity-test-smoke`, 96 fixtures plus 0 clean-reject
fixtures). The gate now
requires the MIR JSON fact surface and checks the `for`
header is reconstructed from `arg0` plus `expr0`/`expr1` bounds, and checks
struct/class declarations, nominal family declarations, owner-qualified class methods, payload-free enum
variants, match-case integer branch conditions reconstructed from
`match_patterns`, and `Option<Int>` `Some(v)`/`None` branches reconstructed from
MIR-owned `match_variant` and `match_bindings` facts. It also checks nested `if`
branches inside loops are not misclassified as loops from phi facts alone. The
gate checks destructure binding-name facts and rejects unsupported declaration
facts before generated C emission. It rejects
reintroducing reads of the transitional `ast` compatibility text. This is the
first verified slice of the actual compiler-core (~96% of the LOC), not the
codegen subset. It is now fact-only for the supported MIR JSON statement,
expression, source-local, CFG, match-case, I/O policy, typed struct field
declaration, field-only class/subject/object/tobject/vessel declaration/method,
ability signature declaration, payload-free enum surfaces, and the Int role
operator dispatch surface. The committed MIR-lower/codegen fixture inventory is
currently **96 PASS / 0 gap plus 0 clean rejects** through this
path. The nominal family now flows through MIR-owned `nominal_kind`/field facts
and reconstructs `Class:` / `Subject:` / `Object:` / `TObject:` / `Vessel:`
instead of collapsing those labels to a generic class alias. Ability
declarations now flow through MIR-owned method signature facts and are treated
as zero-artifact declaration hosts by the self-host codegen pre-passes. Role
declarations now flow as MIR-owned `kind:"role"` facts with `for_type`, impl
ability spans, and method signature facts; the supported Int/`Arithmetic.Add`
operator path is consumed by self-hosted MIR lowering/codegen instead of being a
clean-reject boundary. Payload-free enum variant lists are consumed through
typed arena aux-value rows in self-host codegen.
Richer projection/identity semantics beyond field-only nominal
declarations and payload-bearing enum variants remain observable boundaries, so the
self-host path fails closed instead of silently
dropping operator-overload/domain nominal semantics or emitting undefined C
symbols. New fixtures must preserve that by adding owning facts rather than
text fallback.
`self_hosted_component_contract_smoke` now also ratchets that frontier against
the parity harness itself: the MIR JSON positive fixture inventory must stay at
95, the clean-reject inventory must stay at 0, the scorecard must cite the same
96 PASS / 0 gap plus 0 clean reject boundary, and stale fixture-count wording
is rejected. The positive inventory now includes `examples/binary_search.pgy`
as an example-origin fixture after all 70 committed self-host codegen fixtures,
not only purpose-built MIR-lower fixtures.

The self-hosted `mir_lower/` implementation is now split by source-of-truth
owner rather than living as one monolithic `main.pgy`: `error_owner` owns the
stage-specific `MirLowerFailClosed` boundary, `mir_json_input_owner` owns argv/file/schema input gating,
`json_fact_read` owns bounded JSON/MIR fact access, `decl_lower` owns declaration
inventory reconstruction, `program_lower` owns document-order Program assembly
and supported routine selection, `routine_inventory_owner` owns routine
discovery and bounded routine header facts, `routine_lower` owns CFG/body
reconstruction for a selected routine, and `stmt_render` owns instruction fact
-> AST statement rendering. The entrypoint `main.pgy` is orchestration only, and
each `mir_lower` source file is below the 600-line owner cap.

**Hard migration opened (2026-06-17):** the codegen rung is the first *hard
compiler-core* substitute, landed after the BDFL decision lifted the
`docs/self_hosted/README.md` freeze. Hard migration proceeds rung-by-rung, each
gated against the C/LLVM oracle before the next opens -- not as an unverified
compiler fork. See `src/self_hosted/codegen/README.md`.

**Self-hosting achieved for codegen (2026-06-17, strengthened 2026-07-02):**
the codegen tool *self-hosts*. A Pergyra-built copy of the owner graph emits C
that gcc-compiles and **reproduces its own source-compilation exactly** --
`gen2 == gen3` byte-identical (last observed 9916 generated-C lines) -- and the
Pergyra-built tool emits byte-identical C to the oracle-built tool on the sample
fixtures. Breadth: the same codegen also compiles the lexer (587 lines) and parser (3338 lines); each codegen-built binary matches its oracle-built counterpart on a sample source -- three real self-host components self-built. Wider survey: the codegen compiles **all 22 of 22** committed self-host components/tools to valid C, each verified run-equivalent to the oracle-built binary on a sample -- the entire committed self-host toolchain (lexer, parser, semantic, codegen itself, + 18 audit tools) is self-built by the Pergyra-written codegen. This includes namespace-imported audit tools (`TextScan::` qualified calls, flattened to `NS_Func` -- import/namespace + DirWalk support added). The earlier 18/22 ceiling was a `pgy --ast` bug (for-each `for x in lines` rendered as `For: x in (null)..(null)`, dropping the collection); FIXED in src/parser/ast_print.c (emit the iterable) + the self-host parser, regenerated 5 parser fixtures, and added for-each lowering + bare-void-return + word-boundary builtin matching to the codegen. The latest hard gap was the typed AST arena fixture exposing that the self-host codegen only knew `Option<Int>` ABI/runtime facts. FIXED by adding `Option<String>` to compiler ABI rows, runtime ABI owner symbols, expression kind facts, and typed `Some`/`None` emission. Parser parity (188 manifest rows) stays byte-equal. The bootstrap gate verifies codegen self-hosts (gen2==gen3) + builds lexer + parser + semantic + mir_lower + 13 audit tools and the backend fuzz generator, all matching oracle-built. Gated by `parity/codegen_bootstrap.sh`
(`make self-host-codegen-bootstrap-test-smoke`).

Reaching the fixpoint drove out and fixed real gaps: `else if` chains,
string-literal-safe builtin rewriting, recursive `Concat`/`ToString`/call-argument
lowering (`Concat` -> `pgy_concat` is a pure name rewrite -- same args -- so it
lowers anywhere), bare-call statements, **string `==`/`!=` -> `strcmp(...)==0`**
(C `==` on `char*` compares pointers; the silent root cause of a non-working
first attempt), and a latent **forward-declaration bug** -- Pergyra arrays pass by
value with a shared element buffer, so `ArraySet` persists across calls but
`ArrayPush` does not; the per-`EmitFunction` `protos` push never reached
`GenerateC`, leaving prototypes empty (fixtures worked only because callees
precede callers). Fixed with a `CollectProtos` pre-pass.

This is component and bounded integrated-driver self-hosting, not the whole
compiler. DRV-2 now composes the Pergyra MIR producer, MIR consumer, semantic
verifier, and codegen for the supported intersection. The C oracle produces MIR
only for canonical parity evidence. Whole-language source-to-MIR coverage, the
rest of codegen, runtime, and released/native compiler driver/LSP replacement
remain open.
The compiler driver has DRV-0/DRV-1 artifact rungs, and LSP has LSP-0
diagnostic payload, LSP-1 squiggle-policy, and LSP-2a..LSP-2i buffered
transport/request/response/session/document-state/feature-shape/session-state/
hover-content rungs. The C LSP dump flag
`pgy-lsp --dump-diagnostics <src>` provides live oracle plumbing for the LSP
payload gate plus fixture-level canonical event comparison across clean plus
logical/undefined/type/condition/unary diagnostic families, but neither LSP rung
is a shipped replacement (docs/150).

**Real-example round-trip (2026-06-17):** beyond the 35 hand-written parity
fixtures, the codegen tool was surveyed against all 118 `examples/*.pgy`. It
compiles **20** to run-stdout-equal output vs the oracle (binary_search,
hash_map, linked_list, queue, deque, graph_bfs, insertion_sort, union_find,
break_continue, for_test, class_test, etl_workflow, hello, + 7 contract/
projection/transfer minimals); 86 are correctly rejected as out-of-subset with an
observable `Exit(1)`; 12 fail under the oracle itself (C-skip). Two bugs surfaced
from the C/LLVM/Pergyra tri-compare: (1) a **codegen self-bug** -- `Log(<int>)`
logged directly (not via `ToString`) was emitted with `%s`; fixed by routing
`Log` / array-index element types through `ExprKind` (silent-failure examples
11 -> 0). (2) an **oracle bug** -- the C and LLVM backends miscount arity for
`Array<String>` parameters (the self-host emitter handles them correctly); filed
separately.

**Lexer parity (2026-06-23):** the committed lexer gate compiles the
Pergyra-origin lexer through both C and LLVM, then proves byte-equal token
output and live `pgy --tokens` drift on 8 source fixtures:
`hello`, `array_literal`, `break_continue`, `basic`, `heap`, and
`binary_search`, plus backend-compare `string_escape_sequences` and
`block_comment`. `main.pgy` is
now only the entrypoint; character/codepoint classification, token keyword/line
rendering, and the scan loop live in `char_owner.pgy`, `token_owner.pgy`, and
`scan_owner.pgy`; lexer tool input is only `Args()[0]` or the no-arg
`examples/hello.pgy` default. The broader lexer scale probe now measures
**993 of 993** examples + backend_compare sources byte-equal to the C lexer
oracle.

**Parser at scale (2026-06-23):** the Pergyra-origin parser produces
byte-equal output vs `pgy --ast` on **120 of 121** committed
`examples/*.pgy` files. There are now **zero byte-drift cases** in the
scale probe: every example that both the live C oracle and the self-host
parser complete is byte-equal. There are also **zero self-host parser exits**;
the one remaining non-match fails under `pgy --ast` itself and is a C-skip
(`secure_slots`). The scale probe is a
coverage probe, not a hard parity gate, but it now fails closed: it removes any
stale generated parser binary before compile and exits if compile does not
produce a runnable parser. The probe and parser entrypoint consume source paths
only through `Args()[0]`; the old `fixture/source.txt` side channel is retired.
The file-based probe exposed an `if let Some(resource)` payload loss in the
self-host parser's generated C, now closed by `ParseIfLetPayload` returning the
payload fact instead of relying on branch-local `String` reassignment.
Previous historical match counts:
105 -> 86 -> 83 -> 80 -> 79 -> 77 -> 72 -> 72 -> 63 -> 59 -> 58 -> 57 -> 53 -> 48 -> 46 -> 43 -> 37 -> 25 -> 11.
Refresh:
`bash tests/self_hosted/parity/parser_scale_probe.sh --failing`.

**Rung-1 parity (2026-06-16):** the committed
`parser_parity.sh` now consumes a **188-row** source/fixture manifest emitted
by `fixture_manifest_owner.pgy` vs `pgy --ast` on both generated C and LLVM parser binaries
(was 83 on 2026-05-29; +103 overall). The added fixture surface covers Option/Result
destructure, slot sugar, transfer short syntax, array literal,
common collection algorithms (queue, stack, deque, heap,
linked_list, hash_map, union_find, graph_bfs), string + stdlib +
io + math builtin surfaces, async/spawn/select/defer/for control
flow, pipe + try operator, ownership /
concurrency / event demos (event_basic, event_minimal,
event_lambda, event_lambda_full, event_closure_probe),
notebook_style_analysis, tagged_union, battle_*, calendar_*,
beta_*, intent contract minimal shapes, authority contract,
action contract inheritance, generic ability requires, four
ad-hoc bsd_test fixtures and the full 11-file bsd_test{,2..11}
family, qubit_test, qubit_quantum_ext, RemoteFuture, for_in_array,
generic_class, subject_object_tobject, vessel_method_test,
test_parallel, eda/etl workflows, channel_parallel,
producer_consumer, projection_*, collections_closure_probe,
class_method_test, channel_test, spawn_blocking_test,
import_test, slots, slots_simple, and a deep nested generic type fixture
(`HashMap<String, List<HashMap<Int, Array<String>>>>`). The parser parity
gate now compiles the Pergyra-origin parser through both C and LLVM before
comparing each fixture byte-for-byte.

Examples that **cannot** be added as fixtures (current state):
- `pgy --ast` itself fails (skipped):
  `secure_slots`.
- Self-host parser byte-drifts vs live `pgy --ast`:
  none as of the 2026-06-22 scale probe.
- Self-host parser exits before producing byte-equal AST:
  none as of the 2026-06-22 scale probe.

Reading this honestly: the self-host journey has *just started*. The
first compiler-internal substitute (`src/self_hosted/lexer/`) lands a
Pergyra-written lexer that handles the measured examples + backend_compare
token surface byte-for-byte.
The parser (`src/self_hosted/parser/`) follows at ~52%: it covers a real
domain grammar subset and has C/LLVM byte-equal parser parity over the
committed fixture set, but still stops short of the remaining scale-probe
exit list and the full parser recovery surface.

Compiler-stage substitutes mirror the C-side `src/<component>/` layout
as siblings of `src/self_hosted/` (`lexer/`, `parser/`, `semantic/`,
`codegen/`, `air/`, `hir/`, `mir/`, `compiler/`, `runtime/`, `lsp/`).
Everything listed under `src/self_hosted/tools/` is *peripheral audit
tooling*. Those tools do not substitute any compiler component; they
only observe text artifacts the C compiler produces. Their LOC is
**not** counted in the substitution percentage.

## Component Coverage

| Component       | C LOC   | Pergyra implementation LOC | Executable coverage | Status            |
|-----------------|---------|----------------------------|---------------------|-------------------|
| `src/lexer/`    |     921 |         825 | measured corpus parity | **993 of 993 sources byte-equal** (examples + backend_compare). `main.pgy` is entrypoint-only; run-boundary, fixture manifest, source input, character/codepoint handling, token classification/output formatting, and scan-loop state are separate SoT owner modules. `scan_owner.pgy` declares its real owner dependencies (`char_owner.pgy`, `token_owner.pgy`), and the lexer run/fixture-manifest owners are part of the real-source semantic selfcheck set. Escaped strings, interpolation, and doc/block comments are covered by the measured corpus. 7 representative sources are committed as parity fixtures. |
| `src/parser/`   |   20579 |        8355 | ~52%     | `src/self_hosted/parser/` parses 188 source/fixture rows byte-equal `pgy --ast` on both C and LLVM parser binaries, and **120 of 121** `examples/*.pgy` byte-equal at scale (2026-06-22; zero byte-drift, zero self-host parser exits, 1 C-skip). Parser ownership is now split into declaration, expression, statement, import/source, cursor, type-name, diagnostic, tree-text, run-boundary, and fixture-manifest owners; `main.pgy` is parser-tool entrypoint only. |
| `src/semantic/` |   46203 |        7792 | rung-2 subset | Checks a bounded function-body subset against the C compiler oracle on C/LLVM-generated binaries across 111 fixtures, including nested generic signature canonicalization, Option `?` payload propagation, and Result core consumption. Artifact-native DRV-2 additionally owns signatures, nominal constructors, locals, assignments, iteration, statement typing, and ordered body verdicts without source rescanning. |
| `src/codegen/`  |  107123 |        7220 | rung-0..21 | **C-emit rung-0..21 (2026-07-12).** The Pergyra emitter covers the committed scalar/string/array/result/option/struct/defer/file/stdin/argv/random/float/long subset across 69 run-equal fixtures. HIR owns the compact AST inventory, row facts, `AstTreeArtifact`, and shared `AstArena`; parser produces that artifact and codegen consumes it without rebuilding the arena. Codegen owns only its arena view, type/symbol/ABI/runtime-call facts, and emission participants. The standalone codegen and integrated parser/codegen driver have separate byte-identical `gen2 == gen3` fixed points. TextBuilder now owns program assembly and hot token rewrites. Out-of-subset input is an observable `Exit(1)`; the released default path remains C-owned. |
| `src/runtime/`  |   29627 |           0 | 0%       | native runtime kernel stays C; portable runtime policy libraries may move later |
| `src/compiler/` |   43304 |        9389 | bounded producer-first DRV-2; released 0% | `driver_pipeline_owner.pgy` owns the shared source->AST spine. DRV-2 composes artifact-native semantics, Pergyra MIR production, MIR consumption, and codegen; C MIR is oracle evidence only. The default native driver remains C-owned. |
| `src/lsp/`      |    1037 |        2066 | bounded LSP-2i; released 0% | released/native LSP replacement remains 0%; LSP-0 diagnostic `publishDiagnostics` payload projection, LSP-1 squiggle policy, and LSP-2a..LSP-2i buffered transport/session/document/hover owners exist under `src/self_hosted/lsp/` and are tracked by docs/150. Full transport/session replacement has not landed. |
| **Live inventory** | `make self-host-progress-metric-test-smoke` | `make self-host-progress-metric-test-smoke` | not a substitution percentage | lexer/parser/semantic/codegen implementation volume and the wider compiler-core inventory are measured at gate time |

Notes:

- *Coverage %* is a rough functional estimate, not a LOC-equivalence
  number. The lexer is 646 LOC and is judged by byte-equal fixture coverage,
  not by line-count parity with the C lexer.
- *Runtime kernel stays C* by current design: allocator/OS/thread/panic/slot
  exports are what the target Pergyra program links against, so substituting
  that native kernel in Pergyra would create a bootstrap cycle. Counted as 0%
  intentionally. Runtime-adjacent Pergyra tools count as soft self-host evidence.
  They remain outside compiler-internal substitution until a Pergyra-written
  runtime component is linked into generated programs.
- `src/lsp/` is the Language Server Protocol implementation. Lower
  priority than the core compiler.

## Peripheral Audit Tools (Not Counted In Coverage)

These 20 tools live in `src/self_hosted/tools/` but do **not** count
toward compiler-internal substitution. They are dogfood validators
that read text artifacts and emit drift verdicts; the C compiler
keeps running fine with or without them.

| Tool                              | LOC (Pergyra) | Function |
|-----------------------------------|---------------|----------|
| `diagnostic_catalog_checker`      | 303           | docs/72 vs diag_codes.h drift |
| `stable_subset_section_checker`   | 122           | docs/107 canonical anchors |
| `air_graph_json_validator`        | 487           | `pgy --air-json` shape gate |
| `air_graph_id_uniqueness`         | 132           | AIR graph duplicate node-id check |
| `air_graph_node_count_integrity`  | 140           | live AIR graph id-count summary check |
| `air_graph_ref_live`              | 138           | live AIR graph back-reference range check |
| `air_graph_ref_integrity`         | 143           | AIR graph dangling endpoint check |
| `air_graph_reachability`          | 166           | AIR graph root reachability/worklist check |
| `backend_output_comparator`       | 135           | paired text diff verdict |
| `completeness_impact_planner`     | 351           | changed-path impact rows -> proof-gate/run-group plan |
| `compatibility_evolution_checker` | 65            | compatibility seed corpus coverage check |
| `module_manifest_resolver`        | 121           | language_module_manifest.json |
| `stdlib_dispatch_inventory_checker` | 107         | C/LLVM dispatch table count parity |
| `doc_link_checker`                | 143           | docs/INDEX.md dead-link audit |
| `production_header_size_checker`  | 108           | DirWalk-owned `.h` 600-LOC cap |
| `production_c_size_checker`       | 127           | DirWalk-owned `.c` 699-LOC cap |
| `examples_inventory_checker`      | 112           | DirWalk-owned examples/ count + non-empty |
| `ast_read_surface_checker`        | 219           | CFG/MIR SoT ratchet parity |
| `linter`                          | 182           | LSP-style diagnostic JSON parity |
| `runtime_boundary_checker`        | 82            | native-kernel vs portable-policy runtime boundary |
| **Total peripheral**              | **3210**      | |

Plus `src/self_hosted/lib/text_scan.pgy` (~47 LOC) shared across scan-based
tools.

## Substitution Roadmap (Honest Order)

The realistic incremental path toward genuine self-host:

1. **Lexer expansion** -- *substantially done* (2026-06-16). Handles
   common keywords, line + block comments, integer + float literals, string
   literals, and common operators. The committed executable gate is the
   8-source C/LLVM parity harness; the broader scale number below is a
   historical measurement and should not be treated as a committed scale gate.
2. **Lexer at scale** -- *historical measurement refreshed* (2026-06-23).
   Pergyra lexer was measured against `examples/*.pgy` plus
   `tests/cases/backend_compare/**/main.pgy`; **993 of 993 byte-equal** vs
   `pgy --tokens`. String interpolation, escaped strings, and doc/block comment
   lexing are now in the measured surface. Coverage target met for this corpus.
3. **Parser bootstrap** -- *expanding* (2026-06-22). `src/self_hosted/parser/`
   parses 188 manifest rows byte-equal `pgy --ast` on parser binaries
   generated by both C and LLVM, and **120 of 121** `examples/*.pgy` files at
   scale with zero byte drift and zero self-host parser exits. It now covers the domain declaration surface (`subject`, `object`,
   `tobject`, `vessel`, `ability`, `role`/`impl`, `zone`, `world`, `party`,
   `event`, `intent ... with retry(n)` metadata), imports, common statement
   forms, full expression precedence, lambda primaries, postfix calls/indexing/
   member access, and deep nested generic type names. Remaining parser work is
   replacing this text-mirror substitute with structured AST ownership and
   keeping the single C-oracle skip honest, not clearing completed-output drift.
4. **Semantic subset** -- *rung-2 active* (2026-06-23). The current rung
   checks `func`, typed `let`, literal/identifier types, return typing, scoped
   `if` / `while` / `for` bodies, unary
   and binary expression operators, call return/arity/argument typing, branch
   conditions, assignment, bare call statements, and simple/compound undefined
   identifier use in Pergyra, then compares against the C compiler accept/reject
   oracle. Recursive import expansion is now owned by `source_bundle_owner.pgy`,
   and the import-backed call fixture proves signatures are consumed from the
  source bundle instead of from a hidden single-file `main` assumption. The
  real-source selfcheck now feeds 204 accepted self-host owner/source files
   through that source-bundle owner rather than a generated import-stripped
   unit. The accepted manifest spans lexer/parser/mir-lower/codegen/compiler-world
  entrypoints, the lexer and mir_lower run/fixture-manifest owners, the compiler path manifest
  owner, target-capability envelope owner, stage-artifact envelope owner, hard-rung
  AIR/artifact/test-harness/subprocess/ABI-row/symbol-row
  compiler-world envelopes, codegen symbol-mangle, ABI-layout, collection-runtime,
  math-runtime, host-I/O-runtime, Option/Result-runtime, and string-runtime owners, semantic run/program/body/call/expression owner files, and audit-tool
   slices inside the current
   subset. The oracle parity runs on C and LLVM
   binaries across 111 fixtures. The same gate now validates the 17-code
   self-hosted semantic diagnostic vocabulary plus its C oracle JSON root-code
   mapping: committed expected `Code:` fields and literal
   `SemanticError...("code")` call sites must be registered in
   `diagnostic_code_owner.pgy`, and invalid fixtures must be rejected by the C
   oracle with that mapped JSON code. The implementation is split
   into source-of-truth owners (`text_scan_owner`, `source_bundle_owner`,
   `diagnostic_owner`, `env_owner`, `expr_type_owner`,
   `expr_validation_owner`, `call_check_owner`, `body_check_owner`,
   `program_check_owner`, `diagnostic_code_owner`, and `semantic_run_owner`) with a thin `main.pgy`
   entrypoint. Expression diagnostics consume `ExprType(...)` facts instead of
   living inside the type-query owner. The builtin/type table now includes the
   scalar math signatures `Sqrt`, `Pow`, `Floor`, `Ceil`, and `Random`,
   C-oracle string-plus and Bool arithmetic result typing, trig/log
   Float signatures from `Sin` through `Log2`, string split/join alias
   signatures, and the first-argument scalar utility contracts for `Abs`,
   `Min`, `Max`, and `Clamp`, newline-free `Print` output calls,
   `Some(expr) -> Option<ExprType(expr)>`, `None -> Option<Unknown>`,
   `None() -> Option<Unknown>`, `UnwrapOption(Option<T>) -> T`,
   `IsSome`/`UnwrapOption` builtin argument rejection for non-Option operands
   and non-concrete `Option<Unknown>` operands, comment-skipping brace/statement scanning,
   and the codegen entrypoint source.
   The next semantic expansion should broaden declarations
   only after that shared-code boundary or another equally narrow fact owner is
   available.
5. **AIR graph consumer passes** -- *rung-1 active* (2026-06-16). Five
   Pergyra-origin graph consumers now run in the self-host preparation suite:
   node-id uniqueness, live-dump node-count integrity, live-dump
   back-reference range checking, fixture-shaped edge referential integrity,
   and root reachability via a push-only worklist. These are still peripheral
   because they do not replace `src/self_hosted/air/`, but they prove the
   deterministic graph substrate the first middle-end pass needs.
6. **C-emit codegen subset** -- *rung-0..21 active* (2026-07-12). A Pergyra
   program (`src/self_hosted/codegen/main.pgy`) takes `pgy --ast` text and emits
   standalone C for: string `Log`/`Concat`, `Log(ToString(<intexpr>))`, integer
   `Let:`/`Assign:`, `while`/`if`/`else` and `for i in a..b` + `break`/`continue`
   (structural lowering), multiple `Int`/`Bool`/`String`/`Void` functions with
   calls, recursion, `return`, `String` types (routed by a variable + function
   type environment; `Concat`/`Substring`/`StringLength`/`StringIndexOf`/
   `StringTrim`/`StringJoin`/`Join` -> runtime helpers, `ToFloat` -> owner-routed
   target `atof`),
   `Bool` (`<stdbool.h>`), growable
   `Array<Int>`/`Array<String>` as a `{data,len,cap}` struct
   (`ArrayPush`/`ArrayLength`/`ArraySet`/`xs[i]` via env-aware index rewriting),
   `Array<Int>` `ArraySort`/`ArrayReverse`/`ArrayMap`/`ArrayFilter`, `Result<Int>`
   `Ok`/`Err`/`IsOk`/`IsErr`/`Unwrap`/`UnwrapOr`, `Option<Int>`
   `Some`/`None`/`IsSome`/`UnwrapOption`, block-local `defer`,
   enum `match` on supported enum facts, `Exit(n)`,
   `FileExists`/`ReadFile` file I/O, `Args()` snapshots, and
   value-passed `Int` / `Bool` / `Float` / `String` field structs plus nested struct-valued fields with
   literals/member reads/params/returns,
   and `Array<Int>` parameter/return flow.
   `lib/json.pgy` now owns the first document-level schema and numeric-field
   readers consumed by the AIR graph JSON validator, in addition to the shared
   JSON string/field/object/array emission helpers consumed by production size
   checkers, the stable-subset section checker, and the module manifest
   resolver. The module manifest resolver now consumes bounded module-array
   object/field counts from the JSON owner instead of global substring counts.
   Round-trip C-emit-by-Pergyra -> gcc -> run -> stdout matches the C/LLVM oracle
   on 69 committed fixtures, with the emitter built through both backends. The
   newest fixture proves scalar readonly-ref reads, aggregate member reads, and
   readonly-ref forwarding consume one value/raw-address fact pair.
   The M2 completeness ledger also now checks all 250 production self-host
   source files through the codegen `--check` path; that path still consumes
   C-oracle `pgy --ast` text, so it is a source-breadth ratchet rather than the
   final self-parser-to-codegen bootstrap. Allocation-free runtime-usage scans
   now reduce the 28,434-node pre-emission probe from 717,696 KiB to 51,968
   KiB while preserving 69-fixture C parity. TextBuilder rung 2 owns final
   C-unit assembly and binding-reference rewriting. Runtime builtin projection
   now uses one identifier scan instead of one full scan per builtin. Two-run
   same-input sampling lowers the codegen-only path from
   3,347.3-3,394.5 MB to 956.1-956.5 MB with byte-identical output; the latest
   green codegen fixed point is 14,673 lines. Remaining integrated-driver
   parser/semantic/MIR text lifetime is tracked separately. Next rungs:
   scope reclamation, block scoping, typed AST-node facts replacing text rows,
   then round-trip
   self-compilation.
7. **Bootstrap loop** -- the bounded parser/codegen compiler subset now compiles
   itself to a byte-identical fixed point and its sample output matches the
   oracle. The rung remains open for semantic/MIR inclusion and released-driver
   replacement.

Steps 1-4 are active staged substitution. Step 6 (codegen) opened 2026-06-17
after the hard-migration freeze was lifted; step 7 has reached the bounded
parser/codegen fixed point but not the whole-compiler terminus. Step 5 (AIR
consumers) and the semantic/MIR bootstrap expansion remain ahead.

## Surface Lifts Required Before Substitution Can Continue

These Pergyra surface gaps will block compiler-internal substitution
beyond the lexer:

- **Process arguments** -- `Args() -> Array<String>` has landed for generated
  binaries, returning the user arguments as an owned snapshot. The lexer and
  parser parity runners now pass source paths through argv, so the first
  compiler-internal substitutes consume the same tool surface they need for
  standalone dogfood runs.
- **Struct-over-arbitrary-types** -- needed to model AST nodes. Pergyra
  already exercises mixed tree shapes as parser/backend evidence:
  `node_traversal_sum`, `tree_walk_recursive`, `tree_grow_recursive`,
  `nested_generic_containers`, and the parser's deep
  `HashMap<String, List<HashMap<Int, Array<String>>>>` fixture prove user
  classes/records and nested generics across C/LLVM-facing gates. These
  mixed tree shapes are parser/backend evidence, not compiler-model
  substitution, and they are not yet a self-hosted compiler AST model. The first
  self-hosted compiler AST model contract now exists in
  `src/self_hosted/hir/typed_ast_arena_owner.pgy`: it owns a flat typed
  arena vocabulary, explicit child lookup, atom lookup, and a small traversal
  payload fixture. `PgyCompilerWorld` now requires that contract through
  `CompilerEmissionFactReady()` before `ProgramEmitter` can claim emission
  readiness. `GenerateC` now consumes `CodegenTypedAstBridgeReady` over the
  owned `CodegenAstTextNode` inventory before emitting, and that guard projects
  the real inventory into `AstArena` rows with node-count, kind, atom, parent,
  indent, and root child-edge checks. `program_emit` now consumes those arena
  facts for first-function indent and owner-body descendant traversal. Current
  parser and most codegen rungs still consume text AST artifacts; the next
  closure is replacing the remaining string-backed expression payloads with
  dedicated expression rows under oracle parity.
- **Raw pointer / FFI** -- if a Pergyra component needs to call into
  the C compiler's runtime (e.g. share the diagnostic emitter), there
  is no stable FFI today. This is intentional for the current compiler-pass
  path: `unsafe` is only a scoped marker, raw pointer helpers stay
  runtime-internal, and `raw_escape_contract_smoke` rejects system-tier escape.
  The alternative remains *no FFI*: build the Pergyra-side compiler as a
  parallel binary that emits text, not as a library that plugs into the C
  compiler. FFI remains intentionally absent from the compiler-pass path until
  a stable ABI contract exists.
- **Subprocess execution** -- needed for in-Pergyra drift guards that
  re-run the C compiler. Currently the parity scripts do this from
  bash; a Pergyra runner would need `Subprocess(...)`.
- **Deterministic collection iteration** -- compiler passes need stable
  output ordering, not just functional map/set lookup. `stage4_determinism_smoke`
  now compares insertion-order variants for `HashMap<String|Int|Long|Bool, T>`
  `MapKeys` and `Set<String|Int|Long|Bool>` `SetValues` through generated
  Pergyra programs on C and LLVM. Compiler-facing symbol/record-like identities
  are canonical string keys, and handle-like identities are stable integer or
  long IDs; the Stage 4 fixture exercises those canonical shapes instead of
  introducing raw aggregate keys as a second collection truth. Compiler passes
  should consume those stable snapshots (`MapKeys` / `SetValues`) rather than
  depending on hash storage traversal.
- **Allocator/arena ownership surface** -- `AllocatorSystem`,
  `AllocatorPool`, `AllocatorDebug`, `AllocatorTracing`, `AllocatorScratch`,
  `AllocatorResult`, and `AllocatorPersistent` now produce the single stable
  `Allocator` value on C and LLVM. The lane-named constructors carry distinct
  runtime kinds and lower through dedicated LLVM runtime init exports instead
  of aliases. `BoxArray(capacity, allocator)` consumes a named allocator local
  so fused array storage keeps an owner with a valid lifetime.
  `AllocatorDestroy(namedAllocator)` is the stable cleanup operation, so
  compiler pass lanes can use `defer { AllocatorDestroy(lane); }` instead of an
  out-of-language cleanup convention.
- **Filesystem directory walk** -- `DirWalk(String) -> Array<String>` has landed
  for generated binaries on C and LLVM. It returns a deterministic sorted
  regular-file snapshot with `/` separators and is gated by
  `filesystem_directory_walk_smoke`. `examples_inventory_checker` now consumes
  `DirWalk("examples")` directly, so the clean example inventory no longer has a
  committed manifest alias. `production_header_size_checker` and
  `production_c_size_checker` now consume `DirWalk("src")` directly, so their
  clean inventories no longer depend on committed file-list fixtures.
  `ast_read_surface_checker` now keeps only the metric/ceiling ratchet spec in
  `tests/ast_read_surface_ratchet.txt` and owns live file discovery through
  `DirWalk(scope)`. Remaining manifest-owned surfaces are document contracts,
  not clean directory file lists.
- **Parser LLVM depth/type-inference parity** -- `parser_parity.sh` now
  compiles the self-host parser through both C and LLVM and includes a deep
  nested generic type fixture. Remaining parser work is grammar breadth and
  the scale-probe exit list, not C-only backend evidence.

The remaining work is mostly actual semantic and codegen pass work against the
C compiler oracle. The one substrate-shaped item that remains as compiler-core
design work is mixed AST-like tree ownership inside a Pergyra pass; current
evidence proves language shape and backend/parser behavior, not compiler-model
substitution.

## How to Update This Document

When a tool lands or expands, update three things:

1. **Headline Number** -- recalculate Pergyra LOC vs C LOC for
   *compiler-internal substitutes only* (not peripheral tools).
2. **Component Coverage table** -- bump the relevant row's `Pergyra LOC`
   and `Coverage %` (be honest -- LOC equivalence is a bad proxy; use
   functional coverage estimates).
3. **Substitution Roadmap** -- check off completed steps, add detail
   where the next step diverged from the plan.

Do **not** add peripheral audit tools to the substitution percentage.
Their job is to keep the C compiler honest, not to replace it.

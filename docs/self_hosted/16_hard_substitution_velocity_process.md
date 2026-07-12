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
- Implementation inventory is 8.94 percent of the C reference inventory.
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

First executable delta, 2026-07-12: array-literal bracket recognition and body
extraction moved from codegen into `SemanticAstLocalBindingFacts`. Codegen now
consumes `SemanticAstLocalBindingArrayLiteralBodyAt`, the old AST-text codegen
owner is deleted, and the negative ratchet forbids text/bracket recovery in the
new view. C-built and LLVM-built codegen tools emitted byte-identical C for the
focused array fixture. The active rung remains open for the other expression
shapes.

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

Mechanized closure delta, 2026-07-12: `SoTAuthority.v` now defines rung closure
as required-owner completeness, authority uniqueness, required consumption,
and zero semantic fallback. It proves that the current array-literal,
try-operand, collection-mutation, enum declaration, nominal/field, and role rows are closed in the
bounded model and that missing facts, duplicate producers, and
owner-plus-fallback bridges are not closed. The source adequacy gate binds only
those modeled rows to live files; future consumers require new bindings rather
than inheriting a global proof claim.

Whole-spine owner declaration, 2026-07-12: the 15 architectural fact families
have stable owner identities in `docs/semantics/sot_owner_spine_registry.md`.
The registry now carries 15 architectural rows plus thirteen bounded self-host
closure rows and matching `SpineFact` / `SpineOwner` constructors in Coq. It is
honestly split into `CLOSED=13 BRIDGE=6 ACTIVE=9`; only executable rung closure
may promote a row. The registry replaces ad hoc top-level owner lists, while
`src/self_hosted/OWNERS.md` remains only a physical module inventory.

## 7. Fifteen-Day Correction

The previous roughly fifteen-day interval delivered substantial owner, gate,
and bounded-rung work, but released/default replacement remained at 0 percent.
That work is useful infrastructure, but it is not sufficient progress by
itself. From this decision onward, progress reports lead with executable
replacement evidence and report SoT work only as the condition that enabled or
blocked that replacement.

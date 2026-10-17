# Indexed String: permanent literal elements across an Array result

Status: APPROVED for bounded integration/commit; canonical and CI remain RED.
Base: main `fb54aed078a62f586bf1ac6bedae13e32fdc139f`.
Navigation only; existing typed array-return, collection and String-source
owners remain semantic authority. No new word, cleanup policy or SoT family.

## Objective and reached chain

- Objective: compile the unchanged canonical `Some(codes[i])` when the exact
  returned array still contains only static String literals. Do not grant
  element ownership/deep destruction or allow arbitrary borrowed elements.
- Priority: exact lifetime/source identity, original typed evidence and one
  owner, missing-fact refusal, actual-role falsifiers, then patch size.
- Owners: existing array-return event/all-return closure, typed collection
  literal origin/transition facts and indexed String source publication.
- Last consumer: immutable collection source evidence, actual HIR/MIR roles
  and the C/LLVM emission validator. No SemanticContext/Type/event-array
  authority may survive its owning compilation stage.
- Forbidden: source-name special cases, BORROWED_ELEMENTS alone as permanence,
  arbitrary source0 as static storage, source-ID rewriting, manual copies or
  drops, deep-release grants, current-time demand/reissue, AST-root rescans,
  a second result checker, known-red push or claiming native work substitutes C.

Native6a0316 passes the previous450:87 boundary, then first refuses
`SemanticDiagnosticCodeFromMessage`, diagnostic_owner.pgy506:30.
`SemanticDiagnosticCodes` returns one immutable local Array literal containing
only AST_STRING children. Its original fact is BORROWED_LITERAL; descriptor
storage is allocated but C emits literal pointers and LLVM emits global String
pointers. Its bytes therefore outlive the descriptor. Caller `codes` is read
through ArrayLength/element reads only; a selected element enters Some.

The existing return-elements-owned certificate correctly remains false.
The missing fact is that exact current returned elements have permanent
literal backing, not that the array owns those String allocations. Existing
pending descriptor preservation and owned-elements preservation do not imply
this property: an owned push copies heap strings and destroys permanence.

Root's HrTPcQ diagnostic pair on exact6a0316 (root receipt
`p0-literal-array-return-observe-20261018-v1.log`)
refuses both minimal literal-return positive and mixed literal/Concat negative
at10:30, rc1/MIR0. Inputs are in the identified observer lab. No execution.

## Fixed complete change set

1. Reuse the existing deferred array-return all-return traversal. After typed
   publication and array-return finalization, check every reachable RETURN
   against its original unique event and descriptor ownership. A permanence
   facet accepts only exact typed String literals, directly returned or from
   an immutable local literal birth whose complete typed use history has no
   alias, replacement, element mutation or unproved handoff. Reuse the existing
   typed claims/child ordinals, not another walk of program AST roots. Missing,
   mixed, recursive or unsupported inputs fail closed. No owned bool changes.
2. Expose prepare/current operations of that same owner with an optional
   observer on its already-validated declaration/claims/RETURN/literal tuple.
   Prepare may build existing inputs. Current establishes the existing flow
   current barrier and cannot demand, append, reset or recreate evidence.
   Callbacks must copy event identity immediately, never retain the private
   growable return-event pointer or a context/type authority after teardown.
3. At the Some String payload issuer, pass the exact Some call to the existing
   borrow owner. Before its immediate nonzero-source refusal, an exact local
   named Array<String> result element may register a pending lifetime
   candidate. This is not ready admission: every registered candidate must
   pass the mandatory prepare/current pass before semantic success. An
   unmatched borrowed/formal/mixed case still refuses; no ready fallback.
4. Join the selected call, exact caller Array birth, source ID and every
   relevant original use to the producer permanence proof. Current caller
   history must exclude ArraySet/Push/owned-push, rebind, alias, capture,
   worker/defer and unproved calls. ArrayLength and element reads use their
   existing checked typed identities. A readonly mask alone is not enough.
   Binding versions cannot reuse an earlier producer's literal property.
5. Store this as a subordinate input of the existing indexed String source
   family. Raw Array-source IDs stay nonzero. A derived query may answer0 only
   from the exact current certificate. The Option payload owner consumes that
   certificate for Some(arrayElement); it must still reject heap/mixed input.
   Reuse result-input call/claims capture and its dependency union. Do not
   manufacture a FieldRecipe, redefine ordinary String ownership or duplicate
   the array-return proof in the Option checker.
6. The existing immutable collection snapshot owns the additional source
   receipts. Deduplicate original claims and MOVE them once after all fallible
   checks/allocations. Destroy receipts exactly once. Any reference from an
   Option result input is non-owning and must refer to an admitted receipt
   owned by that snapshot, not a second allocation/authority.
7. Replay original Array birth/RETURN and exact Some use after semantic
   teardown. Actual DEF/ASSIGN/call/RETURN crosswires, element writes and
   duplicate static births must fail in evidence/full/emission validation.
   Reuse the existing validation-local write identity; shared receipts do not
   create a second write permission. Nonretaining reads remain reusable.

## Interfaces and independent edit scopes

- A semantic_transfer: existing deferred array-return owner and its headers.
  Add prepare/current literal-result traversal plus observer; retain all
  descriptor/deep-owned semantics. A cohesive physical move of the existing
  result traversal/private store declarations is allowed if the unchanged
  size cap requires it. No B/snapshot/test edits.
- B primitive_operator_contract: existing indexed String borrow/publication
  fields, result inputs and Option payload consumption; subordinate permanent
  element receipt code if required. Own pending selection, mandatory ready
  proof, immutable syntax/use/birth/RETURN queries and destruction. No A,
  snapshot/HIR/MIR or tests. Type-checker Some payload call-site is B's scope.
- C selfhost_gate: existing phase/MIR test owners, function-param-flow fixtures
  and registration. No production edits. Register observed results only.
- Root: final public interface coordination, Makefile, collection snapshot
  capture/MOVE/reference ownership/derived write identities, actual HIR/MIR
  consumers; every command/build/WSL/process/Git and independent review.

Before implementations cross the seam, A supplies observer fields and exact
prepare/current prototypes to B/root. B supplies opaque receipt/query/MOVE
interfaces to root. No source overlap, worktree, branch, process kill or
independent parallel implementation track. One shared integration gate.

## Acceptance and falsifiers

- Unchanged literal-array-return/Some/Unwrap positive, missing None branch,
  repeated reads: exact C/LLVM stdout/stderr parity. No manual source release.
- Mixed literal/Concat or owned-array elements, one bad producer return,
  ArraySet/Push/owned-push, alias/rebind or unproved helper before the selected
  read: refuse, MIR0, unsafe execution0. Preserve all old borrowed negatives.
- Missing/changed original literal child/type, RETURN/event/source/callee,
  caller birth or Some use withdraws current proof; restoration passes with
  no growth/demand. No false0 from missing input or an empty dependency set.
- Coherent actual MIR definition/call/RETURN swaps and injected writes or a
  duplicate carrier birth refuse in collection/full/emission; restore passes.
- Keep the no-deep-drop-grant gate from fb54 and all previous owner/alias/
  worker/currency/negative gates. Source lifetime is not cleanup authority.

Static60s, focused5min, integration shard30min. First narrow source proof,
then strict C/LLVM, sanitizers and full unchanged fixed canonical. Actual
bootstrap/gen2==gen3, installed-driver parity and exact-published-SHA CI still
remain the endpoint. The current18k diagnostic cascade is not a defect count.

## Integration receipts (2026-10-18 KST)

Native47ecd2f7f733aae04fb3ca98f602074d68b16dcc4bd47ebad05ea0c232081f4e
in the identified non-Git validation copy passes umRhDl static/build,
phase12312/0 and MIR534/0. StrictKmCJB8 includes the nine observed source
fixtures. Source6ig7dd admits the literal positive and refuses eight negative
controls with MIR0; runtimeVdUboE confirms C/LLVM stdout `alpha\nfalse\nheap\n`,
stderr0 and byte equality. Source-pairs3dXy9J checks134 raw D/Linux pairs.
BroadHTE93e passes six currency controls, semantic3230, transpile1012, AIR156,
Map24+58, four IR flow gates and separate ASan/UBSan phase12312/MIR534
(leaks OFF). Receipt paths live under the validation copy's `.tmp/` directory.

The initially duplicated literal RETURN loop was removed before freezing:
the existing all-return core is unique, with an optional observer. The original
result traversal and snapshot capture were moved cohesively to physical TUs
under their unchanged owners to keep the existing599-line cap.

Initial4amrJ5 had32 phase test failures, not a green receipt. The old fault
loop was accidentally expanded10->17, and the new test incorrectly required
raw source0. Restoring that old loop and separately checking raw nonzero
identity versus actual Option/Unwrap-derived0 produces passing umRhDl without
relaxing production. All17 new evidence-withdrawal controls now run.

Unchanged full fixed/current wvdpM2 is still RED18435 errors/16 warnings/MIR0.
ObserverdXIVDK now first refuses ReadFile(path) at diagnostic_owner.pgy570:79.
The earlier Some(codes[i]) no longer fails immediately; final canonical
deferred admission is still unproved. This candidate is not V1, bootstrap,
installed-driver, self-host substitution or CI closure. No next production
scope is open before this bounded integration is reviewed and committed.

Independent frozen review APPROVED this bounded candidate. Bundle
`E:\PergyraValidation\literal-array-option-20261018-v1` source manifest
`A3502E1DD4B393B78734853F145C3A6420F57D65C84529BB843D03C4B9B602BA`
matches147 source/context files; evidence manifest
`D9E1A2618333325E567495614F691426A078D0118CEDDF5D1C32EB4C5355BA18`
matches153 artifacts. The reviewer independently checked134 source-pair hashes
and source/role/currency/ownership boundaries, but did not rerun the shared WSL
executables. Root rehashed the current tree against the freeze: production and
tests are unchanged; the handoff alone corrects the next diagnostic location.
570:79 is the ReadFile result, not its path argument at88. That next result-source
repair is separate from this approval. AIR is adjacent regression evidence;
leak checking, canonical, bootstrap, installed driver and required CI are not
closed by this bounded result.

# Reached numeric String allocation contract and remaining frontier

Base: b9d51d8c18f1d2abf8ac828bb1ed2eb62fdf0208. Root sole implementation,
build and integration owner. Three reused agents reviewed caller/runtime
contracts, ordered scalar transfer, and disjoint MIR coverage read-only.
Root independently checked their relevant source findings and ran the gates.
This audit is navigation/evidence, not semantic authority or a CLOSED row.

## Implemented supporting fact

The existing expression-identity lexical visit now records strict numeric
ToString calls with their actual operand type and stable owner/lane/call
ordinal, plus exact base and terminal expression nodes. The runtime domain is
heap-or-null, not non-null/nonempty/current/exclusive. Int/Long/Float/Double
are accepted; Bool/String/Unknown and declared targets issue no row.

Body assembly produces the existing CallSpineRoots projection once and shares
it with identity resolution and collection admission. The result owner can
consume a matching numeric call contract for a source function's String
return. Source summaries continue to contain positive source-function IDs;
builtin0 is never a producer function ID. Whole-local reassignment invalidation
and the all-return obligation remain. A function returning a numeric String
on one branch and static bytes on another is not an owned-result producer.

The new owner is registered under the existing ACTIVE collection family, not
a new top-level family. The earlier local-reassignment owner's missing OWNERS
registration was corrected after the full structural gate exposed it.
There is no second scalar lifetime traversal, entry-mode grant, formal-literal
grant, MIR v1 grant, native bypass deletion or compiler install in this change.

Two pre-acceptance mistakes were observed and corrected: a base Call node has
no physical arguments (the terminal CallArgument root owns them), and the
numeric fixture's separate Int-array retirement was blocked by the existing
escaped-descriptor rule. Runtime-array lifetime was not changed to make that
fixture green. Typed index/call operands remain source-level probes.

## Observed executable evidence

- .tmp/self_hosted/collection-inout-effect.CYF4FN: complete current-source
  C/LLVM gate PASS. Each backend observed233 source admissions,13 numeric-call
  identity/type/builtin/family-deletion units,12 reassignment units, and all
  existing location/identity/occurrence/receipt/storage/constructor/formal
  checks. Inputs are analyzed, not emitted or run. Issued owner/import/input/
  native/probe manifests were checked; source builds report9 warnings each.
- Numeric positive is accepted by both analyzers. Bool, String, nested String
  ToString, mixed return, stale reassignment and declared shadow negatives
  refuse deep drop. The existing own-formal/literal proof boundary is unchanged.
- .tmp/self_hosted/ci-root-numeric-contract.rAoFRu: exact current CI input
  mir_collection_receiver_root.pgy refuses syntax5247 / JsonOwnedFragmentWriteFile
  / ArrayDropOwnedStrings(owned_fragment) / borrow_boundary_escape /
  owned_string_drop. C70s and LLVM27s, byte-equal diagnostics, empty stderr,
  matching source/input/native/probe hashes. This is not rung advancement.
- The same numeric source positive attempted through the installed native
  --native-pipeline C and LLVM CLI refuses MIR collection ownership transition
  at Main/values, stage=invalid-state-transition. Fresh targets are not created.
  No numeric runtime executable or installed self-driver success was observed.
- The C native request removed a pre-existing dummy .exe sentinel at preflight,
  before MIR validation. This matches driver_binary_output_owner.h/c and
  docs/205_language_limit_and_fact_gap_closure_design.md section11.3: invalidate
  stale binaries first, publish a new staging binary only after success.
  binary_output_refusal_owner.sh explicitly requires absence after refusal.
  This is NOT a publication-cleanup bug or an additional OPEN implementation
  track. Only a controlled .tmp test artifact was affected; its original text
  was `owned-numeric-output-sentinel` plus newline. Do not claim preservation
  of an executable target, or conflate this policy with JSON artifact contracts.
- Selected changed structural ratchets, shell syntax,14 checker tests and
  diff whitespace passed. Full structural attempt first found the missing
  local-reassignment registration; after correction the60s attempt timed out124.
  This is NOT a full structural PASS. No full compiler/fixed-point matrix ran.
- Exact-base CI37075000783 completed FAILURE in Linux self-host bootstrap,
  job111062842585 at the same Json boundary. Windows, macOS C-only, TSan, Rocq
  and classification succeeded; downstream Linux jobs skipped.

## Full-rung BLOCKED tuple and next falsifiers

Production entry remains PgyCompilerWorld body admission through
SemanticAstCollectionOwnershipVerdictFromResolvedFacts. Direct C bypass is
semantic_collection_ownership_initialize_binding / collection_ownership_fact.c
UNKNOWN and MEMBER_MOVE. Hard substitution delta0; registry70 CLOSED,
23 BRIDGE,2 ACTIVE. No row was closed from the new fact or local PASS.

Missing facts: allocator-formal result-domain dependency with exact actual
caller substitution; completed/current scalar allocation token and one-time
exclusive transfer; retained/alias/later/returned/deferred reads; synchronous
no-retain write obligation; exact literal receiver materialization; disjoint
MIR source/version/SSA/operation coverage including complete family deletion.

Fact owners are resolved typed expression/call/signature/local owners,
ast_owned_string_result_fact_owner and the existing ordered collection fold.
Only completed Let/Assign may select current scalar definitions. The last
consumer is Json deep-drop admission, then MIR producer/reader/CFG and the
target-neutral C/LLVM materialization/retirement decision. Numeric type or own
mode must not substitute for those facts. Result/Pool forwarding through the
same allocator-taking Json callee must stay distinct; unknown/missing fails.

First integration falsifier remains the real CI input plus a safe allocated
caller. Required paired controls are repeated/aliased/returned/after-use,
retained outer arguments, deferred readers, crossed formal/definition/receiver/
version, and deletion of all materialization/drop facts, counts and receipts.
The numeric positive's native MIR transition failure is an additional exact
observed limit, not a reason to remove cleanup, relabel origins or add a
compatibility fallback. Stale executable invalidation is the documented
publication policy, not a missing allocation fact.

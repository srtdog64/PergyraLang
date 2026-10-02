# Reached String caller source and actual collection-lifetime review

Updated: 2026-10-03 (KST). Base: e057e3ffc28eac9620576293436c66e2a2fa571d.
Observations and one bounded implementation candidate; not semantic authority,
registry promotion, installed-driver evidence or compiler completion.

## Objective and parallel boundary

Close only the prerequisite reached by the one active collection-lifetime rung.
Priority: exact first CI/source refusal, owned allocation and element facts,
actual current aggregate/caller discharge, versioned MIR, negative ratchet.
Root alone edited, built and integrates the attached managed worktree. Three
read-only reviewers covered CI/source identity, physical field/current lifetime,
and MIR/consumer obligations. They independently inspected retained results and
hashes but did not execute tests. Temporary coordination is in
docs/agent_work_directives/actual_aggregate_release_execution_2026-10-03.md.
Main dirty work and its six unrelated component-counter hunks are excluded.

## Exact first refusal, observed rather than inferred

Exact-base CI37054116236 completed failure:
https://github.com/srtdog64/PergyraLang/actions/runs/37054116236
Linux self-host-codegen-bootstrap failed; Classification, TSan, Windows, Rocq
and macOS succeeded, with dependent Linux jobs skipped. Completed Linux job
110994576820 reports borrow_boundary_escape / syntax5251 / owned_string_drop
at nominal-array MIR-root control, then make failure. The same first diagnostic
was observed in parent6bcca015; no CI repair is claimed for this candidate.

The exact input tests/self_hosted/parity/fixture/mir_collection_receiver_root.pgy
was traced with the already-issued C/LLVM analyzers in
.tmp/self_hosted/ci-root-owner-trace.MgehWz. Both outputs agree, stderr is empty,
and the observed source identity is:

    JsonOwnedFragmentWriteFile
    src/self_hosted/lib/json_emit.pgy
    ArrayDropOwnedStrings(owned_fragment)
    syntax5251, boundary=owned_string_drop

The import route reaches lib/json_emit through routine_statement_owner,
routine_build_owner, program_fact_owner and domain_topology_fact_owner.
JsonOwnedFragmentWriteFile receives own fragment:String, writes it, returns for
empty input, then puts the original pointer in [fragment] and deep-drops it.
Ordinary literal element materialization is not String duplication. A formal
mode alone does not prove its actual caller allocation or one-time transfer.

Source-only caller inventory found51 calls in6 files:20 direct Int ToString,
25 structurally allocating JSON expressions and6 conditional/field cases.
Int/Long/Float/Double ToString allocate in both native backends. Bool ToString
differs (C heap, LLVM globals); String ToString preserves the pointer. Neither
may be generalized into a common fresh-result grant. No current Json caller
directly uses Bool/String ToString. This inventory is not execution proof.

## Implemented bounded owner join

The production direct-call carriage owner now requires a positive allocation
source for a bound String owner-handle argument. The new
direct_mir_scalar_program_owned_string_argument_source_owner.pgy joins exactly
one same-routine definition and, for a member, the same-type direct constructor
and exact binary/n-ary physical field operand. Borrowed literals, copied locals
or records and unproved formal forwarding fail closed. It does not recursively
reinterpret an alias as a fresh allocation.

The existing direct-result callee-body verdict is reused, not queried twice.
Concat/TextBuilderFinish allocation policy is shared with the existing result
owner. Its previous conditions and private scratch retirement remain unchanged.
The new source owner has no scratch, inout cleanup or private retire privilege.
The guard is reached by expression identity/row admission, graph-plan readiness
and both C/LLVM projections before publication. There is no native retry.

This narrows existing admission. It does NOT prove current/exclusive lifetime,
retained aliases of the original allocation, post-consumption read/return/reuse,
value-result overwrite, literal element transfer or a field lease. A copied
record rejection is not proof that the original record has no surviving alias.
The compiler still has those held paths. No public ownership contract or MIR
origin/receipt was relaxed, and no actual aggregate cleanup was deleted.

## Observed executable and structural evidence

- Latest whole new gate: owned_string_argument_source.CfXFuH, observed exit0.
  Three programs execute on C and LLVM with6 exact outputs. Eight caller
  refusal cases on both backends produce16 exact String carriage diagnostics;
  two reassignment cases produce4 earlier identity_cell_store_target refusals.
  The latter are not attributed to the new source guard. All20 publication
  sentinels remain unchanged; negative programs are not compiled/executed.
- The n-ary record has String/String/String fields: borrowed first/last and a
  fresh middle. Exact middle selection executes on both backends; first/last
  negatives refuse. Earlier PUt72b used Int/String/Int and is not cited as
  evidence for this revised same-type fixture. Latest input hashes verify20
  paths, owner hashes verify2471 Pergyra sources and probe hashes verify2 issued
  artifacts before/after. Native MIR is the explicitly declared oracle; this
  developer projection gate is NOT verified-source or public CLI admission.
- Existing reachable-result gate: owned_string_reachable_proof.QpRcEz,
  observed whole exit0 for20 cases. Positive DAG/cross-edge/4096-chain and
  negative cycles, borrowed/invalid/missing/duplicate/foreign facts retain their
  expected verdicts. Actual generated function entry instrumentation observes
  one public query entry, expected positive terminal inspections, two scratch retires
  and one free per backing. This is a minimal typed-owner probe, not a full
  graph-plan/source/CLI or performance measurement.
- The regression's earlier 6W2WHQ run failed linking obsolete bare observer
  names against generated pgy_u_ symbols. Root derives each unique physical
  definition into call_symbols.h, rejects missing/ambiguous definitions and
  hashes that header plus observer C. A further reviewer found incompatible
  erased extern declarations in y6vOS0's observer. Root removed all four and
  includes the observer after actual definitions in the same translation unit,
  with free interception explicitly ended before the observer's real cleanup.
  QpRcEz reran the whole gate with observed exit0. These repair instrumentation,
  not proof semantics or private-retirement caller authority. Resolver missing/
  ambiguous refusal is code-reviewed, not an executed resolver-negative claim.
- Checker mechanics:14 tests passed. Narrow actual component functions verify
  owner presence, exact production guard and Makefile registration; unchanged
  result cap180, new owner70/cap90 and gate/cap150 pass. Bash syntax and
  git diff --check pass. These are structural evidence only.
- Whole component inventory ended124 under its60-second budget after the
  match-pattern checkpoint; interrupted command ERR141 at line17344. This is
  not whole PASS or a semantic ownership failure. No allowance was increased.

Warnings remain explicit: new probe native emission16, LLVM runtime compile6,
and each positive LLVM compile1 target-triple override; regression probe native
emission15. No baseline comparison justifies calling all of them pre-existing.
Exploratory rj4zNa scratch-retirement refusal, SxhGiI immutable reassignment
fixture refusal and CdBuAi earlier-assignment misclassification are retained.
Corrections reran bounded gates independently; no failed run is counted green.

## Exact still-blocked executable rung

Production entry: PgyCompilerWorld body admission through
SemanticAstCollectionOwnershipVerdictFromResolvedFacts. Direct native bypass:
semantic_collection_ownership_initialize_binding / collection_ownership_fact.c
UNKNOWN and MEMBER_MOVE. The native general UNKNOWN deep-drop allowance is not
a Pergyra ownership grant to copy. Real C replacement delta0 for this candidate.

First missing fact: exact actual scalar String caller ownership and one-time
literal materialization/transfer, including current value, retained aliases and
after-use. Existing semantic ordered result/collection owners, literal MIR
projection and direct-call argument owners must carry that chain. Last reached
consumer: JsonOwnedFragmentWriteFile deep-drop admission. Falsifier: the exact
CI MIR-root input above, followed by borrowed/aliased/reused/wrong-definition and
missing/forged-receipt negatives before any new permission is issued.

Later missing chain: exclusive live storage with releasable elements -> exact
constructor reservation/current aggregate -> actual caller/formal discharge ->
versioned MIR materialization/drop/RetiredZero and field/outer writebacks.
Last consumers remain actual SemanticAstExpressionFunctionTableFactsRelease
and its three initializer/MIR/source-C callers. Preserve3 deep drops,3 retired
field writebacks and the2 outer source-C driver writebacks. Readonly duplicate
constructors stay legal. callable_table_owned_release_positive.pgy remains the
later aggregate_field_entry_unproved falsifier, not the first CI Json refusal.

MIR MemberMove remains Unknown-only and drop receipts EmptyLiteral-only. A new
formal-literal contract must be disjoint/versioned; it cannot reuse Clone/Empty
tags, weaken old validators, erase Unknown/self-escape, introduce private retire,
add a Clone workaround or remove cleanup. No Alrescha, model or credential work.
Registry remains70 CLOSED /23 BRIDGE /2 ACTIVE. Actual Release, C substitution,
installed-driver, stage-dogfood and SoT closure deltas0. Native/public ArrayDrop,
source/full bootstrap/fixed point and performance matrices were not rerun.
The overall requested compiler work remains active, with this exact BLOCKED
owner/consumer/falsifier card before another supporting checkpoint.

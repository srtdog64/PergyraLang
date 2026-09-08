# Logical-record constructor query

Status: PARKED after the user's source-admission parity reprioritization;
the implementation candidate is preserved, not closure evidence.
Base HEAD/origin/main: `5b97f2e10ffa7ecf9cfe932829a83ffffaa3ba12`.
Initial dirty state: 215 entries, zero staged. Existing source-matched seed
723F5707 / manifest 6C95B095 is reverified (`50fa8c`); no compiler job is live.
The previous goal turn made progress: real shape-guard changes, executable
comparisons and a stack identifying this next reached operation. It did not
complete the original LLVM query or full bootstrap.

## Objective and ownership

- Objective: stop validating/looking up the entire logical-record table for
  every argument of a constructor whose exact record row has already passed
  the existing owner. The same 23-result public LLVM input remains the focused
  integration falsifier; full source/comparator/gen2/gen3 remains the end gate.
- Priority: preserve identity, field order/types, missing-fact refusal and
  partial/complete constructor results; remove repeated owned work; measure
  the fixed workload. Do not increase budgets or infer closure from small tests.
- Fact owner: `DirectMirScalarProgramLogicalRecordRow` validates FactReady
  (digest, columns, identities, dependency order and physical ABI) and returns
  the unique exact row. The existing expression owner is its last constructor
  consumer. There is no new semantic family, cache or serialized receipt.
- Lifetime evidence: after that row succeeds, the constructor checks argument
  count against the admitted row. The remaining local loop reads the normalized
  argument types and immutable borrowed record columns; it calls no callback,
  mutator, suspension or publisher. Its field span is valid for this one query,
  not a transferable authorization. Both consumer and callee take readonly ref.
- Candidate edit: in this constructor only, use that validated row's field
  start and field types within its existing count bound. Do not repeat Row or
  FieldType inside the argument loop. Keep standalone FieldType unchanged for
  other callers that have no validated row.
- Forbidden fallback: `valid` alone, bypassed initial Row/digest/ABI checks,
  spelling-derived fields, a second owner, persisted row cache, wider trust
  lifetime, enum-lane expansion or a rerun of expired work without reduction.

## Edit lease and verification

Primary alone edits the existing constructor in
`src/self_hosted/compiler/direct_mir_scalar_program_logical_record_expression_owner.pgy`,
its bounded fixture/manual gate and navigation evidence. Preserve the preceding
member-shape guards and inherited changes. Fact/digest/ABI owners, expression
admission, enum owners, schemas and shared binaries are out of edit scope.

Before changing production, preserve its exact source and compare a native
before/after real-owner query. Use a nonzero record row/field offset, mixed field
types and ordered argument rows; test partial/exact/excess arities, wrong type,
unknown record, claimed callable ID, malformed argument rows, stale digest,
duplicate identity, malformed ABI and empty/invalid facts. Fixed query counts
and record size with bounded arities expose repeated work; timings include
startup/construction and do not measure whole-compiler speed or heap lifetime.

Then use current-source seed/native builds, the existing logical-record C/LLVM
runtime/negative gate, bounded bootstrap and the original 23-result LLVM leg.
Only the reached operation may justify a follow-up slice. Structural source
ratchets are not behavioral evidence. Budgets: static 60 seconds, focused 300
seconds, integration 1,800 seconds. No shared install, staging/commit/push,
remote CI writes or deletion of old evidence. Update the active handoff with
exact terminal results, hashes and the next falsifier; do not change progress
percentages or registry status without actual substitution/closure evidence.

## Terminal evidence before parking

Current source-matched seed driver is 8566043D (manifest 35766D67), and the
independent native build is F7990945; both builds passed. Native before/after
seventeen controls agree. The current four-way gate passed native C and LLVM
but stopped at public C with `borrow_boundary_escape`, `boundary:default`,
`type:Array<Int>` (`9eb853`). Public LLVM did not run. Evidence directory:
`.tmp/self_hosted/record-constructor-query.CxuDGx/`. This reverse admission
discrepancy is preserved; the fixture is not changed to conceal it.

No new original-query or full-bootstrap run follows this result. The active
objective is [source-admission parity](source_admission_parity_2026-09-07.md).

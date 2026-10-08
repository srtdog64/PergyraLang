# Ownership teardown red-team and indexed-update refinement

Status: **IMPLEMENTATION COMPLETE; bounded design model only**.
Base HEAD: `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, dirty main,
300 status entries and empty index at entry. The user reopened the whole
teardown mechanism for red-team repair and bounded performance assessment.
The active self-host ownership/DX hold is unchanged.

## Objective card and chain

- Goal: make admitted retirement schedules duplicate-free and replace
  program-global index filtering for a field/parent update with updates of
  the actual old/new owner rows, without weakening teardown invariants.
- Priority: one semantic owner, existing identity/lifetime guarantees,
  executable negative evidence, locality/observable cost, then patch size.
- Owner: `OwnershipTeardown.v` owns the forest, transitions, indexes and
  reference specifications. No new heap or alternative language contract.
- Last consumers: its arbitrary-step invariant and reuse proofs, RC comparison,
  finite extracted update observer, independent permanent red-team consumer,
  and the production fresh-kernel gate.
- Forbidden fallback: duplicate physical retirement schedules; scanning all
  index rows as the admitted field/parent update; treating opaque/lexical
  assumptions or a bounded cost fixture as production safety/GC superiority.
- Verification: isolated fresh Rocq 9.3.0/rocqchk, typed mutation refusals,
  fresh extraction with an independent finite observer, then one complete
  production snapshot and documentation gates.

Survey: `StSetField -> index_set -> inv_setfield_h -> inv_step/run`;
`StAttach -> kids_move -> inv_attach_h -> inv_step/run`;
`StRelease/StRootDrop -> exact units -> teardown -> generations/incoming
clear/children retirement -> arbitrary-run stale-handle and frame proofs`.
The RC comparison also reaches field updates and index maintenance. Concrete
empty-to-cyclic-link/reuse witnesses are last executable proof consumers.
`formal_semantics_smoke.sh` and `coq_kernel_check.sh` register this owner;
the independent red-team consumer must join that same production snapshot.

The previous review's four probes expose distinct boundaries: absent caller
authority/loans, duplicate schedules, saved local links, and root-id reuse.
Only duplicate schedules are an in-model rule defect being repaired here.
The other probes remain explicit refinement falsifiers, not new safety claims.

## Complete change set and order

1. Preserve old index transformations as clearly named scan specifications,
   never dispatch fallbacks. Prove local transforms pointwise equivalent
   under the already-admitted exact index. Migrate both forest and RC callers.
2. Require `NoDup` for node/root retirement transitions. Construct a unique
   exact unit, migrate every reached proof/witness, and prove duplicate refusal.
3. Permanent red team: success and refusal for cycles, stale subject/parent,
   missing/misdirected index rows, partial units, duplicate units, untouched
   owner rows, saved local handle, and root-id reuse. Mutation tests remove
   the actual guards in isolated copies, never shared process termination.
4. Extract the actual local and scan-spec algorithms. Compare equal fixed
   finite inputs/output oracles and record CPU/allocation plus row visits.
   This is bounded model/OCaml execution, not native compiler/GC measurement.
   No cache, worker, budget increase or unrelated optimization track.
5. Fresh full-kernel integration; update the dated audit, indexes and handoff
   with exact hashes and still-open ownership/physical-refinement obligations.

Unknowns not silently selected: caller/affine ownership issuance, external
borrow/pin admission, escaping root generation and arena-domain identity,
finite generation exhaustion, indexed subtree enumeration and its bound,
finalizer/reentrancy/concurrency, physical allocator/frame issuer and growth.
The existing doc 28 contract owns those requirements. No compiler/runtime
implementation or language syntax adoption is authorized by this model work.

## Edit scopes and validation

Main is sole implementation/integration owner; no subagents or worktrees.
Edit only the named model, independent consumer/extraction/gate, related
Make/formal snapshot wiring, audit navigation and handoff. Preserve the
unrelated shared dirty tree. Use apply_patch, read-only checks, isolated
Rocq/OCaml execution. Static 60s, focused 300s, integration 1800s.
No staging/commit/push/install/CI publication or GUI message in this scope.
Results are bounded implementation evidence, never SoT/self-host CLOSED.

## Observed integration receipt

The whole planned bounded chain is migrated and verified. Both retirement
steps require `NoDup`; exact unique witnesses retain always-successful
release/root drop. Forest/RC transitions and final proof witnesses consume
the local index transforms. The scans remain reference specifications only.
Untouched field rows return directly, not via `row ++ []`.

PASS: fresh focused 3-module kernel, zero assumptions; 98,304 field updates,
16,384 teardowns, 16 parent cases, four uniqueness controls; four guard
mutations, a same-value unnecessary-copy mutation and invalid CLI refusal.
Full snapshot: 64 owners plus seven permanent consumers, 71 kernel modules
and the approval export/binding consumer; only the two existing approved
Slot abstractions. All 71 source hashes still match. Kernel refusal self-test
passes. Documentation quality, evidence-lifecycle registration, scoped UTF-8,
links/whitespace, Bash syntax, Make target and CI YAML bindings pass.
Make/formal-CI wiring exists; remote CI was not run. Final Windows Git view:
303 dirty entries (300 at entry), zero staged paths, unchanged base HEAD.

The fixed 512/4,096/16,384-node observations include dispersed indexes,
crowded old and crowded new target rows, and parent/root rows. Appending to
a crowded new row remains linear; unique-list certificate checking is
quadratic and is not a production issuer. Raw five-sample CPU/allocation,
source/toolchain hashes and exact limitations are in the
[dated audit](../audits/ownership_teardown_redteam_locality_2026-10-08.md).
The self-host hold and all listed physical/authority refinement obligations
remain OPEN, not blockers hidden by a model-only green result.

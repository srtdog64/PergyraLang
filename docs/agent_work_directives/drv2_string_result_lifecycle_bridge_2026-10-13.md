# DRV-2 ordinary result lifecycle bridge

Status: ACTIVE IMPLEMENTATION PREPARATION / INTERFACE REVIEW FIRST.
Base: shared main `3a2eefa827b7b7bd15132a8d5585b07752ccada4`, with 522
remaining dirty paths and an empty index at the initial check. This is one
continuation of canonical DRV-2 admission recovery, not another self-host rung.
Exactly the three existing implementers are reused. No new branch/worktree.

## Objective card

- Objective: connect ordinary result materialization to its actual storage
  owner and eventual caller retirement, without user-written ownership
  ceremony or a diagnostic exemption. Basic closure still requires unchanged
  canonical refusals 0, bootstrap/installed-driver evidence and exact-SHA CI.
- Priority: the adopted value semantics and one admitted owner, deep copies
  before drops, origin/allocator/glue and generation binding, both backend
  consumers, negative evidence, then patch size.
- Semantic authority: `27_ownership_clean.md` sections 4, 5.1-5.4 and 5.10.1.
  The designed ownership-clean join owns the decision; existing checked-call,
  type, runtime-operation and ABI-layout owners retain their identities.
  A new production fact issuer requires its owner/carrier/complete consumers
  registered before source implementation. No CLOSED row is authorized here.
- Last consumers: native C AST/MIR and LLVM AST/MIR materialization and
  retirement, MIR serialization/admission and the reached self-host consumers.
  A transport receipt or runtime leaf test alone does not close this chain.
- Forbidden: String/category/layout/input-retention/body-bit as freshness;
  strdup at one return without a lifetime plan; shallow owning copy; missing
  fact interpreted as borrowed; drop of static, region or borrowed storage;
  a new source annotation/copy workaround; new micro-owners; limit increases;
  fallback to the old decision for a covered operand.
- Integration owner: root. Focused acceptance must observe actual C/LLVM
  output, allocation independence, exact ownership retirement and fail-closed
  validation. The existing full canonical gate then measures the same input.

## Verified whole chain and open decisions

1. Typed expression/call/member/index owners and checked publication bind the
   original result and argument identities. Constructor/Some/Unwrap receipts
   prove dependence, not independently owned storage.
2. The parameter-flow owner retains true returned backing dependencies. Its
   current raw masks and diagnostics do not normalize these into an owning
   result. Three direct-ref String COPY_ONLY exemptions remain open.
3. HIR/MIR currently carry parameter masks, collection ownership and region
   facts, not a joined ordinary-result origin/allocator/glue/retirement fact.
4. Return capture evaluates the RHS once before existing exit cleanup. It
   does not deep-copy a heap part or create caller cleanup responsibility.
5. String is physically char*. Static, region and heap storage are distinct;
   heap class alone does not distinguish a borrowed getter from fresh storage.
   Specialized collection releases are not general ordinary-value retirement.
6. The required producer and final-generation liveness join are missing.
   Existing nominal ABI captures only a bounded physical subset; widening its
   layout cannot serve as an ownership proof.

The fixed canonical receipt kO5m40/gnviet contains 6599 errors: 1830 direct,
4214 callsite and 555 return reports, with source/native guards passing.
Declared return-type counts are navigation only, not independent defects or
eligible copies. The scoped nested-array Clone prerequisite is already on
main; it did not reduce this refusal count.

Before production edits, the three implementation lanes return a joined
interface proposal grounded in the existing owners. Root must fix the exact
producer, supported typed shapes, identity/generation binding, copy/drop
primitive, carrier and complete caller retirement path. An unknown that can
change that ownership decision is investigated here, not hidden in an emitter.
No ordinary-result permission or new automatic drop is released before that
interface and the paired materialization/retirement change set are accepted.

## Dependency order and exactly three implementation scopes

1. A, `drv2_exclusive_flow_impl`: storage/glue boundary. Existing runtime
   String/array/region allocator owners; select actual copy and retirement
   primitives and the required allocator contract. No semantic or codegen
   edits, no changes to global nullable strdup policy. Keep unimplemented
   compound glue explicit. Initial output is the concrete interface proposal;
   root then releases the agreed runtime edits and their fault tests.
2. B, `drv2_descriptor_effect_impl`: producer/carrier boundary. Existing
   semantic publication/origin/summary and HIR/MIR owner chain. Propose the
   joined fact and one generation/lifetime issuer, registration and reached
   serialization/self-host inventory. Do not clear RETURN/COPY_ONLY or infer
   freshness from names/categories. Root releases the registered producer
   only after the complete materialization/retirement interface is fixed.
3. C, `drv2_map_payload_impl`: consumer/falsifier boundary. Existing C/LLVM
   return and ordinary-value consumers plus focused tests. Specify how one
   admitted operand decision reaches both backends and caller last-use drops,
   including temporary, early-exit and stale/missing-fact negatives. No
   duplicate backend ownership policy, builds or executing unsafe/corrupt IR.
   Root then releases the agreed consumer/test files, disjoint from A/B.

Root owns shared contracts, registry, directive, Make wiring and handoff,
interface decisions, heavy builds, integration, review dispatch and publication.
Agents must ask before extending an edit scope. All lanes belong to this one
ordinary-result bridge; their completion is not independent closure.

## Gates and operating limits

- Static 60 seconds, focused 300 seconds, integration shard 1800 seconds.
  Summary work cap4096, pressure bound3072 MiB and original timeouts unchanged.
- Root alone builds with -j2 in identified private output directories. Agents
  do not mutate index/refs, commit/push, install, kill processes or build heavy
  binaries. Sources freeze before root integration; no edits during a receipt.
- Required paired cases: literal/region/heap origin; borrowed array element or
  String field returned then source retired; Some/None active-payload behavior;
  RHS evaluated once; caller owner transferred or retired exactly once; live
  original source preserved; branch/loop/handled-exit paths.
- Missing/stale origin, allocator, glue, call tuple, operand position, generation
  and lifetime facts refuse validation without an artifact. Fault probes do
  not execute invalid LLVM or an actually dangling source-level program.
- Preserve all existing query/Slot/opaque/inout negatives. Reclassifying a
  returned value as safe requires actual normalization, not an expectation flip.
- Frozen candidate goes to the existing designated external red-team chat;
  review is separate from root verification. Scoped acceptance is not installed
  bootstrap or same-SHA required CI acceptance. No unrelated WIP is staged.

## Current flags

- Whole-chain review: mapped, concrete interface decision IN PROGRESS.
- Three implementers: reassigned to the joined bridge; production edits HELD
  only until root resolves the interface, not an indefinite implementation hold.
- General automatic cleanup and canonical closure: OPEN.
- Main CI38319266060: in progress at the initial live check; Windows, macOS
  C-only and TSan passed. Do not project those jobs into compiler green.

### Root interface decisions after the three initial proposals

The existing block-name liveness is insufficient for ordinary-value drops.
It does not carry allocation identity, ordered temporary uses, allocator/glue
or final-generation certificates. Neither backend may fill that gap.

The accepted leaf storage contract is an independent, immutable system-owned
String copy using the existing `pgy_alloc(NULL, bytes, 1)` and retirement using
`pgy_free(NULL, value, bytes)`. Allocation remains budget-charged by that owner.
The original storage is only read. Its static/region/heap class does not grant
cleanup authority. Physical char* and global nullable strdup remain unchanged.
Missing ownership must refuse before this leaf is selected; the runtime leaf
is not an authority issuer. Copy OOM and invalid NULL input have the existing
explicit terminal-panic boundary, not a recoverable rollback claim.

String lifecycle policy belongs to one cohesive shared String glue body, not
to the raw allocator owner or two independently maintained backend algorithms.
A may implement that runtime-only leaf and its actual fault tests after
confirming include/export ownership. This grants no automatic drop or return
permission. B/C remain at interface preparation until the final-generation
producer, complete supported subset and every retirement path are accepted.
Option uses the admitted tag/layout; None never reads its uninitialized payload,
and Some uses the same String leaf. No physical aggregate-return ABI is guessed.

### Observed implementation and immediate executable blocker

A implemented one shared leaf body and the two include seams; C implemented
the actual runtime harness/gate. Root observed `.tmp/string-value-copy.VAlAkh`
terminal PASS: inline and LLVM-linkable runtime_lib bodies, independent
literal/empty/system/region copies, allocator retirement once and six exact
NULL/OOM panic controls, with byte/membership guards. This is not automatic
language cleanup or ASan/LSan evidence. Root found a third reached consumer,
`pgy_runtime_cext_lib.c`; C now adds its real external linkage/execution in
the same tests at the original 300-second total budget. No limit increase.

B confirmed that issuing general ordinary-result permission now would require
a new final-generation normalizer and allocation/retirement solver. Mere
block liveness, TYPE String, copied-result flags or the leaf API cannot supply
that certificate. General producer/backend activation remains unreleased.
An immediate nonretaining call temporary is a possible first complete subset,
not an adopted schema or a solution for the complete canonical program.

CI38319266060 is now terminal RED, not pending: gen0 fails `emit_gen1.c` with
`borrow_boundary_escape`, node107167, `unproved_exclusive_member_mutation`,
`Array<String>`. Formal, Windows, macOS C-only and TSan pass; six dependent
compiler jobs skip. This repeats the base checkpoint blocker; the String
leaf did not cause or fix it. The reached Pergyra exclusive root/statement and
builtin-transition guards must be traced on that fixed published input before
general automatic-cleanup expansion. B is reassigned to that actual producer/
consumer failure set, comparing current WIP with published main, still without
an exemption or source ownership-annotation workaround. Root fixes the next
three implementation edit scopes after that concrete map is available.

# Ownership teardown: checked root epochs and saved local reads

Status: **IMPLEMENTATION COMPLETE; bounded design model only**.
Base HEAD: `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, dirty main,
303 Windows Git status entries, zero staged paths at entry.
The user asked to repair what can be repaired after the indexed-update review.
The active compiler ownership/DX implementation hold is unchanged.

## Objective card and whole chain

- Objective: a retired root identity cannot allocate, adopt or destroy nodes
  belonging to a redeclared root; a saved stale local node link has an actual
  checked read which returns no node, not merely a predicate saying it is dead.
- Priority: one identity owner, complete consumer migration, fail-closed read
  and operation admission, permanent negative evidence, then cost and patch size.
- Owner: `OwnershipTeardown.v` owns root epochs, node generations, admission,
  retirement and checked identity resolution. No wrapper heap or second root
  issuer. This is identity evidence, not a minted caller permission.
- Last consumers: forest invariant and arbitrary-run reuse proofs, all concrete
  transition witnesses, the independent red-team/extraction consumers, OCaml
  observer and the shared fresh-kernel snapshot.
- Forbidden fallback: raw root-id admission for Alloc/Attach/RootDrop; resetting
  an epoch on RootNew; unchecked dereference through a saved node handle;
  equating a valid identity with exclusive authority, loan absence or physical
  allocator/refinement completion.
- Gates: focused fresh Rocq 9.3.0 kernel and extraction, independent checked-read
  oracles and actual-guard mutations; then the complete production kernel
  snapshot and scoped documentation/registration checks.

Source survey: St stores live root ids but no root generations; Alloc and
Attach ignore the supplied generation for ORoot; RootDrop takes only an id.
RootDrop removes every old root-owned node, then RootNew may reuse the id.
The permanent consumer demonstrates that an old raw id can retire new nodes.
Node operations already require `(slot,generation)` and preserve monotonicity;
saved local links remain raw values but have no extracted checked resolver.
All affected root consumers are in the canonical owner and its independent
red-team test. The extraction driver constructs St directly and must preserve
the new field. The RC comparison uses heap/node operations, not St root epochs.

## Complete change set and dependency order

1. Append a root-epoch map to St; preserve it on nonretiring steps and RootNew.
   RootDrop advances only its own epoch, at the same retirement edge as node
   generations. Require the supplied current epoch on all three root-directed
   operations. RootNew remains the lexical declaration transition, not an
   escaping root-handle operation.
2. Migrate every constructor, invariant proof, exact-unit success witness,
   concrete fixture and reuse theorem. Prove root epochs monotone through any
   run, all root-directed operations admit only a current live root, and an old
   root handle stays unusable after retirement through any later redeclaration.
3. Add executable node/root identity checks and prove correspondence with the
   admitted predicates. Show saved local reads fail after teardown and remain
   refused after reuse. These checks do not issue authority or validate a
   caller-supplied retirement unit.
4. Migrate permanent regressions and fresh extraction; independently test
   root reuse, wrong/dead generations, off-frame epochs, and node local reads.
   Mutate the real guards in isolated snapshots and require specific refusals.
5. Run one full integration snapshot; record exact source hashes/toolchain,
   evidence and still-OPEN obligations in a successor audit and handoff.

## Reached closure-blocking operation (21:44 KST)

The focused gate on canonical SHA `f9693bbc8650770b2b8dec79f767cfd0190c7c89907e6abb4ca07dbdf8494938`
and OCaml driver SHA `5b8145c9d5e94b02cb0ec4a4f280ff14bcd94af2a20532372355f0813fc69d28`
completed 98,304 field/16,384 teardown controls, but the fixed 64 root/node
reuse rounds had consumed 65 seconds of CPU and were still running.
The reached owner is phase1; its last consumer is the checked read of the
surviving node in the repeated root-retirement fixture. Fresh extracted code
evaluates `h src` again for liveness and generation, expanding the same older
functional heap at each retirement. Preserve the immutable slot once in phase1;
this is the same value transform, not a cache or an added safety assumption.
Keep the 64-round semantic input and validation budget unchanged. Re-run the
fresh extraction/negative gate, then return to epoch/read integration. The
specific owned probe executable is stopped by its verified PID/path only;
no shared WSL process name or other lane is terminated.

## Boundaries and edit scope

Doc 28 remains the memory-boundary contract. Actual affine root/Slot binding
issuance, stable loans/pins, arena domains, finite generation exhaustion,
physical placement/growth/free, indexed unit issuance and its cost bound,
finalizers/reentrancy and concurrency remain OPEN. In particular Slot.s_val
is not a graph identity issuer. No guessed permission Boolean or alternate
backing store is added to conceal that missing binding.

Main is sole editor; no new subagents or worktrees. Use isolated scratch
compilation, no broad process termination. Edit only the canonical model,
its red-team/extraction/OCaml/gate consumers, necessary formal-CI navigation,
successor audit, documentation and handoff. Preserve unrelated dirty work.
Static 60s, focused 300s, integration 1800s. No staging, commit, push, install,
GUI readiness message or remote CI in this scope. Model closure is not compiler
implementation, SoT or installed-driver closure.

## Observed completion receipt

The whole planned root-epoch/read chain is migrated: all three existing-root
operations consume current identity; declaration preserves epochs, retirement
advances only its own epoch, and arbitrary-run stale identities stay refused.
Saved local reads use the executable resolver, not a bare safety assertion.
Every reached constructor, invariant/always-success witness, permanent typed
consumer and extracted observer is migrated. Raw-id admission is gone; pure
teardown transforms are not advertised as full admission executors.

PASS: focused fresh 3-module kernel (zero assumptions), original 98,304 field
updates/16,384 teardowns/16 parent cases/four uniqueness controls, 64 root/node
reuse rounds and 20,608 checked identities; 11 actual guard/check mutations,
two cost-oracle mutations, invalid command refusal. The same fixed repeated
input completes after source-slot snapshotting; an additional whole-selftest
observation is wall 0.10s/user CPU 0.09s, not a native/GC-speed claim.

PASS: 71 current owner/consumer modules plus approval export/binding,
64-owner formal registration, kernel refusal self-test, documentation quality,
evidence-lifecycle registration, scoped UTF-8/links/whitespace, Bash/Make and
formal-CI wiring. Only the two existing approved Slot abstractions in the full
corpus; warnings/default theory dependencies preserved. All 71 current source
hashes match. Remote CI was not run.

Final Windows Git view: 305 dirty entries, staged paths zero, unchanged HEAD.
The newly scoped directive/audit account for the two extra status entries;
unrelated dirty work is preserved. Authority/loans/arena/physical refinement
remain OPEN exactly as listed above. Compiler implementation hold unchanged.
Exact sources, receipts and next falsifier:
[successor audit](../audits/ownership_teardown_root_epoch_2026-10-08.md).

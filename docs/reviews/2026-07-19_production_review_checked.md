# Production Review Check, 2026-07-19

Source review: external production review of repository head `2575f9a7`.
Checked against `main` at `c76b6236` plus the executable change recorded in
this document. This is a claim audit, not a production-readiness declaration.

Rechecked on 2026-07-20 against the live worktree based on `74281d6a`. The
focused TSan delta below is worktree evidence until it is committed and passes
the native Ubuntu CI job.

## Objective Card

- Objective: separate still-current production blockers from findings already
  closed after the reviewed head, then advance one executable self-host rung.
- Priority: one runtime state home, semantic fact ownership, missing-fact
  rejection, executable substitution, then broader maturity work.
- Fact owner: semantic expression graphs own array-literal shape and types;
  runtime slot owners own capability and budget state.
- Last consumers: statement type facts consume expression graphs; C and LLVM
  runtime paths consume the same capability, budget, and cancellation state.
- Forbidden fallback: text-reparse array typing, per-TU mutable runtime copies,
  or backend-specific manifest/environment precedence.
- Gates: focused DRV-2 C/LLVM parity, component contract, capability
  env/manifest parity, runtime cext contract, and bitcode cancellation witness.

## Findings Closed After The Reviewed Head

The review's A1-A4 and B1 findings were valid for `2575f9a7`, but are not
current blockers at this checked head:

- **A1/A3 capability and budget split:** closed by `6f5ffba1`. The C runtime
  slot accessors have one `PGY_RT_DECL` storage owner, so inline callers and the
  external runtime object observe the same state. The runtime cext contract
  guards the single-home rule.
- **A4 environment versus manifest divergence:** closed by `16d91839`. Both
  runtime twins compute `environment INTERSECT manifest`; neither source can
  widen the other. `tests/capability/run_cap_env_manifest_parity.sh` pins both
  implementations and the disjoint restriction case.
- **A2 LLVM bitcode cancellation split:** closed by `d86b1b3d`.
  `pgy_task_*` and `pgy_async_*` are stripped from the inlined bitcode path and
  resolve to the runtime owner. The bitcode-on join-any blocked/spinloop cases
  are behavioral cancellation witnesses.
- **B1 relative external-runtime source path:** closed by `4b4ff518`. The cext
  runtime translation unit is anchored to the configured runtime directory and
  cold-cache compilation no longer depends on the caller's working directory.

The historical approximately 68 GB probe remains useful evidence of a real
amplification bug, but it is not the current measured routine-lowering
footprint. The bounded 1,816-routine LLVM self-source lowering completes at
794.4 MB peak private memory under a 3 GB fail-closed cap. This does not prove
the complete integrated producer or released driver is bounded.

## Findings That Remain Current

- Released/default compiler replacement remains 0%. Bounded self-host drivers
  and fixed points are executable evidence, not installed-driver substitution.
- `VerifiedProjectionPlan` remains partial. Intent observability has a native
  row, while layout, cleanup, checks, capability retention, composed loss, and
  artifact residue are not fully legalized.
- A focused blocking Linux TSan lane now calibrates the detector with an
  intentional pthread race, then runs pool, channel, cancellation, and zone
  concurrency tests under instrumentation. This is execution evidence for the
  covered battery, not a proof that every runtime interleaving is race-free.
  WSL2 may require `setarch "$(uname -m)" -R make test-tsan` because its normal
  address-space layout can prevent TSan from reserving shadow memory; the CI
  lane runs natively on Ubuntu.
- Capability and budget policy is still process-scoped and trusted-native
  default-open. It is not an untrusted multi-instance sandbox.
- The integrated self-host producer/consumer loop remains the next resource and
  substitution boundary even though the measured routine-lowering core is now
  bounded.

## Executable Delta Landed With This Check

Statement-return array literals no longer use the last semantic text projection
for their declared type. `SemanticAstStatementTypeFacts` consumes the parser
expression graph, fails closed when the graph is absent, and the dead
`SemanticProjectionArrayLiteralMatchesDeclaredType` implementation is deleted.

`array_return_literal` is DRV-2 fixture 37. Focused C-built and LLVM-built
self-host drivers agree on canonical MIR, emitted C, and runtime output. The
array graph gate pins the return instruction, root, final element edge, and
typed C construction. This closes initializer, assignment, and return
array-literal graph typing, not arbitrary expression typing.

## Next Executable Order

1. Carry the bounded routine-lowering result through the integrated producer
   and consumer without restoring native MIR as final authority.
2. Persist semantic place/addressability rows in MIR JSON before direct MIR
   consumers stop running the semantic body fixpoint.
3. Expand `VerifiedProjectionPlan` with one MIR-owned row family at a time,
   beginning with cleanup or runtime checks.
4. Introduce per-instance capability/budget/cancellation state before making an
   untrusted-content sandbox claim.
5. Widen TSan coverage only with deterministic stress fixtures; do not treat
   the focused battery as a whole-runtime race proof.

Current rule:

```text
Historical evidence remains evidence.
A repaired owner is not a current blocker.
A bounded compiler core is not released substitution.
```

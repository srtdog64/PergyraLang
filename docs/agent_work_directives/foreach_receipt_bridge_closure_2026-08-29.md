# Scalar CFG foreach receipt BRIDGE closure — 2026-08-29

Status: `ACTIVE — IMPLEMENTED LOCALLY, PUBLICATION PENDING` (exact base
`d7b785757a6acc7f2e08f54c31731384388294d6`; exact-head run `33224632130`
completed GREEN 30/30 and released the implementation lease)

This directive coordinates one executable closure. It is not semantic
authority, a registry verdict, progress evidence, or permission to open a
parallel implementation track on the same rung.

## Shared objective card

- Objective: close the existing
  `projection.direct_mir_scalar_cfg_foreach_receipt` row by making its reached
  local and returned `Array<Int>` plus local `Array<String>` production paths
  consume one `LoopSyntaxId`-keyed receipt without numeric wrap admission,
  ignored call ABI, or call-as-literal fallback. A valid closure changes the
  census from `CLOSED=54 BRIDGE=33 ACTIVE=1` to `55/32/1`; it does not add an
  authority row.
- Priority: lexical Int32 representability before conversion, exact returned
  call ABI carriage, producer/call ABI cross-seal, wrong-route failure before
  literal decoding, C/LLVM no-artifact negatives, old-path ratchet, then patch
  size.
- Fact owners: `DirectMirScalarIntLiteralInRange` owns canonical signed Int32
  spelling; `MirCapturedRequiredAbiLayoutRowAdmission` and
  `DirectMirArrayIntCapturedAbiReady` own the admitted call-site ArrayInt ABI;
  `DirectMirArrayIntProducerFact` owns the returned element/storage receipt;
  `DirectMirScalarCfgForEachFact` owns the final LoopSyntaxId-keyed execution
  receipt.
- Last legitimate consumers: scalar-CFG foreach C and LLVM emitters, including
  shared storage materialization for repeated returned-collection loops.
- Forbidden fallback: convert an unbounded decimal with `ToInt` before range
  admission, accept wrapped overflow, ignore a carried call ABI, trust only the
  producer ABI, treat a direct call graph as a local array literal, reparse
  `expr0`, retry the legacy range/return/Option route, or broaden this bounded
  row to unrelated effectful or element-ABI families.
- Verification gate: the integration owner runs
  `one_mir_scalar_cfg_foreach_array_int_projection.sh`,
  `one_mir_returned_array_foreach_projection.sh`, and
  `one_mir_mixed_collection_foreach_projection.sh`, then
  `sot_authority_edge_smoke.sh`. Decisive negatives are positive Int32
  overflow, producer/call layout damage, call ABI-ID damage, and false hoist;
  every target must exit nonzero and publish no artifact.

## Edit scope and overlap boundary

- The implementation scope is limited to the ArrayInt graph admission owner,
  foreach collection admission owner, the two focused gates, registry row,
  and coordination/progress documents.
- The primary task is the sole integration owner. Candidate-audit agents are
  read-only; they may not edit, change registry state, build, commit, push, or
  interpret CI as publication.
- No general collection plan, ArrayString ABI materializer, diagnostic catalog,
  enum declaration, zone authority, query/cache, or performance track is open.

## Commands and budgets

- Static owner and registry gates target 60 seconds. Focused C/LLVM parity
  targets five minutes per semantic program. One rebuilt installed DRV-2 is
  reused across all three gates.
- Full matrices remain the exact-head remote publication boundary. No new CI
  job, duplicate bootstrap, timeout increase, cache, or shard is admitted.

## Local implementation evidence

- The first local falsifier was RED: C accepted `2147483648` and emitted
  `-2147483648`. The ArrayInt graph owner now calls the existing lexical Int32
  domain owner before `ToInt`, including index literals.
- Rebuilding DRV-2 exposed a second stale seam: current MIR carries a complete
  ArrayInt ABI on hoisted call-result locals, while the consumer required that
  ABI to be empty. The consumer now admits the call row and matches its ID,
  size, alignment, and four offsets against the producer receipt. A call root
  cannot fall through to local-literal decoding.
- All three focused C/LLVM gates are GREEN. Returned foreach now includes call
  layout and call-ID mutations in addition to producer/layout/identity/CFG
  negatives. The SoT edge reports 88 authorities, 182 carriers, and
  `CLOSED=55 BRIDGE=32 ACTIVE=1`.
- Documentation, progress metric, hard contract, and likeness are GREEN;
  `result_use` is ratcheted at 4480/4480. Coq/Rocq execution is a declared
  local skip because no prover is installed; live owner/consumer binding and
  negative mutations pass. Two supplementary non-row gates do not reach
  behavior because unchanged pre-base owners already exceed their stale line
  caps (`direct_mir_array_int_abi_projection_owner.pgy` 175/160 and
  `direct_mir_multi_routine_projection_owner.pgy` 110/80); neither owner is in
  this diff and neither gate is claimed GREEN.
- Publication remains pending until the scoped diff is committed and pushed
  and replacement exact-head CI is GREEN.

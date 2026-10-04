# Current Work Handoff

Updated: 2026-10-05 KST (Asia/Seoul). Navigation only. Current source, the
SoT registry, admitted contracts, and executable gates override this note.

## Active self-host context

### Checkpoint

- Branch: `main`.
- Published base: `1e2fd61d9ec45f902dc4ef805963e6ca0780e269`
  (`docs: refresh active ownership handoff`), following functional checkpoint
  `c8863017`.
- The source checkpoint carrying this card includes owned-result evidence,
  borrowed String views for signature binding, collection mutation policy,
  source-module/location ownership, and their gates. The revision above is the
  pre-publication base, not the new checkpoint's exact-head CI receipt.
  Resolve the checkpoint with
  `git log -1 --format=%H -- docs/current_work_handoff.md`; verify local HEAD,
  dirty paths, and remote HEAD when resuming.
- Root owns integration and Git publication; Main owns the reached compiler
  implementation. Parallel edit scopes are fixed in
  `docs/agent_work_directives/ownership_checkpoint_green_2026-10-05.md`.
- Preserve `deployment_optimization_guide.md` unchanged for inclusion as a user
  document. Preserve `gmon.out` locally and exclude it from staging. Neither is
  compiler semantic authority or evidence for this rung.
- This remains an OPEN executable-rung checkpoint, not ownership SoT,
  installed-driver, bootstrap, CI, or whole-compiler closure.

### One active executable rung

The active registry row remains
`semantic.hashmap_collection_ownership` (`ACTIVE`). No registry row is promoted
by this checkpoint.

Production entrypoint:
`SemanticAstCollectionOwnershipVerdictFromResolvedFactsWithFormalEffects` in
`src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy`.

Direct native C consumers remain reachable through collection ownership,
storage-release, and type-checker paths. Therefore Pergyra has not yet replaced
the complete C-owned compiler path.

### Objective card

- Objective: move aggregate and lexical collection lifetime decisions behind
  exact Pergyra owners until the reached production path no longer depends on
  C-owned reconstruction.
- Priority: stable identity and one owner; fail-closed missing facts; delete the
  former owner; add a negative ratchet; then parity and bounded performance.
- Fact owner: the self-host collection ownership verdict and its admitted
  formal-use, exact-leaf-transfer, environment, and storage-lifetime facts.
- Last reached consumer: the MIR-root self-host bootstrap route exercised by
  `tests/self_hosted/parity/fixture/mir_collection_receiver_root.pgy`.
- Forbidden fallback: name/address/container-position identity, shallow copy as
  ownership transfer, a whitelist for the reached call, or retaining the old
  cursor/storage owner beside the replacement.
- Last terminal falsifier: bootstrap node `46983`,
  `unproved_formal_element_use_entry`, type `Array<String>`, in
  `codegen-nominal-array-declaration`. The mapped function is
  `SemanticExpressionGraphCollectionReceiverMutationFact` in
  `ast_expression_graph_collection_mutation_owner.pgy`; its `types: Slice<String>`
  parameter is the reached node. The reduced import graph passes, but the
  full-root formal-effect chain still rejects this entry. Numerical node
  movement is not closure.

### Published implementation and pending delta

- Lexical expression environments now own their lifetime directly. The former
  expression-environment storage-lifetime owner was deleted.
- Initializer environment rows replaced the separate cursor owner and its old
  cursor gate; the retired owner and gate were deleted.
- Aggregate member moves carry an exact owned-push leaf receipt. One positive
  loop fixture and eight alias/borrow/defer/restore/duplicate/push negatives
  ratchet the admitted shape.
- Owned formal forwarding admits the exact shallow-forward case while rejecting
  use after the transferred value is dropped.
- Indexed String copy policy admits the owned results of `StringJoin` and
  `TextBuilderFinish` without treating arbitrary calls as copies.
- Three resolved call-target names and one resolved call return type now
  materialize independent String results instead of returning indexed borrows.
- Pending owned-result plan, definition, and return owners derive grounded fresh
  `Array<String>` results from exact declared callable identity. The verdict
  carries `fresh_owned_result_function_syntax_ids`; this is not a name allowlist
  or permission to promote unknown/borrowed results.
- Pending routine body retirement covers 94 backing leaves:
  `67 Array<Int>`, `24 Array<String>`, and `3 Array<Bool>`, including the new
  owned-result function-ID carrier. The two deep destructure arrays retain
  `ArrayDropOwnedStrings`; other backing arrays use
  `CompilerRetireArrayStorage` at their existing last-consumer boundary.
- `SemanticAstSignatureParameterTypesBind` consumes its `own Array<Int>` index
  row, borrows three `Slice<String>` views (generic names, actual types, prior
  bindings), and returns an independent binding row. Callers retain the String
  backing arrays and retire their temporary inputs after return. This API is
  not a general proof of Slice non-retention or GUI `inout` release.

### Observed verification

These are observed receipts, not results for every later dirty-source change.
Root independently checked the cited Main receipts; this documentation refresh
does not run compiler or parity gates.

- `routine_build_storage_lifetime_owner.sh`: PASS, 94-leaf body census
  (`exec-ceb9cb8b-c271-4d5b-915c-33386c8fe824`). This is backing-retirement
  coverage, not a whole-program lifetime verdict.
- `collection_owned_result_owner.sh`: three positive and nine negative cases
  per C/LLVM PASS (`exec-0a8dc1f3`). The probe analyzed source fixtures; those
  fixtures were never emitted or run.
- `generic_return_probe_parity.sh`: native LLVM-only PASS (`exec-563b114f`).
  Installed C failed with `compiler_internal_builtin` against a stale admitted
  caller registry; full C/LLVM and installed-driver parity are not established.
- Collection-policy gate with `BACKENDS=llvm`: native C oracle plus native LLVM
  probe execution/refusal PASS (`exec-29c8ffc3`), not installed C evidence.
- Root's changed component-inventory predicates and the generic-return gate's
  structural prefix PASS. `bash -n` and `git diff --check` PASS. The full
  component inventory on Windows exceeded its unchanged 60-second budget
  (native shell exit `124`); PowerShell had previously collapsed this to `1`.
  This is neither a full component PASS nor a semantic rejection receipt.
- The reduced collection-mutation import graph produced `body_ok=true` with
  no effect-5 rows (`exec-c8b28a2c`). The current full-source native C emission
  succeeded with 0 errors and 16 warnings (`exec-730c1fe7`). The subsequent
  full-root seed failed (`exec-33d6597b`, node `46983`), so neither reduced
  analysis nor native emission establishes root bootstrap success.
- Correction to the previous exact owned-push leaf claim: one positive and eight
  negative cases produced C/LLVM analyzer verdicts. They were not emitted-fixture
  runtime executions.
- Seed bootstrap terminal receipt
  `exec-b71cb70a-ddb1-4e86-9a6c-be6b9cfa147e`: exit `1`, node `46954`,
  `unproved_formal_element_use_entry`, `Array<String>`, work directory
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.nbZkMd`.
- Mapping receipt `exec-e02fe355` identifies the function above at
  `ast_expression_graph_collection_mutation_owner.pgy:134`, with
  `SemanticExpressionGraphCollectionMutationFact` declaration/return atom.
- Latest seed `exec-33d6597b-bdda-4644-b830-8b35b92df0c3`: exit `1`, node
  `46983`, same `unproved_formal_element_use_entry`, work directory
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.bTaLNs`. Mapping
  `exec-ced9ee76` identifies the `types` Slice parameter of the reached function.
- Exact-head [CI run 37211217648](https://github.com/srtdog64/PergyraLang/actions/runs/37211217648)
  on base `1e2fd61d` completed with failure: bootstrap failed, six downstream
  Linux jobs skipped, and five other jobs succeeded. It is not current green.

### Explicit OPEN boundaries

- The last seed bootstrap failed. Pending mutation-view changes do not supply a
  successful fixed-point or same-input terminal receipt by themselves.
- Installed `pgy-self-driver.exe` admission is stale. Native/analyzer build
  success is not installation or installed-driver evidence.
- Pending publication, exact-head CI green, fixed-point bootstrap, full C/LLVM
  parity, full component gate completion, and the platform matrix are OPEN.
- Optimize only an observed operation blocking the next named closure step,
  behind its existing owner and on the same semantic input. Prior timing or
  source repetition does not prove a current bottleneck or authorize a separate
  cache/query/performance track.
- The GUI prerequisite remains separate: caller storage after `inout` is OPEN
  until callable non-retention/exclusivity proves that release is legal.
- The row stays ACTIVE until owner and last-consumer migration, missing-fact
  refusal, old-path deletion, negative gates, installed-driver evidence, and
  exact-head CI all exist.
- Do not hand off completion to `Alrescha GUI 프레임워크 1단계` until the required
  verification is complete. Alrescha remains a separate framework at
  `F:\JDW_project\alrescha`, not a compiler integration or semantic workaround.

### Language and IDE boundary

Pergyra owns machine-verifiable `WHAT MUST HOLD`: state, invariant, authority,
ownership, capability, effect, transition, intent, type, and boundary. Human
`WHY` or rationale remains in ADRs, issues, requirements, design notes, commits,
and discussions. IDEs may link those external artifacts by stable semantic
identity, but prose is not compiler authority and must not become language
syntax. `docs/00_vision.md` owns this boundary.

### Next falsifying case

Use the same `mir_collection_receiver_root.pgy` semantic input and the last
mapped receiver-mutation owner to interpret the next terminal bootstrap
receipt. Check exact formal-use evidence, view lifetime, and the last consumer
against current source. Fix only the reached owner seam; do not substitute a
smaller input, name whitelist, native bypass, compatibility fallback, duplicate
SoT row, or a general query/cache architecture. Publication and a green claim
require receipts for the exact new HEAD, not this navigation snapshot.

## Historical archive boundary

Older checkpoints are evidence in Git history, not an active work queue. Do
not revive them unless the current source, registry, or reached falsifier points
back to them.

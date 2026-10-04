# Current Work Handoff

Updated: 2026-10-04 KST (Asia/Seoul). Navigation only. Current source, the
SoT registry, admitted contracts, and executable gates override this note.

## Active self-host context

### Checkpoint

- Branch: `main`.
- Functional checkpoint:
  `c8863017` (`checkpoint: advance collection ownership lifetime closure`).
- This handoff refresh is the doc-only descendant of that functional commit.
  Verify the exact local and remote HEADs with `git rev-parse HEAD` and
  `git ls-remote origin refs/heads/main` when resuming.
- Untracked `deployment_optimization_guide.md` and `gmon.out` were deliberately
  excluded. They are not evidence for this rung.
- The checkpoint is an OPEN executable-rung checkpoint. It is not a claim that
  the ownership SoT, installed driver, bootstrap, CI, or self-hosting is closed.

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
- Current falsifier: bootstrap node `45859`, boundary
  `ArrayPushOwnedString`, diagnosed as `borrowed or unknown Array<String>` in
  `codegen-nominal-array-declaration`.

### Landed implementation

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
- Routine body storage retirement now accounts for 93 backing leaves:
  `66 Array<Int>`, `24 Array<String>`, and `3 Array<Bool>`. The two genuinely
  deep destructure arrays still use `ArrayDropOwnedStrings`; other backing
  arrays retire through `CompilerRetireArrayStorage`.
- Three resolved call-target names and one resolved call return type now
  materialize independent String results instead of returning indexed borrows.

### Observed verification

- `git diff --cached --check`: PASS before the functional commit.
- All changed shell gates: `bash -n` PASS.
- `make -j2 pgy`: PASS; current native `bin/pgy.exe` rebuilt.
- Compiler-internal builtin registry generator `--check`: PASS.
- `semantic_expression_environment_owned_lifetime_smoke.sh`: PASS.
- `routine_build_storage_lifetime_owner.sh`: PASS, including the 93-leaf body
  backing census.
- Exact owned-push leaf fixture: C and LLVM positive execution PASS; all eight
  negative boundary expectations PASS in both backends.
- The component contract passed its early 14 checker tests, line caps, lexical
  sizes, query signatures, record-shape order, and constructor order. The
  60-second budget expired afterward, so this is not a full gate PASS.
- The formal-effect report for the first bootstrap blocker changed
  `function_names` from effect `5` to `3`; the next independent return-boundary
  fix advanced bootstrap from node `45047` to node `45859`.

### Explicit OPEN boundaries

- Seed bootstrap is FAIL at node `45859`; its last observed work directory was
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.BhN0Xc`.
- Initializer-projection and compiler-internal provenance gates still delegate
  to a stale installed `pgy-self-driver.exe` and fail against the current
  registry. Native rebuild success is not installed-driver evidence.
- Exact-head CI green, fixed-point bootstrap, installed-driver parity, full
  component gate completion, and the platform matrix are not established.
- The collection formal-effect graph scan remains the main measured performance
  blocker (about 40-45 seconds inside an about 88-second body run; the full
  producer was about 106 seconds). Two attempted indexing optimizations did not
  materially improve it and were reverted.
- The GUI prerequisite remains separate: caller storage after `inout` is OPEN
  until callable non-retention/exclusivity proves that release is legal.
- The row stays ACTIVE until owner and last-consumer migration, missing-fact
  refusal, old-path deletion, negative gates, installed-driver evidence, and
  exact-head CI all exist.

### Language and IDE boundary

Pergyra owns machine-verifiable `WHAT MUST HOLD`: state, invariant, authority,
ownership, capability, effect, transition, intent, type, and boundary. Human
`WHY` or rationale remains in ADRs, issues, requirements, design notes, commits,
and discussions. IDEs may link those external artifacts by stable semantic
identity, but prose is not compiler authority and must not become language
syntax. `docs/00_vision.md` owns this boundary.

### Next falsifying case

Map bootstrap node `45859` exactly in
`tests/self_hosted/parity/fixture/mir_collection_receiver_root.pgy`. Identify
the `ArrayPushOwnedString` receiver's storage source, its last legitimate
consumer, and the missing transition fact. Fix only that owner seam and add a
focused positive/negative gate. Do not add a name whitelist, compatibility
fallback, duplicate SoT row, or general query/cache architecture.

## Historical archive boundary

Older checkpoints are evidence in Git history, not an active work queue. Do
not revive them unless the current source, registry, or reached falsifier points
back to them.

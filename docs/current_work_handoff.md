# Current Work Handoff

Updated: 2026-10-04 KST (Asia/Seoul). Navigation only. Current source, the
SoT registry, admitted contracts, and executable gates override this note.

## Active self-host context

### Checkpoint

- Branch: `main`.
- Whole-tree functional checkpoint:
  `c29f801c5e00261774dc486ea00526090054c8c1`
  (`checkpoint: consolidate ownership closure work`), parent
  `165c66b28721867716631056d464eeb3bdc23652`.
- This navigation refresh is the doc-only descendant of that functional
  checkpoint. Verify its exact HEAD and remote identity with `git rev-parse`
  when resuming; publication status is recorded below after push.
- The checkpoint has broad accumulated compiler, fixture, gate,
  audit, grammar, and vision changes. It is an explicit checkpoint, not proof
  that every included experiment is a closed SoT or that CI is green.
- No OpenAI/Codex co-author trailer belongs on the commits.

Publication record: functional checkpoint `c29f801c5e00261774dc486ea00526090054c8c1`
was pushed to `origin/main` and its remote identity was observed. The following
cleanup/navigation commit removes only three byte-identical, unconsumed root
copies of canonical `src/self_hosted/lexer/language_*` projections and records
this handoff; verify the final remote SHA when resuming.

### One active executable rung

The active registry row remains
`semantic.hashmap_collection_ownership` (`ACTIVE`). Current registry census is
`CLOSED=70 BRIDGE=23 ACTIVE=2`. No row is promoted by this checkpoint.

Production entrypoint:
`SemanticAstCollectionOwnershipVerdictFromResolvedFactsWithFormalEffects` in
`src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy`.

Direct native ownership consumers remain reachable through
`src/semantic/collection_ownership_fact.c`,
`src/semantic/array_storage_release_owner.c`, and related type-checker paths.
Therefore C-owned compiler-path substitution is not complete.

The active bounded slice prevents a `String` obtained by indexing an
`Array<String>` from being used after `ArrayDropOwnedStrings` retires its exact
storage source. Stable SyntaxNodeId/storage-definition identity is authority;
names, `Symbol *`, raw addresses, and container positions are not.

### Objective card

- Objective: make indexed-String use after deep drop inexpressible in native
  and self-host semantic admission while preserving an explicit owned copy.
- Priority: exact identity; fail-closed unknown/multi-source join; negative
  ratchet; C/LLVM parity; then broader mutation and interprocedural coverage.
- Fact owner: native
  `src/semantic/indexed_string_borrow_owner.c` and self-host
  `ast_collection_indexed_string_borrow_*_owner.pgy`.
- Last reached consumers: native identifier-use validation and self-host
  collection ownership verdict event fold.
- Forbidden fallback: clearing a loan because an expression is unsupported,
  because one branch rebinds the String, or because a call is named
  `ToString`.
- Falsifiers: direct alias after drop, branch-only reassignment, unresolved
  user-call pass-through, and `ToString(String)` pass-through. The positive
  control is `Concat("", values[0])` before drop.

### Current implementation and observed evidence

- Native and self-host owners carry exact source storage identities and a
  conservative unknown/multi-source sentinel. Assignment joins provenance; it
  does not erase a possible loan.
- `Concat`/`StringConcat` are admitted as explicit independent copies through
  resolved stdlib identity. `ToString(String)` is not a copy and preserves an
  unresolved possible loan.
- Deep drop invalidates exact-source aliases and unknown/multi-source aliases.
- Native `make -j2 pgy` passed on 2026-10-04.
- Native direct diagnostics rejected all four negative fixtures with
  `PGY_SEM_BORROW_ESCAPE` / `indexed_string_use_after_deep_drop`:
  direct alias, branch rebind, unresolved call, and ToString pass-through.
- Native C and LLVM executed the explicit `Concat` copy control and printed
  `owned-value` with zero errors and warnings.
- Current self-host source compiled through native C and LLVM with zero errors
  and nine existing unreachable-statement warnings per backend.
- The issued C and LLVM self-host analyzers rejected the same four negatives
  and admitted the explicit-copy positive (`body_ok=true`).
- The early component-contract checks passed, including 14 component-checker
  tests, line caps, lexical sizes, query signatures, record-shape order, and
  constructor order. The 60-second budget expired after the
  `match-pattern placement` checkpoint; this is a timeout, not a full gate
  PASS.
- During that run, two independent dirty-tree inconsistencies were corrected:
  the retired recursive `JsonCollectScalarFieldValues` path is now negatively
  gated, and a direct `Die` consumer imports `text_owner.pgy`.
- `git diff --check` and `git diff --cached --check` passed before publication.

### Explicit OPEN boundaries

- This slice does not yet invalidate indexed String loans for every storage-
  changing operation. `ArrayPush`, `ArrayPushOwnedString`, `ArraySet`, and
  `ArrayPop` realloc/mutation effects need their own exact transition evidence
  and negative fixtures before the indexed-loan seam is complete.
- Whole-program interprocedural String return/retention provenance remains
  conservative. An unresolved call is not treated as an owned copy.
- Loop/back-edge completeness has not been independently demonstrated beyond
  the existing collection transition gates.
- The GUI prerequisite is separate: caller storage after `inout` remains OPEN
  until callable non-retention/exclusivity proves that the caller may release
  it. The indexed-String deep-drop guard does not close that contract.
- No installed-driver, fixed-point bootstrap, full platform matrix, performance
  acceptance, or exact-HEAD CI green result is claimed here.
- The full `semantic.hashmap_collection_ownership` row remains ACTIVE until
  owner/last-consumer migration, missing-fact refusal, old-path deletion,
  negative gates, installed-driver evidence, and exact-head CI all exist.

### Language/IDE boundary retained

Pergyra owns only machine-verifiable `WHAT MUST HOLD`: state, invariant,
authority, ownership, capability, effect, transition, intent, type, and
boundary. Human rationale remains in ADRs, issues, requirements, design notes,
commits, and discussions. IDEs may link those external artifacts by stable
semantic identity, but prose is not compiler authority. The corresponding
vision text is in `docs/00_vision.md`.

### Next falsifying case

Add exact storage-version invalidation for the smallest reallocating operation,
starting with `ArrayPushOwnedString`: borrow `values[0]`, mutate the same exact
storage, then use the alias. The negative must reject in native and issued
self-host C/LLVM while an unrelated-array mutation and a final use before
mutation remain positive. Do not broaden syntax or create another SoT row.

After that slice, return to the separate GUI prerequisite: replace blanket
post-`inout` invalidation with an existing callable effect/non-retention proof,
then rerun the frozen Alrescha receipt with the exact current launcher hash.

## Historical archive boundary

The former 3,223-line accumulated archive was removed from this navigation
file on 2026-10-04. It remains recoverable in Git history. Do not reconstruct
old cards here; consult the relevant commit, audit, registry row, or executable
gate when historical evidence is needed.

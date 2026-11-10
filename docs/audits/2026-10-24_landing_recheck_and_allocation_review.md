# Formal-store landing recheck and allocation review

Status: scoped repair verified on a dirty tree; V1 acceptance remains open.
Base: `06a7980be850b695b9fdfd8715e1bc6a5e701269`, 2026-10-24 KST.

## Objective and reached chain

The selected ownership copy must be built once, with its source identity and
diagnostic location intact. Failure must preserve the source value and permit
a retry on the same tree. Priority: ownership/identity, explicit failure,
existing owner reuse, then patch size.

The driver merges selected source identities and reloads the program;
`semantic_analyze_with_landings` applies loan copies through
`ast_wrap_nodes_in_builtin_call`. Array/Map type checking applies formal-store
copies through `semantic_formal_string_store_landing`. Ordinary Clone type
checking, publication and final collection-effect consumers still decide
admission; C/LLVM consume that admitted MIR.

These existing parser and formal-store owners retain the decision. No new
semantic owner, copy policy or user annotation is introduced. Forbidden paths:
success after partial allocation, nested copies from rechecking one landing,
or treating a source function named Clone as a compiler-owned copy.

## Confirmed findings and repairs

| Finding | Evidence before repair | Repair |
|---|---|---|
| Planned formal-store recheck adds another Clone | raw owner-protocol probe fails for both a bare value and a source Clone call | recognize the compiler-built wrapper; ordinary type/ownership admission still runs |
| Synthetic formal-store copy loses origin path | both source-location assertions fail | duplicate origin before publishing the wrapper |
| AST wrapper admits a missing callee name after allocation failure | injected malloc failure returns success and changes the source node | reject a missing name and destroy the uncommitted wrapper |
| Formal-store allocation failure leaks a partial wrapper / consumes setup state | source inspection of unowned early returns | reserve a null argument first; destroy on failure; publish source and cursor only after all allocations succeed |

The first two cases produced four failing checks before repair. The separate
callee-name fault probe printed `refused=0 preserved=0` before repair.

## Observed verification

- `readonly_map_query_fact_phase_unit`: 13,457 checks, zero failures.
- `ownership_landing_allocation_unit`: 107 checks, zero failures; covers every
  reached recoverable malloc/calloc/realloc failure and same-tree recovery,
  including failure while preparing the second wrapper in a two-copy batch.
  The same unit passed with the four affected implementation modules under
  ASan/UBSan and leak detection enabled; the other frontend objects were
  reused from the normal build, so this was not a whole-frontend sanitizer run.
- Parser battery passed. Formal-store family: 21 positive admissions and three
  MIR-free refusals, preserving existing Clone/owned-push counts and diagnostic
  identities/locations where the existing fixture table specifies them.
- Five fixtures produced the independently expected output on C and LLVM:
  value map/set `tt1`, local/member alias `tt`, contiguous batch `a1b2c3`.
  Evidence: `.tmp/landing-next-runtime.46ebu6pb/`.
- Native member release/restore passed two C/LLVM executions and four pre-MIR
  refusals (`.tmp/self_hosted/native-member-release.4Oytfk/`). Collection
  ownership semantic-to-HIR-to-MIR projection passed.
- The allocation unit is wired into the existing flow phase gate and the
  sanitizer batteries. No production allocator interface is added.

Compiler: `.tmp/codex-review-flow-bin/pgy`, SHA-256
`1508ea335488f2094f3925b4d0ba457857927612d5965426800d6787eca7f901`.
The current 841-case complete flow gate, canonical census, bootstrap, installed
driver and remote CI were not rerun for this slice. The preceding full-flow
PASS predates this repair and is not claimed as current evidence. Other
workers' String-result proof, self-host and truth-state changes were preserved.
No commit or push was performed by this side review.

## Follow-up objective: keep reloads in the selected source generation

Base: `4ddb879e72995c0c50a8eeaf0a262039c86bc8a1`, with the shared dirty
changes above preserved. Scope: the native driver's landing reload boundary.

- Invariant: a landing identity is consumed only with the source contents and
  ordered canonical module paths that produced it. An equal syntax-ID ceiling
  does not establish this identity.
- Priority: exact input identity, fail-closed refusal, existing loader reuse,
  then patch size. No ownership-policy, public-syntax or annotation change.
- Fact owner: the import resolver retains the actual buffers it parsed when
  the module loader requests an input snapshot. The module graph owns their
  lifetime. No second file read, independent cache or hash-only authority.
- Last consumer: `driver_semantic_analyze_landed`, before it applies a prior
  round's store/copy selections. Retire the previous graph after comparing the
  new graph's inputs, including on a failed reload.
- Forbidden fallback: accepting a changed input because its node count or
  paths match; returning an earlier semantic result when a copy-plan merge
  fails; reconstructing a full AST through the expression-only `ast_clone`.
- Required changes: import loader capture/cleanup, graph ownership/comparison,
  driver comparison and explicit plan-merge failure, focused executable gate.
- Falsifiers: same-size root/import edits, empty imports, changed module paths,
  missing input evidence, and same-source restoration. Preserve the existing
  landing positives, MIR-free negatives and recoverable-allocation checks.
- Scope limit: source buffers are retained for the graph lifetime (one copy of
  each parsed buffer per live graph). This is correctness storage, not a new
  cache or a claim of lower compile memory. Canonical/CI closure stays open.

### Follow-up implementation and observed evidence

The import resolver transfers its parsed buffers into the requested snapshot;
the module graph retires them. Before consuming any previous-round selection,
the driver compares the ordered canonical paths and full source texts with
the previous graph. A changed input fails before MIR. The syntax-ID ceiling
check remains a separate namespace invariant, not an input-identity fallback.
Copy-plan merge failure now destroys the semantic result and emits an explicit
failure instead of returning the preceding result silently.

Fresh native compiler SHA-256:
`973fa7bc20bb72f088b13a3fee2d05e939b6eea5acdce402970803f3d5fb4828`.
The reached loader/driver hashes stayed fixed during the recorded checks.

- `module-source-snapshot-test`: 55 checks, zero failures. Covers unchanged
  reloads, same-cardinality root/import edits, empty imports, changed import
  paths, missing snapshot evidence, failed imported parse cleanup, nonempty
  snapshot preservation, and exact restoration.
- Fresh phase unit: 13,457 checks, zero failures. Allocation unit: 107 checks,
  zero failures. Both were rebuilt after integrating `4ddb879e7`.
- Actual-driver falsifier: test-only `fopen` interposition substitutes a
  same-cardinality changed source on its second read. The current compiler
  refuses with `semantic landing source contents changed after reload` and
  emits zero MIR bytes. A separately linked comparison-always-true mutant
  admits it. Restoring the exact input reproduces byte-equal original MIR.
  Evidence: `.tmp/landing-generation.CDxjtd/`; harness:
  `.tmp/landing_source_generation_probe.sh`. The shim touches no repo source.
- Actual-driver plan failure: a test-only allocator fault armed at the
  landing boundary produces exit 1, zero MIR bytes and
  `semantic landing plan merge failed`. The probe accounts for this host's
  fortified `__fprintf_chk` calls; earlier unarmed probes are not evidence of
  reaching the merge failure. Evidence: `merge-fault.*` in that same directory.
- Loader/import resolver/unit under ASan, UBSan and leak detection: 55 checks,
  zero failures (`.tmp/landing-source-sanitized.Pf2fTD/`). Other frontend
  objects are reused normally: this is not a whole-frontend sanitizer run.
- Three independently expected outputs pass on both C and LLVM: value-map
  `tt1`, contiguous store batch `a1b2c3`, shadowed user Clone `2ba`.
  Evidence: `.tmp/landing-source-runtime.p3X9hv/`.
- Shell syntax and `git diff --check` pass. The build retains existing const
  qualifier warnings in `collection_ownership_effect_capture.c` and a missing
  `formal_ordinal` initializer warning in `function_param_publication_deferred.c`;
  those unrelated files were not changed here.
- The complete flow smoke's first invocation ended with exit 1 and an empty
  log, so it is not a pass. Its unbuffered diagnostic rerun reported
  `FLOW_EXIT=124` at the unchanged 300-second focused-run budget. Physical-return,
  typed-String, signature-arity, String-spoof, missing-return CFG, constructor
  and private-Int-store controls had printed PASS; the complete matrix and its
  later C/LLVM runtime/demand-budget controls did not finish. No timeout or
  safety allowance was increased. Log:
  `.tmp/landing-source-generation-flow-retry.log`.

This side follow-up changes only the reached loader/driver boundary, its
focused gate and this audit. Main-owned truth-state files and unrelated
semantic/self-host edits remain untouched. No commit, push, canonical census,
bootstrap, installed-driver or remote CI result is claimed.

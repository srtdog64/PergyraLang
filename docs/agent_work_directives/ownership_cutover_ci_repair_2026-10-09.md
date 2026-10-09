# Ownership cutover candidate: exact-SHA CI repair

Status: `ACTIVE / CANDIDATE PUSHED; CI NOT GREEN`.
Original candidate base: `13fdf36b8fb914a282ce0f6c38b9d9bb57d119fa`.
The user explicitly authorized commit, push and CI repair, then requested
`main` as the working branch and the former candidate renamed to `backup`.
Main is pushed at `24d62232`; local/remote backup preserve `aa7f0d65`.
Remote main was the ancestor `b9a75ba2` when converted. Every then-dirty file byte was
preserved. This branch convention does not activate incomplete automatic
cleanup, install the candidate officially or imply P10/CI acceptance.
Other-lane documentation updates were observed and preserved; bind current
inputs rather than assuming continuous freeze. One shared checkout, no
parallel edit lanes.
Untracked `gmon.out` is preserved and excluded.

## Objective card and reached chain

- Objective: run the complete existing CI on an identified candidate SHA and
  repair its observed failures without changing safety or validation scope.
- Priority: exact input/artifact identity, required executable and kernel
  checks, explicit refusal, then build portability and patch size.
- Owners: `ci_change_scope_owner.sh` owns full/Markdown classification;
  `ci.yml` owns artifact dependencies. `rocq_toolchain_owner.sh` and the
  installer own the admitted stable proof toolchain. Compiler meaning stays
  with its existing typed fact owners and doc 27.
- Chain: full dispatch -> native/codegen fixed point -> receipt-bound DRV-2
  -> the same installed pair for Linux contracts, backend shards and
  sanitizers; independent Windows/macOS/runtime and stable Rocq kernel jobs.
  Every reached failure is checked against that SHA, not an older main run.
- Last consumers: all existing CI jobs, fresh kernel checker and negative
  toolchain tests. A locally green slice is not remote CI or ownership closure.
- Forbidden: skipping a red job, enlarging budgets/caps, accepting an old
  artifact, compiler/backend fallback, manual source ownership ceremony, or
  changing the classification to Markdown-only. P1 dependencies remain.
- Integration owner: GPT. Gate: the full `CI` workflow on the pushed SHA.
  Falsifiers: version/stdlib/kernel mismatch, an unadmitted seed or driver,
  a failed existing test, and any required full job not executing.
- Budgets: static 60 s, focused 300 s, integration 1800 s at the unchanged
  memory ceiling. Actual remote job limits remain unchanged.

## First observed failure: fresh Rocq switch bootstrap

Run `37866547046`, job `113614585641`, fails before any proof is compiled.
The checksum-pinned opam succeeds, but fresh `ocaml-system` switch creation
cannot admit the bootstrap image's opam-managed OCaml as a system compiler.
The solver reports an unavailable `ocaml-system` invariant. Do not describe
this as a theorem failure or weaken Rocq 9.3.0 admission.

Bounded change: the existing installer creates a fresh project switch using
an explicitly versioned, source-built `ocaml-base-compiler`, independent of
the image's active switch. Existing installed switches are preserved; there
is no retry or system-compiler fallback. The same installer still pins opam,
Rocq core/runtime and Stdlib; the same runner executes fresh kernel checks.
Validate shell syntax, an isolated fresh-prefix installation, existing
version/kernel refusal tests and the full remote proof job before claiming
the bootstrap repaired. Outputs here are candidates and observations only.

## Second observed failure: macOS bounded test execution

The same run's macOS job `113614585476` passes native unit batteries and
three boundary regressions, then fails with `timeout: command not found`
in `string_result_control_ownership_smoke.sh`. This is a runner dependency,
not an ownership verdict. Its existing 45 s compilation and 10 s execution
budgets must stay bounded.

Route all three subprocess sites through the existing
`portable_process_helpers.sh` owner (GNU timeout, macOS gtimeout or bounded
Perl execution). Keep separate stdout/stderr evidence, every expected output,
three deep-drop negatives per backend, exact semantic refusal status and
absence-of-artifact checks. No unbounded branch or new timeout implementation.
Validate C/LLVM on the identified native candidate and force the existing
Perl route before rerunning macOS CI. Kernel/compiler meaning is unchanged.

Observed local validation: shell syntax and existing Rocq stable-version,
Stdlib and checker refusals PASS. The unchanged native candidate
`262768f6...919b0` passes String-result C/LLVM success/negative paths and the
forced Perl C route. Documentation quality PASS. The fresh isolated OCaml
4.14.2 / Rocq core/runtime 9.3.0 / Stdlib 9.2.0 installation completed with
exit 0 under a new project prefix, independent of the existing WSL switch.
Its fresh corpus kernel check PASS: 78 modules plus approval binding consumer,
only the two existing approved Slot abstractions. Version/Stdlib/checker
refusal self-test also PASS with that fresh toolchain. Source UTF-8 PASS.
This is local bootstrap/kernel evidence, not remote CI success. The original
candidate's Windows, TSan and codegen fixed-point jobs have passed; its DRV-2
and downstream jobs still need observation.

## Third observed failure: DRV-2 aggregate release evidence

The same run completed with the toolchain job `113618319727` refusing the
complete `driver_bootstrap_main.pgy` graph after about 384 seconds. The fresh
Pergyra codegen seed reports `borrow_boundary_escape`, syntax `214840`,
boundary `aggregate_release_plan_unproved`, type `Array<String>`. This is an
ownership-fact failure, not the earlier runner/bootstrap failures. The run's
Windows, TSan and codegen bootstrap jobs passed; dependent Linux/driver/shard
jobs were skipped. Environment repairs are pushed as `aa7f0d65`; no full CI
result is claimed for that commit yet.

Bounded whole-chain investigation (one edit owner, GPT): source artifact and
resolved expression/call identities -> admitted local member origins and
formal effects -> `ast_collection_owned_element_parameter_requirement_owner`
and field-entry requirements -> constructor field inputs/storage-definition
authority -> `ast_collection_aggregate_value_lineage_owner` -> final
`ast_collection_aggregate_release_plan_owner` and uniqueness -> ordered
release reservation/replay -> body bundle -> source codegen admission ->
receipt-bound DRV-2 -> installed-pair consumers. Identity, current generation,
exclusive storage and exact release/restoration obligations must all survive.

Before choosing a repair, identify the failing callable and enumerate all
reached lineage demands on the fixed full driver graph. Use a diagnostic-only
scratch copy of the exact CI seed if needed: each demand gets fresh mutable
trace/plan state, only the admitted read-only inventory is shared, and the
original refusal still executes. Never continue admission with a failed
partial plan. A scratch census is evidence, not a compiler fallback or a new
fact owner. Uniqueness/event replay obligations remain explicit until reached.

Required change set is fixed from that census before production edits. Repair
the existing fact producer/consumer seam, not source annotations, manual
carrier restoration or speculative copies. Preserve all six admitted
aggregate source cases, sixteen alias/borrow/stale/repeated-release/deferred
falsifiers, execution-context generation controls and C/LLVM parity in
`collection_aggregate_release_source_owner.sh`. Then re-run the complete DRV-2
source graph with the identified same-source codegen seed and full exact-SHA
CI. No P1 automatic cleanup activation or registry CLOSED claim is implied.

### Measured census obstruction: enum-value environment names

The exact downloaded CI release seed, and a diagnostic-only copy of it at
the same `-O3 -fwrapv -fno-strict-aliasing` profile, cannot reach the release
owner under a 3 GiB address-space diagnostic cap. Both stop while computing
expression places. This is not the Windows sampled-private measurement and
is not a semantic verdict. Full source hashes remained unchanged.

Scoped allocation counters identify `SemanticAstExpressionSeedEnumValues`:
by `expression-places:start` it has run 58,612 times and its two qualified-name
concatenations have allocated 19,693,632 strings, with 634,557,760 cumulative
allocator-usable bytes (not peak/live heap, and excluding chunk metadata).
Initializer refinement alone makes 9,833,376 of these calls; its total concat
delta is 9,833,378. Logs and bound inputs are under
`.tmp/ownership-cutover-ci-2026-10-09/enum-profile.*`. The unchanged original
release guard remains; no successful release census has yet been obtained.

Bounded objective: remove only this repeated name materialization so the
complete DRV-2 release census can execute within the existing ceiling.
Owner: existing `SemanticAstEnumFacts` / `ast_enum_fact_owner.pgy`, registry
family `selfhost.enum_declaration_rows`; the qualified source name is a row
projection, not a new fact family, cache, symbol identity or ABI policy.
Chain: artifact enum issuer -> same-generation admission/matching -> enum
environment seed -> existing initializer/refinement/place/assignment/body
consumers -> environment's independently owned String copies and clear.
The enum facts stay immutable through their existing artifact lifetime.
Last legitimate consumer of each per-environment copy is the existing clear.

Required change set: issue `variant_qualified_names` once alongside existing
ordered variant rows; check equal row counts and exact enum/dot/variant bytes
without allocating; bind the projection in artifact matching; update the
single invalid-fact constructor; make the environment read this column, with
no reconstruction/missing-column fallback. Canonical callable spellings,
bare-name shadowing, payload exclusion and backend behavior stay unchanged.
Retain owned per-environment copies and their existing retirement contract.
No annotations, deep-drop recipes or memory allowance are added.

Acceptance/falsifiers: C/LLVM execution checks exact qualified and bare rows,
payload exclusion, shadowing, retention after environment clear, missing or
corrupt qualified columns, foreign artifact facts and refusal without partial
publication. The existing enum lifetime/source ratchet forbids local concat
reconstruction; the existing callable projection and aggregate release gates
remain required. First preserve a byte-bound full source falsifier, then run
the new executable on that SAME input and on the current production driver.
If another operation still obstructs the census, measure it before extending
the change; faster execution alone is neither substitution nor SoT closure.

## Fourth observed failure: proof receipt publication

Full run `37872111584` binds `aa7f0d65`. macOS and Windows now pass. Stable
Rocq installation, full kernel checks, version refusals, GC comparison,
memory-boundary composition and teardown red-team execution pass remotely.
The proof job then fails because its upload names a nonexistent old root-epoch
directory; the executed owner writes `ownership-teardown-authority-2026-10-08`.
This is publication failure, not a failed proof or green full proof job.

Objective/chain: preserve the bounded proof evidence actually produced by
`ownership_teardown_redteam_smoke.sh` and `ownership_cleanup_smoke.sh` through
their existing CI upload consumers. Those scripts own the paths and bytes;
the workflow derives the exact narrow paths, includes their hidden `.tmp`
location, and keeps `if-no-files-found: error`. No broad scratch upload,
skip, guessed receipt or proof/toolchain change. The existing beta-readiness
gate ratchets both path bindings and hidden-file publication. Local structural
validation is not a live upload; the full new-SHA proof job must publish both
artifacts successfully before it can be reported green.

## Current bounded candidate observations

The artifact-owned enum projection passes native C/LLVM execution: exact
qualified/bare names, payload exclusion, bare-name shadowing, eight refused
missing/corrupt/foreign inputs and retained facts after environment clear.
The existing aggregate-source gate still passes all six positives and sixteen
falsifiers plus generation guards per backend. These fixtures are parsed and
analyzed; they are not installed-driver or target-runtime evidence. Enum and
environment lifetime ratchets pass. Two stale structural expectations were
aligned with their already-existing owners: body assembly lives in
`ast_body_type_bundle_assembly_owner.pgy`, and independently owned actual
type names are issued by `ArrayPushOwnedString`, not local `Concat`.
No source ownership annotation, release guard, backend fallback or cap changed.

The new task-owned native compiler is
`.tmp/ownership-cutover-ci-2026-10-09/native-rebuilt/pgy.exe`, SHA-256
`22eb1794d89dd426281f67abe0cb1001ea9cf558e8310bff1022550e7e3f9c5a`.
GPT relinked this path after incorrectly ordering the portable-timeout output
arguments and truncating the old scratch-only native executable. The old
`262768f6...919b0` receipt remains historical, not a usable current executable
identity. Official `bin` files and source were not overwritten. No old MIR
receipt or fixed-point admission is transferred to this replacement.

A native-built engine containing the new Pergyra projection crosses places
and assignments on the exact preserved full driver AST, but terminates with
signal 11 at statement analysis under the unchanged 3 GiB address-space cap:
222.41 s, max RSS 3,087,792 KiB, input binding unchanged. The cause is not yet
established; this is not a semantic verdict. Its self-codegen attempt also
fails with allocator OOM, 158.90 s / 3,098,332 KiB, and produces no gen1.
Neither run is receipt-admitted gen2 or a successful full-driver test.

Diagnostic-only replay of the exact same row projection in the downloaded
CI gen2 C keeps that seed's original lineage, uniqueness and final refusal.
It is explicitly a modified scratch diagnostic, not a real rebuilt seed,
compiler fallback or fixed-point artifact. All lineage demands get separate
mutable witnesses and the original admission executes afterward. Small
controls retain both outcomes: an owned table passes (three demands), and a
repeated-release case is still refused after six lineage demands. Full fixed
AST census remains pending; no whole-chain ownership decision is inferred
from those controls. Same-input/cap/binding and original/new diagnostic
artifact identities must be retained before extending the bounded repair.

That full fixed-AST replay also exits with signal 11 during statements,
159.72 s / max RSS 3,087,952 KiB, unchanged endpoint hashes. The enum-seed
concat counter stays zero through 218,621 seed calls; the last completed
assignment stage reports 1,744,653,184 allocator-used bytes plus 276,795,392
mapped bytes. These are scratch `mallinfo2` observations, not Windows private
memory or proof of the signal's cause. No full release census was reached.
The next obstruction is statement analysis; measure its reached operation
before extending the repair. Do not loosen release admission or the cap.

The changed environment owner's comment-excluded size remains at the
existing 680-line cap. Its small readiness routine checks the two qualified
rows; the executable fixture owns the complete four-name/type/mode matrix
including both bare aliases. No cap increase or removed negative is used.

Documentation quality, evidence-lifetime correspondence including its RED
self-test, and native-source UTF-8 checks pass on the current tree. Full P1,
production automatic cleanup, complete driver/installed pair and remote CI
are still OPEN.

## Fifth observed failure: container Git metadata for model receipts

Main `f8173daa0ac607cdf604d1452c43a81e06b983ba`, full run `37875813974`,
proof job `113644095458`: the bounded teardown artifact uploads successfully.
Fresh adequacy and ownership extraction/control checks pass. Receipt export
then fails with Git's `detected dubious ownership` at the mounted checkout;
it never publishes a model-cost receipt. No theorem or performance superiority
claim follows from this infrastructure failure or the passed controls.

Objective/owner chain: authenticated Actions checkout -> this container
step's exact Git trust context -> unchanged receipt generator's bound-source
status and HEAD queries -> existing model-cost JSON and narrow artifact upload.
The workflow owns the approved checkout context; the generator must continue
to fail on unavailable status/HEAD, changed sources or incomplete measurements.
Required change: clear inherited safe-directory allowances for this step and
allow only `github.workspace` through process-scoped protected Git config.
Do not write a persistent/global wildcard, guess clean status, catch the Git
failure or alter proof/extraction/benchmark inputs. A focused executable gate
must force the ownership check: the approved checkout succeeds, an unrelated
fresh repository is refused, and an explicitly approved fixture succeeds.
Register that gate in the existing Make/CI owners and ratchet the workflow
binding in beta-readiness. Local context controls do not substitute for the
new exact-SHA remote cost receipt and both successful uploads.

Observed local controls: the new gate passes through the same project-owned
opam runner under `GIT_TEST_ASSUME_DIFFERENT_OWNER=1`. The unchanged cleanup
gate then kernel-checks five modules with zero assumptions, executes its
decision/refusal/composition/read-only/place/exit controls and exports all
30 fixed cost cases. Receipt HEAD is `f8173daa`, bound-source dirty entries
zero. This validates process-context propagation and the actual receipt
exporter locally; it is not the new workflow's remote upload or production
memory-management performance. Beta-readiness and shell syntax pass.

## Second measured census obstruction: array-storage mutation

Before any production edit, a separate scoped counter on the same preserved
full AST and diagnostic seed replay confirms an allocator refusal during
statement analysis. Bindings compare equal; address-space cap remains
3 GiB. At 165.11 seconds, row 19069/node 67698, a four-byte malloc fails in
`SemanticExpressionGraphArrayStorageMutationFact`. Its 919 reached mutation
checks have made 26,868,025 concatenations and accumulated 712,945,336
allocator-usable bytes. These are cumulative scoped allocation observations,
not peak/live heap, Windows private memory or a successful semantic census.
Evidence: `storage-mutation-profile.*` under the existing log root.

Objective: unblock the same full aggregate-demand census, not create a
performance track. Owner chain: artifact-bound local/initializer admission ->
same-generation local function/node/type columns -> the existing collection
mutation owner -> statement `ArrayPush`/`ArrayPop` and release `ArrayDrop`
consumers -> body verdict -> codegen/DRV-2. Last legitimate consumer is that
mutation verdict; no facts or independently owned relevant type copies change.
The public statement path admits local/initializer projections; production
body assembly admits analysis and builds its initializers before both routes.
An irrelevant row's allocated type copy is not an admission check.

Required change set: before type copying, exclude only other functions and
declarations at/after this use. Retain every relevant declared/inferred copy,
Slice type test, lexical scope check, exact call identity, backing storage
identity and diagnostic. Both reached consumers already use this one owner;
no second owner, index/cache, missing-fact fallback, mode annotation or
copy/drop workaround is added. Do not loosen the existing live-Slice rule or
implement a last-use lifetime contract here.

Acceptance: extend the existing executable value-wrapper/read-only source
observer gate, reusing its one import-composed analyzer per C/LLVM backend.
Same-backing live growth, pop and release must refuse with the exact Slice
diagnostic; different function, future declaration, completed lexical scope
and sibling backing must remain admissible. Keep all existing descriptor,
alias and aggregate-release controls. Ratchet the old unconditional-copy
ordering, then run the same full AST/cap with identified source/executable
bindings and original final release admission. Full current-source DRV-2,
installed-pair and exact-SHA CI remain separate acceptance obligations.

Observed candidate: the expanded value-wrapper gate passes both C and LLVM
on the rebuilt task-owned native compiler: eight source positives and fifteen
refusals per backend, plus existing executable wrapper/recursive values.
The aggregate source gate still passes all six positives, sixteen falsifiers
and generation guards. These source cases are parsed/analyzed, not emitted
target programs. Beta-readiness, documentation, evidence lifetime/RED self-test,
shell syntax and gate reachability pass; no validation allowance changed.

The same fixed pretty-AST diagnostic now completes statement/verdict analysis
within the cap: 279.67 s, max RSS 2,985,152 KiB, bindings unchanged. Across all
3,877 storage-mutation checks it records 88,468 concats / 2,264,960 cumulative
usable bytes. The old partial run stopped at check 919; do not report these
unequal reached counts as a full-run speedup. It then refuses
`compiler_internal_builtin`, syntax 64082, rather than reaching the release
census. This pretty-AST route is not the production source/provenance route;
it is useful for the fixed allocation comparison but not semantic evidence
for the CI aggregate failure. Do not bypass the internal-builtin guard or
invent missing declaration provenance. The next diagnostic derives directly
from the downloaded `f8173daa` gen2 and uses the actual source entrypoint,
with endpoint hashes over all self-host source files and original admission.

That fresh-seed diagnostic passes independent controls through the SOURCE
entrypoint: three owned-table lineage demands pass; six repeated-release
lineage demands finish, followed by the original uniqueness refusal. The
complete current-source run was in progress at that observation. The diagnostic GCC build emits
existing generated const-qualifier warnings; this scratch build is neither
a receipt-admitted rebuilt seed nor an installed compiler. It does not change
the admitted source guards, final release/uniqueness verdict or validation cap.

The subsequent full SOURCE run exits 134 at named-boundary graph validation:
231.98 s / max RSS 3,065,416 KiB, unchanged 3 GiB address-space cap and
2,531 source plus four diagnostic endpoint bindings. A two-byte malloc fails in
`AstExpressionSignedDecimalWithin` via arena and carried-call-target
readiness. This is not a semantic release failure or a full census. A scoped
counter on that owner subsequently measured 2,231,009 calls, 4,755,689
temporary character strings and 114,235,064 cumulative allocator-usable
bytes before the same refusal. Wall time was 360.62 s / max RSS 3,065,036
KiB; all 2,535 endpoint bindings match. These are scoped cumulative costs,
not peak/live heap or proof that this operation dominates the entire run.
Candidate `24d62232` is pushed and full run `37879549165` completes RED at
the installed-driver build; dependent jobs are skipped.
Its remote proof job now succeeds and publishes both narrow artifacts. The
downloaded model-cost receipt binds this exact HEAD, reports zero bound-source
dirty entries and contains all 30 cases. Windows, macOS, TSan and codegen
bootstrap also pass; DRV-2, the installed pair and P1 closure remain OPEN.

### Bounded numeric payload repair card

- Objective: remove only the measured character-string allocation inside
  `AstExpressionSignedDecimalWithin` to unblock the same actual-source
  aggregate-demand census under the unchanged 3 GiB address-space cap.
- Priority: identical admitted numeric payloads and graph identity, explicit
  invalid-input refusal, same-input census, then allocation cost and patch size.
- Owner/chain: typed expression payload issuer -> existing expression-graph
  numeric admission -> arena/carried-call-target validation -> named-value
  boundary/body bundle -> final release admission -> source codegen/DRV-2.
  Parser primary literals, direct-MIR scalar readiness/kinds and MIR expression
  sequence validation also consume these same Int/Long predicates; their
  public identities and range rules remain unchanged.
  Last legitimate consumer is numeric admission; no retained fact is added.
- Required change: read ASCII sign/digit/zero bytes through the existing
  nonallocating `CodegenCharCodeAt` owner. Preserve signed width bounds,
  leading zeros, suffix handling and all arena/edge/call-identity checks.
  Explicitly reject a negative or beyond-text digit extent: the old raw
  predicate accepts `("1", 2)` because an out-of-range character becomes the
  empty string. Public Int/Long issuers already supply exact valid extents.
  Leave Long's separate suffix operation and other scans unchanged.
- Forbidden: cached readiness, bypassed graph admission, weaker range rules,
  a new fact owner, annotation/manual-drop workarounds or raised budgets.
- Acceptance/falsifiers: native-built C/LLVM execution of 78,322 comparisons
  against the frozen old algorithm on valid byte extents, independent Int/Long
  boundary/sign/zero/malformed/Unicode cases and invalid-extent refusal.
  The original implementation fails the beyond-text negative with exit 3.
  Ratchet the allocating read path within this one predicate. Then rerun the
  same source entrypoint/cap with all source and diagnostic bindings recorded,
  fresh lineage witnesses and the original final admission. Modified seed
  diagnostics are not receipt-admitted executables or installed-driver proof.

Observed numeric candidate: the focused native-built C/LLVM gate passes all
78,322 valid-extent comparisons, 33 independent Int/Long payload cases and
their arena checks, unknown-kind/scalar-child controls and invalid extents.
Existing Long exactness/overflow, documentation, evidence-lifetime/RED,
program-graph, source UTF-8, beta-readiness and gate-reachability checks pass.
Small SOURCE census controls preserve three successful owned-table demands
and six repeated-release demands followed by the original refusal.

The whole-source comparison reads a non-worktree `24d62232` snapshot. Git
archive changed the raw checkout line endings of 24 files; the hash guard
refused that initial input before running the executable. Only those 24
scratch files were copied from independently baseline-hash-matched originals;
no working source was rewritten. All 2,531 raw source hashes then match the
previous numeric run exactly and remain stable through execution.

Same input/cap, identified candidate `6fbb1cbd...a709`: 2,926,301 numeric
calls, zero character-string allocations and zero scoped cumulative usable
bytes. Named-boundary, generic and owned-result validation now finish. The
run still exits 134 at `SemanticAstCollectionFormalEffectsWithSurfaceOrder`
when a 24-byte allocation fails: wall 425.76 s / max RSS 3,063,828 KiB,
source and all four diagnostic binding comparisons zero. This is a later
partial run, not a full-run speedup or successful release census. Measure
that next operation before changing it; do not infer dominance or increase
the cap. The complete failure set and the producer/consumer ownership repair
are still OPEN. Numeric repair does not change formal modes, releases, ABI,
automatic-cleanup activation or a SoT registry status.

Exact remote result `24d62232` / `37879549165`: DRV-2 job `113661282480`
refuses syntax 214857 / `aggregate_release_plan_unproved` / `Array<String>`
after about 382.58 s. The downloaded seed's source/binary hashes match its
receipt (`e180a92c...42c2`, `faf77ad0...cf64`). Proofs and both publications,
Windows, macOS, TSan and codegen fixed point pass; dependent jobs are skipped.
The numeric source follow-up requires its own exact-SHA CI, not those results.

Full remote run `37875813974` completed RED. DRV-2 job `113649446103` refuses
the actual source graph at syntax 214852 / `aggregate_release_plan_unproved`
after about 392.79 s. Dependent jobs are skipped. Windows, macOS, TSan and
codegen fixed point pass; stable proof controls pass but cost publication
is red as described above. Neither bounded allocation repair closes that
release contract or activates automatic cleanup.

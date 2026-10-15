# Archived work integration audit — 2026-10-13

Status: READ-ONLY REVIEW; PARTIAL INTEGRATION / UNRESOLVED DISPOSITIONS.
This is navigation and source evidence, not compiler semantic authority,
independent red-team approval, gate execution or a main landing receipt.

## Boundary and method

Reviewed main `56595078bada004e3fc5df3d88aa98e0633f4d50` and its actual working
files, using `.tmp/main-integration-census-20261013.json`: 71 archived tips,
10 ancestors, 36 patch-equivalent-only tips, 25 unmatched targets and 89 distinct
unmatched commits. Commit objects were read; no branch or worktree was recreated.
The verified external bundle remains at the directive's backup location.

For every one of the 89 unmatched commits, `git diff-tree --raw --no-abbrev`
provided its old/new blob and path. Every existing working path, including
untracked files, was compared using `git hash-object --path` against that exact
postimage and HEAD's tree. Generated evidence is
`.tmp/main-archived-postimages-20261013.json`; the bounded reader is
`.tmp/main-archived-postimage-audit.ps1`. Source-backed semantic findings below
are distinct from those mechanical comparisons.

1710 changed-path occurrences, NOT unique files or integrated behaviors:
144 exact HEAD postimages; 469 exact working-only postimages; 878 divergent
existing files; 211 missing-path occurrences; 3 retained deletions and 5 old
deletions not retained. Multiple commits repeat the same path. Divergence does
not prove omission; matching bytes do not prove executable closure.

No source/contract/gate/handoff edit, build, runtime probe, commit, push, branch,
worktree, process termination or subagent occurred. Only this audit and private
inventory artifacts were written. Existing dirty implementation is preserved.

## Most important real omissions and false omissions

### 1. False deletion: the active DRV-2 owners are present

`function_param_publication_owner.c` and
`function_param_flow_program_point_index_owner.c` are untracked relative to main,
but exist. Their Git-normalized blobs are respectively
`be6761b15c595a0f53e225d4a2612c47a42b0f03` and
`ead17b5009873b0a67621203a055545db3346138`, exactly the postimages in `995bda22`.
Plain tracked-only diff must not label them deleted. The active implementation
is working-tree-only, NOT integrated into the committed main tree.

### 2. Source-backed missing overlap protections, not merely a rename

Archived `5f14ef00` adds two protections in
`type_check_function_symbol_call`: a readonly REF formal must not be forwarded
to MUT_REF, and the same actual must not be supplied to overlapping REF/MUT_REF
formals. The current `type_checker_helpers_late.c:199` checks MUT_REF-only
duplicates; the current Pergyra alias verdict also checks inout-only collisions.
The four archived additional fixtures are absent:
`direct_mir_readonly_ref_to_inout_negative`,
`direct_mir_ref_inout_same_binding_negative`,
`direct_mir_ref_ref_same_binding_control`, and
`parser_expression_graph_rows_self_alias_negative`.

Disposition: those exact protections are missing from the surveyed owner chain.
C subsequently ran identified `78fVEh` analyze-only probes with start/end guards:
readonly REF Int -> inout, REF+inout same binding, and reverse inout+REF all
returned rc0 and produced MIR; a REF/REF control also passed. This confirms no
equivalent current admission rejection for these shapes. A did not execute
these probes or any generated artifact; semantic admission is not a runtime UB
claim. Root owns C's exact source/command/hash receipt and integration decision.

The archived `parser_expression_graph_rows_self_alias_negative` is NOT a valid
paired ownership falsifier as written: C observed TYPE_MISMATCH because its
callee expects Array<Rows>, not Rows. Preserve its historical content, but do
not count that type error as borrow-overlap coverage or demand an unsound grant.
Do not blindly restore the old name/scope-based patch. Root must reconcile the
current typed identity/borrow contract and authorize one producer/consumer plan.

### 3. Genuine absent GUI artifacts; validity/discard decision unresolved

All 11 postimage paths of `2c2d6010` are absent, including
`examples/gui_framework/main.pgy`, apps/settings/todo/checkout,
`missing_variant.pgy`, two GUI smoke scripts and four audit/directive documents.
The archived comparison document calls this a console typed-state/reducer
prototype, explicitly NOT a native GUI runtime. No replacement or deliberate
discard decision for these exact console artifacts was found in main history:
`git log HEAD --all --full-history -- examples/gui_framework ...` returns no
GUI add/delete/move history. The archived addition remains a disconnected
preserved commit, not an observed main deletion.

The current product responsibility has a concrete replacement boundary:
`F:/JDW_project/alrescha/README.md` and `AGENTS.md` define Alrescha as an
independent framework; Pergyra's `gui_compiler_boundary_backlog_2026-10-07.md`
keeps ADT/reducer/pixel/runtime policy there and binding/type/MIR/ABI here.
`public_array_storage_release_2026-10-01.md:125` also preserves the explicit
user boundary. This supersedes treating a GUI framework as compiler-owned
product work, but does not prove byte-for-byte migration of the old console
example or an explicit decision to discard its regression fixtures.
Disposition: framework scope replaced by the independent Alrescha contract;
exact historical sample/fixture retention decision OPEN. Do not restore a
framework coupling, open a GUI track or import the old snapshot automatically.

### 4. Large collection snapshots cannot be called fully superseded

`b47512a0`, `ebc91a25`, `6c4433b7`, `24c97519`, `43601b61`, `c04e2bc0`
contain missing native collection parameter/call-boundary owners, Pergyra
operation/parameter-contract/index owners, and many positive/negative gates.
Examples: `collection_parameter_boundary_fact.[ch]`,
`collection_call_check_schedule.[ch]`,
`collection_parameter_contract_fact_owner.pgy`,
`collection_call_boundary_*_owner.pgy`, `collection_operation_*_owner.pgy`,
`array_drop_storage_smoke.sh`, `array_push_move_string_smoke.sh`,
`collection_parameter_boundary_smoke.sh` and own/inout/deep-drop fixtures.

Current collection ownership/deep-drop/retirement owners exist, but a current
equivalent of the old `semantic_collection_own_transfer_paths_exclusive`,
forwarded-anchor and call-check schedule seams was not established. The old
native parameter owner alone was 1552 lines, so wholesale restoration would
also conflict with current cohesion/cap policy. Disposition: partially present,
actual missing artifacts, semantic replacement/validity UNRESOLVED. Root and
C's acceptance map must join this family before claiming all valid work landed.

### 5. Site demotion conflicts with the retained current site

`e2d579c2` deletes site files and adds homepage-generator fixtures. Current
`site/README.md`, `site/pergyra_home.pgy`, build/output and site smoke remain;
the new archived fixture paths are absent. Current README and document164 still
name the official generated homepage. Disposition: old deletion not adopted;
current retained site is explicit evidence, but the validity of the demotion
and replacement fixture decision is unresolved, not an approved discard.

## Evidence-backed applied / replaced families

These findings establish source integration, not a fresh gate execution.

| Behavior | Archived / main evidence | Current owner/last consumer |
| --- | --- | --- |
| Never and all-path return | `9a24f9f0` -> main `dd654896`; `6219d11b` -> `58421224` | old Never owner removed; `ast_body_flow_verdict_owner.pgy:412-419` owns return/fallthrough, existing Never gate remains |
| File read error vs EOF | `5ae7ab3b` -> `d0a2aafa` | both runtime readers use `ferror` -> READ_FAILED vs EOF; five postimages exact in main, runtime test exists |
| LLVM runtime-only attributes | `fac2de5d` -> `ac018238` | registry runtime_entrypoint bit; `llvm_api.c:344-345` gates attributes; attribute smoke exists |
| Enum/literal/or match guards | `a03a2b50` -> `761531dc` | current LLVM MIR match-condition owner; exact fixture/stdout/gate postimages in main |
| JSON string read Option | `b1ad0fcd` / `be0e265e` -> `3b0f9567` | current bounded JSON readers; later typed/result ownership changes mean patch mismatch is not omission |
| Builtin unresolved diagnostic | `fd7bd834` / `c07e0864` -> `8b391d0a` | native_pipeline_only builtin owner exists and builtin surface gate remains registered |
| Qualified enum constructor | `5fb06606` -> `e5f86a34` | existing checked enum constructor route, current source/parity consumer |
| Enum body method parsing | `de981159` -> `a0b33bc7` | current parser route; two exact postimages in main |
| UnwrapErr/native builtin parity | `961206f3` -> `78a9b6e2`; `0731325c` -> `925449b4` | existing result/variant producer and builtin surface parity |
| Numeric conversion | `576abf54` -> `82d40e82`; `36233f36` -> `1ee9c4d0` | current ToInt/ToFloat checked producer/runtime, not old patch byte equality |
| Parent-relative output / stale artifact | `4aaacf98` -> `16fc7141`; `8893be61` -> `71c286da` | existing driver output/invalidation route; archived snapshot's extra changes still require path-specific review |
| Four-leg loop stack | `4a7d78d3` -> `3ee1df37` | current test exists; two changed postimages exact in main |

An equal subject/main counterpart alone was not used as semantic proof; the
named current source or retained fixture was also inspected. Later divergence
inside these commits does not automatically dispose of every historical hunk.

## All 25 unmatched target dispositions

| Archived target | Bounded disposition |
| --- | --- |
| arith-fixes | arithmetic/receiver changes have current route and parity evidence; audit/doc divergences unresolved |
| backup-drv2-wip-20261012 | active current working implementation, NOT committed main; history contains deliberately rolled-back producer experiment |
| batch2 | parent-relative path/ToInt/body-flow applied; UTF8 argv/extern-cap/cap-inventory differing hunks need exact consumer review |
| batch3 | unresolved-builtin behavior applied in current owner/main counterpart |
| builtin-name | Never/Option applied; shadow/reservation policy differences unresolved |
| codex/recovery-main-dirty-20260929 | partial implementation only; GUI/site/borrow-overlap/collection boundary omissions above remain |
| f12-final | unmatched Never work replaced by current body-flow owner |
| fix-builtins | native UnwrapErr and diagnostic/parity paths applied; no entire-commit closure claim |
| fix-llvm | runtime attribute and four-leg storage behavior applied |
| fix-native-wip | several exact source/test postimages present; differing subject/zone/prototype hunks unresolved |
| fix-review | existing match/UnwrapOr/codegen/parity evidence; independent audits/CI wording do not prove all hunks integrated |
| l3d-family | Never/qualified enum/stale-binary behavior applied |
| l4a | same plus Option reader applied |
| llvm-guard | match guard behavior applied |
| nb2 | Never/Option applied |
| next-batch | Never/Option applied |
| par-drf | current reach-owner and many exact fixtures present; per-hunk equivalence still unresolved |
| pp-fixes | parent-relative output current counterpart exists |
| rt-fixes | partial exact current enum/runtime tests; divergent hardening hunks unresolved |
| series | Never applied/replaced |
| series-b3-old | conversion applied; tmp-final2 divergence unresolved |
| sot-enum | conversion and enum-method parser applied |
| sot-enum3 | conversion applied |
| wip/collection-assignment-lifetime-2026-10-03 | current nominal probe evolved in active rung; owned_string_transfer_frontier_probe absent/unresolved |
| remote codex/try-file-read-error | runtime error/EOF behavior source-integrated |

## Active backup's 14 commits and current rung

The chronological snapshots are not 14 simultaneous authorities. The initial
`076dae23` has 420 exact working-only postimages; later snapshots refine its
same publication/flow chain. `a0cbb9cd` input-reader contracts, `7b5a4317`
returned backing, `11d4314e` output relations, `5701b347` no-output headers,
`1e39d7f6` batch origins and `995bda22` CAST/child seals are current working
implementation, with corresponding registry/projection/phase/query consumers.
They are still not main-tree publication and canonical is OPEN at 6898/9.

`b97d9a51` preserved a RED broad expression-publication/error-counter experiment;
`d7f57386` explicitly restored the earlier producer. It is deliberate
supersession, not missing work to reapply. Current `type_checker_expr.c` and
diagnostic owner contain no broad local-error-attempt counter restoration.
The remaining five navigation commits (`ed841902`, `71959df2`, `3d46122a`,
`06dcf1ae`, `2f6f47ad`) and snapshot handoff edits record boundaries,
not additive executable permission. Historical handoffs are superseded by
current source, owner contracts and current execution evidence.

Of `995bda22`'s 13 changed paths, nine are exact working postimages; the four
divergences are handoff, retaining CAST fixture, query gate and phase unit.
The latter three are C's approved type-correct Long comparison/diagnostic
precision repair, not lost production. No historical source spelling is to be
reinserted to erase that legitimate test correction.

## Required decisions before a whole-main integration claim

1. Resolve the missing overlap protections using current typed identities and
   paired controls, or identify the existing equivalent owner with evidence.
2. Decide GUI artifacts/site demotion and large collection boundary snapshots
   as valid missing, explicitly superseded/discarded, or incomplete WIP. Current
   absence or branch deletion is not that decision.
3. Check remaining divergent WIP hunks by behavior/consumer, not whole-file ours
   merge. Preserve the external bundle and private postimage inventory.
4. Integrate the current working-only DRV-2 owners only after original matrices,
   full canonical zero, independent review, bootstrap/installed-driver and
   exact-published-SHA required compiler CI. Native green is not canonical green.

This audit does NOT conclude that all valid work is integrated. Unknowns are
explicit rather than recorded as discarded. No new implementation track was
opened by this review.

## Per-commit mechanical coverage appendix

Every unmatched commit is listed below. `main-source` means the source family
has the current counterpart described above; `working-active` is not a main
landing; `missing/unresolved` is not a discard. A mechanical file comparison
does not override the family-level open obligations.

| Commit | Subject | Disposition / comparison evidence |
| --- | --- | --- |
| `0525f4b6` | wip: gates and cases | unresolved semantic hunk comparison; divergent-existing:5;exact-in-main:7 |
| `05afb487` | wip: host builtin inference, fixture | unresolved semantic hunk comparison; divergent-existing:1;exact-in-main:1 |
| `06dcf1ae` | docs: record guarded admission trace and interrupted review | working-active / chronological supersession; divergent-existing:1;exact-working-only:1 |
| `0731325c` | test: hold native C and the default route to one builtin surface | main-source counterpart; remaining hunk differences scoped; divergent-existing:4 |
| `076dae23` | WIP checkpoint: preserve frozen DRV-2 candidate; canonical regression OPEN | working-active / chronological supersession; deleted-current:1;divergent-existing:17;exact-working-only:420;path-missing:3 |
| `11d4314e` | fix: preserve inout output dependency recovery | working-active / chronological supersession; divergent-existing:11;exact-working-only:11 |
| `18550462` | wip: audit ci note | exact main postimage; exact-in-main:1 |
| `1d8b26e9` | wip: regenerate word inventory | unresolved semantic hunk comparison; divergent-existing:1 |
| `1e39d7f6` | wip: batch backing origins in the existing parameter index | working-active / chronological supersession; divergent-existing:9;exact-working-only:1 |
| `1fbb9617` | wip: fix expected output and bash 3.2 | unresolved semantic hunk comparison; divergent-existing:1;exact-in-main:1 |
| `24c97519` | feat(self-host): carry and retire collection operation facts | missing or unresolved; see family obligations; divergent-existing:60;path-missing:22 |
| `28cad271` | wip: parallel capture storage reach | unresolved semantic hunk comparison; divergent-existing:4 |
| `2a2b2194` | wip: inout subject receiver, enum payload types, zone param carriage | unresolved semantic hunk comparison; divergent-existing:3;exact-in-main:3 |
| `2b2a3f35` | wip: self-host review fixes | unresolved semantic hunk comparison; divergent-existing:7;exact-in-main:3 |
| `2c259c7f` | wip: parallel block rows, default C UnwrapOr | unresolved semantic hunk comparison; divergent-existing:6;exact-in-main:1 |
| `2c2d6010` | feat(examples): add typed GUI state framework | missing or unresolved; see family obligations; path-missing:11 |
| `2f651d8a` | wip: receiver order and audit | unresolved semantic hunk comparison; divergent-existing:4 |
| `2f6f47ad` | docs: record the frozen candidate and external review request | working-active / chronological supersession; divergent-existing:1 |
| `36233f36` | fix(runtime): compute ToInt the same way on every leg | main-source counterpart; remaining hunk differences scoped; divergent-existing:4;exact-in-main:4 |
| `36f38195` | wip: handle owner uses body flow block lookup | exact main postimage; exact-in-main:1 |
| `3d46122a` | docs: record verified batch and remaining admission census | working-active / chronological supersession; divergent-existing:2 |
| `3f31ee9c` | fix(driver): invalidate stale compiler outputs | unresolved semantic hunk comparison; divergent-existing:8 |
| `42c50a59` | wip: zone param diagnostic span | exact main postimage; exact-in-main:1 |
| `43601b61` | wip(snapshot): preserve staged closure work before main consolidation | missing or unresolved; see family obligations; divergent-existing:158;exact-in-main:2;path-missing:25 |
| `464a9b53` | wip: pp-020 utf8 argv | unresolved semantic hunk comparison; divergent-existing:4;exact-in-main:6 |
| `47cf0e4e` | wip: audit round five | unresolved semantic hunk comparison; divergent-existing:1 |
| `49812216` | wip: audit update | unresolved semantic hunk comparison; divergent-existing:1 |
| `4a7d78d3` | test: run loop stack storage on all four legs | main-source counterpart; remaining hunk differences scoped; divergent-existing:4;exact-in-main:2 |
| `4aaacf98` | fix(compiler): resolve a parent-relative output path before the self-host child | main-source counterpart; remaining hunk differences scoped; divergent-existing:3;exact-in-main:4 |
| `4fb191ef` | wip: parallel capture reach gate and docs | unresolved semantic hunk comparison; divergent-existing:6;exact-in-main:14 |
| `535aef04` | wip: integration fixes (direct C args, strtoll ban scope, caps) | unresolved semantic hunk comparison; divergent-existing:2;exact-in-main:2 |
| `5530041f` | fixup! refactor(self-host): return JSON string reads as Option | unresolved semantic hunk comparison; divergent-existing:1 |
| `5701b347` | wip: separate no-output proof from return eligibility | working-active / chronological supersession; divergent-existing:10;exact-working-only:2 |
| `576abf54` | fix(semantic): make ToInt and ToFloat take a String | main-source counterpart; remaining hunk differences scoped; divergent-existing:5;exact-in-main:3 |
| `5ae7ab3b` | fix(runtime): distinguish file read errors from EOF | main-source counterpart; remaining hunk differences scoped; divergent-existing:2;exact-in-main:5 |
| `5b63523e` | wip: match fixture | exact main postimage; exact-in-main:1 |
| `5f14ef00` | fix(semantic): reject overlapping inout borrows | missing or unresolved; see family obligations; divergent-existing:3;path-missing:4 |
| `5fb06606` | fix(self-host): resolve a qualified enum payload constructor | main-source counterpart; remaining hunk differences scoped; divergent-existing:4;exact-in-main:1 |
| `614a5a96` | wip: report the reach path | unresolved semantic hunk comparison; divergent-existing:2;exact-in-main:1 |
| `6219d11b` | fix(self-host): refuse a non-Void function that can reach its end | main-source counterpart; remaining hunk differences scoped; deleted-current:1;divergent-existing:16;exact-in-main:14 |
| `652f5235` | wip: codegen review fixes | unresolved semantic hunk comparison; divergent-existing:4;exact-in-main:4 |
| `6c4433b7` | feat(self-host): carry semantic ownership effects | missing or unresolved; see family obligations; divergent-existing:82;path-missing:3 |
| `6fbbc3fa` | wip: red team fixes for verification | unresolved semantic hunk comparison; divergent-existing:4;exact-in-main:5 |
| `71959df2` | docs: fix ownership baseline finish line and stop speculative expansion | working-active / chronological supersession; divergent-existing:2;exact-working-only:1 |
| `7616cb34` | wip: line cap | unresolved semantic hunk comparison; divergent-existing:1 |
| `78c3078a` | chore: ignore local agent work artifacts | unresolved semantic hunk comparison; divergent-existing:1 |
| `798d62d0` | WIP builtin-name shadowing | unresolved semantic hunk comparison; divergent-existing:10 |
| `7a803abd` | wip: admit the compiler pair in push jobs | unresolved semantic hunk comparison; divergent-existing:3 |
| `7b5a4317` | fix: preserve returned backing at retaining consumers | working-active / chronological supersession; divergent-existing:13;exact-working-only:17 |
| `81dbd5d2` | wip: enum regression fixes for verification | unresolved semantic hunk comparison; divergent-existing:6;exact-in-main:2 |
| `8517fb8c` | WIP builtin-name reservation | unresolved semantic hunk comparison; divergent-existing:19;exact-in-main:3 |
| `8893be61` | fix(driver): drop the previous binary when a compile is refused | main-source counterpart; remaining hunk differences scoped; divergent-existing:15;exact-in-main:7 |
| `961206f3` | fix(c): accept UnwrapErr on the native pipeline | main-source counterpart; remaining hunk differences scoped; divergent-existing:6;exact-in-main:1 |
| `995bda22` | wip: admit checked numeric casts and seal consumed child edges | working-active / chronological supersession; divergent-existing:4;exact-working-only:9 |
| `9a24f9f0` | feat(lang): add the Never return type | main-source counterpart; remaining hunk differences scoped; divergent-existing:17;exact-in-main:11;path-missing:1 |
| `9dab98af` | wip: rewrap | exact main postimage; exact-in-main:1 |
| `9ecf5b56` | wip: review fixtures | unresolved semantic hunk comparison; deleted-current:1;divergent-existing:2;exact-in-main:1 |
| `a0315d23` | wip: review fixes for parallel reach | missing or unresolved; see family obligations; divergent-existing:5;exact-in-main:3;path-missing:1 |
| `a03a2b50` | fix(llvm): honour a match guard on enum variant, literal and or-pattern cases | main-source counterpart; remaining hunk differences scoped; divergent-existing:1;exact-in-main:3 |
| `a0cbb9cd` | fix: restore sealed input-only reader contracts | working-active / chronological supersession; divergent-existing:7;exact-working-only:4 |
| `a8a0c234` | wip: zone cases in the reach gate | unresolved semantic hunk comparison; divergent-existing:3;exact-in-main:3 |
| `ae7a80b8` | wip: docs for receiver and review findings | unresolved semantic hunk comparison; divergent-existing:2 |
| `b1abf6a9` | wip: fix makefile | unresolved semantic hunk comparison; divergent-existing:1 |
| `b1ad0fcd` | refactor(self-host): return JSON string reads as Option | main-source counterpart; remaining hunk differences scoped; divergent-existing:15 |
| `b2720dc6` | wip: gate fixes and CI budget | unresolved semantic hunk comparison; divergent-existing:7;exact-in-main:3 |
| `b47512a0` | feat(native): seal collection ownership facts | missing or unresolved; see family obligations; divergent-existing:81;path-missing:4 |
| `b97d9a51` | wip: preserve readonly closure and red producer experiment | working-active / chronological supersession; divergent-existing:11;exact-working-only:1 |
| `ba6090e3` | wip: model host fields beyond classes; split the reach owner | unresolved semantic hunk comparison; divergent-existing:3;exact-in-main:1 |
| `bb4ad5c0` | wip: pp-025 extern caps, contract pin for child output path | unresolved semantic hunk comparison; divergent-existing:3 |
| `bdad135e` | wip: param modes and channel ops in the reach walk | unresolved semantic hunk comparison; divergent-existing:1 |
| `be0e265e` | refactor(self-host): return JSON string reads as Option | main-source counterpart; remaining hunk differences scoped; divergent-existing:15 |
| `c04e2bc0` | wip(snapshot): preserve remaining closure work before main consolidation | missing or unresolved; see family obligations; divergent-existing:84;path-missing:40 |
| `c07e0864` | fix(self-host): report an unresolved builtin call as a user error | main-source counterpart; remaining hunk differences scoped; divergent-existing:9 |
| `c81a93fb` | wip: arithmetic red team fixes for verification | unresolved semantic hunk comparison; divergent-existing:8;exact-in-main:3 |
| `c891d6c9` | wip: fix message escapes | unresolved semantic hunk comparison; divergent-existing:1 |
| `d2023bfd` | wip: fix recursion test escapes | unresolved semantic hunk comparison; divergent-existing:1 |
| `d7f57386` | wip: preserve producer rollback and canonical budget evidence | working-active / chronological supersession; divergent-existing:5;exact-in-main:1;exact-working-only:1 |
| `de981159` | fix(self-host): parse a method inside an enum body | main-source counterpart; remaining hunk differences scoped; divergent-existing:4;exact-in-main:2 |
| `e053515a` | wip: preserve uncommitted collection-assignment-lifetime work | missing or unresolved; see family obligations; divergent-existing:2;path-missing:1 |
| `e2d579c2` | refactor(site): demote homepage to compiler fixture | missing or unresolved; see family obligations; deletion-not-current:5;divergent-existing:3;path-missing:4 |
| `e37a528e` | wip: UnwrapOr call type | exact main postimage; exact-in-main:1 |
| `ebc91a25` | fix(runtime): enforce owned collection storage | missing or unresolved; see family obligations; divergent-existing:23;path-missing:92 |
| `ed841902` | docs: freeze WIP scope and focus DRV-2 canonical recovery | working-active / chronological supersession; divergent-existing:2;exact-working-only:1 |
| `f16a570f` | wip: transpile test follows staged specialization | unresolved semantic hunk comparison; divergent-existing:1 |
| `f58434a5` | wip: staged function prototypes | unresolved semantic hunk comparison; divergent-existing:2;exact-in-main:3 |
| `f880af4a` | wip: pp-007 parent-relative output path | unresolved semantic hunk comparison; divergent-existing:3;exact-in-main:4 |
| `f964d002` | tmp final2 | unresolved semantic hunk comparison; divergent-existing:1 |
| `fac2de5d` | fix(llvm): give runtime attributes only to runtime entrypoints | main-source counterpart; remaining hunk differences scoped; divergent-existing:5;exact-in-main:4 |
| `fd7bd834` | fix(self-host): report an unresolved builtin call as a user error | main-source counterpart; remaining hunk differences scoped; divergent-existing:9 |

## Frozen consumer follow-up: nominal and direct String boundaries

Read-only source map after D8doL7; this is not a new implementation approval or a post-repair canonical census.

- Nominal entry is `type_checker_helpers_late.c:type_check_function_call` -> `type_checker_call_constructor.c:type_check_constructor_symbol_call`. The latter selects the declaration by symbol kind, applies visibility, derives positional/named field metadata, exact struct versus optional domain arity, Channel field restrictions and world/zone embedding checks. Non-generic fields use the resolved field type at the actual argument typed site, assignability, array-storage escape and constructor-field borrowed escape. Generic classes deliberately type arguments without field-type substitution; absent field metadata is skipped after expression checking. Arity/field/type diagnostics do not prevent this owner returning `sym->type`. Thus a returned Type alone is not a completed field-association certificate, and generic/skipped/default/domain routes cannot inherit a hypothetical ordinary struct success latch. No selected nominal/field result-relation seal was found in this owner. A prospective receipt must distinguish every actually checked field, original declaration/argument IDs and successful concrete types from these partial routes; it must carry dependency, not freshness.
- Direct String input bypasses are explicit: `type_checker_ownership_param_summary.c:semantic_check_param_summary_escapes` skips COPY_ONLY ref parameters before querying/writing escape summaries; `type_checker_ownership_call.c:semantic_validate_borrowed_boundary_call_argument` returns before pending ref-call recording and callee retention admission; `type_checker_ownership_boundaries.c:semantic_validate_borrowed_escape` returns without a diagnostic for COPY_ONLY. `semantic_classify_ownership_type` derives this category from boundary tracking, not allocation or static backing lifetime. These three consumers are separate from publication/raw flow masks and cannot use a zero filtered mask as a nonretention certificate.
- Channel<String> is a materially different last consumer: `pgy_runtime_channel_string_inline.h:pgy_channel_send_String` duplicates the payload, refuses allocation failure before enqueue, and queues the owned duplicate. This supports call-duration input borrowing for that exact operation, not a generic String freshness or Slot/extern retention allowance. Unknown/opaque calls and stored Slot payloads require their actual retention contracts; a blanket rejection of String sends would contradict this runtime owner.
- Paired falsifiers for the next contract decision: fully checked scalar-field constructor versus borrowed String-field constructor escaping through a holder; valid constructor versus diagnostic/generic/skipped-field/mutated declaration or field type; direct ref String helper observing locally versus retaining via opaque call/Slot payload; immortal literal versus heap-backed local String passed to the same retaining helper. Existing D8 record-field/Some positives and retaining negatives remain acceptance evidence, not justification to admit every nominal or COPY_ONLY String.

### Primitive RETURN eligibility and cached LET claims

DRettZ/RiCkFj is the observed whole canonical result: 6672 errors/9 warnings, including 538 RETURN reports. No production change is authorized by this audit. `publication_routine_return_dependency_is_scalar` currently requires the no-output header (REF/DEFAULT only), a physical primitive/Void return, and physical DEFAULT parameters. The last restriction prevents even attempting complete backing refinement for `JsonArrayObjectFactFieldCount(ref table, field:String)->Int`; it is not a runtime output-type property. A possible bounded change must still require complete current-generation all-formal backing proof: summary evaluation removes only RETURN, then ORs all backing effects, retaining pre-existing CALL/CHANNEL; `backing_store` records mutable-output edges and CALL, and incomplete proofs retain RETURN/refuse. Removing the DEFAULT-String gate alone is not proof that this specific routine's remaining children/outputs are complete. Inout exclusion likewise remains a distinct header/output obligation, not an arbitrary primitive-result exemption.

Required pairs: observed/default String key with physical Int result versus the same body storing the key into an inout/Slot/opaque helper; nested getter loan consumed locally versus retained through a helper; primitive result versus String/Option<String> forwarded result; complete versus missing/stale binding, body, call receipt or unresolved recursion. Genuine `Some(facts.module_paths[low])` result loans must keep RETURN.

P2 cache review: scalar-local list records original LET pointers/IDs, not an initializer-freshness fact. Its permission is used only when an assignment target joins that original LET and current target/RHS physical-value queries succeed. The claim ledger additionally compares every recorded node shape/ID, child count/pointer/ID, identifier binding, checked CALL relation, and physical assignment current binding/target/RHS. Thus ordinary LET substitution, initializer edge substitution, target binding, and RHS shape drift are already covered by the ledger, including the no-store cache path; merely repeating those checks is not a new safety repair. Same-pointer/same-ID internal field mutation coverage depends on the corresponding publication/operation claim: a strict review must name that exact unsnapshotted field rather than equate cached LET IDs with immutability. No specific remaining LET drift bypass was demonstrated in this read-only task.

### Type parameter-summary projection lifecycle (read-only next obligation)

`type_system_function.c` creates a zeroed mask array and routine-wide `has_param_escape_summary_facts=false`; it has no begin/invalidate operation or per-ordinal issuance bit. The setter writes one ordinal; finish sets one routine-wide flag even though no ordinal was written. Destruction frees the array in `type_system.c`. `semantic_consume_param_publication` first demands raw facts for all REF formals, then `semantic_check_param_summary_escapes` projects only REF non-COPY_ONLY formals. DEFAULT/OWN/MUT_REF, null metadata and REF COPY_ONLY are skipped, yet routine finish still marks every ordinal present. Thus an unset zero and a proved zero are indistinguishable through `type_function_has_param_escape_summary`. The observed direct String typed-zero is such a filtered projection, not evidence of no retaining effect or a copied Channel<String> UAF.

Last semantic readers are `type_checker_call_contract_helpers.c:semantic_callable_param_escape_summary`, called by ownership_call's call admission and ownership_param_summary's delayed call/routine checks. The callable API chooses Type projection after publication is finalized but raw current owner before finalization or without the flag. It therefore changes authority at finalization; the Type rows themselves retain no current program/declaration/formal/type generation seal. IR/tests may inspect Type projection as attachment metadata, which is a different responsibility from semantic permission.

Smallest coherent proposed correction is to make the callable semantic API consume only the existing current-generation function-param-flow owner, with no Type-then-raw fallback. Keep Type rows as explicitly non-authoritative projection unless all intended ordinals are issued from completed owner facts and missing/omitted state is representable. Category-specific diagnostic policy must remain separate from raw retention facts; computing/stale/missing owner evidence cannot be substituted with typed zero. This proposal does not authorize new bodyless-callee semantics or broaden String rejection. Required negatives: omitted COPY_ONLY/DEFAULT ordinal versus real completed zero; finalization cannot change raw/callable retention; stale program/header/formal binding/type/body facts refuse rather than reading old Type masks; incomplete recursive closure is not zero. Preserve actual String channel-copy success and Slot/opaque retaining failures independently.

### Executable String / Option<String> return landing inventory

Read-only after EDuQVu native PASS and original l78R6W canonical RED6661/9. Section 27 §5.4 requires one type copy/drop descriptor, deep-copy-before-drop order, String byte allocation, active Option payload copy/drop, and no shallow alias cleanup. It explicitly records missing general per-type glue; proof-level D1 is not an implemented return bridge.

Actual native chain: `type_checker_ownership_return.c:type_check_return_stmt` types the original return expression; final param-flow/raw escape records the loan. `hir_lower_cfg.c` retains that same AST in the return terminator; `mir_hir_block_projection.c` places it in `MIRInstruction.expr0` with routine ABI type/layout. There is no return copy/materialization/disposition receipt in this projection. `mir_cleanup.c:mir_add_cleanup_instruction` carries existing RIR resource cleanup edges, not generic String byte copy/drop glue. Existing collection receipt machinery proves Array<String>/MapKeys/Clone local operations, not this returned String buffer.

C AST `transpiler_func_class_flow_emit.c:emit_return_stmt` constructs expression text, optionally captures a temporary ONLY for inout writeback, then emits defers/region destruction before the textual return expression. Option Some lowering emits `Some_String(arg)`; `pgy_runtime_result_option_inline.h:PGY_OPTION_DEFINE` stores the pointer as `o.value=value`, and unwrap returns that pointer. C MIR `transpiler_mir_terminator_emit.c` uses the same optional inout capture. LLVM AST `llvm_stmt.c:llvm_emit_return_stmt` and MIR `llvm_mir_block_emit.c:MIR_INST_RETURN` run defers before evaluating the return expression; MIR then destroys its region before `LLVMBuildRet`. Neither route materializes borrowed String bytes or active Option payloads. ABI/sret owners change physical return shape, not storage ownership.

Existing facilities: `pgy_runtime_region_inline.h` owns strdup/concat region allocation; transpiler/LLVM region owners destroy the callee's region at exit, so a returned region pointer cannot simply be handed out. `direct_mir_scalar_program_owned_string_result_fact_owner.pgy` proves selected existing fresh Concat/TextBuilderFinish results via exact definition/call ABI, not arbitrary indexed/user-return copies. Self-host `value_return_emit_owner.pgy:EmitReturnValue` returns wrapped String expressions and active Option expressions without a general copy; return ABI admission is type admission only. These are useful owners to migrate, not interchangeable lifetime authority.

Smallest PRINCIPLED proposed step is one existing semantic/IR landing responsibility carrying original routine/return-expression/field-or-index IDs, checked concrete String or Option<String> shape, decision COPY versus already-owned MOVE, and transferred heap-result disposition. Both C and LLVM consume the same admitted fact, evaluate once and materialize BEFORE defers/writeback/region destruction; None does not allocate, Some copies only its active payload. Allocation failure must use the existing `PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED` fatal boundary (or a separately approved explicit result contract), never return the source pointer/empty string/None as silent fallback. Caller DEF, argument, aggregate field, chained return and discarded result need one destination-owner/last-use drop/transfer fact, with static-versus-heap distinction; a callee-only strdup patch leaves leaks and double drops unresolved. Region-only cleanup is insufficient for values escaping that region.

Required existing edit seams, pending root contract: ownership return/publication + HIR/MIR return projection and fact validators/wire; existing lifecycle/cleanup consumer; C AST/MIR return emitters and LLVM AST/MIR return emitters; self-host return/type/DAG and owned String/Option caller consumers. No new owner, caller Concat, annotation, GC or RC is proposed. Unknowns changing approval: existing RIR/AIR evidence can represent ordinary String destination ownership or needs an internal extension; exact caller retirement and Option move/unwrap semantics; externally visible returned-string ABI ownership; partial-copy failure cleanup; native JSON/self-host wire preservation. Those are executable dependencies, not a reason to relax RETURN first.

Paired execution probes: indexed String return and Some(indexed String) retained across source deep-drop; side-effectful index evaluated once; defer mutates/drops source before return; None/no allocation; returned value stored/forwarded/discarded/loop-reassigned with one cleanup; stale/missing landing and wrong original field/index refuse; allocation failure refuses without alias fallback. Run original native/HIR/MIR/transpile and C/LLVM parity/negative gates, then unchanged full canonical and installed-driver/CI. This read-only map does not mark any seam closed.

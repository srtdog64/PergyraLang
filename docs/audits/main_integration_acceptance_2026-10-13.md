# Main integration acceptance audit — 2026-10-13

Status: READ-ONLY ACCEPTANCE MAP, not external review approval or closure.
Owner: implementation lane C. Production, shared gates, contracts and build
outputs were not edited or executed for this audit.

## Initial historical identity and integration inventory

Observed HEAD is `56595078bada004e3fc5df3d88aa98e0633f4d50`. Preserved WIP
`2f6f47ad` is an evidence checkpoint, not an alternative semantic authority.
The directive's 422-path census and this audit's later 414-path census are
different snapshots; the latter contains 159 tracked modifications and 255
untracked entries. Staged state was not changed. Do not call either a frozen
acceptance manifest while other lanes are writing their assigned reports.

`git diff --diff-filter=D 2f6f47ad` lists 276 deleted paths, but 264 actually
exist on disk as untracked files. This includes new compiler/semantic owners.
A plain tracked diff therefore cannot justify dropping those implementations.
Of 11,062 WIP tree paths, 141 do not exist at their old paths; examples include
archived directives and historical build scripts/logs. Absence, relocation and
supersession must be reconciled with A's archived-work audit. This report does
not authorize restoration or classify unmatched commits as missing behavior.

The required landing manifest must enumerate tracked AND untracked source,
generator inputs and tests, identify generated outputs, and account for every
valid dirty/archived behavior before staging. Existing owner APIs, cap policy,
original negative controls and retired-path deletion remain acceptance inputs.

## Observed evidence, separated by rung

Native receipt `C:/Users/user/AppData/Local/Temp/pgy-native-final.78fVEh` has
executables under `bin/`, not at its root. Its native binary SHA-256 is
`9852d1bd73f1bd5ba9890206fe4b9e29d470240b4d616d796a40f7191891977c`.
The phase log reports 1,881 checks / zero failures; status.tsv reports zero
for native semantic/HIR/MIR/transpile, metadata, transition, query, provenance,
projection and registry gates. Query receipt `.tmp/readonly-map-query.RezrA1`
reports 18 C/LLVM positives and 47 analyze-only negatives. These are observed
bounded native/current-source facts, not installed-driver substitution.

The unchanged full canonical input still has 6,898 errors / nine warnings in
`abLmfq`; log-only `zXdZzt` reproduced its CRLF-normalized diagnostics with
generation guards. Earlier 2,336 and 13,809 counts refer to different compiler
generations and must not be substituted for this current result. There is no
current zero-refusal canonical, bootstrap fixed point or installed-driver
acceptance receipt established by this audit.

The precision repair makes the numeric retaining CAST negative type-correct
and requires every ERROR to be exactly `PGY_SEM_BORROW_ESCAPE` /
`semantic:borrow_escape`; it cannot pass on an unrelated binary type error.
It does not add a result-copy or cleanup permission.

## Owner, command and acceptance matrix

Commands are root-orchestrated recipes, not commands run in this audit.
Use a newly frozen executable/input manifest for implementation changes.

| Target / fact owner | Actual command or entry | Exact input / acceptance | Budget and remaining obligation |
| --- | --- | --- | --- |
| Native typed publication / flow / index | `.tmp/native-final-replay-2026-10-12.sh`; `make -j2` with its explicit private BUILD_DIR/BIN_DIR | Current `.c/.h/.def`, Makefile and units; semantic/HIR/MIR/transpile and phase all pass with start/end source and binary guards | Existing unit 30s and suite 60s; native green does not establish canonical admission |
| Query lifetime and backend readers | `PGY_BIN=<native> READONLY_MAP_FACT_UNIT=<receipt>/bin/readonly_map_query_fact_phase_unit.exe bash tests/readonly_map_query_smoke.sh` | Original 18/47, exact stdout under C and LLVM; unsafe inputs rc1/exact cause/no MIR; current precision ownership-only ERROR check | Focused 300s unchanged; independent result freshness is not inferred from input borrowing |
| Flow fixed point / budget owner | `PGY_BIN=<native> bash tests/function_param_flow_summary_smoke.sh` | Recursive fixture, unchanged independent4097 and single-closure4096, actual work=budget4096 refusal/no HIR, last episode telemetry, old-path/attachment negatives | Each pressure case 30s; original full gate remains required on joined generation |
| Full canonical analyzer | Native compiles `tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy` as C/LLVM source probes | Entire `src/self_hosted` import graph and original canonical input; zero errors, successful executable compilation, exact source/native/probe guards | Focused build budget unchanged; current 6898/9 is the principal blocker |
| Composed Pergyra ownership consumers | `collection_inout_effect_owner.sh`, `collection_copy_field_owner.sh`, `member_completed_read_owner.sh`, Map payload/written-root gates in `tests/self_hosted/parity/` | Reuse one canonical pair only after all four parent manifests/exact source/binary identity checks; retain direct fault modes and source negatives | Existing focused300 / integration1800; a historical pair cannot certify new native generation |
| Direct MIR disposition and final consumer | `tests/self_hosted/parity/direct_mir_owned_collection_execution_owner.sh` plus native HIR/MIR units | Real typed baseline before each mutation; drop missing/duplicate/final flip, receipt/actual definition/LocalRef faults; validation-only corrupted inputs | Existing focused300; native-stage0 JSON evidence is not installed-driver evidence |
| Structural inventory / residue | `tests/self_hosted_component_contract_smoke.sh`, `tests/semantic_core_shape_smoke.sh`, `tests/cfg_body_dataflow_smoke.sh`, `tests/mir_declaration_inventory_smoke.sh` | Current responsibility/import/cap and forbidden-path inventory; entire source membership guard | Static60; component timeout is INCOMPLETE, not behavior failure or green |
| Production codegen predecessor | `make LLVM_ENABLED=1 self-host-codegen-bootstrap-test-smoke` -> `codegen_bootstrap.sh` | Actual `src/self_hosted/codegen/main.pgy`; generation/fixed-point and runtime source identities | Existing script/CI budgets; seed-only is not full fixed point |
| Installed compiler / DRV2 | `make self-host-compiler`; `BACKENDS=hard PGY_BIN=<native> bash tests/self_hosted/parity/driver_rung2_body_parity.sh` | Pergyra codegen seed builds `src/self_hosted/compiler/driver_bootstrap_main.pgy`; real hard driver body/negative parity | Root controls installation and integration; native compilation of a source probe cannot replace this step |
| Full bootstrap and installed CLI | `make self-host-driver-bootstrap-full-test-smoke self-host-installed-driver-cli-mode-test-smoke` | Actual generated sibling driver/manifest, full fixed point and public route execution; no missing installed-driver fallback | Keep existing pressure owner and workflow budget; successful binaries alone do not prove route execution |
| External review | User-designated `Red-team review DRV-2 candidate` | Frozen exact source/native/input candidate plus focused and canonical receipts; resolve actual findings before final landing | Review is independent; this lane's audit is not review approval |
| Hosted publication CI | Push final approved main SHA; inspect `CI` and explicit `Platform full` / `Self-host parity` as needed | Exact remote SHA and required compiler jobs terminal success, bootstrap/installed route included; skipped jobs do not count | Existing workflow budgets; repository branch protection requirements were not queried/confirmed here |

## Actual hosted configuration and current run

Read `.github/workflows/{ci,platform_full,self_host_parity,formal_proofs}.yml`
and `scripts/ci_{linux,macos,windows}_steps.sh`. CI triggers on main push/PR and
manual dispatch. It classifies changed paths; Markdown-only paths skip compiler
jobs. Platform full is manual/scheduled/tag-triggered, and Self-host parity is
manual/tag-triggered. A main push alone is not evidence that either release
matrix ran. Scripts retain function-param summary smoke, with LLVM0 on macOS
and Windows; Linux self-host/codegen/driver jobs explicitly run real bootstrap.

Read-only `gh run list` and `gh run view` confirmed run **38269842263** on exact
HEAD 56595078 is completed/success. Only `classify-changes` and `build-linux`
succeeded. The following jobs were skipped: tsan, macOS C-only, Rocq, codegen
bootstrap, backend toolchain/compare, Windows, sanitizers, self-host bootstrap,
and Linux self-host contracts. This is documentation-scope green, **not actual
required compiler CI green**. Verified run:
https://github.com/srtdog64/PergyraLang/actions/runs/38269842263.

## Remaining falsifiers and joined acceptance decision

Existing native controls cover checked CAST and cached consumed-edge identity;
the unchanged canonical remains the strongest real-input falsifier. Some/
Unwrap scalar count/index and genuine String/record retained loans require the
current B producer/consumer decision, not a new source-name grant. Private
Option probes in root receipt `option-origin-current.2KlaoO` showed scalar
rc0/MIR and String/record negatives with only borrow errors. They have not been
added as shared protocol fixtures or claimed to close the full canonical.

Preserve original Keep/Slot/inout setters, returned alias/recursive output,
68-formal stale bitset, unknown/missing/stale identity and release-witness
controls. Channel<String> has an actual copying runtime and is not evidence
for a shallow-pointer queue attack. Constructors/Option/Unwrap are not fresh
backing certificates; literal controls are not proof for heap-backed loans.

Smallest next acceptance order: root fixes one measured incomplete-fact
contract -> three disjoint existing-owner/test slices -> joined manifest and
native original matrix -> same full canonical zero errors and C/LLVM compile
-> composed/direct release and negative gates -> real codegen/driver fixed
point/installed routes -> designated external review -> valid dirty/archive
integration into main -> remote SHA and actual compiler hosted CI. Do not
enlarge the baseline after this user-defined end line is met; only a real
reproduced user case or explicit request reopens refinement.

## Additive private inout and Some/Unwrap observations

These are analysis-only native observations, not shared gate execution. Native
is the same `78fVEh/bin/pgy.exe` identified above. Reproducible private recipe:
`PATH=C:/msys64/ucrt64/bin:<existing> python .tmp/C-inout-option-baseline-20261013.py`.
Each command is `pgy.exe --native-pipeline --mir-json --error-format=json
<absolute private source>` with timeout 30s, captured output and native/input
SHA-256 start/end checks. No malformed MIR was emitted to a backend and no
Pergyra program was executed. All observed end guards passed.

| Exact private source / originating property | rc / MIR | All ERROR codes and causes | Interpretation |
| --- | --- | --- | --- |
| `C-inout-direct_mir_readonly_ref_to_inout_negative.pgy`, original 5f14ef00 | 0 / yes | none | Readonly ref Int forwarded to inout is admitted; missing historical negative is executable, not another owner's refusal |
| `C-inout-direct_mir_ref_inout_same_binding_negative.pgy`, original 5f14ef00 | 0 / yes | none | Same binding occupies ref/inout; overlap is admitted |
| `C-inout-reverse-same-binding.pgy` | 0 / yes | none | Inout/ref reversed order also admitted |
| `C-inout-direct_mir_ref_ref_same_binding_control.pgy`, original 5f14ef00 | 0 / yes | none | Ref/ref readonly alias remains the legitimate positive control |
| `C-inout-parser_expression_graph_rows_self_alias_negative.pgy` | 1 / no | `PGY_SEM_TYPE_MISMATCH` / `semantic:type:boundary_arg_mismatch` | Old scalar-rows actual no longer matches the current Array-of-rows formal; this is not an ownership refusal |
| `C-map-some-local-positive.pgy` | 1 / no | two `PGY_SEM_BORROW_ESCAPE` / `semantic:borrow_escape` | Ref String map payload -> Some -> Unwrap -> local StringLength is currently rejected |
| `C-map-some-keep-negative.pgy` | 1 / no | two same borrow errors | Same payload reaches opaque Keep; original unsafe boundary remains refused |
| `C-field-some-local-positive.pgy` | 1 / no | two same borrow errors | Ref record String field -> Some -> Unwrap -> local scalar read is rejected |
| `C-field-some-slot-negative.pgy` | 1 / no | two same borrow errors | Actual Main/Publish owned local record -> Option field loan -> caller Slot store remains refused |

The parser fixture's original relative import was first invalid at the scratch
location. The private import was normalized to `../src/self_hosted/parser/
expression_graph_owner.pgy` and rerun before recording the type mismatch above.
Do not count its first file-not-found result as safety evidence. Current
`ParserExpressionGraphsAppendRowsInto` accepts a different aggregate shape;
do not restore the obsolete fixture unchanged merely to obtain rc1.

Historical `5f14ef00` had exact readonly-ref -> inout and mixed ref/inout checks
in `type_checker_helpers_late.c`. Current native lines 199-210 collect only
MUT_REF names, while current `ast_inout_argument_alias_verdict_owner.pgy`
collects only mode-1 inout bindings. The current docs/mut_borrow_parameters.md
explicitly document named variables and inout/inout exclusion, but do not by
themselves settle all mixed-mode authority. Existing 5f14 intent and actual
current acceptances require root's ownership decision and a joined native /
self-host consumer migration; neither name matching nor the audit grants it.

Some/Unwrap scalar alone was already a passing control and is not a substitute
for these real String payload cases. Some construction/Unwrap do not produce
fresh backing. The positive/negative pairs currently share a conservative
borrow refusal, so this native result does not establish a complete origin or
retention distinction. Keep actual loan negatives while fixing missing facts.

Inventory correction: the original 414 number used short status's directory
collapsing. The directive's 422 uses `git status --porcelain=v1
--untracked-files=all`. Repeating that exact boundary now yields 426 paths,
after the assigned audit files were added. Do not compare 414 and 422 as a
source deletion count or retroactive generation guard.

No source, test, workflow, contract, branch or index mutation was made for this
audit. No build, gate runtime, unsafe emission/execution, commit or push was
performed. Only this named audit file was created.

## Current RETURN acceptance census — DRettZ / RiCkFj

Read-only checkout observation for this follow-up: HEAD
`e2240cb2abf741f30629d72c811a460894a28f91`, 443 paths using the same
`git status --porcelain=v1 --untracked-files=all` boundary. This is a dirty
checkout census, not the root's frozen build manifest or a landing verdict.

Root's identified current native replay and work-pressure receipts
`t9teV9` / `r_g_2igj` pass their bounded gates. External scoped review passes;
String backing and P2 remain OPEN. The unchanged canonical input still fails:
`collection-copy-field.RiCkFj/c-source.compile` has **6672 errors / 9 warnings**,
partitioned by root into 1853 direct, 4281 callsite and 538 RETURN diagnostics.
No zero-refusal, bootstrap, installed-driver or same-SHA required-CI closure
follows from these native results. The focused matrix remains 20 C/LLVM
positives / 51 analysis-only negatives; current joined execution, not source
registration, owns its verdict.

One bounded current-source signature inventory maps the 538 RETURN headers
to 362 distinct routine names. The diagnostic-weighted declared-result map
is below. These are **navigation classes, not checked type or backing proof**.
An Int/Bool/Void spelling is not permission to erase child retaining effects;
an Option spelling alone does not prove a String loan or independent storage.
The primitive bucket also contains inout/Void mutators such as
`SemanticAstCapabilityObserveCallRange` and `SemanticFutureJoin`; it is not a
count of eligible scalar refinements. Option<Int> and Option<String> must be
separated by the actual typed payload before any admission decision.

| Declared result | RETURN diagnostics | Required next evidence |
| --- | ---: | --- |
| Primitive / Void | 182 | Exact typed result and all child output/retention effects; distinguish false scalar refinement from actual output mutation |
| String | 35 | Original backing source and last caller lifetime, or an existing actual copy/cleanup landing |
| Option | 67 | Actual payload type/source and result-carrier dependence, not constructor freshness |
| Array/HashMap/Slice/List/Set/Queue | 14 | Existing D1 collection transfer/copy and lifetime authority |
| Nominal / other aggregate | 240 | Exact carried fields and origin/lifetime; no blanket constructor grant |

The single name absent from `src/self_hosted` is resolved in the actual
imported input: `tests/self_hosted/fixtures/generic_default_selection_unit.pgy:38`
declares `InspectGenericDefaultSelection(ref analysis) -> Bool`. Its one
diagnostic is included in the primitive total, not discarded. Repeated
diagnostics from one routine remain weighted here; routine count is not
substitution progress.

Concrete navigation anchors:

- `src/self_hosted/lib/json_fact_table.pgy:465`
  `JsonArrayObjectFactFieldCount(ref table, field: String) -> Int` returns its
  scalar counter after local `JsonArrayObjectFactAt` and field-reader calls.
  The related count functions at 479 and 508 also return Int. Their default
  String input and local aggregate-reader path require actual summary evidence,
  not a universal String exemption.
- `src/self_hosted/hir/ast_source_module_fact_owner.pgy:96`
  `AstSourceModulePathForNode(...) -> Option<String>` carries indexed module
  path backing through Some; its result is not independent merely because it
  is wrapped.
- `src/self_hosted/semantic/string_window_extent_proof_owner.pgy:79` and
  `string_window_extent_owner.pgy:61` return indexed signature/name Strings.
  No copy or cleanup certificate is established by this signature inventory.
- `src/self_hosted/hir/ast_destructure_graph_owner.pgy:78` returns Option<Int>;
  this physical payload differs from Option<String>, but still needs the actual
  typed relation and completed origin/effect evidence.

Private, unexecuted analysis-only candidates (no shared fixture changes):
`C-return-primitive-default-string-positive.pgy` versus its `-retainer-negative`
pair combines ref String getter -> Some/Unwrap local StringContains -> Int
with the same heap-backed Main record and an added opaque Keep. The separate
`C-return-option-field-loan-negative.pgy` versus
`C-return-option-literal-control.pgy` distinguishes returning caller-owned
record field backing from immortal literal backing. They assert no automatic
copy, fresh constructor, view permission, or successful parsing/execution yet.
All reside under `.tmp`; root must identify the executable and observe exact
diagnostics/no MIR before using them as evidence. No corrupt/unsafe source was
emitted or executed during this census.

Reproduction inventory script:
`.tmp/C-return-signature-census-20261013.py`, using the exact RiCkFj log and one
`src/self_hosted` source pass. Explicit Windows Python completed the inventory;
the implicit MSYS Python attempt did not produce a terminal verdict when this
note was written and is not a second acceptance result. No production, shared
gate, registry, timeout, input or work-cap edit was made by this follow-up.

## Next direct-ref-String acceptance boundary (read-only)

Root observed corrected phase **2295/0** and focused query **20/51** in
`SZa6OJ`, with guards. Full native `R0dhWA` is still running at this update;
its completion is not inferred from the focused result. Canonical zero,
bootstrap, installed-driver and required exact-SHA CI are still required.

The minimum existing paired cases distinguish these authorities:

| Boundary | Existing input / evidence | What is actually covered |
| --- | --- | --- |
| Static literal -> ref String -> caller Slot | `readonly_map_query_slot_literal_store_valid.pgy` | StoreText literal plus physical Int control; immortal literal is not a certificate for arbitrary String input |
| Heap record getter -> local scalar read | `readonly_map_query_local_string_getter_valid.pgy`, `completed_observe_valid.pgy` | Borrow remains local; no implicit result copy or field-drop certificate |
| Owned String-map value -> local read / opaque Keep | `readonly_map_query_some_map_local_valid.pgy` / `some_map_keep_invalid.pgy` | Actual MapSet-owned backing and dependent MapGet/Some/Unwrap relation; Keep is potentially retaining, not an observed post-free read |
| Getter -> opaque Keep direct / alias / forwarding helper | Existing `return_provenance_nested_keep`, `alias_keep`, `alias_helper` negatives | Full Main/Publish/ref-record chain and genuine retaining call; unknown extern behavior is never executed |
| Getter -> caller Slot direct / alias / ref String helper | Existing `return_provenance_nested_slot`, `alias_slot`, `ref_slot_helper` negatives | Main reads retained Slot after Publish returns; current String field cleanup at return is unproven, so this is not an observed use-after-free claim |
| Ref String -> local String Channel | Phase physical-return diagnostic `c1cqhL`, private `C-physical-return-channel-local-string.pgy` | Final callable mask0 versus raw CHANNEL32. Runtime send owns a duplicate; this case must not be mislabeled unsafe merely because raw CHANNEL is present |

Designated historical review
`C:/Users/user/AppData/Local/Temp/pgy-scalar-return-redteam.2fa1c2a3/review.md`
and `slot_escape_reached.pgy` explicitly did not execute an unsafe program or
assume that Publish frees its String field. Their five actual source chains
are preserved in the focused negatives above. The scalar-return repair now
preserves those boundaries, but that scoped result does not settle the separate
direct-ref-String final-cache policy.

Current runtime anchors, not guessed source-type permissions:

- `src/runtime/pgy_runtime_map_string_inline.h:234-247` duplicates String
  values on MapSet; replacement at 239 frees the old owned value. MapGet at
  315/331 returns that stored pointer, not a new allocation. MapRemove at
  364/384 and MapDrop at 178/199 free map-owned values.
- `src/runtime/pgy_runtime_lib_slot_exports.h:109-120` stores `s->value = v`;
  Slot is not an automatic copy boundary for String payloads.
- `src/runtime/pgy_runtime_channel_string_inline.h:54-56,206-217` duplicates
  payload before enqueue. Its LLVM library twin bridges to its runtime strdup
  in `pgy_runtime_lib_channel_string_exports.h:23-32`. This supports the input
  copy distinction, not universal result cleanup or a new StringDrop contract.

The genuinely missing strongest source falsifier is: owning String MapSet ->
MapGet loan -> original ref String getter/retaining helper -> caller Slot;
replace the same map key (an actual runtime old-value free), then reach a Slot
read of the previously retained pointer. Pair it with a literal-only store and
with Channel's owned-copy read after the same source replacement. This would
distinguish a concrete reachable post-free use from the existing return-scope
cleanup model. It must be type-checked/analyzed only until refusal and existing
owner evidence are established; no unsafe executable, source annotation,
manual Clone, invented StringDrop or constructor freshness may be used. These
new cases have **not** been created, parsed, analyzed, compiled or run during
the current freeze. Root must fix the joined contract and authorize their
existing-gate registration first.

## Diagnostic-only incomplete census (Mx7gfU)

Root executed the separate forced-refusal observer on the unchanged canonical
input; all source, input, private-object and binary guards passed. Its 13,841
diagnostic errors are NOT the production baseline of 6,661 errors / 9 warnings.
No observer result was admitted or emitted. The log contains 1,765 incomplete
events, 603 diagnostic continuations and two work-censored episodes; it is not
an exhaustive uncensored whole-program proof.

Events by existing issuer reason: 1,641 unfinished callee dependencies; 91
opaque backing origins; 12 nonidentifier/value-call consumers; five each
missing binding, unhandled origin shape and missing storage-place identity;
three nonlocal stores; two work-limit events; one unhandled consumer shape.
Propagation events are not independent defects or safe-to-clear effects.
The 91 opaque-origin events include 22 StringTrim, 11 Some, nine nonidentifier
targets, two TextBuilderFinish, one StringSplit and 46 named nominal targets.
These are source navigation classes, not canonical-target or freshness proof.
The two censored routines (107048 and 137660) visited 9/34 and 10/33 roots;
25 and 23 roots respectively remain unobserved because the original 4096 work
limit still applies. Nominal result dependence is the approved prerequisite;
String allocation/cleanup, method/value calls and missing place identity stay
separate OPEN obligations, not incidental permission from this census.

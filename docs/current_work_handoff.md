# Current Work Handoff

Updated: 2026-10-06 KST (Asia/Seoul). Navigation only. Current source, the
SoT registry, admitted contracts, and executable gates override this note.

## Active self-host context

### Independent native lexer local checkpoint (2026-10-06)

Base main: 430f0b9103c971ca9bba6dda84140ae73ce81768. Only generated gmon.out
was dirty before this slice; it is preserved and excluded. This note ships in
the local lexer commit; inspect git log for the resulting SHA. No push or
Actions run is authorized by this checkpoint.

Native skip_whitespace now refuses unterminated block comments instead of
silently returning EOF, matching the existing self-host ScanTokens refusal.
Error tokens use the existing token-text/stream owner and receive initialized
stream identity and increasing ordinals. The existing lex diagnostic code,
reason and fix routing is unchanged. Error location remains the reached EOF.
Collection/inout owners and the active P0 contract below are unchanged.

Final source checks: GCC and Clang -std=c11 -O2 -Wall -Wextra -Werror builds
pass six unclosed-comment refusals, six valid controls and a literal-error
stream/ordinal control. The same test fails on the base lexer (EOF, not error).
parser_lexer_diagnostic_smoke.sh now reaches the executable regression and
passes locally; git diff --check passes. No full bootstrap, installed-driver,
Linux sanitizer, backend matrix or remote CI claim. This is a correctness
change; no speedup measurement or C-path substitution is claimed.


### P0: caller cleanup after synchronous inout

- Priority: finish caller ArrayDrop after synchronous inout before general SoT.
  Alrescha is a separate read-only consumer; no compiler coupling or whitelist.
- Checkout observed before this checkpoint commit: main @
  59a214ff26f9db0beaea0b4a46cfb012f3e2693f. Current dirty candidate replaces
  Bool-labelled type-row retirement with scope-owned String-array epochs and
  consumer-time serialization of admitted binding facts. All earlier user/worker changes are already
  published. This card travels with the implementation checkpoint; after its
  commit/push, expect no tracked dirty files. Verify actual HEAD/remote/status.
  Generated gmon.out remains preserved/excluded.
  Root owns P0 integration
  and Git; Main is stopped following the user's
  explicit choice. The user also authorized committing all current user/other-
  worker changes, including the new examples and deleted deployment guide.
  All 184 changed files were committed/pushed as checkpoint 4415adf4; the
  subsequent external documentation commit was fast-forwarded locally and
  runtime ABI scalar/registry correction 5deb67b4 and routine lookup migration
  8b1f17ec and instruction use publication f49be4dd were committed/pushed.
  Named-use compatibility 057c93c1 was merged with the external guild-economy
  example commit d5f90c54 as cf0fe236 and pushed; example behavior was not
  independently verified here. No local source edits were lost in that merge.
  LocalRef publication a59c01a6 and owned unique-use accumulation ad4f133a
  were committed/pushed as well.
  Shared readonly aggregate-call context df0df7a4 (26 files) is published.
  Slice length 004f4f24 was merged with the external async/type-alias/subject-
  address/intent changes as 7e054c1f and pushed without losing either side.
  Those external changes are retained, not independently certified by this note.
  Preserve/exclude generated gmon.out. This is not a green/installed claim.
- Objective/owner boundaries:
  docs/agent_work_directives/inout_array_release_2026-10-05.md.
  Native parameter-flow descriptor preservation, source call-preservation and
  untrusted direct-MIR lifetime owners independently supply cleanup evidence.
  Existing definition/order, formal-effect and member-read owners keep their
  facts. No mode-only grant, guessed callee, alias promotion or C fallback.
- Current reached seam: the conditional String-field cleanup and
  owns_local_rows Bool are deleted. CodegenTypeEnvState owns an Array<String>
  scope epoch, keeps its current local_rows as a view, and retires the array
  once after its last consumer. Child adoption copies into the parent epoch;
  the statement consumer then retires the child explicitly. Binding facts hold
  typed identity only; their formerly eager env_rows field is deleted and every
  former consumer is migrated. Global/preseal/local lookup order is unchanged.
  The sealed physical StringJoin call now supplies its actual runtime fresh
  heap-or-null result fact to the existing domain/exclusivity owners. String
  type, a Bool, a user return or a foreign target still supplies no authority.
  Current scope/join gate4 passed C/LLVM exact values, primitive-identity
  mutations and thirteen lifetime refusals/backend within 300s:
  .tmp/inout-array-release-type-env-state-gate4.log,
  .tmp/self_hosted/type-env-state-retirement.QYBasF. Existing nine observer
  warnings; source/input/native hashes were rechecked. The state owner is 115
  lines under its 140 cap; domain owner is 200 under its unchanged 200 cap.
  The preseal smoke target now reaches this executable gate. Structural checks
  forbid the retired Bool, scalar-field cleanup and eager binding rows.
  Native full-source C build passed with zero errors / sixteen warnings after
  six temporary-ref call sites were corrected by giving the resource-free
  four-String binding value an ordinary by-value serialization interface:
  .tmp/inout-array-release-type-env-native-gen0-check.log.
  Fresh original-input v55 terminated after unchanged MIR controls passed:
  gen1 source admission refused aggregate_release_plan_unproved at node 21859.
  Its reported GenericParameterFactRowsDrop extraction may be the first-
  requirement fallback; this is not sufficient evidence identifying the
  conflicting demand. The
  existing uniqueness/order policy now lives in one responsibility-named owner
  and preserves the failed call's syntax. Plan construction is 113/160 lines;
  the new uniqueness owner is 118/140. Old definitions are deleted. This changes
  diagnostics only, not alias, generation or repeated-release permission.
  .tmp/inout-array-release-codegen-bootstrap-v55.log,
  .tmp/inout-array-release-bootstrap-v55-boundary.log,
  .tmp/inout-array-release-codegen-bootstrap-v55-source.sha256 were checked at
  terminal failure before the diagnostic change. The subsequent unchanged
  whole-codegen diagnostic observer exhausted 300s without output; its wrapper
  reported exit 1, not a semantic verdict. No larger allowance was introduced.
  Current aggregate diagnostic/parity gate8 passed six positives, sixteen
  refusals and generation/coverage mutation guards per C/LLVM. Repeated release
  and duplicate storage still fail at the same boundary, now naming Main's
  physical release call rather than the imported field extraction. Input,
  import and native hashes were verified by the gate:
  .tmp/inout-array-release-aggregate-release-gate8.log,
  .tmp/self_hosted/aggregate-release-source.1nSzkj. The original
  gen1/fixed-point/default-driver/GUI contract remains OPEN. No v56 was started.
  Next falsifier: the unchanged original compiler input's actual failed demand,
  then the fresh default C/LLVM synchronous inout/ArrayDrop consumer contract.
  v54 stopped at the six native unnamed-ref errors, not semantic parity.
  Component checker unit tests and changed shell syntax checks passed. Full
  Windows structural3 ended BASH_STATUS=124 under its unchanged 60s budget;
  earlier silent exit-1 observations were not proof of a missing enum import.
  The actual enum text_owner import is present. No full inventory PASS is claimed.
  Exact 59a214ff CI 37377911373 failed gen1 node 58906 / owned_string_drop;
  Windows/macOS/TSan/Rocq passed and downstream Linux jobs skipped. The earlier
  hosted runner acquisition issue did not recur. This candidate is not CI green.
  Named allocated-String own arguments remain admitted only at their exact
  physical consuming node, with all other local uses before it. An own String
  is by-value consumption, not a caller binding-slot write; inout/unknown domain
  exposures remain rejected.
  Named gate5 passed current C/LLVM values, thirteen semantic refusals/backend
  and one separate unsupported-async parser refusal/backend:
  .tmp/inout-array-release-named-string-own-entry-gate5.log,
  .tmp/self_hosted/named-string-own-entry.l9VhLg. Nine existing observer warnings.
  The existing collection-effect integration matrix now includes the seven
  parsed named-entry rows; the full matrix was not rerun in this checkpoint.
  Retention-registry smoke passed after its stale direct-reader assertions
  migrated to the actual shared exclusivity/read-context owners; no registry
  row changed. Expression-root regression gate6 also passed:
  .tmp/inout-array-release-expression-root-consumption-gate6.log,
  .tmp/self_hosted/expression-root-consumption.1zhB3w.
  Prior 59a214ff observers refused the old imported state probe at syntax 1622
  / owned_string_drop (superseded by the scope/join gate3 above):
  .tmp/inout-array-release-type-env-state-frontier-current-c.log,
  .tmp/inout-array-release-type-env-state-frontier-current-llvm.log.
  The ignored array-epoch prototype has source/C/LLVM-native probe evidence;
  it is not an original-input memory-cost or installed-driver receipt.
  Do not remove cleanup, grant Bool-based ownership, or copy and leak inputs.
- Prior expression-root seam: RewriteSemanticMemberAccess ->
  CodegenCExpressionTextCommitRoot. Signature-owned body availability no longer
  excludes owning formals, including its readiness consumer. Canonical builtin
  consumption identity is shared by formal use, negative storage effects and
  ordered transfer. The caller's existing own admission remains the storage/
  element authority; its exact current entry is not prior retirement.
  CommitRoot validates, copies either result, then retires once. No Bool-based
  alias proof, conditional-move grant, name exception or native bypass.
  Gate5 passed C/LLVM production/copy values, two fatal epoch modes, four source
  positives with formal_ready=true and fourteen lifetime refusals/backend:
  .tmp/inout-array-release-expression-root-consumption-gate5.log,
  .tmp/self_hosted/expression-root-consumption.588HWL.
  The changed effect owner is 210 lines under its unchanged 210-line cap;
  comments are excluded by the repository metric, blanks still count.
  Existing 123 own/indexed source verdicts match baseline/current in both
  backends. These are not full driver/fixed-point or replacement receipts.
  Original-input v53 passed four C controls/eight cycle refusals, then gen1
  source admission refused node 58906 / owned_string_drop. It names
  CodegenTypeEnvStateReplaceOwnedLocal -> ArrayDropOwnedStrings(retired) inside
  if retire_old. No gen1/gen2/fixed point/default pair/GUI receipt exists.
  Source/input/native hashes matched at terminal failure before the two
  whitespace-only cap wraps; gate5 observes the current bytes.
  .tmp/inout-array-release-codegen-bootstrap-v53.log,
  .tmp/inout-array-release-bootstrap-v53-boundary.log,
  .tmp/inout-array-release-codegen-bootstrap-v53-source.sha256.
  Next owner: existing type-env state/owned String actual exclusivity, last
  consumer CodegenTypeEnvStateReplaceOwnedLocal. Reproduce its smallest
  imported source falsifier; neither retire_old nor a String/heap domain is
  permission. Then fresh unchanged original-input bootstrap/default pair.
  Do not revive 57194, query architecture, optimization or unrelated SoT.
  Full structural inventory ended exit 1 without its final PASS; its exact
  failure remains unidentified, and no full inventory success is claimed.
  Exact b477912d CI 37373270125 failed hosted runner acquisition before tests;
  all dependent jobs skipped. That is not compiler parity or green CI.
- Current focused evidence: aggregate gate6 passed C/LLVM 6 positive/16
  falsifying source inputs. Gate7 added generation/coverage mutation guards;
  its combined 300s run timed out after all C rows and LLVM rows 0..18.
  With native/source/input hashes unchanged, LLVM rows 19..21 and generation
  guards separately passed; all 22 C/LLVM outputs and guard outputs match.
  This is resumed row evidence, not a combined gate7 time-budget pass.
  .tmp/inout-array-release-aggregate-release-gate6.log,
  .tmp/self_hosted/aggregate-release-source.iVOx1C.
  Member gate8 passed current C/LLVM 5 positives/11 compile-only refusals:
  .tmp/inout-array-release-member-read-gate8.log,
  .tmp/inout-array-release-shared-context-member.sha256.
- Original whole-MIR-root source admission now passed in the fresh LLVM
  observer: body_ok=true, empty diagnostic, within the unchanged 300s budget.
  Native C/LLVM executions independently passed the exact six-line receiver
  identity/invalid-graph oracle, existing 15 warnings. Hashes were checked
  before/after. This removes the observed v43 aggregate refusal; it is not
  an official seed, fixed point, default installation or substitution claim.
  .tmp/inout-array-release-shared-context-whole-root-llvm.log,
  .tmp/inout-array-release-shared-context-whole-root.sha256,
  .tmp/inout-array-release-shared-context-mir-root-native.sha256.
- Official seed v44 built current gen0 and passed two call-argument executions
  plus twelve pre-emission refusals. It then passed the old ownership boundary
  and failed C emission of the original MIR control at unsupported collection
  runtime kind: Slice<String>. Source hashes matched through terminal failure.
  .tmp/inout-array-release-codegen-bootstrap-v44.log. Linux CI 37329776930
  independently failed that same kind; all four Windows/macOS/TSAN/Rocq jobs
  passed and dependent Linux jobs were skipped. Not a green/install receipt.
- Current reached fix: ArrayLength's Slice operand now consumes SliceRuntimeFact,
  not the array runtime owner. The private read-length symbol/block and existing
  direct-MIR constructor carry the same fact; missing length symbols fail closed.
  A descriptor snapshot evaluates the admitted operand once and retires nothing.
  Foreach/iteration support is unchanged; an early trial was withdrawn.
  Focused gate2 passed within 300s: native C/LLVM and freshly built self-host C
  each execute eight exact Int/String, default/ref and empty length rows; both
  native fact probes pass unsupported-family/missing-field guards and three
  fatal missing/unsupported-symbol modes. The self-host negative still refuses
  a compound operand whose codegen type fact is absent; no type-text guess.
  .tmp/inout-array-release-slice-length-gate2.log,
  .tmp/self_hosted/slice-length-codegen.bwqMT6.
  This pre-merge receipt was refreshed as gate3 on native v4 and passed.
- Official seed v45 on merged 7e054c1f passed source admission and emitted the
  original MIR control, then failed C compilation. Generated PgySlice_Int/String
  typedefs and public pgy_slice_get/copy/array_slice names collided with the
  required pgy_runtime.h definitions, whose Array descriptors are not private
  three-field arrays. All 6963 source/input/native hashes matched at failure.
  .tmp/inout-array-release-codegen-bootstrap-v45.log,
  .tmp/self_hosted/codegen_nominal_array_declaration/run.unnwy4/root-codegen_cc.log.
  Exact-head CI 37337689083 failed that same C compile stage; Windows/macOS,
  TSAN and Rocq passed, dependent Linux jobs were skipped. Not CI green.
- Current namespace fix: source SliceRuntimeFact owns pgy_self_slice_Int/String
  and private get/copy/array-slice symbols; source AbiLayoutCValueType consumes
  that fact. Compiler ABI rows and the direct-MIR public projection are unchanged.
  Gate4 passed within 300s on native v4: existing length/fact guards plus native
  C/LLVM and fresh self-host C HashMap-header coexistence, index/copy and exact
  values; the gate forbids reopened native Slice typedef/function definitions.
  .tmp/inout-array-release-slice-length-gate4.log,
  .tmp/self_hosted/slice-length-codegen.WPgus8.
  This fix is committed/pushed as 32d2b1a9. Default install remains OPEN.
- Official seed v46 passed the unchanged original MIR root: native/self-host C
  compile/exact execution, four controls and eight cycle refusals. Gen0 compiling
  its own source then refused borrow_boundary_escape, syntax 49433, ArrayPush.
  The production parser mapped that handle to GenericInstanceClosureFromRecipes
  in generic_instance_closure_owner.pgy: borrowed recipe/substitution text was
  retained in actuals. Source/input/native hashes matched after failure. Exact-
  head CI 37340961801 independently passed the root and refused that same node;
  four other jobs passed, dependent Linux jobs skipped. Not a seed/fixed point.
  .tmp/inout-array-release-codegen-bootstrap-v46.log,
  .tmp/inout-array-release-bootstrap-v46-boundary.log,
  .tmp/inout-array-release-ci-37340961801-codegen.log.
- Current generic owner fix: actual tuple projection, append and recipe
  publication copy retained text with ArrayPushOwnedString. Append copies the
  existing member text into a fresh owned buffer before adding new elements;
  native's unknown-uniform-ownership guard remains intact. Numeric identity,
  deduplication, expansion and missing-fact rules are unchanged.
  Generic gate4 passed within 300s, native C/LLVM tuple/finite/epoch values and
  caller cleanup independence, source owner admission, raw-retention entry
  refusal and unproved returned-array deep-drop refusal. That last value fixture
  executes natively but remains refused by source at ArrayDropOwnedStrings(projected);
  no general returned-String-array cleanup grant is inferred.
  .tmp/inout-array-release-generic-owned-actuals-gate4.log,
  .tmp/self_hosted/generic-owned-actuals.uRErHr.
  This fix is committed/pushed as bebfc164. Default install remains OPEN.
- Official seed v47 passed the original MIR root's four controls and eight
  cycle refusals, then gen1 source admission refused syntax 54263 /
  unproved_formal_indexed_read_entry. The production parser mapped it to
  CodegenValueWrapperUsageCollectType calling Contains after recursive inout
  growth. Source/input/native hashes matched through the terminal failure.
  .tmp/inout-array-release-codegen-bootstrap-v47.log,
  .tmp/inout-array-release-bootstrap-v47-boundary.log.
  Exact-head CI 37345034540 independently failed at the same node/boundary;
  Windows/macOS/TSAN/Rocq passed, dependent Linux jobs were skipped.
- Reached formal-effect observation: the recursive formal is mode 1 / effect 6
  (proved shallow mutation), with no unknown/escape site. Retirement and non-
  deep site 40176 in the imported owner fixture name the recursive call before
  membership reading; Contains is mode 0 / effect 3. The argument-effect owner
  already distinguishes sibling retirement from current descriptor retirement,
  but the formal call-effect carrier previously discarded that distinction.
  .tmp/inout-array-release-formal-descriptor-frontier-v1-observe.log,
  .tmp/inout-array-release-formal-descriptor-frontier-v1-site.log.
- Current correction carries that bit through the existing physical call pass
  into formal_descriptor_retiring_sites. One current-descriptor permission owner
  supplies synchronous read and shallow-forwarding entry; unknown/escape,
  consumption, deferred/reordered uses and sibling-view refusals stay distinct.
  No owned-element, deep-drop or recursive caller-release permission is inferred.
  The Slice membership/common indexed-type trials were completely withdrawn;
  the original wrapper implementation and original bootstrap/MIR input remain.
  A former "unknown" fixture contained only a proved synchronous shallow call;
  that success case is now separate from a genuinely unproved deferred mutator.
  This correction is committed/pushed as d2fe7ab6. The next observed integration
  boundary is the branch-local String retirement below, not the withdrawn Slice
  trial, general returned-array cleanup, or an unrelated SoT/performance track.
- Current focused gate4 passed within 300s on native v4: C/LLVM production
  wrapper and recursive-current-read value oracles, three source positives and
  twelve preserved refusals per backend. The fresh LLVM owner observer then
  admitted the actual import-composed wrapper input (body_ok=true), showing
  the recursive formal's sibling retirement at 40176 and no current descriptor
  retirement. Input/import/native/executable hashes matched before/after.
  .tmp/inout-array-release-current-formal-descriptor-gate4.log,
  .tmp/self_hosted/value-wrapper-view.8K3BIz,
  .tmp/inout-array-release-formal-descriptor-frontier-v2-observe.log.
  Recursive caller cleanup was independently refused by native during an
  initial fixture trial; that extra grant is not part of this read proof.
  The structural inventory also found an obsolete DIR intent-step read
  requirement contradicting its explicit borrowed action-contract input. Its
  existing owner-boundary ratchet is corrected without restoring the old read;
  this source-only inventory is not a behavioral or green-CI receipt.
  Its corrected whole inventory rerun reached the unchanged 60s limit without
  a terminal verdict: .tmp/inout-array-release-current-formal-descriptor-inventory2.log.
  Keep that omission explicit; no cap or time allowance was raised.
- Official v48 used fresh fixpoint-only mode with the same 1800s integration
  budget and original compiler/MIR input. It passed four original C controls
  and eight cycle refusals, then gen1 source admission stopped at syntax 55727 /
  owned_string_drop. The production parser maps this handle to
  CodegenExpressionMemberTypeFromGraph in expr_semantic_type_owner.pgy:
  ArrayDropOwnedStrings(retired_variant_key), where [variant_key] is created
  under IsSome(receiver_type). All source/input/native hashes matched at failure.
  No gen1/gen2 executable, fixed point, default installation or GUI receipt exists.
  .tmp/inout-array-release-codegen-bootstrap-v48.log,
  .tmp/inout-array-release-bootstrap-v48-boundary.log,
  .tmp/inout-array-release-codegen-bootstrap-v48-source.sha256.
- Exact-code-head CI 37355012849 independently passed the original C controls
  and refused the same 55727 / owned_string_drop. Windows/macOS/TSAN/Rocq
  passed, dependent Linux jobs skipped. This is still RED, not whole CI green.
  .tmp/inout-array-release-ci-37355012849-codegen.log.
- Reached query lifetime correction: LookupQualifiedKindTypeMatches in the
  existing type-environment owner constructs and retires its temporary key
  before returning a scalar comparison. The member consumer constructs an
  escaping key only after success. Original row identity/lookup precedence,
  result spelling and owned-key construction are unchanged. Domain/transfer
  proofs have not been relaxed, and no original key is copied then leaked.
- Focused gate2 passed within 300s on native v4: C/LLVM actual member selection,
  missing/mismatched queries, global/preseal/local precedence, source admission
  of the actual query owner's retirement and eight existing refusal fixtures
  per backend. Input/import/binary hashes match. Native values have 13 warnings;
  observer builds have nine. Static shell syntax and git diff checks also pass.
  .tmp/inout-array-release-qualified-kind-match-gate2.log,
  .tmp/self_hosted/qualified-kind-match.difPMY.
- The earlier full-root literal guard diagnostic timed out at 180s without any
  output; no allocation-domain or guard verdict was observed. Its diagnostic
  source remains available and never emits or executes the supplied input.
  .tmp/inout-array-release-owned-literal-frontier-v1-observe.log.
- Query correction is committed/pushed as 276e6725. Fresh official v49 passed
  the same original four C controls/eight cycle refusals, then refused gen1
  syntax 55975 / ArrayPushOwnedString(fragments, terminated) in
  CodegenPrefixOwnedStatementLine. Source/input/native hashes match. Exact-head
  CI 37360522726 independently refused the same node; Windows/macOS/TSAN/Rocq
  passed, dependent Linux jobs skipped. Still RED, no gen1/gen2 or install.
  .tmp/inout-array-release-codegen-bootstrap-v49.log,
  .tmp/inout-array-release-bootstrap-v49-boundary.log,
  .tmp/inout-array-release-ci-37360522726-codegen.log.
- Statement-prefix gate2 passed within 300s on native v4: actual C/LLVM output
  values (zero warnings), two source positives and twelve preserved refusals
  per backend; observer builds have nine warnings. The original caller uses
  Concat, not a replacement allocation input. Direct Concat freshness uses the
  exact sealed call/argument context and independent heap-or-null allocation,
  not a heap-domain or function-name shortcut. One duplicate borrow policy is
  deleted; no ordering/alias/exposure/return-chain guard is removed. Current
  observers also admit the earlier query lifetime and preserve its three
  additional local borrowed/shadow/alias refusals per backend. Hashes match.
  .tmp/inout-array-release-owned-statement-prefix-gate2.log,
  .tmp/self_hosted/owned-statement-prefix.5ASIfM.
  Gate1 refused the direct Concat actual after the callee was admitted; it was
  not passing evidence. The exact missing fact and blocked executable rung
  are recorded in the directive before this next supporting proof commit.
- Statement-prefix repair is committed/pushed as 1ea75fa4. Fresh official v50
  passed the unchanged four C controls/eight cycle refusals, then refused gen1
  syntax 57194 / unproved_formal_element_use_entry at the expression-root call
  above. Source/input/native hashes match. No gen1/gen2, fixed point, default
  pair or GUI receipt exists. .tmp/inout-array-release-codegen-bootstrap-v50.log,
  .tmp/inout-array-release-bootstrap-v50-boundary.log,
  .tmp/inout-array-release-codegen-bootstrap-v50-source.sha256.
- Exact 1ea75fa4 CI 37364141275 terminated before any compiler tests: GitHub's
  hosted runner failed to acquire classify-changes after multiple attempts;
  every dependent job was skipped. This is infrastructure failure, not a CI
  reproduction of 57194, and not green. No blind retry was sent because local
  original-input admission already has a known semantic refusal.
- Next falsifying fixture: tests/self_hosted/parity/fixture/
  codegen_expression_root_commit_frontier_probe.pgy imports the actual epoch
  owner. Fresh current C/LLVM source-only observers both reproduce
  unproved_formal_element_use_entry; input/import hashes match. The supplied
  fixture was never emitted or executed. .tmp/inout-array-release-expression-root-frontier-c.log,
  .tmp/inout-array-release-expression-root-frontier-llvm.log.
  Next: identify the actual reached formal-use flags and the missing ordered
  observation/consumption fact; preserve borrowed, alias, post-drop, repeated,
  deferred and missing-fact refusal before another frozen original-input run.
- Current native v4 SHA-256:
  c4f4dd1fb3e735f5afa0516a7c9d0485e481f74833febedf0092ef1a8e074f98.
  Fresh GCC build passed with 12 observed warnings. Native public gate16 passed
  15 executed positives/backend and 30 preserved-artifact refusals, no public-
  driver/default installation claim. .tmp/inout-array-release-native-v4-gcc-build.log,
  .tmp/inout-array-release-native-public-gate16.log.
- Prior pre-merge native v3 SHA-256 (not current-head native evidence):
  4bdb8869388dce2a8c02a5791f1670ada26b9e1cacd47c0b7d8195353e4673ce.
  Native public gate passed 15 executed positives/backend and 30 refusals;
  four C/LLVM dev/release inout/drop assertions ran. Not installed evidence.
  Exact LLVM observer compile passed in 57.46 seconds under original 120s
  budget after the measured development machine-emission fix.
- Before current context migration, scalar/storage gate45: C/LLVM each 28 analyzer positives/41 compile-
  only refusals plus actual copied type and typed success/failure cleanup
  values, independent numeric-array snapshot mutation, binding type guards
  with explicit caller copies, and match fact snapshots/no-mutation, exit 0.
  Imported match-owner source admission also passed for C/LLVM.
  .tmp/self_hosted/collection-borrowed-descriptor-read.mNokLy,
  .tmp/inout-array-release-borrowed-read-gate45.log. Inner Break/Continue,
  same-scope reuse, retained raw scalars and shallow-to-owned mutation refuse.
- Gate46 reached the same 28/41 source rows and imported match/runtime ABI
  owner admission in C/LLVM, then passed all prior native value controls. The
  combined run hit its unchanged 300s budget before the final ABI execution;
  it is incomplete, not a full gate pass. With the source unchanged, the
  final C artifact and freshly compiled LLVM artifact separately passed exact
  last-binding/device/missing/malformed-length values using caller copies.
  .tmp/self_hosted/collection-borrowed-descriptor-read.9GMYnb,
  .tmp/inout-array-release-borrowed-read-gate46.log,
  .tmp/inout-array-release-runtime-abi-type.sha256.
- The routine ABI consumer's duplicate lookup was removed; its two branches
  now use the existing expression ABI lookup. Native C/LLVM exact values and
  last-binding/missing/malformed guards passed with the routine import, and
  both migrated calls/old-symbol deletion were checked. Receipt:
  .tmp/inout-array-release-routine-abi.sha256.
  Its full imported source observer reached the unchanged 60s limit without
  a verdict; do not infer source admission. The original MIR root remains the
  integration falsifier instead of repeatedly validating that cumulative graph.
- Before current context migration, member gate7: C/LLVM each 5 positives/11 compile-only refusals, exit 0.
  .tmp/self_hosted/member-indexed-read.eXURNt,
  .tmp/inout-array-release-member-read-gate7.log. Composed direct readonly
  views admit; copied/owned/inout/deferred roots and value-formal handoff refuse.
- Reached producer corrections use existing contracts: owned String insertion
  and scalar copy, mutable clause-array Clone, required canonical contract ref.
  Native value probes passed exact naming, participant selection, inherited
  names, cloned input survival, repeated resolver and copied-record values
  after input cleanup. Actual canonical generic/scalar parsing and String-array
  classification values passed native C/LLVM. These are not whole-root or
  default-installation receipts; detailed history is in the directive above.
- DIR gate6 passed exact oracle/mutation rows and native C/LLVM malformed-view
  guards. FactsReady still owns the public shape/receipt/range boundary; its
  inner membership predicate receives direct views derived by that wrapper,
  not external replacement views. No new artifact admission was added.
  .tmp/inout-array-release-dir-graph-participant-view-gate6.log.
- Latest official seed v32 passed call-effect construction and refused node
  70449 at unproved_formal_execution_context. Two scratch cleanup defers in
  the ownership verdict made its whole callable opaque under the unchanged
  execution-context contract. The old scan moved to ownership_scan_owner:
  same 21 success/error exits now fill the existing typed verdict and return
  Void. The admission/preparation caller alone retires both scratch buffers
  after the scan, for success/failure alike. No context relaxation, copied
  type arrays, new error API or private user-array release was introduced.
  .tmp/inout-array-release-bootstrap-v32-exact-boundary-context.log,
  .tmp/self_hosted/codegen_nominal_array_declaration/run.6vkeOW.
- Native AIR on the full analyzer import graph passed, existing warnings,
  .tmp/inout-array-release-ownership-scan-native-air.log, 0 errors/9 warnings.
  Gate38 then passed explicit typed success/failure cleanup and retained the
  deferred-context counterexample. No whole-root or installed proof yet.
  Official seed v33 refused node 70281: canonical verdict argument 7 retained
  the readonly result-plan Array<Int> member. Publication now clones that
  member through the existing Clone contract; no borrow/release rule changed.
  .tmp/inout-array-release-codegen-bootstrap-v33.log,
  .tmp/inout-array-release-bootstrap-v33-exact-boundary-context.log.
  Gate42 passed: snapshot independence plus raw-member ordinary-call refusal,
  matching the reached canonical verdict constructor function.
  Numeric member/extracted-array public cleanup remains outside the existing
  release frontier; no member cleanup claim is made by that new fixture.
  Official seed v34 passed verdict publication, then refused node 70720 in
  SelfMirCollectionOwnershipMemberSourceReady. The binding-type owner now
  copies only its selected local/parameter String; canonical field-type input
  also copies its selected scalar. Gate43's new value probe was refused because
  native ordinary String-result lifetime is unknown. Gate44 uses explicit
  caller copies to check values/missing/duplicate guards, not that missing
  lifetime grant. Gate44 passed values and missing/duplicate guards with those
  explicit copies. The native fail-closed contract stays unchanged. Logs:
  .tmp/inout-array-release-codegen-bootstrap-v34.log.
  .tmp/inout-array-release-bootstrap-v34-exact-boundary-context.log.
  Seed v35's handle was missing after goal continuation; process inventory
  confirmed no matching bootstrap/codegen process. Its log stops after status
  checks without a semantic verdict or gen2 receipt. This is incomplete
  execution, not admission or a timeout-based semantic verdict.
  .tmp/inout-array-release-codegen-bootstrap-v35.log.
  Official seed v36 passed binding-type lookup and refused node 71293:
  match case publication retained raw indexed String payloads. The existing
  match owner now copies retained scalars and validates incoming binding rows
  before destination mutation. Gate45 passed values and unchanged-row guards;
  destination deep-release authority is not promoted. Seed v37 passed that
  boundary, then refused node 71641 in SelfMirExpressionRuntimeAbiLocalType.
  The existing owner now reads inventories by ref and copies its selected
  String. Last-binding lookup and missing/malformed guards remain unchanged.
  No native ordinary String-result lifetime grant is inferred.
  .tmp/inout-array-release-codegen-bootstrap-v36.log.
  .tmp/inout-array-release-codegen-bootstrap-v37.log.
  Seed v38 passed that query, then refused node 71741 in the duplicate routine
  lookup. Linux CI 37307402549 independently refused the same node/boundary.
  .tmp/inout-array-release-codegen-bootstrap-v38.log,
  .tmp/inout-array-release-bootstrap-v38-exact-boundary-context.log,
  .tmp/inout-array-release-ci-37307402549-codegen.log.
  Seed v39 passed the duplicate lookup deletion, then refused node 71932 in
  SelfMirRoutineAddInstruction -> SelfMirCfgAddInstruction: raw use Strings
  were retained. The existing instruction-row owner now copies each retained
  use and accepts the input by ref; the routine only forwards that view.
  Native C/LLVM use text after source cleanup, empty-row offsets/counts and
  instruction/block identities passed. Existing copied/raw-row source controls
  passed in both observer backends; no destination deep-drop right was granted.
  .tmp/inout-array-release-codegen-bootstrap-v39.log,
  .tmp/inout-array-release-bootstrap-v39-exact-boundary-context.log,
  .tmp/inout-array-release-instruction-use.sha256.
  Seed v40 stopped before self-host admission: native rejected two unnamed
  use-array results at the new routine ref boundary. Both simple-statement
  consumers now bind their uses for the call. Original native MIR root C/LLVM
  compilation/execution passed its unchanged six-line identity/mutation oracle.
  .tmp/self_hosted/codegen_nominal_array_declaration/run.Kvgg26/root-native.err,
  .tmp/inout-array-release-mir-root-named-uses.sha256,
  .tmp/inout-array-release-mir-root-named-uses-llvm.sha256.
  The gate now prints the native owner's failure cause instead of only its
  wrapper label. Self-host root admission has not yet been rerun on this fix.
  Seed v41 passed native named-use compatibility, then refused node 71966:
  SelfMirInstructionLocalRefRowsAttachExpr0 retained raw local-ref Strings.
  The existing LocalRef row owner now takes readonly inventories and copies
  retained String text in attachment/append paths, without destination deep-
  drop promotion. Native C/LLVM shadowing/version selection, source String
  cleanup survival, malformed/repeated no-mutation guards and nonempty-prefix
  append offsets passed. Existing copied/raw-row source controls also passed;
  original native MIR root C/LLVM kept its exact six-line oracle.
  .tmp/inout-array-release-codegen-bootstrap-v41.log,
  .tmp/inout-array-release-bootstrap-v41-exact-boundary-context.log,
  .tmp/inout-array-release-local-ref.sha256,
  .tmp/inout-array-release-mir-root-local-ref.sha256.
  This new oracle does not claim numeric version-scratch cleanup: the first
  native draft's ArrayDrop there was refused, and the release rule is unchanged.
  LocalRef owner is 142 effective lines under the existing 180 cap.
  Seed v42 then refused node 72305 in SelfMirExpressionGraphUsesAppend at
  unproved_formal_shallow_mutation_entry. Linux CI 37314795454 independently
  refused that same node/boundary. The first insertion-copy candidate passed
  native values but not source admission. The existing unique-use owner now
  uses ArrayPushOwnedString; graph-use and assignment consumers copy selected
  indexed text before the ordinary String formal. No permission rule changed.
  New source copied/raw pair passed both C/LLVM observers, with the raw case
  keeping the exact formal shallow-mutation refusal. Native C/LLVM source-
  cleanup survival, first-occurrence order/deduplication, invalid-graph no-
  mutation and original MIR-root six-line values passed. Structural mutation-
  graph pins and Bash syntax checks passed; not full inventory/integration.
  .tmp/inout-array-release-bootstrap-v42-exact-boundary-context.log,
  .tmp/inout-array-release-ci-37314795454-codegen.log,
  .tmp/inout-array-release-unique-use.sha256,
  .tmp/inout-array-release-mir-root-unique-use.sha256.
  Next falsifier is whole-root admission on the original MIR control, not a
  narrow formal-effect count, native value check or elapsed-time inference.
- OPEN: full integration10 timed out at its original 60s actual C unit; its
  unchanged isolated unit later passed seven rows in 46.35s, not full-shard
  evidence. A development C whole-MIR-root observer timed out at 300s; the
  LLVM observer reached a semantic refusal. Neither timeout is admission.
  Fresh seed/fixed-point/installed pair, full C/LLVM integration, 15 structural
  cap failures and current-head CI remain OPEN. The all-current-work checkpoint
  is published; subsequent CI repair remains required. The new user examples
  are included without an execution claim. Official seed v43 refused aggregate
  finalization with node=-1; a fresh diagnostic observer identified its exact
  WithFunctionTables call. Independent whole-root body admission now passes,
  but the gen0 nominal-root codegen consumer has not yet established official
  route evidence on this source. Seed v44 is the next exact falsifier, followed
  by fixed-point/driver receipts; v44's terminal Slice failure and current
  focused fix above supersede that earlier next-step note. Never install from
  an observer or stale seed.
  SemanticAstBodyTypeBundleFromAnalysis remains the body owner and
  mir_collection_receiver_root.pgy the unchanged official consumer input.
  Installed P0 is OPEN, not a CLOSED row or self-host substitution claim.
  The collection cap block was directly rechecked: 16 -> 15 after the reached
  context/control split, without raising caps; not a full inventory pass.
  The new fact/builder/read-target/member owners independently pass their
  unchanged caps. .tmp/inout-array-release-collection-caps-current.log.
  semantic.hashmap_collection_ownership stays ACTIVE. Current authority-edge
  attempts 3/4 returned no verdict in their 60s budget; no current gate pass.
  CI 37305685187 was
  cancelled by the external documentation push; 37305703308 failed a stale
  derived-fact symbol. The corrected live pin and three existing local-view
  registrations passed the narrow authority-edge gate: 95 authorities/200
  derived carriers and 96th-owner refusal. No status/count authority changed.
  .tmp/inout-array-release-sot-authority-edge-current2.log.
  Current published CI 37316971496 completed with failure at the same Linux
  v43 aggregate-finalization boundary. Windows, macOS C-only, TSAN and Rocq
  passed; dependent Linux jobs were skipped. Logs:
  .tmp/inout-array-release-ci-37316971496-codegen.log. Not current green.
  Later df0df7a4 CI 37329776930 reached the Slice emission failure described
  above; its exact job log is .tmp/inout-array-release-ci-37329776930-codegen.log.
  Next after seed: serial full integration, official fresh driver,
  public all-lane gate and default C/LLVM execution of Alrescha's
  F:/JDW_project/alrescha/tests/repros/array_inout_release_frontier.pgy.
- Current official bin hashes are still native 88526595396deba7532991e55d6ae3d74f260e3f34c3c15bea9ecf117a55f3de
  and driver 707dcd40049a1697a5827b2a7c8d3cf509573aa3c0031f2eee338c9fa0d78ec7,
  independently read here. The consumer's current receipt still reports native
  C/LLVM inout/drop refusals. Tested isolated native v3 is not that installation.
- No completion message sent to Alrescha GUI 프레임워크 1단계. Send it only after
  complete P0 verification/current CI. Then resume the reached SoT rung.

## Historical archive boundary

Everything below is lookup evidence, not an active work queue. Current source,
the top card, exact executable receipts and the SoT registry override old status.

### P0 evidence history

- User priority: finish the GUI-blocking caller `ArrayDrop` contract before
  reopening general SoT closure. Alrescha is a separate, read-only consumer at
  `F:\JDW_project\alrescha`; no compiler integration or callee whitelist.
- Verified local HEAD and `origin/main`:
  `d0fa49ea95928c007a6670f2e2fbfb7b7479441b`. Main independently published
  indexed-signature materialization, exact formal-use order and fresh-generation
  cleanup slices. The P0 implementation below is still uncommitted; Root owns
  its integration. Preserve local `gmon.out` outside the integration commit.
- Objective card and edit boundaries:
  `docs/agent_work_directives/inout_array_release_2026-10-05.md`. Native demanded
  parameter-flow ownership carries a separate descriptor-preservation facet;
  self-host source and direct-MIR consumers independently prove preservation.
  No six-bit Slot mask reinterpretation or previously absent permission grant.
- Last legitimate consumers: native `semantic_array_storage_call_argument`,
  source `SemanticArrayStorageCallArgumentEscapes`, and direct-MIR storage
  lifetime admission. Plain element mutation may preserve caller cleanup;
  descriptor rebind, alias retention, resource results and deferred/worker
  handoff remain negative obligations.
- Observed native gate: 15 executed positives per C/LLVM and 30 compile-only,
  preserved-artifact refusals; exit 0. Receipt:
  `.tmp/inout-array-release-native-v2-gate.log`, evidence
  `.tmp/self_hosted/public-array-drop.h3c9Ps`.
- Demanded flow owner gate: PASS including the unchanged 4096-work budget and
  independent 4097-demand refusal. Receipt:
  `.tmp/inout-array-release-param-flow-v2.log`.
- Earlier C/LLVM admission observers: six positives, five ownership
  refusals and four missing/duplicate synthetic-identity refusals each; exit 0.
  Receipt `.tmp/inout-array-release-source-admission-gate2.log`, evidence
  `.tmp/self_hosted/inout-array-storage.BAv0fj`. These are analyzer verdicts,
  not installed-driver or input-program execution evidence.
- The benign Alrescha `array_inout_release_frontier.pgy` ran successfully using
  the native C/LLVM routes. Fresh default/installed C/LLVM remains OPEN.
- Latest terminal official seed v9 passed the previous indexed-read,
  repeated-call and terminal-tail boundaries but failed before generation 2:
  node `10473`, `aggregate_release_source_not_live`; evidence
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.oQXyuf`, log
  `.tmp/inout-array-release-codegen-bootstrap-v9.log`. Exact parser mapping is
  the return of `SemanticAstGenericParameterFactRowsFromOwnerNode` in
  `ast_generic_parameter_fact_owner.pgy`. Terminal constructor inputs were
  incorrectly required to have a continuing escape bound. The current
  reservation candidate instead consumes their exact admitted input identity
  at ordered completion, preserving all prior escape/element obligations.
- The fresh-generation cleanup candidate passed focused C/LLVM analysis:
  four positives and ten compile-only ownership refusals each. Evidence:
  `.tmp/self_hosted/collection-owned-generation.bXzBSA`. Terminal return and
  continue branches do not revive a consumed generation; same-scope use,
  duplicate transfer and nested repeated/deferred use remain refused. The
  current candidate reuses the existing retirement owner's loop-order proof.
- Readonly-field prerequisite: three positives and eight compile-only refusals
  per C/LLVM analyzer passed. Evidence
  `.tmp/self_hosted/member-indexed-read.d42Idl`, log
  `.tmp/inout-array-release-member-read-gate5.log`. This reuses the ordered
  member pass and propagates a negative whole-root borrow obligation through
  exact forwarding edges. Copies, transfers, field moves and deferred use do
  not become read permission. This is focused analysis, not installation.
- Current-local-read and repeated-call candidate: five positives and eleven
  compile-only refusals per C/LLVM analyzer passed in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.ZS61LG`, log
  `.tmp/inout-array-release-borrowed-read-gate4.log`. The negative effect owner
  and existing definition closure carry descriptor consumption separately from
  sibling invalidation. Exact current owned storage receives shallow copyout;
  alias, unknown, detached and repeated consuming cases remain refused.
- A later terminal-site candidate passed 6 positives/11 refusals in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.aA3bHJ`, but Root
  independently found C/LLVM both wrongly accepting
  `.tmp/inout-array-release/terminal-hides-later-retention-negative.pgy`.
  It is **not an accepted integration candidate**. Findings:
  `docs/audits/inout_array_release_terminal_bounds_2026-10-05.md`.
- Source-issued MIR contract shard: five executed positives per C/LLVM,
  two source refusals and five MIR storage-lifetime refusals per backend; exit
  0. Evidence `.tmp/self_hosted/inout-array-storage-mir.54XVsd`, log
  `.tmp/inout-array-release-mir-contract-gate2.log`. Scaffolds were native-built
  calls into current production owners, not an installed driver. Subsequent
  cleanup-owner changes still require a fresh integrated receipt.
- The user selected Main interruption and sole completion here. Main was
  observed idle/interrupted; Root now owns source, integration and Git. No
  new Main work queue or worktree. Preserve all pre-existing P0 edits.
- Current terminal/continuing descriptor candidate: C/LLVM each passed 11
  analyzer positives and 19 compile-only ownership refusals, including normal
  path retention, compound terminal retention, stale sibling read, legitimate
  short-circuit RHS/own cleanup, owned aggregate return and borrowed aggregate
  refusal. Receipt `.tmp/inout-array-release-borrowed-read-gate14.log`, evidence
  `.tmp/self_hosted/collection-borrowed-descriptor-read.M0zIPw`. This is not
  installed-driver or self-host fixed-point proof.
- Full collection integration7 reached its final compiler-scale C actual
  producer unit but timed out at the unchanged 60-second limit; LLVM was not
  reached. Receipt `.tmp/inout-array-release-collection-integration7.log`,
  evidence `.tmp/self_hosted/collection-inout-effect.F86gp4`. A diagnostic
  run on the same source input observed 152.48 seconds and an element-use
  refusal, not semantic PASS. Do not enlarge the gate budget to call it green.
- Next falsifiers: the exact owned aggregate return at the unchanged official
  seed, then default installed C/LLVM caller cleanup. Seed v10 was interrupted
  during gen0 compilation when its process handle disappeared; no receipt was
  completed. Fresh official seed v11 uses
  `.tmp/inout-array-release/codegen-bootstrap-v11` and
  `.tmp/inout-array-release-codegen-bootstrap-v11.log`; inspect the live process
  or final result rather than inferring success from generated files.
- Seed v11 has since terminated with an owned-mutation compound-return refusal:
  node `10504`, `compound_terminal_storage_effect`, evidence
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.OyB1fW`. It passed the
  prior aggregate reservation boundary. Exact parser mapping is the final
  Boolean return in `SemanticAstGenericParameterFactContractReady`, which
  populates two fresh arrays before reading their current elements. The current
  terminal-entry candidate requires unshared exact storage for owned-content
  mutation; aliases and constructor capture still prevent that proof.
- Measured formal-use lookup delta: the unchanged compiler-scale C actual
  producer unit passed all seven formal observations in 49.55 seconds, within
  its original 60-second budget. Receipt
  `.tmp/inout-array-release-identity-context-c-actual.log`, executable/hash
  `.tmp/inout-array-release/identity-context-c.exe` and
  `.tmp/inout-array-release-identity-context-c.sha256`. Only the repeated global
  retention identity lookup changed to the existing context-bound owner.
  Full C/LLVM integration and post-terminal-entry revalidation are still OPEN.
- Fresh paired installed-driver parity, seed/fixed-point completion and
  exact-head CI are OPEN. Published run `37244277987` for `d0fa49ea` failed its
  nominal MIR-root bootstrap at `unproved_formal_indexed_read_entry`; it is not
  green. Do not mark the SoT row CLOSED or
  send a completion handoff to `Alrescha GUI 프레임워크 1단계` yet.
- Optimization remains permitted only at an observed operation blocking this
  named closure step. No new cache, query engine or parallel implementation rung.
- Current focused terminal-entry receipt: C/LLVM each passed 13 analyzer
  positives and 21 compile-only refusals. Evidence
  `.tmp/self_hosted/collection-borrowed-descriptor-read.pDJC5H`, log
  `.tmp/inout-array-release-borrowed-read-gate18.log`. Complete direct-inout
  formal-use and absent-retention facts admit chained terminal mutation;
  formal retention, aliases and same-expression constructor capture remain
  refused. This is not installed-driver evidence.
- Collection integration9 passed its full C leg, including all 371 source
  inputs, forged-identity checks and the unchanged 60-second actual producer
  unit. It then timed out compiling the LLVM source observer at the existing
  120-second limit, before LLVM execution. Evidence
  `.tmp/self_hosted/collection-inout-effect.McarMK`, log
  `.tmp/inout-array-release-collection-integration9.log`. Later source changes
  and the additional formal-chain fixture require a new integrated receipt.
- Seed v12 passed the fresh-local terminal mutation case and refused node
  `31679` in `SemanticAstIntentExpressionSeedEnvironment` at
  `compound_terminal_storage_effect`. Evidence
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.5UKIwY`. The current
  direct-formal candidate passed focused gate18; official seed v13 later passed
  that boundary and refused node `51211`, `unproved_inout_copy_entry`, in the
  generic-specialization environment assignment. Evidence
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.8F0FHm`; exact mapping
  `.tmp/inout-array-release-bootstrap-v13-exact-boundary-context.log`.
- LLVM compile diagnostic on the exact unchanged observer input took 228.07
  seconds; pipeline timing attributes 214.394 seconds to the backend. Receipt
  `.tmp/inout-array-release-source-context-llvm-profile.log`. This diagnostic
  used a separate 300-second inspection bound, not a larger acceptance budget
  or a collection integration PASS. The timestamped diagnostic subsequently
  took 144.82 seconds: approximately 7 seconds IR emission, 34 seconds IR
  optimization and 95 seconds aggressive target object emission. The existing
  dev profile now selects unoptimized machine emission while keeping the O2 IR
  pipeline, mandatory verification and release's aggressive policy. This
  candidate still requires a fresh native executable and unchanged integration
  gate. The local Clang native build failed linking unavailable `libomp`;
  an isolated rebuild uses the existing GCC toolchain instead.
- The collection structural size inventory currently has 17 cap failures,
  including retained pre-existing owner growth. Do not report the complete
  component contract green or raise those caps silently. No SoT row closes
  from a focused observer, seed or documentation update.
- Focused gate19 passed 14 positives and 23 compile-only refusals each on
  C/LLVM. Evidence `.tmp/self_hosted/collection-borrowed-descriptor-read.6KckA7`,
  log `.tmp/inout-array-release-borrowed-read-gate19.log`. Call admission now
  consumes the existing conditional fresh-generation proof; its duplicated
  narrower definition check was removed, and the file is 105 counted lines
  under its restored 115-line cap. No intervening loop/deferred boundary was
  admitted. Official seed v14 is running on that frozen candidate; judge its
  terminal receipt before installed-driver work or further semantic edits.
- Fresh GCC native v3 build completed with existing repository warnings;
  SHA-256 `4bdb8869388dce2a8c02a5791f1670ada26b9e1cacd47c0b7d8195353e4673ce`.
  Native public ArrayDrop revalidation passed 15 executed positives per C/LLVM
  and 30 native compile-only preserved-artifact refusals. Evidence
  `.tmp/self_hosted/public-array-drop.paDMfA`, log
  `.tmp/inout-array-release-native-v3-gate.log`. This native-only stage did not
  execute public self-host or direct-MIR lanes despite their summary text.
- The inout/drop assertion also ran successfully with native v3 on C/LLVM in
  both dev and release profiles: four fresh executed artifacts, pinned by
  `.tmp/inout-array-release-native-v3-profile.sha256`. Full collection
  integration10 subsequently timed out at its final C actual producer unit
  (unchanged 60 seconds), before the LLVM leg. Evidence
  `.tmp/self_hosted/collection-inout-effect.xYfWKR`, log
  `.tmp/inout-array-release-collection-integration10.log`. Do not infer a full
  integration PASS from the earlier small-case or native receipts.
- Exact LLVM source-observer compilation with native v3 completed in 57.46
  seconds under the original 120-second limit; backend timing is 49.917 seconds.
  Receipt `.tmp/inout-array-release-source-context-llvm-dev-v3.log`. This fixes
  that compile-budget blocker but is not the complete C/LLVM integration gate.
- Official seed v14 passed conditional mutation and refused node `51242`,
  `unproved_indexed_read_entry`, at generic-call capture inside an inner work
  loop. Evidence `.tmp/self_hosted/codegen_nominal_array_declaration/run.XgxCl9`,
  exact mapping `.tmp/inout-array-release-bootstrap-v14-exact-boundary-context.log`.
  Next falsifier: a fresh outer-loop descriptor read repeatedly before its
  generation's later owned cleanup. Preserve real prior/nested/deferred
  consumption, alias and escape refusals; do not skip a zero retirement bound
  in a read consumer.
- Separate unchanged-input C pressure diagnostic: parse 5359 ms, analysis
  3172 ms, body 69438 ms, total 78.36 seconds; body verdict remains
  `unproved_formal_element_use_entry`. Receipt
  `.tmp/inout-array-release-body-stage-pressure-run.log` and its SHA manifest.
  It is diagnostic, not gate success. The same-input timestamped rerun finished
  in 48.73 seconds with that refusal; no further performance change followed.
- Integration10's isolated, unchanged C actual producer unit subsequently
  passed the exact seven expected formal rows in 46.35 seconds under its
  original 60-second limit. Receipt
  `.tmp/inout-array-release-integration10-c-actual-isolated.time` and adjacent
  raw/error/expected-row files. This does not supersede the full shard timeout.
- Focused gate20 rejected the first generation-relative retirement candidate
  at `unproved_indexed_read_entry`. Gate21 then rejected its terminal-scope
  revision at `owned_argument_storage_not_live`: an early terminal own call
  preceded a later continuing cleanup. Neither rejected candidate is accepted.
- Current focused gate22 passed C/LLVM analysis: 15 positives and 26 compile-
  only ownership refusals each. Evidence
  `.tmp/self_hosted/collection-borrowed-descriptor-read.X2bzgH`, log
  `.tmp/inout-array-release-borrowed-read-gate22.log`. Known, unique consumption
  is projected at its generation-relative site; nested reads prove membership,
  not repeatable transfer. A terminal own entry may precede a later continuing
  bound, but cannot erase a prior one. Post-transfer, nested-consumption,
  terminal-hides-normal-consumption and deferred negatives remain refused.
- Official seed v15 is running alone on that frozen candidate at
  `.tmp/inout-array-release/codegen-bootstrap-v15`, log
  `.tmp/inout-array-release-codegen-bootstrap-v15.log`. Seed/fixed-point,
  fresh installed-driver, full collection integration and current CI are OPEN.

### Earlier collection checkpoint

The previous collection-ownership checkpoint below is lookup evidence only,
not the active work queue. Current source, exact executable receipts and the
SoT registry override its older status.

### Checkpoint

- Branch: `main`.
- Published compiler checkpoint: `8abceacd04aad7144edd87f7495e160205f2d0ab`
  (`checkpoint: integrate owned-result and borrowed-view contracts`). Root
  verified that `origin/main` matches this SHA after push.
- This 63-file source checkpoint includes owned-result evidence,
  borrowed String views for signature binding, collection mutation policy,
  source-module/location ownership, and their gates. The follow-on commit
  carrying this card changes only inventory transport, its checker fixtures,
  and coordination/handoff documents; compiler implementation remains at this
  checkpoint. Resolve the inventory revision with
  `git log -1 --format=%H -- tests/self_hosted_component_contract_smoke.sh`.
  Only `gmon.out` is intentionally excluded from publication. Verify local HEAD,
  dirty paths, and remote HEAD when resuming.
- Root owns integration and Git publication; Main owns the reached compiler
  implementation. Parallel edit scopes are fixed in
  `docs/agent_work_directives/ownership_checkpoint_green_2026-10-05.md`.
- `deployment_optimization_guide.md` was preserved unchanged in published user
  document commit `9157646e538a0528a85188f54012a059cf6e0c0a`. Preserve `gmon.out`
  locally and exclude it from staging. Neither is
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
- Executable rung: BLOCKED at that formal-use entry, not closed by inventory
  fixes. The missing fact is a successful full-root formal-element-use proof
  for the reached `types` parameter and its call chain. The producer is
  `SemanticAstCollectionFormalEffectsFromResolvedFacts` through the formal-use
  and fixed-point owners; the last admission consumer is
  `SemanticAstCollectionCallArgumentVerdict`, followed by the MIR-root bootstrap.
  The falsifying input remains `mir_collection_receiver_root.pgy` in the full
  import context. Do not substitute a reduced graph's PASS for this proof.

### Published implementation

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
- Owned-result plan, definition, and return owners derive grounded fresh
  `Array<String>` results from exact declared callable identity. The verdict
  carries `fresh_owned_result_function_syntax_ids`; this is not a name allowlist
  or permission to promote unknown/borrowed results.
- Routine body retirement covers 94 backing leaves:
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

These are observed receipts, not results for every later revision. Root checked
the cited Main receipts and its own structural checks; this documentation
refresh does not run compiler or parity gates.

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
  component inventory on both Windows and mounted WSL exceeded its unchanged
  60-second budget (exit `124`); PowerShell had previously collapsed the native
  shell result to `1`. Neither run is a full component PASS or a semantic
  rejection receipt.
- Follow-on inventory transport isolates GNU make's response-file expansion in
  a temporary `BUILD_DIR`; installed/standalone/admitted bootstrap counts remain
  `0/1/0`. Root independently ran the actual checker on two positive and eight
  negative fixture cases plus the existing 14 lexical unit tests: PASS, exit
  `0`, within 60 seconds. Syntax and diff checks PASS. The production Makefile
  graph check on mounted WSL reached exit `124`; full component completion is
  still OPEN. GNU 3.x fixture support is present but GNU 3.x/macOS was not run.
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
- Prior exact-head [CI run 37211217648](https://github.com/srtdog64/PergyraLang/actions/runs/37211217648)
  on base `1e2fd61d` completed with failure: bootstrap failed, six downstream
  Linux jobs skipped, and five other jobs succeeded. It is not current green.
- Published-checkpoint [CI run 37223614427](https://github.com/srtdog64/PergyraLang/actions/runs/37223614427)
  on `8abceacd04aad7144edd87f7495e160205f2d0ab` selected `run_full=true`.
  [Codegen job 111498732648](https://github.com/srtdog64/PergyraLang/actions/runs/37223614427/job/111498732648)
  failed in step 4 at `2026-10-04T18:20:46Z`: node `46983`, boundary
  `unproved_formal_element_use_entry`, diagnostic type `Array<String>`,
  nominal MIR-root control refusal, Make exit `2`. Native/codegen seed
  publication steps 5 and 6 were skipped. The CI diagnostic type is not the
  source parameter spelling `Slice<String>` from the local mapping above.
  The run completed with failure: 5 jobs succeeded, codegen failed, and 6
  downstream job families were skipped. This published checkpoint is not green.

### Explicit OPEN boundaries

- The last seed bootstrap failed. Published mutation-view changes do not supply a
  successful fixed-point or same-input terminal receipt by themselves.
- Installed `pgy-self-driver.exe` admission is stale. Native/analyzer build
  success is not installation or installed-driver evidence.
- Publication is complete for the source checkpoint above. Exact-head CI green,
  fixed-point bootstrap, full C/LLVM
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
SoT row, or a general query/cache architecture. A green claim requires terminal
receipts for the exact published source HEAD, not this navigation snapshot.

Older checkpoints are evidence in Git history, not an active work queue. Do
not revive them unless the current source, registry, or reached falsifier points
back to them.

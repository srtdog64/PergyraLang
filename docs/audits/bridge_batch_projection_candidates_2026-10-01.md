# ArrayString ABI projection review and LLVM implementation candidate

Status: SOURCE CANDIDATE FROZEN; installed-driver behavior and CLOSED promotion
remain root-owned and unverified by this subagent.
Base HEAD: `62a83a8256bf4fa34878cca8f1f14641736fdeba`.
Scope: one `abi.mir_array_string_layout_projection` executable rung.

## Observation and boundary

The captured ArrayString row and shared four-field target projection already
own physical identity. Readonly-ref, value-result, literal, Split/Join/Args and
scalar type definitions consume the admitted projection. Remaining consumers
discarded its storage or element alignment, and the owned-parameter binding
and LLVM DirWalk adapter accepted no required projection at their boundary.

Element-lifetime facts are separate. The existing cleanup-policy path which
falls from a missing ownership transition into a legacy expression check or
default owned cleanup is not a layout read. Partial consuming moves,
ownership-return chains and conditional cleanup are not closed by this patch.
The four bounded collection projection rows cannot be promoted solely because
their existing parity fixtures pass. Their broader operation/carriage frontiers
are not silently replaced by this ABI work.

## Changed LLVM files

- `src/self_hosted/compiler/direct_mir_scalar_program_llvm_owned_array_string_parameter_binding_owner.pgy`:
  own-parameter copy-in requires exact optional projection when a matching
  parameter exists, and uses its descriptor and storage alignment. No matching
  parameter remains an explicit no-op.
- `src/self_hosted/compiler/direct_mir_scalar_program_llvm_dir_walk_materialization_owner.pgy`:
  unused DirWalk is an explicit no-op; used DirWalk requires its exact captured
  projection and projected descriptor, with contextual missing/drift failure.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_array_string_llvm_value_owner.pgy`:
  validates the LLVM projection and uses carried String element alignment.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_storage_emission_owner.pgy`:
  backing elements and four descriptor fields consume separate carried element
  and storage alignment.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_emission_owner.pgy`:
  one collection projection lookup boundary feeds read/set/condition consumers.
  A typed address fact carries emitted instructions, pointer and element
  alignment to both read and set, rather than reopening the layout decision.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_mutation_emission_owner.pgy`:
  push and length observations require the target projection and carry storage
  versus element alignment.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_pop_llvm_operation_owner.pgy`:
  the admitted pop effect's live descriptor length uses carried storage
  alignment; this does not expand the bounded pop lifetime contract.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_typed_llvm_emission_owner.pgy`:
  String element loads consume `abi.element_align`. Cursor and scalar-local
  alignment are deliberately unchanged and do not belong to this ABI row.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_llvm_storage_owner.pgy`:
  legalized String roots use carried element and storage alignment; the shared
  Int root uses its existing storage projection. Unknown element kinds fail
  explicitly rather than being interpreted as String.

The shared ObjectFieldDeclaration signature is unchanged. Its Int and reverse
callers were inspected but not edited. No C file, registry, shared gate, OWNERS,
handoff, installed binary or Git index/history was modified by this subagent.

## Root-owned caller integration

The new signatures are:

```text
CopyIn(plan, routine, projection_opt: Option<DirectMirArrayStringAbiProjection>)
DirWalkBlock(plan, runtime, projection_opt: Option<DirectMirArrayStringAbiProjection>)
StringArrayLlvmPush(plan, target, operation)
StringArrayLlvmPop(plan, target, operation)
StringArrayLlvmLengthLog(plan, target, operation, formatted_print)
```

Root reported integration in `direct_mir_scalar_cfg_program_llvm_emission_owner`,
`direct_mir_scalar_program_llvm_string_collection_materialization_owner`,
`direct_mir_scalar_cfg_llvm_collection_mutation_emission_owner` and
`direct_mir_scalar_cfg_llvm_collection_operation_emission_owner`.
Only the registered owner factory chooses physical mapping; no AST/MIR rescan,
new compatibility path, new fact family or lifetime default was added here.

## Observed narrow checks

- `git diff --check` passed for these files and the live shared tree.
- Repository comment-excluding counts: owned copy-in 36/40, CFG value 54/60,
  String storage 140/140, read/set/condition 146/220, mutation 69/70, pop 36/40,
  legalized collection storage 172/180. DirWalk is 32 lines. Caps were not raised.
- An isolated development probe imported all changed LLVM owners plus the
  existing local-ref identity owner and ran the installed native compiler's
  explicit `--native-pipeline --emit-c` path. Exit 0; 15 existing unreachable
  statement warnings, no errors. Probe artifacts/logs are under
  `.tmp/self_hosted/bridge_batch_projection_20261001/`.
- The first probe lacked the existing local-ref identity import and failed on
  `DirectMirScalarCfgLocalRefForEach`; the probe import was corrected without
  changing any production source for that missing symbol.

This checks merged syntax/type admission and native C generation only. The
emitted probe C was not compiled or executed. It is not Pergyra-built driver,
fixed-point, installed route, ABI mutation or behavior evidence.

## Integration falsifiers still required

Root's `array_string_layout_consumer_closure_owner.sh` must cover each unique
general-program and legacy-CFG MIR target once for C and LLVM, preserve output
sentinels on missing or mismatched captured ABI, and reject repaired-digest
offset/size/alignment/cross-family drift. Explicit root-level projection
missing/drift negatives must reach owned-parameter and DirWalk consumers;
source ratchets must prohibit the deleted alignment/layout reads. Existing
Int/Bool shared consumers and public ArrayDrop require non-regression.

The remaining `align 8` in these source files is the existing i64 index/cursor
or scalar-local contract. The typed foreach condition's live-length alignment
belongs only to the Int pop branch; it does not infer a String layout. No
unsupported lifetime contract or new target-profile support is claimed.

## Final supported-local consumer extension — same ABI rung

Root's final audit reached ordinary ArrayString local allocation, initialization,
parameter copy-in, expression loads and operation-result stores. These are
supported physical consumers of the same captured ABI, not a successor rung.
Root authorized three additional LLVM consumers and one existing projection
owner. All thirteen edited LLVM production files are now source-frozen for
root's sole integration/build run.

- `direct_mir_scalar_cfg_llvm_local_emission_owner.pgy` now receives the existing
  program `Option<DirectMirArrayStringAbiProjection>`. String allocation, empty
  initialization and local/parameter copy-in use projected descriptor type and
  storage alignment. Bool/Option scalar stores and all non-String copy-in load
  alignment retain their prior contracts. The legacy wrapper supplies typed
  `None`; an unexpected required String local fails explicitly rather than
  recovering a literal layout.
- `direct_mir_scalar_cfg_program_llvm_operation_owner.pgy` uses the captured
  projection for String expression-result store type and alignment. Other
  value families retain the existing scalar/type contracts.
- `direct_mir_scalar_program_llvm_expression_owner.pgy` uses that projection
  for String ordinary-local and value-result parameter loads. Ordinary local
  String loads now spell their required storage alignment; other local types
  keep their existing implicit alignment. The root-owned Slice API call is
  integrated with the existing `array_string_projection, target` arguments.
- `direct_mir_scalar_program_array_string_abi_projection_owner.pgy` owns one
  repeated required-storage admission boundary:
  `DirectMirScalarProgramLlvmArrayStringStorageProjection(ref fact,
  projection_opt, context)`. It rejects missing projection with owned context,
  then checks the existing LLVM `ReadyForFact` contract before returning the
  exact carried projection. It reconstructs no layout and adds no fallback.

Exact root caller signature:

```text
LocalDeclarationsInRange(plan, start, count, routine,
    array_string_projection: Option<DirectMirArrayStringAbiProjection>)
SliceExpressionAt(kind, result, result_type, left, left_type, right, right_type,
    arguments, argument_types,
    array_string_projection: Option<DirectMirArrayStringAbiProjection>,
    target: CompilerTargetProjectionFact)
```

The root general-program caller was observed passing its same existing Option.
The only other local-range call is the explicit legacy `None` wrapper.

Observed additional checks: `git diff --check` passed. Comment-excluding counts
are local 214/215, operation 165/165 and expression 359/360. The existing ABI
projection owner grew from 48 to 60 while its exact scalar-program owner cap is
50. That outstanding cap decision was reported to root; this subagent neither
changed the cap nor hid/compressed the admission contract to satisfy it.

The expanded isolated probe imported these consumers and re-ran native
`--emit-c`. First attempt stopped on root's new Slice preamble passing an
unnamed unwrapped ABI value through a ref boundary. Root bound that exact ABI to
a named local; the second attempt exited 0 with 16 unreachable-statement
warnings and no errors. The earlier suggestion that this was a target
expression was an inference and was corrected by root's exact source check.
The regenerated probe C was not compiled/executed. This remains development
syntax/type/native generation evidence, not installed-driver, negative-ABI,
fixed-point, behavior or registry-CLOSED proof.

Remaining acceptance owner is root's one focused integration gate, including
the new storage admission missing/drift paths and deleted local-layout reads.
No new lifetime support, Int/Bool layout-policy migration or target profile is
claimed by the extension.

## Read-only integration diagnosis: current producer foreach route refusal

Root's private frozen candidate built successfully. Its full ABI gate then
found the producer-issued `src/self_hosted/codegen/fixture/for_each.pgy` MIR
refused before either physical ArrayString backend materializer was reached.
This section is a read-only diagnosis, not another implementation or a CLOSED
claim.

Independent direct-C admission replay used
`.tmp/self_hosted/array-string-layout-closure.xxIlb3/foreach.mir.json` and unique
logs under `.tmp/self_hosted/foreach-route-audit.6d33b060/`. Both drivers exited 1
with `CODEGEN ERROR: direct MIR Option match program envelope is invalid`.
Neither output C file was published. No rejected artifact was executed.

- Existing installed driver SHA-256:
  `63AF81BA7CB3E84FB0A36732DD8F2C7B0EB1F3DC5761B63EC84E24139BB3DB14`.
- Private candidate
  `.tmp/self_hosted/bridge-array-string-layout.HXnrKG/bin/pgy-self-driver.exe`
  SHA-256:
  `111F187631749E163103E6C1670A8E58446ED18CFA0529D6E715BB160B8CA762`.

### Exact source and supplied facts

The one Main routine has seven blocks, two sequential foreach iteration facts
(Int/Array<Int> at syntax 8 and String/Array<String> at syntax 14), and one
collection ownership row. The names local has binding/origin syntax 12,
`borrowed-elements`, `live`, `borrowed-literal`, source binding 0. Its producer
String definition carries required ABI layout id 703020034, size 32, align 8,
data/length/capacity/allocator offsets 0/8/16/24, runtime
`pgy_array_new_String`, inner C type `char*`. The ABI fact is supplied; the
observed refusal does not report a missing or mismatched layout.

The producer also emits a final block-6 `return` instruction sourced as
`AST_RETURN_VOID`, with no expression, physical ABI or uses, and no successors.
Generics, parameters, resource-flow symbols and destructure facts are empty.

### Actual selected failure path

1. `DirectMirScalarProgramRouteAdmissionFromAdmitted` offers the single-routine
   control-flow/builtin routes. Their current instruction-kind claims exclude
   `loop-init`, which this MIR supplies twice. The scalar control-flow claim's
   allowed kinds are branch/def/assign/phi/stmt/return; builtin claim is also
   incompatible with loop-init/phi. Neither supplies this foreach route.
2. `DirectMirScalarCfgGraphRouteClaimed` first calls
   `DirectMirCfgProgramEnvelopeReady`, whose last consumer is
   `DirectMirRoutineHasNoUnsupportedFactsExceptLoop` in
   `direct_mir_scalar_graph_admission_owner.pgy:93`. That boundary demands both
   empty `collection_ownership_facts` and count 0. The actual live borrowed row
   therefore rejects the legacy CFG envelope. This is an admission contract
   mismatch, not an absent ABI fact.
3. `CompileAdmittedDirectMirForTargetObserved` in
   `direct_mir_backend_projection_owner.pgy:215` then selects the Option route
   solely from block_count 7. The input has no Option match purpose/fact.
4. `DirectMirOptionMatchCfgPlanFromAdmitted` in
   `direct_mir_option_match_cfg_plan_owner.pgy:15` checks the same strict CFG
   envelope and dies. That final diagnostic masks the actual foreach owner.

The typed ownership carrier owner already exists:
`BuildMirCollectionOwnershipFacts` in
`src/self_hosted/mir_lower/collection_ownership_fact_owner.pgy`, consumed by
`BuildMirRoutineFactIndex`. The missing boundary is a supported legacy-loop
consumer admission of that existing live borrowed fact, not a new ownership
inference or ArrayString layout default.

Two additional source contracts prevent a one-line route fix:

- `DirectMirScalarCfgRoutineRouteClaimed` explicitly accepts def/phi/stmt/
  loop-init/branch but not `return`. Even after the ownership envelope changed,
  the actual terminal void return would reject this legacy route. The old
  emitter already exits terminal blocks as C `return 0` / LLVM `ret i32 0`, but
  a returned MIR instruction still needs exact semantic admission; it cannot
  just be silently ignored.
- Adding loop-init to the general control-flow claim is insufficient.
  `DirectMirScalarCfgProgramInputReady` in
  `direct_mir_scalar_cfg_program_routine_admission_owner.pgy:33` requires both
  range and foreach plan counts to be 0. Its actual consumer at line 127 dies
  on an external collection plan. The general GraphPlan program extension does
  not currently admit these producer foreach plans.

### Smallest safe proposal and scope boundary

There is no verified ABI-only source fix for this refusal. The current layout
delta cannot claim that its new foreach materializer executed. A conservative
legacy-route repair would need a CFG-specific envelope consuming the existing
typed ownership index, admitting only proved live borrowed-literal collection
reads, plus exact terminal Void-return admission. Keep the strict Option
envelope unchanged and reject missing/wrong-binding/retired/owned-transition
facts explicitly; do not erase ownership rows from MIR or relax shared guards
to accept arbitrary new facts. Reuse or extract the existing program owner's
void-return checks for terminal position, absent successors/expressions/ABI,
and zero uses. This restores a currently advertised legacy feature but touches
route/collection semantic admission, outside physical ABI mapping alone.

Minimal affected responsibilities are the legacy graph-route envelope,
`DirectMirScalarCfgGraphPlanFromAdmitted` terminal instruction admission, and
the shared typed ownership/return boundary used by those consumers. The exact
edit scope and gate must be reopened by root before implementation. A general
foreach/program-extension merge is a larger
`projection.direct_mir_scalar_cfg_program_extension` dependency, not a shortcut
inside this ABI patch. The semantic collection owner remains ACTIVE.

Separately, the seven-block Option dispatch should require an actual owned
Option-match claim or retain the original typed route refusal instead of
masking it with a block-count guess. That diagnostic containment would not by
itself execute foreach or close the ABI row.

Required positive: unchanged producer-issued mixed Int/String foreach MIR
must reach its intended plan and direct C/LLVM materializers, then execute once
per backend with `60` and `abbccc`. Required negatives: missing/forged borrowed
ownership identity; retired/move/drop facts; nonterminal/value/ABI-bearing void
return; malformed foreach CFG; repaired ABI drift; seven-block non-Option
program retaining its own diagnostic and previous output. None of those new
behavioral checks was claimed here. Production sources remain frozen; only
this audit was updated, and no rebuild/install/Git mutation occurred.

## Read-only integration diagnosis: ToInt builtin signature drift

The producer-issued `str_builtins2.pgy` document at
`.tmp/self_hosted/array-string-layout-closure.A1pifk/builtins.mir.json` reaches
the intended scalar-program GraphPlan admission, unlike the foreach refusal.
Independent replay of old installed and private candidate drivers for both
direct C and LLVM exited 1 at:

```text
stage=builtin-call node=3 row=7 source=AST_LET_DECL
```

Unique logs are under `.tmp/self_hosted/builtin-route-audit.c593ab5e/`.
All four runs published no output artifact. No artifact was executed; no source,
test, build, binary, Git or registry was changed during this diagnosis.

The rejected instruction is `let n: Int = ToInt("42")` (MIR instruction id 2,
flat row 7). Its carried graph is exactly leaf ToInt (node 0), direct call ToInt
(node 1, target syntax id 0, carried runtime ABI id 0), String literal `"42"`
(node 2), terminal CallArgument (node 3, left 1, right 2). Expected result is
Int. This is the builtin target, not a declared/user function or ArrayString
layout read.

The first failed owner join is:

- `SemanticBuiltinSignatureRows` in
  `src/self_hosted/semantic/builtin_signature_owner.pgy:27` owns canonical
  `ToInt^Int^String`.
- `DirectMirScalarProgramCollectionBuiltinSignatureExpected` in
  `direct_mir_scalar_program_collection_builtin_signature_owner.pgy:55`
  still projects `["Int", "Unknown", ExprToIntString]`.
- `DirectMirScalarProgramBuiltinSignatureFactFromName` in
  `direct_mir_scalar_program_builtin_signature_projection_owner.pgy:114`
  strictly requires registry parameters equal the expected schema. The actual
  `String != Unknown` comparison returns its invalid signature before argument
  prefix or runtime-call identity admission.
- `DirectMirScalarProgramBuiltinCallFactFromGraph` consumes that invalid
  signature and returns recognized-but-invalid. The expression admission owner
  then reports builtin-call at terminal argument node 3.

No missing runtime implementation is implicated. Existing argument admission
already requires String for ToInt. Existing expression kind
`DirectMirScalarProgramExprToIntString` is 27; collection expression readiness
requires String -> Int. The runtime requirement/projection owners already
select the registry `to-int` / `string_to_int` helper. C calls its owned symbol,
and LLVM emits `call i64 @... (ptr ...)`. Carried graph runtime id 0 is already
the allowed policy for this signature family.

The smallest bounded correction proposal is to change only ToInt's expected
parameter schema from Unknown to String in the existing collection builtin
signature projection. Keep the strict registry comparison, existing argument
prefix check, expression kind and runtime identity policy. This consumes the
current canonical semantic owner; it does not add an overload, change runtime
parsing semantics, choose an ABI layout or require a new fact family.

This proposal was not implemented or behavior-tested here. It can correct the
first observed mismatch, but full fixture success remains unverified until
root's fresh source-current driver reaches all later builtin consumers.
Required positive: producer-issued String -> Int, plus the existing Split /
StringSplit and Join / StringJoin fixture on C and LLVM. Required negatives:
wrong argument type/cardinality, declared target or forged target identity,
unexpected carried runtime id and prior-output preservation. Do not solve the
drift by weakening registry equality or accepting Unknown as a fallback.

## Authorized bounded ToInt correction — fresh-driver integration pending

After root fixed the reached dependency card, this subagent changed exactly
one production line: ToInt's expected parameter schema in
`direct_mir_scalar_program_collection_builtin_signature_owner.pgy` is now
String, matching the canonical semantic owner. The strict registry join,
argument prefix, runtime identity and expression-readiness owners were not
changed. Comment-excluding size remains 120 lines; `git diff --check` passed.
All other production source remains frozen in this subagent's scope.

An isolated development probe under
`.tmp/self_hosted/to-int-signature-probe.8f8b31af/` was compiled and executed
with installed native `--native-pipeline --run` (the probe selected LLVM).
Exit 0 and observed stdout `to-int-signature-positive-and-refusals` establish
the following bounded owner checks:

- canonical ToInt signature is valid, one String parameter, Int return and
  existing ToIntString expression kind;
- an owner-constructed direct-call graph with one String argument has a
  recognized, valid, complete builtin call fact;
- the same call with an Int argument fact is recognized but invalid;
- a direct call missing its carried callee target name is recognized but
  invalid;
- native `ToInt("42")` evaluates to 42.

The probe reported 15 unreachable-statement warnings and no errors; the
warnings were not treated as installed-driver acceptance evidence.

The probe used the existing graph-row factory, not mutated producer MIR or a
rejected program artifact. Its negative checks inspect explicit invalid facts;
they do not execute refused application artifacts. This is bootstrap/native
developer evidence only, not a fresh Pergyra-built compiler, installed route,
whole producer-fixture parity, fixed-point or CLOSED claim. Root must rebuild
its one frozen candidate and independently validate the immutable producer
fixture and required negatives before treating this correction as integrated.
No full driver build, installation, shared test edit or Git mutation occurred
in this subagent's work.

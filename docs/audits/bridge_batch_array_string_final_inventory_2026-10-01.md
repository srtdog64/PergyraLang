# ArrayString ABI final source inventory

Status: READ-ONLY source audit, not a CLOSED decision or executable gate result.
Observed HEAD: `62a83a8256bf4fa34878cca8f1f14641736fdeba`.
Date: 2026-10-01, Asia/Seoul. Source observation window: 13:44-14:00 KST.
The shared implementation tree changed during this audit. Findings below record
the pre-migration falsifiers; root owns their edits and all executable evidence.
No source, tests, registry, binary, Git index or compiler build was changed by
this reviewer. Only this report is written.

## Claim and inspection boundary

- Preserve `abi.mir_array_string_layout_projection`, its identity
  `ArrayStringLayoutProjectionId`, owner
  `direct_mir_array_string_abi_fact_owner.pgy#DirectMirArrayStringCapturedAbiReady`,
  and existing installed source -> verified MIR -> C/LLVM production routes.
- Inspect all 29 registered consumers, then expand beyond the registry's file
  list: 64 compiler files contain the concrete ArrayString carriers/projection
  names. Search the entire compiler for physical four-field definitions,
  positional initializers, constant field indices and alignment/element size.
- Trace the actual emission callers, not only the newly edited leaf files.
  Text-pattern absence is supporting inventory evidence, not behavioral proof.
- Canonical named aliases such as `PgyArray_String`, `pgy_as` and
  `%pgy.array.string` do not by themselves define a second physical layout.
  Named C member access is likewise not a positional layout reconstruction
  when the admitted projection owns the declaration and canonical field names.
- The physical constants in `direct_mir_array_storage_layout_contract_owner.pgy`
  and `direct_mir_array_string_abi_projection_owner.pgy` are the named owner
  contract. The same constants in a backend consumer are the migration problem.

## Exact falsifiers found outside the initial leaf migration

Paths in this section are under `src/self_hosted/compiler/`. Line references
describe the observed pre-migration source; root's later edits can move them.

| Owner and responsibility | Physical old read | Production reach / minimal delta |
| --- | --- | --- |
| `direct_mir_scalar_program_llvm_string_join_materialization_owner.pgy#DirectMirScalarProgramLlvmStringJoinBlock` | Two ArrayString element loads used `align 8`; the function already received `abi` | String collection materialization -> join body. Consume `abi.element_align`, element/value type and explicit projection readiness. Root's replacement was independently reread in the later shared snapshot; both loads now consume the projection. No execution was run here. |
| `direct_mir_scalar_cfg_llvm_local_emission_owner.pgy#DirectMirScalarCfgLlvmLocalDeclarationsInRange` | ArrayString alloca/zero-init at old lines 49-53; common init store at 165; value-result load at 190; parameter store at 204 all emitted numeric alignment `8` | General program routine emission calls this function at line 74. Carry the already admitted projection into ArrayString local/parameter storage only; do not add a type-name-to-layout fallback. |
| `direct_mir_scalar_cfg_program_llvm_operation_owner.pgy#DirectMirScalarCfgProgramLlvmOperation` | Expression-definition local result store at old line 141 used generic `align 8`, including ArrayString | General routine emission calls this at line 90 and already supplies `array_string_projection`. Use its storage alignment for ArrayString results. |
| `direct_mir_scalar_program_llvm_expression_owner.pgy#DirectMirScalarProgramLlvmExpressionAt` | Value-result collection parameter read at old line 185 emitted `.local, align 8` for ArrayString | The expression evaluator has the carried projection already. Validate and consume the ArrayString descriptor alignment; canonical named aliases remain legitimate. |
| `direct_mir_scalar_program_llvm_slice_expression_owner.pgy#DirectMirScalarProgramLlvmSliceExpressionAt` | ArrayClone reads at old lines 113-114 extracted ArrayString data/length using indices 0/1 | Existing Clone expression path. Consume ArrayString data/length indices; the Slice descriptor's own indices belong to the Slice owner, not this row. |
| Same file, `DirectMirScalarProgramLlvmArraySliceHelper` | ArrayString source data/length extract indices 0/1 at old lines 157-158 | Slice preamble emits this for String array slices. Pass the ArrayString projection into the existing responsibility; do not reopen an external aggregate-return ABI. |
| Same file, `DirectMirScalarProgramLlvmStringSliceCopyHelper` | Old lines 225-251 used byte multiplier 8, allocation alignment 8, element load/store alignment 8, output insertvalue indices 0/1/2/3 | `DirectMirScalarProgramLlvmSlicePreamble` calls this for actual String SliceCopy/ArrayClone at old line 363; general program LLVM emission calls the preamble at old line 279. Consume element size/alignment and output descriptor indices; preserve existing allocator/panic failure classes. |
| `direct_mir_scalar_program_c_slice_expression_owner.pgy#DirectMirScalarProgramCSlicePreamble` | String path passed `(pgy_as){NULL, 0, 0, NULL}` at old line 187 | General program C emission calls this at old line 251. Root's updated source was reread: the String branch requires the projection and uses `DirectMirScalarCfgArrayCInitializer`. Remaining positional initializers in this file are Int-only, not ArrayString. |

After the StringJoin fix, the stable additional migration list was five modules
and seven responsibility functions (the other seven rows above). Root accepted
them as part of the same ABI rung. The last expanded source-pattern round found
no further confirmed ArrayString physical-layout reconstruction beyond this list.
That is a bounded source audit, not a substitute for root's post-edit checks.

### Post-edit source reread

At the end of the audit, all seven additional responsibility functions had been
migrated in the shared source. The reviewer independently reread these changes:

- ArrayString local alloca, initializer store, value-result load and parameter
  store use the checked carried storage projection; general program routine
  emission supplies it. Other scalar families retain their existing alignment.
- ArrayString operation result stores and local/value-result expression reads
  use the checked carried storage projection rather than numeric alignment.
- Clone and Array.Slice read ArrayString fields through admitted storage indices.
- String SliceCopy consumes element size/alignment/type and all output indices;
  missing projections and drift are explicit failures. Slice descriptor indices
  stay with the separate Slice contract. Existing typed allocator/panic calls
  remain in the body.
- C String SliceCopy uses the projection-derived designated initializer and
  explicit missing-projection failure. Int-only initializer literals remain.

No new confirmed ArrayString physical old read was found in the final broader
pattern scan. Root's focused behavior, negative ratchet and installed-driver
verification are still required before this observation becomes a CLOSED claim.

## False positives and smaller observations

- Typed foreach's `i64` cursor and string accumulator alignment are not
  ArrayString element alignment. Its String element load now consumes
  `abi.element_align`.
- `DirectMirScalarCfgForEachTypedLlvmCondition`'s remaining literal alignment
  for `.length.field` is guarded by `ArrayIntForEachPrefixPopMode`; the String
  route extracts the length with its admitted projection index.
- `DirectMirScalarCfgStringArrayLlvmIndex` loads a scalar Int index with
  alignment 8; that is not an ArrayString storage load.
- Process Args' `argv` and its external source pointer load are process ABI,
  whereas the resulting ArrayString alloca/load consume `projection.storage.align`.
- `DirectMirScalarProgramLlvmArrayStringSetMaterialization` uses
  `projection.storage.align` for an element store. The current admitted owner
  requires storage and element alignment both to be 8, so this is not a second
  numeric authority. Consuming `projection.element_align` would express the
  element responsibility more precisely; no current mismatch was demonstrated.
- The new public ArrayDrop expression's literal data index is on the
  Int/Bool/Long/record materialization paths. That function has no ArrayString
  descriptor materializer; do not count it as a reached ArrayString layout
  reconstruction or as proof that every ArrayString operation is supported.
- Logical-record array/Int/Bool/Long physical literals may belong to other
  open ownership seams. They must not be bundled into this row merely because
  their modules also mention ArrayString.

## Smallest existing fixture coverage map

All stdout below comes from current existing gate expectations, not a new run.
`\\n` denotes a line ending and the listed base outputs end with a final newline.
These programs should each be produced once, then projected to C and LLVM from
the same verified MIR using root's single current-source/installed driver batch.

| Existing source | Exact base stdout | Distinct path it covers / existing gate |
| --- | --- | --- |
| `src/self_hosted/codegen/fixture/str_array.pgy` | `alice\nbob\ncarol\nBOB\n` | Legacy CFG StringArray indexed read/while length/static set and storage declarations; `one_mir_string_array_mutation_projection.sh` asserts indexed artifact markers. |
| `src/self_hosted/codegen/fixture/str_array_push.pgy` | `abbccc\n3\n` | Legacy CFG StringArray push capacity/storage plus indexed while read; `one_mir_string_array_push_projection.sh`. |
| `src/self_hosted/codegen/fixture/for_each.pgy` | `60\nabbccc\n` | Typed mixed Int/String foreach projection; `one_mir_mixed_collection_foreach_projection.sh`. This does not replace legacy indexed/push coverage. |
| `src/self_hosted/codegen/fixture/str_builtins2.pgy` | `yes\n3\nbb\na|bb|c\n43\n1\n2\nleft\nleft/right\n` | General-program Split/StringSplit, get, length, StringJoin/Join and storage preamble; `one_mir_string_collection_builtin_projection.sh`. |
| `tests/self_hosted/fixtures/direct_mir_dir_walk_direct_call.pgy` | `2\n` | General-program C/LLVM DirWalk materialization and nested direct call; `direct_mir_scalar_dir_walk_direct_call_owner.sh`. Must create exactly tree/a.txt and tree/sub/b.txt, and remove tree/written.txt before each run because the program writes it. |
| `tests/self_hosted/fixtures/direct_mir_owned_array_string_parameter.pgy` | `released\n` | General-program caller-local owner move, callee-owned storage/cleanup; `direct_mir_scalar_owned_array_string_parameter_owner.sh`. |
| `tests/self_hosted/fixtures/direct_mir_bool_two_array_string_two_array_int_value_result.pgy` | `mixed-four-copyouts-ready\nmixed-interleaved-ready\n1\n1\n1\n1\n` | General-program interleaved 2+2 collection copy-in/out/local result and parameter storage; `direct_mir_scalar_bool_two_array_string_two_array_int_value_result_owner.sh`. |
| `tests/cases/backend_compare/slice_copy/main.pgy` | `2\n20\n30\n2\nred\nblue\n0\n` | Actual returned String Array -> Slice -> owned copied Array, including empty copy; `slice_copy_semantic_bridge_owner.sh` owns native/public C/LLVM expectations. Necessary for the newly found Slice ABI reads. |
| `tests/concept_semantics/hashmap/string_array_clone_independence.pgy` | `alpha\n` | String Array Clone source descriptor read, deep element copy and independent cleanup. Stdout is directly implied by this existing source; this reviewer did not locate a gate-owned stdout literal or execute it. |

The first seven sources are a small responsibility-covering existing set for
the initial leaf batch. SliceCopy and Clone add distinct supported old-read paths
and cannot be omitted after their migration. This is not a mathematically proven
minimum set. Reusing one source's MIR must not falsely attribute it to a route it
does not exercise.

Useful additional exact sources when the corresponding responsibility changes:

- `tests/self_hosted/fixtures/direct_mir_array_string_value_result_dynamic_indexed_assignment.pgy`
  -> `left\nmiddle\nright\n`; general-program dynamic Set, not legacy static Set.
- `tests/self_hosted/fixtures/direct_mir_owned_array_string_return.pgy`
  -> `1\n2\n0\n1\n2\n0\n1\n2\n`; standalone owned return and cleanup.
- `tests/self_hosted/fixtures/direct_mir_array_mutation.pgy` ->
  `index\nint-value\n10\n1\n7\n2\n8\n2\n9\n8\n1\nstring-value\nb\nb!\n1\n0\n1\n0-set\n0\nlate\n`;
  local and value-result ArrayPop/Set plus mixed families, from
  `direct_mir_scalar_array_mutation_owner.sh`. This is general-program Pop,
  not evidence that the legacy StringArrayPop leaf executed.

No dedicated existing source that proves execution of the legacy
`DirectMirScalarCfgStringArrayPopLlvmOperation` leaf was identified in this
bounded audit. Its source migration is not executable coverage by itself.

## Full-row closing decision

The initial leaf changes alone could not close the full row: the source
falsifiers above were supported materializers that actually reconstruct ABI.
They are not merely future ownership/lifetime features. Their removal is a
real same-rung migration without changing this row's identity or claim scope.

After root's source migration, closure remains conditional on observed evidence:

1. Reread the stable post-edit source and prove no listed backend old reads remain.
2. Exercise the reached legacy CFG and general-program materializers against one
   current-source driver and the installed production driver in C and LLVM.
3. Run existing missing-layout, offset/alignment drift, complete cross-family row
   rejection and stale-receipt negative cases, preserving explicit owned failure
   and non-publication. Add negative coverage for newly threaded projection seams
   through root, not by claiming a source grep is a behavioral gate.
4. Keep unsupported partial conditional moves, fresh-result/literal owner moves,
   ownership-return chains and conditional cleanup visible at their actual
   lifetime owners. Unsupported frontiers neither prove ABI closure nor create
   a second ABI authority by themselves. Do not relabel them out of this row to
   reduce the BRIDGE count; retain the full existing claim and demonstrate the
   required missing-fact refusal where admission reaches those boundaries.

This reviewer ran no gate and makes no installed/current-source execution claim.
The registry's observed status remained BRIDGE during this source audit.

## Post-Bau8ob registry-obligation addendum

This addendum is a read-only comparison by the projection reviewer after root's
private `bridge-array-string-closure.Bau8ob` integration reached legacy pop.
It does not replace the earlier observation window or make a CLOSED decision.
HEAD remains `62a83a8256bf4fa34878cca8f1f14641736fdeba`; the registry row is
unchanged and BRIDGE. Only this audit is edited by this reviewer. Existing root
logs/artifacts are inspected, not rerun; no compiler build, installation, source,
shared-test, or Git mutation is performed here.

### Exact registered consumer inventory

The row currently has 29 consumer paths and 21 forbidden fallbacks. Preserve all
29, including its four move/cleanup fact consumers: they cannot be removed just
because they are not physical descriptor emitters. Paths below are all under
`src/self_hosted/compiler/`, in the registry's original order. The second column
names the responsibility inspected, not an independent source of layout facts.

| Registered consumer | Relevant receipt consumption / last responsibility |
| --- | --- |
| `direct_mir_scalar_cfg_foreach_string_collection_owner.pgy` | `DirectMirScalarCfgForEachStringCollectionFromOwners` carries captured layout identity/size/alignment/offsets into the shared typed foreach fact. |
| `direct_mir_array_string_abi_projection_owner.pgy` | `DirectMirArrayStringAbiProjectionFromCanonicalLayout` and `ReadyFor` bind that exact row to one admitted target. Canonical element constants here are owner contract, not backend reconstruction. |
| `direct_mir_scalar_cfg_foreach_typed_c_emission_owner.pgy` | `ForEachTypedCPreamble` / `CDeclarations` use `abi.storage` for descriptor declaration, assertions and named-field initialization. |
| `direct_mir_scalar_cfg_foreach_typed_llvm_emission_owner.pgy` | `ForEachTypedLlvmBlockEntry` uses the String data index and element alignment from its admitted projection. |
| `direct_mir_scalar_program_array_string_abi_owner.pgy` | `ArrayStringAbiFactFromAdmitted` joins exact parameter/definition/return captures; `AbiCaptureMatches` rejects divergent row identity. It does not infer a physical row from type alone. |
| `direct_mir_scalar_program_array_string_abi_projection_owner.pgy` | `AbiProjectionFromFact` / `ReadyForFact`, C carrier and LLVM storage boundaries compare the existing captured fact to the target projection. Canonical absence is only unused-family absence. |
| `direct_mir_scalar_program_array_string_mutation_projection_owner.pgy` | General Set/Pop use carried aggregate type, field indices and alignment; C delegates to the same declared carrier's runtime materializer. |
| `direct_mir_scalar_program_c_string_collection_materialization_owner.pgy` | Collection preamble composes storage, DirWalk and Process Args materializers with the same Option projection; no private layout factory. |
| `direct_mir_scalar_program_c_process_args_materialization_owner.pgy` | `CProcessArgsBlock` requires exact fact/projection readiness before converting the public descriptor. |
| `direct_mir_scalar_program_c_array_string_storage_materialization_owner.pgy` | `CStringArrayStorageBlock` delegates field order, assertions and empty initialization to the admitted storage projection. |
| `direct_mir_scalar_program_c_dir_walk_materialization_owner.pgy` | `CDirWalkBlock` uses projected field names, not positional four-field reconstruction. |
| `direct_mir_scalar_program_llvm_string_collection_materialization_owner.pgy` | LLVM composition threads the same admitted projection to storage/access/join/DirWalk/Args blocks. |
| `direct_mir_scalar_program_llvm_process_args_materialization_owner.pgy` | `LlvmProcessArgsBlock` uses projected ArrayString alloca/load alignment. External argv loads remain a separate process ABI contract. |
| `direct_mir_scalar_program_llvm_array_string_storage_materialization_owner.pgy` | Push/access/drop/split blocks consume projected field indices, element facts and descriptor alignment. |
| `direct_mir_scalar_program_llvm_string_join_materialization_owner.pgy` | `LlvmStringJoinBlock` now requires projected value/element type and element alignment for both String element loads. |
| `direct_mir_scalar_program_llvm_array_readonly_ref_owner.pgy` | Readonly-ref parameter load validates the carried String projection and uses its descriptor alignment. |
| `direct_mir_scalar_program_llvm_array_value_parameter_storage_owner.pgy` | Addressable by-value CopyIn validates String fact/projection and consumes descriptor alignment. |
| `direct_mir_scalar_cfg_program_c_emission_owner.pgy` | One root Option projection is passed to signatures, collection preamble, expressions, value-result storage and returned values. |
| `direct_mir_scalar_cfg_program_llvm_emission_owner.pgy` | One root Option projection is passed to owned CopyIn, ordinary locals, operation results, expressions, Slice/Clone and copy-outs. |
| `direct_mir_scalar_cfg_program_c_signature_owner.pgy` | ArrayString signatures require checked carrier projection; no type-name-to-physical-layout fallback. |
| `direct_mir_scalar_program_c_payload_enum_owner.pgy` | Payload field type selection consumes the checked ArrayString carrier through the signature boundary. Enum layout remains a separate fact family. |
| `direct_mir_scalar_program_c_array_string_value_result_owner.pgy` | Exact value-result parameter identities select checked C CopyIn/CopyOut; unrelated/absent formals are explicit no-ops. |
| `direct_mir_scalar_program_llvm_array_string_value_result_owner.pgy` | Exact value-result identities select checked aggregate CopyIn/CopyOut and descriptor alignment. |
| `direct_mir_scalar_program_owned_array_string_move_fact_owner.pgy` | Sealed move row-set identity includes layout IDs and the existing complete-exit-set coverage receipt. This is lifetime evidence, not a second physical layout owner. |
| `direct_mir_scalar_program_owned_array_string_move_admission_owner.pgy` | Move admission requires an eager single occurrence, a named caller local, ready ABI and complete consuming-exit coverage. Fresh-result/literal arguments are not converted into local proof. |
| `direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy` | Existing transition state and admitted move fact choose cleanup/retirement. No offset, size or descriptor reconstruction occurs here; its remaining lifetime frontier must remain visible. |
| `direct_mir_scalar_program_c_array_string_literal_expression_owner.pgy` | Literal lowering requires the String projection and projected named-field empty initialization. |
| `direct_mir_scalar_program_llvm_array_string_literal_expression_owner.pgy` | Literal lowering requires projected aggregate spelling and descriptor alignment. |
| `direct_mir_scalar_program_owned_array_string_move_flow_owner.pgy` | Eager-path, definition, exit-path and last-use checks own consuming-flow proof; their input digest binds exact typed flow rather than backend re-proving it. |

The fact owner is still
`direct_mir_array_string_abi_fact_owner.pgy#DirectMirArrayStringCapturedAbiReady`;
the physical contract is still `DirectMirArrayStorageLayoutContract`. Neither
this inventory nor a materializer acquires physical ABI authority.

### Additional direct consumers the registry file list does not spell out

These are required expanded last-consumer/old-read ratchet targets for this
same row. The existing 29-path list is not exhaustive merely because root's
orchestration owners call these leaves. Inventory them explicitly rather than
substituting another row identity or counting every incidental String mention.
All paths below are likewise under `src/self_hosted/compiler/`.

| Additional direct consumer | Physical responsibility |
| --- | --- |
| `direct_mir_scalar_cfg_array_c_materialization_owner.pgy` | `ArrayCStruct` / `ArrayCInitializer`: projected field order, assertions and designated fields shared by C emitters. |
| `direct_mir_scalar_cfg_string_array_c_storage_emission_owner.pgy` | Legacy String collection preamble/declarations from the exact collection row. |
| `direct_mir_scalar_cfg_collection_plan_c_storage_owner.pgy` | Generic collection-plan String descriptor and initialization from carried per-value layout. |
| `direct_mir_scalar_cfg_collection_plan_llvm_storage_owner.pgy` | Generic collection-plan String backing globals/descriptor fields from carried per-value layout. |
| `direct_mir_scalar_cfg_array_string_llvm_value_owner.pgy` | String array element storage and projected descriptor insertvalue indices. |
| `direct_mir_scalar_cfg_string_array_llvm_storage_emission_owner.pgy` | Legacy String backing storage, alloca/store and object-field GEP indices. |
| `direct_mir_scalar_cfg_string_array_llvm_emission_owner.pgy` | Exact legacy projection factory, descriptor address/read/set/length condition. |
| `direct_mir_scalar_cfg_string_array_llvm_mutation_emission_owner.pgy` | Legacy push/length log descriptor and element alignments. |
| `direct_mir_scalar_cfg_string_array_pop_llvm_operation_owner.pgy` | Legacy String pop descriptor alignment and length index. |
| `direct_mir_scalar_cfg_foreach_typed_llvm_condition_owner.pgy` | String foreach length extraction consumes admitted index; the remaining numeric field alignment is Int prefix-pop only. |
| `direct_mir_scalar_program_llvm_owned_array_string_parameter_binding_owner.pgy` | Owned String parameter CopyIn requires exact present projection and projected storage alignment. |
| `direct_mir_scalar_program_llvm_dir_walk_materialization_owner.pgy` | LLVM DirWalk adapter requires exact present projection for its resulting String descriptor. |
| `direct_mir_scalar_cfg_llvm_local_emission_owner.pgy` | ArrayString ordinary local/parameter allocation, initialization and value-result reads. |
| `direct_mir_scalar_cfg_program_llvm_operation_owner.pgy` | ArrayString operation result stores. |
| `direct_mir_scalar_program_llvm_expression_owner.pgy` | ArrayString local and value-result parameter loads; forwards the exact projection to Slice/Clone. |
| `direct_mir_scalar_program_llvm_slice_expression_owner.pgy` | ArrayString Clone/Slice source data/length and String SliceCopy element allocation/copy/output descriptor. Slice's own descriptor remains separate. |
| `direct_mir_scalar_program_c_slice_expression_owner.pgy` | String SliceCopy/Clone carrier and output initializer from the exact String projection. |

The root C/LLVM emission owners own the callers of these physical leaves.
Existing move coverage/query and C/LLVM cleanup leaves are also transitive
lifetime consumers; they must remain in the lifetime proof inventory, not be
deleted as unrelated bookkeeping. In particular:
`owned_array_string_move_coverage_admission_owner.pgy` owns the single exit-set
proof, `owned_array_string_move_query_owner.pgy#...MoveReadyForAbi` compares
every move layout ID to the ABI fact, and C/LLVM `array_string_cleanup_owner.pgy`
consume the cleanup policy. These names have the usual
`direct_mir_scalar_program_` prefix.

### All 21 forbidden fallback obligations remain

No existing fallback is dropped or waived by the new layout batch.

| Obligation group | Exact existing fallback identities | Required evidence |
| --- | --- | --- |
| Physical reconstruction | `backend_local_array_layout`, `backend_string_element_reconstruction`, `capacity_as_length`, `scalar_program_preamble_layout_literal`, `c_dir_walk_positional_array_string_layout` | Supported C/LLVM leaves consume the captured projection; field/size/element/order mutation refusal plus old-read ratchet. Length and capacity must not become interchangeable just because one literal initializes both equally. |
| Captured row and sealing | `layout_id_without_row_admission`, `post_issue_layout_mutation`, `complete_cross_family_row_acceptance`, `duplicate_scalar_program_array_string_projection_readiness` | Exact row admission; coherent repaired wire-ID drift and complete Int-to-String row substitution reject before publication; consumers reuse the named readiness boundary. |
| Runtime identity | `array_runtime_symbol_guess` | Captured String row checks canonical runtime/inner type; materializers retain existing registry-owned runtime facts, not emitted symbol guesses. |
| Carried boundary projection | `process_args_array_string_storage_alignment_literal`, `process_args_without_target_projection`, `llvm_readonly_ref_array_string_storage_alignment_literal`, `readonly_ref_array_string_without_target_projection` | Actual Args/readonly storage reads consume target-qualified alignment; missing/drift projection refuses, unrelated process ABI loads are not misclassified. |
| Owner-handle integration | `owner_handle_array_string_without_caller_move_fact`, `caller_cleanup_after_owner_handle_array_string_move`, `owner_handle_array_string_use_after_move`, `multiple_owner_moves_rejected_as_scalar_fact` | Supported single/non-entrypoint/permuted/multiple caller-local move positives and missing/duplicate/moved-use negatives must retain one ABI-bound move receipt and exact caller cleanup retirement. |
| Complete consuming flow | `orphan_or_repeated_consuming_expression`, `stale_owned_move_flow_receipt`, `duplicate_final_owned_move_cfg_proof` | Existing terminal-flow and repaired-digest sealed-flow gates retain exact exit-set/input binding and reject orphan/repeated/lazy/bypass uses. A layout mutation gate is not a substitute for this proof. |

### Classifying the current open_reason without narrowing the row

The existing reason combines physical consumer migration and unsupported
lifetime semantics. Source owners, not prose alone, establish the distinction:

| Existing reason clause | Actual source owner / remaining proof |
| --- | --- |
| `remaining non-literal expression materializers` | Genuine layout migration: ordinary local/operation/expression storage, owned CopyIn, LLVM DirWalk, StringJoin, Clone/Slice/SliceCopy, and legacy collection leaves above. These supported reads must be removed and negatively ratcheted; their absence cannot be inferred from a general builtin positive. |
| `partially consuming conditional owned-parameter moves` | Existing move coverage requires every post-definition exit to hit a consuming block. `OwnedArrayStringMoveExitPathsCovered` refuses a terminal exit without a consume; changing this needs lifetime/cleanup proof, not a different ABI offset. Keep the unsupported boundary and bypass-exit falsifier visible. |
| `fresh-result or literal moves` | `OwnedArrayStringMoveFactFromProgram` requires the argument to be an exact named `ExprLocal` with a routine-local identity. A ready String ABI does not turn a fresh expression into ownership provenance. |
| `ownership-return chains` | Caller partition, definition operation, source local and call/return graph identity belong to the existing move admission/coverage owners. Standalone admitted owned return is already a distinct supported claim and still needs its layout/copy-in/out proof. Do not treat every return chain as that supported case. |
| `conditional cleanup` | `ArrayStringLocalCleanupRequired` / `CleanupDropSymbol` consume existing transition and move state. A physical descriptor projection does not authorize branch-sensitive retirement or deep element ownership. |

These lifetime boundaries already live in the Pergyra projection move/cleanup
owners reached by `projection.direct_mir_scalar_cfg_program_extension`, with
upstream collection provenance in ACTIVE `semantic.hashmap_collection_ownership`.
Neither row is closed by this layout audit. Conversely an unsupported lifetime
program does not by itself demonstrate a remaining numeric layout read.

This distinction is not permission to delete clauses/consumers/fallbacks or
shrink the stable ABI row into "literal-only" or "known-good-fixture-only".
Its supported complete consuming-exit-set and admitted return claims remain;
the unsupported boundaries must still refuse missing proof. A closing review
must state these limits at their current owners and retain the listed positive
and negative obligations rather than promise newly supported ownership semantics.

### Current observed evidence and smallest next falsifier

Read-only artifact inspection confirms root's
`bridge-array-string-closure.Bau8ob/private-layout-gate.log` stopped after 13
successful C/LLVM semantic bases at `pop c compilation failed`. The actual
generated markers classify **12 general-program bases and one legacy foreach**.
`indexed` and `push` now contain `pgy_r0_block_` and private projected `pgy_as`;
their historical fixture names must not be counted as execution of legacy
String read/set/push leaves. The legacy foreach artifact has one Int descriptor
and one public String descriptor assertion.

`array-string-layout-closure.DQKj7T/pop.c` is a reached legacy artifact with
`pgy_ai` typedefs at lines 10 and 40, two `sizeof(pgy_ai)` assertions, and one
public String descriptor. Its C compile log reports the two conflicting
anonymous typedefs. This is duplicate descriptor materialization in composition,
not evidence of two different canonical layouts or a new String lifetime policy.
Root's current source has the minimal existing `ForEachPrefixPopMode` guard in
`DirectMirScalarCfgArrayIntCPreamble`: only that mode reuses the already-issued
foreach descriptor. This post-Bau8ob source delta is not present in the frozen
Bau8ob binary and must not inherit its build receipt.

The smallest next executable falsifier is therefore the **same producer-issued
`array_pop.pgy` semantic input**, not a larger fixture or ownership frontier:

1. After root's exact frozen rebuild, the legacy C output must contain exactly
   one foreach-owned Int descriptor; C and LLVM must both compile and emit
   `30\n2\n2\na\n`.
2. The LLVM marker `%pgy.op.7.pop.length` must establish that the legacy String
   pop leaf actually executed. General-program ArrayPop coverage cannot stand
   in for it. No C/LLVM pop execution was completed in the observed failed run.
3. Continue the existing common gate to reverse, its 14 borrowed/terminal route
   falsifiers, and its coherent ABI mutations over indexed/copyout/Slice/Clone
   bases. The failed pop run reached none of those later negative loops.
4. Preserve the registry's existing owner-handle/terminal/sealed-flow obligations.
   A particularly small independent lifetime falsifier is the existing
   `direct_mir_owned_array_string_bypass_exit.pgy`: a post-definition early exit
   misses the consuming call. `direct_mir_owned_array_string_lazy_move.pgy` also
   falsifies eager occurrence. Neither is an ABI-field reconstruction test.

Root's separate `private-array-drop-gate.log` records PASS stage=all, including
native/source/direct positives and preserved-artifact negatives. It is useful
shared-family non-regression evidence, not proof of the remaining ABI mutation
loops or terminal/sealed-flow clauses. This reviewer does not execute any
artifact or make an installed-driver acceptance/registry-closure claim.

# 2026-11-16: direct MIR refusal census on the compiler's own entries (DM0)

Slice 0 of the [last-use release decision](../decisions/2026-11-16_compiler_temporaries_are_released_at_last_use.md):
count every reason the self-host direct MIR emitter refuses the compiler's
own entry.

Run by the Claude root lane, track DM0, on WSL `Ubuntu-C-WSL`.
- Tree `/root/tdm0` at `cd33a0031` (origin/main), built with `make pgy` and
  `make self-host-compiler`.
- The census driver is a scratch copy in `/root/tdm0c` (same commit plus the
  patches listed below, never committed).
- Every heavy process ran alone under a watchdog that kills it above
  9 GiB RSS.

## Which MIR the LLVM leg consumes

- `pgy --emit-llvm` (the self-host LLVM leg) runs
  `pgy-self-driver --emit-source-llvm-ir-verified SRC -o OUT`
  (`src/compiler/self_host/self_host_llvm_driver.c:41`).
- That route builds the self-host source MIR payload in memory, the same
  producer as `--emit-mir-json-verified`. It refuses the program if any
  compiler Clone landing has no MIR carrier
  (`driver_source_llvm_projection_publication_owner.pgy:35`). Then it calls
  `CompileMirJsonTextToDirectBackendVerifiedObserved`.
- `--mir-json-backend=llvm FILE` reads the same JSON from a file
  (`MirJsonReadMachineAdmittedInput`) and skips only the landing check.
- So the census used `--emit-mir-json-verified` once per input and
  `--mir-json-backend=llvm` on the file. Native
  `--test-native-mir-json-oracle` is a different producer and was not used.

## Inputs and the stock driver

| Input | MIR bytes | routines / instructions | MIR: s, peak RSS | stock direct MIR: s, peak RSS | stock first `CODEGEN ERROR` |
|---|---:|---:|---|---|---|
| `semantic/main.pgy` | 41,149,084 | 1,094 / 18,461 | 5 s, 0.63 GB | 73 s, 1.88 GB | `program_readiness=26` |
| `mir_lower/main.pgy` | 155,165,005 | 3,679 / 61,659 | 31 s, 2.33 GB | 147 s, 1.48 GB | routine admission `collection_ownership_transition` (ordinal 817) |
| `codegen/main.pgy` | 199,622,171 | 4,536 / 79,462 | 45 s, 3.19 GB | 256 s, 2.84 GB | expression admission `builtin-call` (`ArrayReverse`, ordinal 1865) |
| `driver/driver_rung2_main.pgy` | 412,992,307 | 9,470 / 149,546 | 202 s, 8.50 GB | 320 s, 0.73 GB | route `scalar-program-route` stage `role-declaration` |

- The DRV-2 MIR has 947 declarations: 818 struct, 77 class, 15 role,
  15 ability and 22 enum.
- **Compiler Clone landings without a MIR carrier: 0 on all four inputs.**
  The census driver re-produced each MIR and listed every landing. Its MIR
  is byte-equal to the stock MIR.

## Census method

The scratch driver records each refusal on stdout as `[dm0-census] ...`
(owner, stage, code, routine) and continues. The census turned these refusal
sites into records (paths under `src/self_hosted/compiler/`):

- `direct_mir/scalar_program/direct_mir_scalar_program_route_admission_owner.pgy`
  - every routine's `callable-route-envelope` and `callable-signature`
    rejection is recorded, not only the first;
  - the `role-declaration` rejection (:144) is recorded and bypassed.
- `direct_mir/scalar_cfg/direct_mir_scalar_cfg_program_routine_admission_owner.pgy`
  - every instruction refusal (`refusal = ...; break`) is recorded and
    admission resumes at the next instruction;
  - `block_reachability_identity` is recorded;
  - `collection_ownership_transition` (:433) is recorded and the routine
    keeps the fact it had before;
  - the `local_inventory` Die (:136) is recorded;
  - an invalid inout indexed-assignment fact set is recorded and replaced by
    an empty set, so the Die at
    `scalar_cfg/direct_mir_scalar_cfg_local_ref_plan_owner.pgy:65` is not
    reached.
- `direct_mir/scalar_program/direct_mir_scalar_program_expression_admission_owner.pgy:556`:
  a refused expression hands back a set marked valid. Without this every
  later expression of the program failed with `graph-sequence`: 14,330
  records on `mir_lower`, nearly all of them cascade.
- `direct_mir/scalar_program/direct_mir_scalar_program_collection_ownership_transition_admission_owner.pgy`:
  each of its 18 invalid returns logs its line.
- `direct_mir/scalar_cfg/direct_mir_scalar_cfg_value_type_owner.pgy`:
  - `DirectMirScalarCfgValueTypePlanRejected` (:28) records instead of dying;
  - a parameter with a non-value carriage that is rebound in the body keeps
    its declared type.
- `direct_mir/scalar_cfg/direct_mir_scalar_cfg_program_admission_owner.pgy:37-44`:
  the extension-fact Die records every expression node that is not ready.
- `direct_mir/scalar_cfg/direct_mir_scalar_cfg_graph_plan_verification_owner.pgy:15-25`
  (`program_readiness=N`) runs a full readiness census instead of returning
  the first code. It reports codes 1-29 per block, operation, expression,
  local and node. Per-item attribution covers codes 22, 26, 27 and 28.
  - The census goes on to emission only when the extension fact is ready.
  - The O(n²) code-11 use scan in
    `scalar_cfg/direct_mir_scalar_cfg_program_extension_admission_owner.pgy:195-200`
    is replaced by one pass with the same verdict.
- `owned_local_lifetime_owner.pgy:382,385`, through
  `owned_local_call_carriage_owner.pgy:86,159`: the `lent-release-leaves` and
  `lent-beside-inout` refusals are recorded per call.
- The readiness re-check Dies are bypassed on the census path:
  - `scalar_program/direct_mir_scalar_program_projection_owner.pgy:45`;
  - `scalar_cfg/direct_mir_scalar_cfg_llvm_emission_owner.pgy:30`;
  - `scalar_cfg/direct_mir_scalar_cfg_program_llvm_artifact_emission_owner.pgy:16`.
- `direct_mir/scalar_program/direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy:104,108,125,158`:
  the four emission-time `Array<String>` cleanup refusals are recorded and
  emission continues without that drop. Its two replacement Dies (:212, :224)
  were not converted and were not reached.
- `driver/driver_compiler_clone_route_owner.pgy`: each uncarried landing is
  logged with its callee.

Limits:
- A refusal is not undone. Later records in the same routine can be its
  consequences, so the tables count routines, and say when a cause is the
  routine's only one.
- Readiness codes 14, 23, 24 and 25 are reported plan-wide only.
- Emission was reached on `semantic` only. On `mir_lower` and `codegen` the
  extension fact is not ready (expression nodes below), so the plan-wide
  checks after code 23 and emission did not run.
- **The DRV-2 census was killed by the watchdog at 9.45 GB RSS after
  1,255 s,** during routine admission at routine 5,486 of 9,470. The stock
  driver never gets that far because it stops at the role declarations.
  Linear extrapolation puts direct MIR routine admission of the whole DRV-2
  MIR near 16 GB. That is an estimate, not a measurement.

Raw records: WSL `/root/tdm0/.tmp/dm0/census/<input>.out`. Grouping:
`<input>.groups.json` beside them.

## Result per input

| Input | routines | refused at the stages reached | not refused | stages reached |
|---|---:|---:|---:|---|
| semantic | 1,094 | 49 | 1,045 | all, through LLVM emission (8.48 MB IR) |
| mir_lower | 3,679 | 86 | 3,593 | route, admission, extension fact, readiness up to code 23 |
| codegen | 4,536 | 110 | 4,426 | route, admission, extension fact, readiness up to code 23 |
| driver_rung2 | 9,470 | 46 in the first 5,486 | n/a | route, then admission of 5,486 routines (killed) |

Each program is refused as a whole even when one routine is refused.
Program-level refusals:
- `semantic` fails the HashMap lifetime rule, code 27;
- `mir_lower` and `codegen` fail plan-wide codes 26 and 23 (owned
  `Array<String>` move digest);
- `driver_rung2` fails on its role declarations.

## Grouped refusals

In each cell, the first number counts the routines with the cause and the
number in brackets those for which it is the only cause.

| Group | Cause | sem | mir_lower | codegen | drv (partial) | Example routine | Refusing owner (`src/self_hosted/compiler/direct_mir/`) |
|---|---|---:|---:|---:|---:|---|---|
| coverage | HashMap builtin (`MapHas/MapGet/MapSize/MapKeys`) on a record-member receiver | 0 | 33 (32) | 36 (34) | n/m | `SemanticAstBodyTypeBundleFocusRecoveryReceiptDigest` | `scalar_program/direct_mir_scalar_program_hashmap_owner.pgy:105-107` (receiver must be Local or Parameter) |
| coverage | non-Void routine whose last block ends in `Die` and has no `return` (code 6) | 0 | 2 (2) | 16 (16) | n/m | `AbiLayoutCValueType` | `scalar_cfg/direct_mir_scalar_cfg_program_extension_admission_owner.pgy:105-124` |
| coverage | `ArrayStringPushOwned` on a record-member receiver | 0 | 7 (7) | 8 (8) | n/m | `SelfMirCfgAddBlock` | `scalar_program/direct_mir_scalar_program_collection_expression_readiness_owner.pgy:90-94` |
| coverage | inout `Array` parameter indexed-assignment fact not derivable | 0 | 5 (0) | 5 (0) | 5 (0) | `SemanticAstCollectionCompleteDefinition` | `scalar_program/direct_mir_scalar_program_indexed_assignment_fact_owner.pgy:66-188` → Die at `scalar_cfg/direct_mir_scalar_cfg_local_ref_plan_owner.pgy:65` |
| coverage | builtin `CompilerRetireArrayStorage` (a manual drop) / in-place `ArrayReverse` | 0 | 2+1 | 2+1 | 5+1 | `SemanticAstIndexedStringBorrowInvalidationStorageRetire` | `scalar_program/direct_mir_scalar_program_expression_admission_owner.pgy:275` |
| coverage | statement `ArrayPush`/`ArrayPop` on a nested member path, or a discarded `ArrayPop` | 0 | 2 (0) | 3 (0) | 3 (0) | `SemanticAstBuiltinArgumentRetentionRecordResolvedCall` | `scalar_cfg/direct_mir_scalar_cfg_program_routine_admission_owner.pgy:298` |
| coverage | parameter with a non-value carriage rebound in the body (`build: SelfMirRoutineBuild`) | n/a | 0 | 0 | 1 (1), 10 routines in the MIR | `SelfMirIntentExecutionRoutineAppend` | `scalar_cfg/direct_mir_scalar_cfg_value_type_owner.pgy:75-78`, `direct_mir_routine_parameter_set_fact_owner.pgy:106-109` |
| route-shape | HashMap lifetime rule: a program with HashMap locals needs a `Void` entry and no `Exit` (code 27); `Exit` sites | 20 (20) | n/e | n/e | n/e; the MIR has 64 `Exit` in 40 routines | `ParseFunctionObserved` | `scalar_program/direct_mir_scalar_program_hashmap_lifetime_owner.pgy:95-117` |
| route-shape | role declarations make the program unclaimed | 0 | 0 | 0 | program | (`compiler/authority_owner.pgy:89-179`) | `scalar_program/direct_mir_scalar_program_route_admission_owner.pgy:144-145` |
| ownership | collection ownership transition row not admitted | 0 | 31 (16) | 40 (23) | 45 (27) | `SemanticCallSpineViewFromGraph` | `scalar_cfg/direct_mir_scalar_cfg_program_routine_admission_owner.pgy:433` ← `scalar_program/direct_mir_scalar_program_collection_ownership_transition_admission_owner.pgy` (sites below) |
| ownership | `Array<String>` push of a call result that is neither owned nor copyable (code 26) | 4 (3) | 13 (12) | 10 (10) | n/m | `SeedDeclaredFunctionSignatures` | `scalar_program/direct_mir_scalar_program_collection_ownership_transition_plan_readiness_owner.pgy:118-133` |
| ownership | `Array<String>` local with no ownership row and no runtime producer (emission) | 16 (16) | n/e | n/e | n/e | `SemanticDiagnosticCodeKnown` | `scalar_program/direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy:150-158` |
| ownership | manual `ArrayDropStorage` binding not provably releasable (code 28) | 4 (3) | n/e | n/e | n/e | `AstSourceLocationFactsFromParserRows` | `direct_mir_array_storage_release_lifetime_owner.pgy:222-257` |
| ownership | owned local lent to a `ref` formal is released while the call result leaves | 3 (3) | n/e | n/e | n/e | `CheckCollectionMutatorReceiver` | `../owned_local_lifetime_owner.pgy:381-383` |
| ownership | `Array<String>` storage shared with another holder / constructor input with no hand-off fact (emission) | 2+1 | n/e | n/e | n/e | `SemanticAstSignatureTypeExpressionFactsMatchRows` | `scalar_program/direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy:103-109` |
| ownership | uncarried compiler Clone landings | 0 | 0 | 0 | 0 | none | `../driver/driver_compiler_clone_route_owner.pgy:39` |
| other | readiness codes 8, 9 and 11 (`DirectCall` definitions, unused expression rows) | 0 | 7 (1) | 8 (1) | n/m | `SemanticAstCollectionMemberMoveUsePublish` | `scalar_cfg/direct_mir_scalar_cfg_program_extension_admission_owner.pgy` |

Key: n/m = stage not measured on that input; n/e = check not evaluated
because the extension fact was not ready.

Routines per group:

| Input | coverage | route-shape | ownership | other | refused routines |
|---|---:|---:|---:|---:|---:|
| semantic | 0 | 20 | 29 | 0 | 49 |
| mir_lower | 56 | n/e | 43 | 7 | 86 |
| codegen | 75 | n/e | 50 | 8 | 110 |
| driver_rung2 (5,486 routines) | 19 | program | 45 | 0 | 46 |

The collection ownership transition refusals, by site in the transition
owner:

| Line | Reason | mir_lower | codegen | drv |
|---|---|---:|---:|---:|
| 192 | the routine's receipt plan cannot be built from the MIR rows (`DirectMirCollectionOwnershipRoutineReceiptPlanFromJson`) | 14 | 15 | 18 |
| 276 | owned-collection flow not derivable (`DirectMirOwnedCollectionFlowFrom`) | 10 | 15 | 15 |
| 231 | an ownership row's binding has no `Array<String>` local in the routine | 4 | 4 | 5 |
| 287 | the terminal state of a multi-block CFG is not derivable | 2 | 3 | 3 |
| 483 | the final retire claims are not consistent | 1 | 2 | 3 |
| 370 | the receipt disagrees with the expected effect | 0 | 1 | 1 |

No route-shape refusal from the two-routine or three-routine owners was
reached. The general scalar program route claims all four programs, and the
shape routes run only after it declines.

## Union against DRV-2

- 171 distinct routines are refused across the four inputs. 159 of them are
  in the DRV-2 MIR, and 152 of those fall in the 5,486 routines the DRV-2
  census reached before it was killed. That census recorded only 46,
  because it stopped during routine admission, before the extension-fact,
  readiness and emission stages.
- 37 of the 49 `semantic` refusals are routines of the DRV-2 MIR.
- Lower bound for DRV-2:
  - 159 refused routines (1.7%);
  - the role-declaration refusal;
  - the HashMap and `Exit` rule (64 `Exit` sites);
  - nothing is known for 3,984 routines. On `semantic`, emission added 19
    refused routines (1.7%) to what the earlier stages found.

## Estimated work, ordered by routines unblocked

Estimates are judgement, not measurement.
1. HashMap builtins on a member receiver: 34 routines on codegen where it is
   the only cause. Small to medium: add a member-path receiver to the
   HashMap node and its C and LLVM emission.
2. Collection ownership transition: 40 routines on codegen (23 only cause),
   45 on DRV-2. Large. It is the fact that slices S1 and S2 of the decision
   replace with `owned_value_facts` and `value-retire`. Fixing site 192, the
   receipt-plan reader, would be a smaller first step for 15-18 routines.
3. HashMap lifetime rule: 20 routines on `semantic`, but it refuses the whole
   program, and DRV-2 also has HashMap locals and `Exit`. Small to medium:
   release live maps on the `Exit` path, or treat `Exit` as an exit that
   leaks.
4. Non-Void routine ending in `Die`: 16 routines on codegen, each with no
   other cause. Small: count a `Die` statement as a no-return terminator,
   the way `Exit` is counted.
5. Emission cleanup of an `Array<String>` with no ownership row: 16 on
   `semantic`, with storage-shared and constructor inputs 19. Medium: the
   decision's origin and retire rows for literal and static `Array<String>`.

Then:
- 10-13 routines: owned or copyable call-result rows for code 26 (the
  decision's `fresh/lent/static/unproved` result row);
- 7-8 routines: `ArrayStringPushOwned` on a member;
- the role-declaration erasure (program);
- 5 inout indexed-assignment routines;
- 5 manual-drop builtin routines, which V1 deletes;
- 3 statement shapes;
- about 10 non-value-carriage rebinding routines.

**Distance from admitting the DRV-2 entry.** The causes found so far fall
into 12 families:
- 6 emitter-coverage gaps;
- 2 program-level shape rules;
- 4 ownership-fact families.

Nine of them look small to medium. The collection transition and the
`Array<String>` release rows are the ownership work the decision already
plans as S1 and S2.

What remains unknown:
- the last 42% of DRV-2 routine admission;
- every emission stage except on `semantic`.

Memory is also a blocker of its own: direct MIR admission of the DRV-2 MIR
passed 9.4 GB at 58% of the routines.

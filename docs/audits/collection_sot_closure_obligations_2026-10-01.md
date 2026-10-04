# Collection SoT closure obligations — 2026-10-01

Status: READ-ONLY PREPARATION. Source observations and proposed falsifiers only.
No compiler, fixture, parity, bootstrap, installed-driver, CI, or acceptance gate
was executed. This report does not change an owner, registry state, or active rung.

## Observation boundary

- Directive base: `5cf41dcf5a974d19fad42ae3b3fd7ace24035be7`; 67 starting dirty
  entries. Inspected HEAD: `d7abecfb76902368dee180ece99fc31a704f281b`; 66 dirty
  entries before this output. HEAD advanced while preparation was starting.
- Final observation after writing: the same HEAD, 69 dirty entries including
  this report and concurrent preparation outputs. Dirty count drift is not
  attributed to this agent beyond its one exclusive report.
- `d7abecfb` is the current receipt-free Clone retirement source change. It is
  landed implementation, not an absent feature and not an observed acceptance.
- The native collection fact, Pergyra member/verdict, function-table producer/
  release and semantic parity gate hashes below match the directive inputs.
  `driver_rung2_owner.pgy` and collection CFG-flow owner are already dirty;
  their working-tree bytes, rather than HEAD alone, define this observation.
- Registry line 110 remains `ACTIVE`, naming
  `semantic_collection_ownership_initialize_binding` as the native entrypoint.
  Current registry counts are 2 ACTIVE and 24 BRIDGE, including this family.
  The other 25 rows are passive dependency inventory, not additional work tracks.
- Existing September 30 handoff gives navigation and a prior installed mismatch;
  neither that binary identity nor yesterday's result was revalidated here.

## Objective card

Objective: prepare closure of the reached aggregate-formal String-element
lifetime seam while retaining real callable-table retirement. Priority: stable
identity and one owner, producer-to-last-consumer facts, reached bypass deletion,
negative ratchet, then current-source production parity. Existing fact owner:
Pergyra collection verdict, member identity/transition and receipt owners.
Production entrypoint: `pgy-self-driver --emit-mir-json-verified`; final consumers
are source-C intent/codegen admission and direct C/LLVM cleanup. Forbidden fallback:
OWNED from descriptor type, spelling, `inout`, readiness, native success, missing
identity, or a second cleanup inference. Integration owner is the existing main
chat; gate is `tests/self_hosted/parity/collection_ownership_semantic_owner.sh`
after the carrier prerequisite. All falsifiers below are unexecuted obligations.

## Current owner-to-consumer chain

1. Native binding admission calls
   `semantic_collection_ownership_initialize_binding` from
   `src/semantic/type_checker_ownership_let.c:539`; deep-drop admission calls
   `semantic_collection_admit_owned_string_drop` from
   `src/semantic/type_checker_builtins_stdlib_array.c:294`.
   `src/semantic/collection_ownership_fact.c:502` records function/binding/origin/
   source identities, element state and disposition. It distinguishes exact
   MapKeys, Clone, local binding moves and member moves; descriptor movement is
   not String-element ownership.
2. The Pergyra semantic producer is
   `SemanticAstCollectionOwnershipVerdictFromResolvedFacts`
   (`src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy:33`).
   `ast_collection_ownership_identity_owner.pgy` checks resolved callable kind,
   name, syntax ID and runtime ABI ID for builtins, rather than using a name alone.
   Member identity comes from
   `SemanticAstCollectionMemberMoveIdentityForNode`
   (`ast_collection_ownership_member_move_owner.pgy:18`). Its current root query
   resolves ordinary local bindings; it does not resolve a formal aggregate root.
3. Native rows pass through `hir_attach_collection_ownership_facts`
   (`src/compiler/hir_semantic_fact_projection.c:495`), MIR source-fact validation
   (`src/compiler/mir_branch_source_facts.c`) and
   `mir_json_emit_collection_ownership_facts` (`src/compiler/mir_json_dump.c:568`).
   Pergyra rows pass through `SelfMirCollectionOwnershipFactRowsAppendRoutine`
   (`src/self_hosted/mir/collection_ownership_fact_owner.pgy:219`) and
   `src/self_hosted/mir/json_projection_owner.pgy:385`.
4. The MIR input reader is `BuildMirCollectionOwnershipFacts`
   (`src/self_hosted/mir_lower/collection_ownership_fact_owner.pgy:203`). Its
   `MirCollectionOwnershipMemberSourceReady` at line 76 already resolves a local
   OR formal parameter source and exact declaration field identity/type. The
   member-move row requires UNKNOWN element ownership at lines 319–326. This
   existing identity validation is not missing formal-element lifetime proof.
   Do not add a second reader authority or promote UNKNOWN merely after an ID join.
5. Direct C and LLVM use the shared
   `DirectMirScalarProgramArrayStringCleanupDropSymbol`
   (`src/self_hosted/compiler/direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy:60`)
   and local-retirement query at line 87, consumed by the C/LLVM array-string
   cleanup owners. A transition row chooses storage/owned/no further cleanup.
   The same owner still contains the old expression-based fallback at lines
   80–84; see obligation O5 before treating the last consumer as migrated.

## Legitimate production lifetime that a refusal must preserve

`SemanticAstExpressionFunctionTableFactsFromArtifact`
(`src/self_hosted/semantic/ast_expression_environment_owner.pgy:256`) builds
`names`, `returns`, `params` from empty arrays using
`SemanticAstExpressionFunctionTables` and owned pushes for constructor, enum,
declared function and intent rows. `SemanticAstArtifactAnalysis` carries that
table (`ast_artifact_verdict_owner.pgy:55,174`). The value currently contains
`ok` and the three arrays; `Ready` proves row shape, not release authority.

`SemanticAstExpressionFunctionTableFactsRelease`
(`ast_expression_function_table_fact_owner.pgy:31`) extracts those three fields
from an `inout` formal, deep-drops each local, writes the retired descriptors
back and clears `ok`. Its comment says body analysis is the last consumer, but
current callers establish a longer source-C lifetime:

- `ast_initializer_type_fact_owner.pgy:438–449`: local producer, initializer
  derivation, then release.
- `compiler/driver_rung2_owner.pgy:176–187`: projection terminal analysis, table
  release and write-back. The MIR projection's materialized consumers must finish
  before this release.
- `compiler/driver_rung2_owner.pgy:359–384`: source intent admission inside
  `CodegenAdmittedCViewFromFactsOrDie` precedes release of
  `codegen_analysis.function_tables`. Moving this release to body analysis would
  violate the current consumer order.

Producer-owned evidence must survive return, aggregate storage, analysis
carriage, local extraction and the release formal. A hand-built owned table or a
borrowed formal refusal alone does not prove that this actual producer survives.
Failed/partial tables can also contain produced owned strings; `ok == false`
must not be confused with absence of a cleanup obligation.

## Auditable closure checklist

Every checkbox is pending closure evidence. Existing source coverage is recorded
separately and must not be read as a failed or passed execution.

| ID | Current source fact | Missing closure obligation, exact last consumer and minimum falsifier |
|---|---|---|
| O1 — formal identity | Member root uses the local-only identity owner. Verdict assigns `member-move` only when `moved.ok` (lines 148–163). | Carry the existing stable formal binding and declaration field identity from semantic production; never identify ownership by name. Last consumer: verdict, then existing MIR readers. Falsifier: by-value and inout `Bundle.values` formal extraction, plus a same-spelling wrong binding/field ID mutation. |
| O2 — element lifetime | Native UNKNOWN member-move deep drop returns true for a current parameter at `collection_ownership_fact.c:788–790`. Pergyra UNKNOWN guard at verdict lines 449–459 only catches member-move/call-result/binding origins; an unresolved formal member can stay generic UNKNOWN. This is a source-derived admission gap, not an executed result. | One Pergyra owner must prove or refuse producer-to-caller-to-formal field element lifetime, including storage-only versus deep release. Last consumer: `SemanticAstExpressionFunctionTableFactsRelease`, then C/LLVM cleanup. Falsifier: `tests/concept_semantics/hashmap/collection_parameter_member_move_deep_drop.pgy` and borrowed/inout controls; pair with the genuine FromArtifact producer and all three release callers. |
| O3 — fact carriage | Semantic/HIR/MIR rows, receipt carriers and local/formal field-ID readers already exist. Member-move remains UNKNOWN; exact ID joins do not certify elements. | Carry the new reached proof through the existing owner boundaries, preserving producer revision and function/binding/field identity. Last consumer: native MIR validation and both self-host projections/readers. Falsifier: remove or substitute one required source/binding/field fact, forge owned/retired state, or drop one push/drop receipt; readers must refuse before artifact replacement. |
| O4 — explicit missing-fact refusal | Local deep-drop/provenance and empty-array receipt checks exist; ordinary direct Array<String> formal requirements are separately scanned in `ast_collection_owned_element_parameter_requirement_owner.pgy`. They do not prove aggregate formal fields. | A missing aggregate proof must produce an explicit diagnostic without granting OWNED from `inout`, `ok`, type or builtin-like names. Last consumer: source semantic admission and direct backend plan admission. Falsifier: borrowed literal table passed to real Release, missing producer receipt, same-name user Clone/MapKeys/drop callable. No unsafe negative binary may run. |
| O5 — old cleanup read path | `DirectMirScalarProgramLegacyArrayStringLocalBorrowsElements` reconstructs literal/push behavior. When a transition row is absent, CleanupDropSymbol uses that test and defaults to an owned-string drop at line 84. | Establish the reached upstream admission boundary, then remove or negatively fence this old decision path for migrated facts. Last consumers: `DirectMirScalarProgramCStringArrayCleanup` / `DirectMirScalarProgramLlvmStringArrayCleanup`. Falsifier: remove a required transition row from an otherwise valid reached program; both routes must refuse, preserve the old artifact, and never choose a default deep drop. Reachable unsafe behavior is not claimed here because upstream readiness was not exhaustively traced. |
| O6 — bypass and authority migration | Native current-parameter exception remains; registry owner is native initialization. Structural Symbol-owner deletion ratchets are present in `tests/collection_ownership_fact_projection_smoke.sh:16–31`. | Delete the reached production UNKNOWN exemption only after the positive production cleanup survives. Show actual production C-path substitution by the existing Pergyra owner and migrate registry consumers; distinguish a deliberately bounded bootstrap oracle from a production second authority. A deleted filename, a fixture pass or structural inventory cannot prove substitution. Falsifier: absent Pergyra proof plus otherwise-successful native route must still refuse; ratchet the removed exemption/old read. |
| O7 — current production acceptance | Current semantic gate contains move, MapKeys, member/restore, Clone, forged receipt, HashMap alias and return-path checks. Its existence is unexecuted evidence in this review. | Main must record current source/seed/private/installed identities and observe the carrier prerequisite then semantic parity on native, self-host and public C/LLVM. Use the genuine table producer through initializer/MIR projection/source-C last consumer; verify post-release refusal and exactly-once cleanup. Old installed success/mismatch and a private slice are not source-current installed acceptance or CI. |

## Recent implementation that must not be reopened as absent

- Local binding move: `d2dfb686` and subsequent source/reader consumers carry a
  retired source and one live destination. Current `origin=binding` is allowed
  only with that exact relationship; yesterday's blanket alias refusal is obsolete.
- HashMap: `667f11ec` bounded lifetime and `1767e18a` routine-local release are
  present. `direct_mir_scalar_program_hashmap_lifetime_owner.pgy:21` rejects map
  parameter/return/opaque-alias cases and proves one MapNew in the owning entry
  block. C/LLVM HashMap cleanup owners consume this proof. The semantic gate
  contains alias-refusal and bounded return-path probes; normal fallthrough
  acceptance still needs an observed source-current check, not a new map rung.
- MapKeys: `0ff4a5dc` admits exact retirement; the semantic gate already asserts
  one moved-snapshot deep drop and one map drop in both targets (lines 480–507).
- Clone: `d7abecfb` now uses
  `DirectMirCollectionOwnershipReceiptFreeRetirementReady` to require one matching
  retired owned `clone` fact and one actual ArrayClone expression definition.
  The MapKeys branch is separately exact. General UNKNOWN/member/call-result
  retirement is not implied by this bounded allowance.

## Gate and progress boundaries

`tests/self_hosted_component_contract_smoke.sh` is structural inventory/old-path
residue only. The fact-projection smoke contains structural Symbol-owner deletion
and executable carrier checks; neither was run here. The carrier gate owns row
shape/refusal, and the semantic parity gate owns executable behavior. No document,
fixture count, route declaration or source file is SUBSTITUTING evidence.
Route-level world reachability must not be promoted to stage-level substitution.
No CLOSED claim is supported until migrated consumers, missing-fact failure,
old-path deletion, negative ratchet and current production evidence all exist.

## Inspected SHA-256 identities

```text
3B07A6A1867CACBF220FF06071D1E43A761335255CED65AB8EBCC3859110CBA3  docs/semantics/sot_owner_spine_registry.md
2C1FE4058BA60DD4300913D805E5A53A9D48B0D198EA5942EB463DE472DDAA39  src/semantic/collection_ownership_fact.c
3504AB775DB78CA434399CE5BCB9000501D92A15805EA68D1FD4B3DD51B9E02E  src/self_hosted/semantic/ast_collection_ownership_member_move_owner.pgy
CDD2DD93093A40BC64573093B2EB7E9C20269B838B6709529FD9C3B1F0C614F4  src/self_hosted/semantic/ast_expression_function_table_fact_owner.pgy
F3EDDEF784F5B08FFE2B5BB06742E1A2A456CC1430133376987C9EEB7E38E04F  src/self_hosted/semantic/ast_expression_environment_owner.pgy
85D206EE7ABBAEAB5B95B1F227B4EB063697847336007D0BE26CE265BD345B9D  src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy
530AB47EE71C034EC57AC4ABD49443F30FF3C09A60E4DBA0D5641068173116C1  src/self_hosted/compiler/direct_mir_scalar_program_collection_ownership_retirement_admission_owner.pgy
FD36CC85B6C84EDA485BD7025D974DBD4BC56A785A53AE6AB3B0F151CD1D58D8  src/self_hosted/compiler/direct_mir_scalar_program_hashmap_lifetime_owner.pgy
7A6F5F3718548590088435C94CADCC8368153CEEE5F902E4CE7D7F3C8C816D6D  src/self_hosted/mir_lower/collection_ownership_fact_owner.pgy
32AB6613979F18A6C7C9FB886A57E45E1C0AFA56429AF6333B0A70EC6D3E5229  src/self_hosted/compiler/direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy
9429C47E150CD57298682AE8EAF83AE884DECBCC4B500327B5B2B8EE0520C5B5  src/self_hosted/compiler/driver_rung2_owner.pgy
9F92945F17B4251523873BFC8567CFF63D98EF90D385673B31406CE51E7B6B3E  src/self_hosted/compiler/direct_mir_scalar_program_collection_ownership_cfg_flow_owner.pgy
66F4F4C065449E224705BD2F762082490120A0F032E597E5F0C91F4A8C634DF2  tests/self_hosted/parity/collection_ownership_semantic_owner.sh
```

Hashes are a bounded working-tree snapshot, not a guarantee against subsequent
concurrent edits. Main must recheck relevant hashes before integrating this packet.
Only this new report is edited by preparation A; no source, runtime, existing test,
registry, handoff, binary, shared temporary evidence, commit or push is changed.

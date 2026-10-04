# Vision-aligned owned-result integration review

Date: 2026-09-30 (Asia/Seoul)
Status: READ-ONLY REVIEW COMPLETE / implementation acceptance belongs to root

This review supports the single active collection-ownership P1. It does not
start native-C implementation, declare a new compiler fact family, or authorize
P2-P6/S0-S6. The integration directive is
`docs/agent_work_directives/pergyra_vision_owned_result_integration_2026-09-30.md`.
The authoring skill was applied to actual purpose, authority and evidence
lifetime, rather than keyword or file-size counts.

## Snapshot and evidence boundary

Observed HEAD: `7a9fe09d8293170c4ce676de6464486cd25ca348`.
At review start, vision/handoff were modified and earlier audits, directives
and the user-local conditional-push fixture were untracked. No existing file,
shared build cache, installed binary, semantic registry or compiler source was
modified by this review. Other agents may change this shared checkout later.

Source SHA-256 values at observation, before the implementation candidate:

| File | SHA-256 |
|---|---|
| `docs/00_vision.md` | CAA40E5F5C03A088117465A9800FC8601D77C91727F7002B5D7ADA9DFCEA177E |
| `docs/self_hosted/14_target_compiler_world.md` | B270F38629EC9CD4A81C34C0E8977FD055D0462629B2674B14430CFC1DC91DCE |
| `src/self_hosted/compiler/world.pgy` | 08D3913013D83FFFB2FA437C2C98F03F4E701A396A8B56BF2F4544D2A47F8EAD |
| `src/self_hosted/compiler/driver_rung2_owner.pgy` | 1CE67A4D01649A8A71AA471DFF7CC640D86AC46560C6ED204C263AE0269D4A5E |
| `src/self_hosted/compiler/direct_mir_scalar_program_owned_string_result_fact_owner.pgy` | 4B3D4508720ECC22EA78D4F1C9466F1D90BE4FB11F19C9576964463B78FC9DF6 |
| `src/self_hosted/semantic/ast_owned_string_result_fact_owner.pgy` | 62B9F4034C36B4123DA12EB065951B7A99C8F5AF0976E6466BCCD14D49C1E447 |
| `src/semantic/collection_ownership_fact.c` | 7310FEC8E9255B9BD8C7CA12CEEC5530E3EE0DA35740B02830373032CA6AA317 |
| `src/semantic/type_checker_func_decl.c` | 46D4E11FB263E9D971776D5B3FF1C09CA423F5473DC7F919D9BD5D949D764CBA |

Runtime probes deliberately use the existing copied audit packet, not the
shared installed sibling and not a fresh-HEAD candidate:

| Artifact under `.tmp/worldview_attack_20260930_6d75/` | SHA-256 |
|---|---|
| `pgy-pinned.exe` | 261504A560D9E6EAEC7C1C79A019C3B90F23578440505D3458CBDA510CE98F2E |
| `pgy-self-driver-pinned.exe` | E40DE66B33DA69CE6C398D091B5D714C19CC0B5AC25813682EDFCC2B85188094 |
| `pgy-self-driver-pinned.machine-layer-manifest.json` | 0A83B0DB5EFE3C00C6D9413C63045C4B17AFF079781213B280442C588E5A9C19 |

All three hashes were independently rechecked. Probe outputs were created only
under `.tmp/vision_integration_review_20260930/`. The scoped
`PGY_SELF_DRIVER_BIN` environment override was restored after each probe loop.
These are reproduced baseline defects, not proof that the new source is fixed.

## Main-integration findings

### F1 — reproduced: ownership depends on an unrelated routine

The smallest existing source is
`docs/audits/repros/owned_string_wrapper_2026-09-30.pgy`:
`Owned0` returns `Owned1()`, `Owned1` creates a fresh String using
`TextBuilderFinish(builder, AllocatorResult-local)`, and Main pushes then
deep-drops that String. The padded control changes only an unused function
after Main. No subject, action or resource Slot is needed for these pure
producer computations.

Independently rerun public routes on the pinned packet:

| Input | C | LLVM |
|---|---|---|
| wrapper, three routines | exit 1, `program_readiness=26` | exit 1, `program_readiness=26` |
| same wrapper plus unused tail | exit 0, exact expected output | exit 0, exact expected output |

Expected output is exactly `worldview-owned-string-ready`. The positive LLVM
leg emitted one target-triple override warning; this is not a warning-free
claim.

Source cause at the observed hash:

- `direct_mir_scalar_program_owned_string_result_fact_owner.pgy:62,130`
  constrains depth by the total routine count.
- Expression/local traversal and callable-body traversal both increase depth.
  This wrapper reaches depth four but only has three routines. An unrelated
  fourth routine therefore changes the verdict.
- `direct_mir_scalar_program_collection_ownership_transition_plan_readiness_owner.pgy:54`
  asks the body proof again for a direct-call push.
- `direct_mir_scalar_cfg_program_extension_readiness_owner.pgy:213` maps that
  failure to code 26, and `direct_mir_scalar_cfg_graph_plan_verification_owner.pgy:22`
  renders a general plan-identity error without the producer/source position.

Integration condition: acyclic wrappers, declaration reorder and unrelated
routine insertion must agree, while local/callable cycles, borrowed producers
and forged exact-source facts still refuse. Raising a routine-count multiplier
is not evidence of cycle handling or semantic stability. A traversal-wide
visited set must not reject a lawful shared DAG/callee merely because another
return path previously visited it; active cycles and completed facts are
different states.

### F2 — reproduced, separate scope: native forward-summary order defect

Native C and LLVM were independently rerun using the pinned launcher and the
existing `owned_2_forward.pgy` / `owned_2_reverse.pgy` audit fixtures:

| Declaration order | Native C | Native LLVM |
|---|---|---|
| wrapper before fresh producer | exit 1, MIR invalid-state-transition | exit 1, MIR invalid-state-transition |
| fresh producer before wrapper | exit 0, exact expected output | exit 0, exact expected output |

Both forward failures report `0 error(s), 0 warning(s)` before the MIR
collection-ownership refusal. This is not a semantic diagnostic parity pass.

`collection_ownership_fact.c:93-104` requires the exact callee symbol's already
recorded `BODY_SUMMARY_RETURNS_OWNED_STRING`. Its structural function summary
is recorded once from `type_checker_func_decl.c:394-396`. The caller checked
before its fresh callee consequently lacks the owned-return summary and is
not revisited. In contrast, the Pergyra semantic producer in
`ast_owned_string_result_fact_owner.pgy:169,188` grows exact callable facts to a
fixed point. The declaration-order dependency is therefore a real independent
oracle discrepancy, not a reason to retry production compilation natively.

Main must not interpret expected native-forward refusal as failure of the
self-host repair, silently pad/reorder fixtures, or claim full native/self
parity. Native implementation was intentionally left untouched; its exact
smallest falsifier and source cause are reported for the owning integration
task to schedule within P1.

### F3 — static structural gap: admitted facts are still re-proved

This is a source observation and cost candidate, not a measured bottleneck.

The semantic producer computes exact owned-result facts once, but the
direct-MIR owner derives the body's ownership again. External MIR must be
validated at its trust boundary; simply trusting an owned tag or String type
would be unsafe. The gap is that the same admitted GraphPlan's downstream
consumers repeatedly perform the complete readiness operation:

- plan sealing calls `DirectMirScalarCfgGraphPlanVerified`;
- verification computes the digest and invokes `GraphPlanReady`;
- the projection driver invokes `GraphPlanReady` again;
- C or LLVM emission invokes `GraphPlanReady` again;
- readiness recomputes the digest and runs program-extension checks, including
  the owned-result proof.

The exact call sites are `direct_mir_scalar_cfg_graph_plan_seal_owner.pgy:72`,
`direct_mir_scalar_cfg_graph_plan_verification_owner.pgy:14-15`,
`direct_mir_scalar_program_projection_owner.pgy:39` (multi-routine path),
`direct_mir_scalar_cfg_projection_owner.pgy:37` (single-routine path),
`direct_mir_scalar_cfg_c_emission_owner.pgy:27`,
`direct_mir_scalar_cfg_llvm_emission_owner.pgy:30`, and
`direct_mir_scalar_cfg_graph_readiness_owner.pgy:26,233`.
The digest-mutation self-check also calls readiness but fails at the digest
guard; it is not counted as a full repeated ownership traversal.

The current fix can remove the erroneous semantic bound without closing the
vision's admitted-proof reuse obligation. A future reached P1 consolidation
should distinguish external admission from consumption of immutable,
revision-bound owned evidence. No cache, general query engine, new authority
row or second implementation track is proposed by this review.

A concrete next cost falsifier is an acyclic wrapper DAG where each function
has two terminal return arms calling the same next wrapper. The current body
walker independently proves both arms, so proof-call expansion can double per
layer although distinct routine/return-node input grows linearly. This is a
static recurrence observation, not a measured benchmark or a newly compiled
fixture. Start with a small bounded 6/10-layer comparison, count proof calls,
and reject further expansion before proposing cache/worker/budget growth.

## Vision target and reached production path

Observed execution organization:

| Vision responsibility | Current reached owner / remaining gap |
|---|---|
| Purpose composition | `driver_bootstrap_main.pgy:13` constructs one `PgyCompilerWorld`; world has four real route zones at `world.pgy:282-285` |
| C artifact purpose and failure | `world.pgy:338` calls `CompilePergyraCArtifact`; `DriverSourceCExecution.Compile` admits input, commits payload and records typed outcome |
| LLVM artifact purpose and failure | `world.pgy:361` calls `CompilePergyraProgram` and checks its stored typed outcome against intent completion |
| Revision-scoped facts | Source-to-C still serializes MIR then re-admits JSON at `driver_rung2_owner.pgy:482-495` |
| Fact-directed projection | Claimed scalar/nested direct-MIR slices bypass reconstruction; the general C path still rebuilds AST text and semantic analysis at `driver_rung2_owner.pgy:278-313` |
| Actual stage admission/sealing | `world.pgy:16-63` stage actions return readiness only; they are not world members and do not close stage transitions |
| Unified artifact transaction | Real commit/rejection exists, but remains route-organized rather than one target-parameterized transaction zone |

`CanonicalizeMirJsonVerified` similarly reconstructs a tree/artifact and
performs expression-graph/semantic analysis at
`canonical_mir_execution_owner.pgy:39-74`. Canonicalization is a distinct
external operation; its existence must not disguise source-to-C's repeated
internal reconstruction.

The existing C artifact intent has a legitimate publication purpose despite
having one action. It should not be removed on action-count grounds. Pure
owned-result proof remains `func`/`struct`; making it a new subject/action/zone
would create ceremony without a resource or authority transition.

## Grade and acceptance boundary

- `SURFACE`: declared lexer/parser/semantic/MIR stage topology and readiness
  actions. Syntax presence is not stage dogfood evidence.
- `REACHABLE`: the one world, four production route zones and genuine
  compile/publication action/intent paths. They are not the final
  revision/target/transaction world shape.
- `SUBSTITUTING`: existing installed public C/LLVM routes have separately
  documented bounded native-bypass replacement evidence. This review ran
  their pinned packet to reproduce defects, but did not freshly rebuild,
  republish or re-admit their fixed-point proof. The proposed local repair does
  not create a new whole-compiler substitution count.

`semantic.hashmap_collection_ownership` remains ACTIVE and still registers the
native bootstrap owner; `selfhost.semantic_artifact_admission` also remains
ACTIVE. The repair alone does not consolidate five shape rows into
`collection_program_plan`, move registered authority, prove the complete
35-cell obligations, remove every production old path, eliminate MIR/AST
reconstruction, unify route transactions, or implement Check/Format/Debug.

The active handoff still names checkpoint `035d621a`, while live HEAD was
`7a9fe09d`; root must refresh navigation from current executable evidence.
The dirty vision's final historical current-state/deferred-self-host prose
also needs careful reconciliation with its revised compiler-world section.
Neither inconsistency permits replacing current source/registry evidence with
document claims, and this review did not edit those shared documents.

## Executed checks and omissions

- Read AGENTS, the complete authoring skill and all five routed references,
  coordination directive/README, vision, target contract and active handoff.
- Inspected the reached world/driver path, semantic producer, direct-MIR proof,
  readiness/projection consumers, native summary and registry rows.
- Rechecked source and pinned-packet hashes; independently executed eight
  baseline runtime legs and observed each exit/output/refusal above.
- A source-search invocation used Windows-incompatible literal wildcard paths
  and exited 1 after the runtime loop; searches were rerun using `rg -g`.
  The enclosing shell exit is not a suite PASS claim.
- No fresh source build, new-candidate acceptance, full matrix, fixed point,
  sanitizers, remote CI, performance benchmark or installed publication was
  performed. Root owns the fresh isolated candidate and independent focused
  composition/compatibility acceptance.

Only this report was added. Scratch executables are diagnostic outputs, not
new language/compiler authority.

During this review, an external concurrent task changed the direct-MIR owner to hash
`B2DB3A6E2BB25BAB2CDE4DB65ECA70658CF5A4FB9D8CA143F0123A1A43D08A80`
and also edited the existing acceptance gate and a wrapper fixture. The root's
implementation agent did not apply its competing proposal and switched to
read-only review. Our test agent added a separate composition gate and fixtures;
root has not attached that gate to the externally edited entry point. This
report's old-source line numbers and pinned runtime failures deliberately
remain baseline evidence. Their acceptance belongs to the root's fresh
candidate, not to this report. `git diff --check` passed on that shared
snapshot; the new report was separately checked for trailing whitespace.

The candidate diff separates callable-hop depth from local-expression depth;
only their corresponding transitions increment them. Unlike the original
mixed-unit bound, a cardinality bound for each finite graph family can admit
every well-formed acyclic traversal independently of unused padding while
terminating cycles. This is a reasonable narrow repair candidate, pending
root's executable falsifiers. It still has repeated recursive proof rather
than active-cycle/completed-proof state or admitted evidence reuse; no broader
closure or performance improvement is inferred.

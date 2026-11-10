# Mutable bindings v12: bounded verification and integration hold

Observed 2026-11-08, HEAD `60fd1e26d8ea3987efbcbfc65f59b892b234701a`,
with the existing dirty/staged tree preserved. No staging, commit, push or
installed-binary replacement occurred in this lane. This record describes the
binding candidate, not main's separate frozen V.001 candidate.

## Fixed identities and commands

WSL distribution: `Ubuntu-C-WSL`. Native root:
`/root/pgy-binding-20261108-okx3hp09`.

- Native `bin/pgy-native-gate-v12`, SHA-256
  `b5e5a8db11c3b13d4864fc999fdbce705653d77cb1662f7f0001a4ca174db5d5`.
- Typed probe `/root/pgy-binding-slice-20261108/.tmp/semantic-binding-v4`,
  SHA-256 `53648359c041b41409c01161778b638bbcc40286403ad7c9d36f4730fed7180a`.
- Evidence directory: `D:/PergyraLang/.tmp/compound-assignment-20261108/evidence/`.
- Full-input command: pinned v12 `--native-pipeline --emit-c
  src/self_hosted/compiler/driver_rung2_main.pgy -o .tmp/allocator-retention-full-v1.c`.
  Exact command, source hashes/archive and logs: `allocator-retention-full-v1/`.
- The native source manifest is in `native-v12-integration/build-source.json`.
  Earlier v10/v11 binaries included scratch no-store diagnostic tracing despite
  restored source timestamps. v12 explicitly rebuilt those restored owner files;
  earlier results remain identified by their actual binary hashes.

## Observed gates

| Gate | Result | Evidence subdirectory/file |
| --- | --- | --- |
| Binding/compound execution and refusal, C + LLVM | 148/148 | `native-v12-integration/bindings/` |
| Parser, semantic, transpile units | PASS; semantic 3394/0, transpile 1035/0 | `native-v12-integration/` |
| Region-retention unit and generated projection | PASS | `native-v12-integration/registry-*.log` |
| TextBuilder owner and prior-diagnostic isolation | C/LLVM PASS | `native-v12-integration/builder-*.log` |
| Original nonowner, builder and allocator cases | 36/36 unchanged admissions | `native-v12-integration/result.json` |
| Actual String-copy and local-collection release | C ASan/UBSan, LLVM LSan PASS; cleanup-deletion mutants detected | `native-v12-integration/string-release.log` |
| Disjoint parallel Slice controls | PASS | `native-v12-integration/parallel.log` |
| Six registry generators | drift checks and C/LLVM executions PASS | `generator-closure-v3/` |
| Actual self-host typed body admission | 70/74 | `self-semantic-v4/` |
| Original ownership admission census | 920 checked, eight differences | `ownership-pair-census-v3/` |
| Full compiler input | FAIL: 20 errors, 24 warnings | `allocator-retention-full-v1/` |

The native integration subset ran with `PGY_VERIFY_FLOW_CURRENCY_MEMO=1`.
The original admission census checks acceptance, not all of the shell gate's
MIR assertions. Its unchanged expectations were not rewritten.

The full input's first final error is `world.pgy:333`, `CompileSourceToC`:
required return-backing relation incomplete or stale. The other final errors
include storage escape/stale-use reports in source admission, MIR projection,
identity epoch rebinding, expression graph verification and driver fixture args,
followed by the generic summary-snapshot allocation diagnostic. The run is not
evidence of OOM: GNU maximum RSS was 2,637,672 KiB and exit status was 1. GNU
wall time was 14:10.20; the Python timing field is separately 790.266 seconds.
No generated compiler, installed-driver execution, gen2 == gen3 or CI result
can be claimed from this failed full input.

## Changes exercised

Native parser facts now carry `var`/`let mut` equivalently and numeric compound
operations through assignment MIR to C/LLVM. Exact declaration permission is
checked for rebinding, aggregate writes, readonly parameters, inout calls and
captures. Place and old-value evaluation precede the RHS. Existing event
subscription behavior remains covered on native C/LLVM.

Slice aliases, rebindings, loop backedges and nested scope escapes preserve
backing permission; an unproved returned/projected Slice cannot grant a write.
The typed self-host owner now also rejects indexed-store backing changes in
RHS/index expressions, including formal/member/nested-call cases, while readonly
and disjoint controls pass. Its default-formal precision remains conservative;
this direct-place gate is not an alias/lifetime completeness claim.

Empty-array nonowner receipts still reject real replacement/escape histories.
An exact independent Clone rebinding selects the existing String landing,
which must be reproved in the next analysis generation. The original
discarded-transfer negative remains refused.

`TextBuilderAppend` receiver and `AllocatorDestroy` retention rows now say that
their descriptor addresses are not kept after their synchronous calls. Runtime
implementation was checked; single-owner, consumption and allocator-lane loan
checks remain separate. Pool use after destroy and outward pool loans still
refuse. These rows do not by themselves repair the full return-backing failure.

Changing declarations in six generated projections were migrated in the
renderers themselves and regenerated. Compiler-resolved declaration migration
receipts are `migration*.json`, `parallel-migration.json`,
`ownership-fixture-migration-v1.json` and `unit-value-migration-*/result.json`.
Some receipts explicitly say the original census was incomplete; the final
full-input result above is the integration evidence, not those interim counts.

## Remaining falsifiers, without changed goldens

Five original negative inputs now admit:

- `aggregate_mutable_local_direct_member_owned_push_invalid`
- `aggregate_result_mutable_root_field_push_invalid`
- `direct_member_default_mutable_root_invalid`
- `aggregate_member_landed_string_push_mutable_carrier_invalid`
- `aggregate_member_mutable_local_chain_owned_push_invalid`

All five were compiled and linked on C/LLVM with v12. Eight of the ten sanitizer
runs report leaks (`mutable-admission-runtime-v1/`). The first C case leaks
22 bytes in four allocations; its emitted code allocates the struct's Array
and Strings but has no corresponding exit cleanup. The current local-collection
MIR pass enumerates local collection facts/registered resource rows, not an
owned field cleanup path for that nominal root. Native admission is therefore
not proof that aggregate automatic cleanup is complete. Two LLVM runs without
a leak report are not a general cleanup proof, particularly under optimization.

Three original positives now refuse writes through `using` aliases:
`return_loan_using_alias_opaque_inout_valid`,
`return_loan_mutated_literal_two_aliases_valid`,
`return_loan_two_alias_assignment_valid`. A permission-only experiment made
C/LLVM update a copy instead of the original, and was reverted. Alias identity
must be transported through MIR and both emitters before granting that write.

The four self-host admission differences are `nested` (parser rejects nested
assignment), `event` (`OnHit` undefined), `mutable_capture` (lambda initializer
type unresolved), and `string_field_copy_set` (member use after move). The probe
executes `ParseRootProgramArtifact` -> `SemanticAstArtifactAnalyzeTyped` ->
`SemanticAstBodyTypeBundleFromAdmittedAnalysis`; it is not an installed driver.

## Patch handoff

Shared edits remain uncommitted. Review bundle:
`D:/PergyraLang/.tmp/compound-assignment-20261108/handoff-v12/`.
`review.patch` is a delta against the initial dirty source archive, not Git HEAD.
Its manifest records each baseline/target hash and lists mixed shared-owner
files requiring a merge with main's concurrent work. Do not apply it wholesale
or stage this candidate as a completed V.001 implementation. Main's publication
lane retains ownership of installed-driver, bootstrap and CI integration.

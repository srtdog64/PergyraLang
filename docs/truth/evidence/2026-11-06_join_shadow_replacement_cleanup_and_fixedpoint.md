# Join-shadow and installed String-array replacement

Frozen run record, completed 2026-11-06 KST. This records the tested R8
generation, not every change in the concurrent main checkout.

## Subject and identities

- Shared HEAD: `60fd1e26d8ea3987efbcbfc65f59b892b234701a`; published
  `origin/main` was still `39a8fc1db0dc7162ba75ac7f9584f4cf096d8efe`.
  The shared tree retained 535 tracked changed paths and untracked work.
  This lane staged, committed and pushed nothing.
- Fixed source: `/root/pgy-join-driver-20261105/r8`.
  Input ledger: `/root/pgy-join-driver-20261105/r8-inputs.sha256`, SHA-256
  `2f9b650f6c55c8810c14a798c4d4b0d706ca049892aa4bbea70a0e0dfa4cf246`.
  The ledger passed before/after integration.
- Native: `/root/pgy-join-driver-20261105/source/bin-join/pgy`, SHA-256
  `c0e0a6d89e44580a80e09c688bba316d124589babd1b9ddc7aa05c339cfeacab`.
- Normally installed Pergyra-produced driver: `r8/bin/pgy-self-driver`,
  SHA-256 `4ccfe9e2c1ad2522f231541d48a42ca8bae3e7f0aae4d9bc8da2f9c493df9cf5`.
  Native/installed executable hashes were unchanged after the run.
- Installer receipts and generated C remain under
  `r8/.tmp/self_hosted/compiler/bootstrap/`.
- A separately frozen corrected regression harness, `.tmp/callee-identity-final.sh`,
  SHA-256 `f82d2c4d3e714e15e963ad9aa3901864bc623146e9bb5ba0271afa3c31d0783e`,
  used `PGY_TEST_ROOT` to read the unchanged R8 production/test inputs.

## Repair and fact chain

The original Join-shadow "negative" safely appends an owned copy through an
inout descriptor. Its old program is preserved as the restored positive;
the negative now forwards storage to an opaque declared `Keep`. The native
owner refuses that real unproved retention. A scratch name-only Join exemption
falsely admits it, while the normal owner refuses it. No production name
exemption or safety relaxation was added.

The installed source route originally refused intermediate blocks of an Int
entrypoint as if each needed a terminal return. The route classifiers and
readiness now consume the existing Void/Int entrypoint contract, preserving
unsupported signature and forged terminal-return refusals.

Once admitted, the old installed emitters leaked the first iteration's
descriptor and two Strings: 36 bytes on both C and LLVM. The existing
collection-transition issuer now carries borrowed/pure-empty literal rows
and a digest-covered `replacement_modes` facet. Its owner checks the typed
definitions of each local once and issues storage-only or owned-element
replacement authority when the lifetimes agree. Both emitters evaluate the
fresh RHS, retire the previous descriptor using that fact, and publish the
fresh value. Existing final cleanup retires the last value. This contract
does not assert immediate lexical loop-exit cleanup.

The consumer uses the admitted routine/local row. Reconstructing a
`declaration:id:0` string incorrectly excluded legitimate minimal SSA wires;
that duplicate decision was removed. The exact row query retains its identity
behind one owner, with duplicate/unequal-vector refusals. The fact file stays
within its existing 180-line cap; GraphPlan schema is v82.

## Observed gates

| Gate | Result |
| --- | --- |
| Installed replacement gate | PASS, C and LLVM |
| Original Join | each 6 allocations / 6 frees / 0 live, 72 bytes freed, peak 3 |
| Early return, false loop, repeated static literal | each 4 allocations / 4 frees / 0 live, 128 bytes freed, peak 2 |
| Suppressed emitted release calls | LSan finds leaks on C and LLVM |
| Premature emitted C release | physical observer rejects peak 1 instead of required peak 2 |
| Mode/identity controls | valid modes accepted; forged/missing mode and conflicting identity controls rejected on native C/LLVM |
| Retention / unknown replacement / missing owner / unsupported Main | refused without an artifact on both installed backends |
| Existing native loop cleanup | PASS, preserved safe/unsafe cases on C/LLVM |
| Ownership fact carrier | PASS, native/self producer-consumer parity and 54 malformed rows refused |
| Existing callee ownership negatives | 11/11 refusals preserved, including preservation of an existing output on failure |
| Existing explicit entrypoint return | PASS, C/LLVM cleanup and forged-return refusal |
| Original call-argument bootstrap regression | 2 executions and 12 pre-emission refusals PASS |
| Nominal array bootstrap regression | 6 executions and 8 pre-emission refusals PASS |

Physical evidence:
`r8/.tmp/self_hosted/collection-replacement.jav3byoo/`;
result SHA-256 `1851618812c7b7ecd7c52b217ecef254a24c10001d06a9b7f3fbd4eb77237413`.
Native loop evidence: `r8/.tmp/loop-local-collection.wSDrlK/`.
The normal native proof/bootstrap builds reported existing unreachable-code
warnings (15 for the mode probe, 17 for codegen gen0); no warning-free claim.

## Fixedpoint and installed artifact connection

The unchanged repository codegen bootstrap ran on the full import-composed
`src/self_hosted/codegen/main.pgy` input, in its explicit fixedpoint-only mode.
`gen2.c == gen3.c`: 117965 lines, both SHA-256
`3493568f56645a329a18f9bcc777cced27599bd16d0139dae7678afdc04b1fbb`.
Current gen2 executable SHA-256:
`0d75a9b4cc359314f36e6e449140a90cbfa0292b3c9082ce3d94ab7c71f51914`.

That codegen program has a separate import graph from the changed direct-MIR
driver owners. Therefore its fixedpoint is not called a driver fixedpoint.
To join the artifacts, current gen2 emitted the complete
`src/self_hosted/compiler/driver_bootstrap_main.pgy` again. Its normalized C
was byte-equal to the C actually used by the normal installer:
`40f2c6e83b1abcc12467ddc6a44ea05a23612f3e30492c966ece6de6da3ae3be`.
No redundant relink was needed for identical source/runtime/toolchain inputs.
Evidence: `r8/.tmp/driver-source-equivalence/result.json`.

## Open obligations and main reflection

- The full native driver input remains RED: status 1, 207 errors, no C artifact.
  The first three are `machine_request` passed from a ref boundary to default
  getters at `driver_mir_c_protocol_owner.pgy:97-99`; they are followed by one
  publication failure and 203 missing String-receipt diagnostics. The count
  and primary failures match the earlier generation. No rule was weakened.
  Full **driver** gen2/gen3 and same-commit required CI are not established.
  Evidence: `r8/.tmp/full-native-driver-check/result.json`.
- The callee-identity regression's complete gate remains RED. Three existing
  fixtures execute on C/LLVM and all 11 negatives pass. Its real-join fixture
  imports `SubContainsWithLen`, absent from the general expression projection.
  The exact MIR fails at builtin-call node 3/global row 13 in both baseline R1
  and R8. This pre-existing support gap was not hidden by changing the fixture.
  Its test runner's separate stale panic-symbol link guess was removed;
  LLVM links the canonical runtime once per gate.
- Main native SHA `afeca8529410906fa0c626b14cd4ae81edc161bac7a0d3516248699f45814376`
  now emits source-binding identities and two scalar kind-3 retirements for
  the raw/View branch case. This confirms native integration only.
- Main made a concurrent admission variant that issued modes before final
  operation states and rejected inactive literal mode 0. Installed main SHA
  `9c638e38839166f3a403cc84bfba70df60665ad0db86f0c915f0e763e5019d79`
  actually refused the existing `first_callee` positive. The variant was
  preserved as `.tmp/join-shadow-closure-20261105/main-variant-before-restoration.patch`.
  Only those shared-source hunks were restored to tested R8; other main work
  and its live prefix were preserved. Main needs a fresh rebuild/verification.
- Installed raw/View source parsing/indexed writes and native retirement-edge
  consumption remain distinct open paths from this String-array replacement
  repair. Public raw pointers, whole-language memory safety, V1 publication
  and CI closure are not asserted.

Plan: [bounded chain and falsifiers](../../agent_work_directives/join_shadow_installed_cleanup_fixedpoint_2026-11-05.md).

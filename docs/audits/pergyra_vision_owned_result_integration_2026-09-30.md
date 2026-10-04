# Vision-aligned owned-result integration evidence

Date: 2026-09-30 (Asia/Seoul)
Status: FOCUSED INTEGRATION VERIFIED — bootstrap pressure OPEN, not P1 closure
HEAD: `7a9fe09d8293170c4ce676de6464486cd25ca348`

## Scope and responsibility

The user authorized vision-aligned self-host compiler implementation and
multiple subagents. Root fixed the objective card first and delegated one
implementation track, independent tests and read-only vision/integration review.
Coordination: `docs/agent_work_directives/pergyra_vision_owned_result_integration_2026-09-30.md`.
Review: `docs/audits/pergyra_vision_owned_result_integration_review_2026-09-30.md`.

The authoring skill kept pure ownership proof in `func`/typed facts rather than
adding ceremonial subjects, actions or zones. Real route composition and
artifact transitions remain with existing world/action/intent owners. This
slice addresses semantic invariance within active collection-ownership P1;
it does not start P2-P6, S0-S6, or change a registry authority.

Before our implementation agent wrote its candidate, an external concurrent
task changed the same direct-MIR owner and existing acceptance script. Our
implementation agent stopped and reviewed that patch instead of overwriting it.
The compiler patch and `direct_mir_owned_string_wrapper_result_push.pgy` belong
to that external work. Our changes are the new regression gate, seven fixture
programs, narrow Makefile registration, reports/directive and handoff note.

## Exact packet and source evidence

Scratch: `.tmp/vision_owned_result_6e732613/`.

| Item | SHA-256 |
|---|---|
| direct-MIR owned String result source | B2DB3A6E2BB25BAB2CDE4DB65ECA70658CF5A4FB9D8CA143F0123A1A43D08A80 |
| pinned public/native launcher | 261504A560D9E6EAEC7C1C79A019C3B90F23578440505D3458CBDA510CE98F2E |
| baseline installed driver copy | E40DE66B33DA69CE6C398D091B5D714C19CC0B5AC25813682EDFCC2B85188094 |
| pinned codegen seed | DFBC278129C8398AF3FA9EBF272C052837F3946A901ACD7925E79F2F70D50D61 |
| fresh native-bootstrap candidate driver | 4AAD9E879AA302080F8A744665CADB016ED79C4E1E507AAA1529C07757F2EC59 |
| fresh emitted candidate C | 7B5C69AEE9FCD77ECF36B4D80F177280887554E26D9959BC76FB2B262E933F50 |

The copied codegen seed matches its existing bootstrap artifact receipt. Its
whole-driver source emission timed out at 300 seconds, exit 124, without C.
No timeout/memory increase or fixed-point completion was inferred.

Root then emitted the complete candidate from current Pergyra source using the
explicit native bootstrap oracle and compiled it through the existing emitted-C
`test` profile (`-O0 -fwrapv -fno-strict-aliasing`). This is a native-bootstrap
functional candidate, not a freshly proved Pergyra-built or installed driver.
No `bin/` file was replaced. Existing copied machine-layer declaration accompanies
the candidate; its validation does not grant new machine capabilities.

Whole self-host source graph fingerprints before/after native candidate build:
`0a0beae27f560552e7c3394639f03fe49e1b2c786aa89bf9c2f9d5ece44c5a3f`.
Both match. The build exited 0 in the observed 170,174ms probe and emitted
48,190,331 bytes of C. Native emission reported 0 errors and 4 warnings:
unreachable statement and three redundant intent-step clause warnings.

### Build pressure limitation

Root-only process-tree sampling excluded MSYS2-reparented native workers.
Its reported tiny private-memory peak is **not** valid whole-build evidence.
Separate exact-process observations saw the Pergyra seed at about 2,570MB and
the native emitter at 3,730.5MB private memory, above the 3,072MB guard. The
latter is a lower bound on peak, not a continuously measured maximum. When
root checked the exact native PID/path for cancellation, it had already exited
and emitted C; no unrelated process was stopped. Host C compilation completed.

Functional use of that diagnostic packet does not clear the pressure violation.
Do not expand whole-driver/bootstrap work or raise memory/time allowances on
this evidence. The timed-out seed and native-memory observation must remain
visible to the main integration owner; owner-directed repeated work must be
identified before declaring bootstrap/performance acceptance.

## Candidate behavior and integration gates

The external patch separates callable-hop depth from local-initializer depth.
Only each corresponding edge increments its counter; function entry resets
local-expression depth. Finite graph bounds now count the right units. This is
not an arbitrary multiplier increase. The file remains 160 lines / cap180 and
both external consumers start callable depth0.

New gate: `tests/self_hosted/parity/direct_mir_owned_string_result_composition.sh`.
It checks minimal/reordered/unused-tail/long-chain/local-alias/shared-callee
positive programs. The unused tail preserves original callable and receipt
identities. The shared-callee case executes both terminal return paths.
Invalid controls include borrowed and cyclic source, wrong/missing target,
forged receipt source, a valid-digest self-call and a fresh/borrowed return join.
The last two must reach `program_readiness=26`, not merely fail JSON digest.

Artifact refusal policy derives from the actual owners:

- MIR/projected artifacts: preserve the previous artifact on rejection.
- Public executable requests: invalidate the exact previous binary before
  admission, as `driver_binary_output_owner.h` and
  `tests/self_hosted/parity/binary_output_refusal_owner.sh` explicitly require.

Initial generic-preservation assumptions were corrected after those contracts
were read. The observed binary invalidation is not recorded as a defect.

The new gate is registered after the existing call-result gate in Makefile
target `self-host-direct-mir-scalar-owned-array-string-parameter-test-smoke`.
Root did not edit the externally modified test script. Standalone candidate
gate execution and compatibility results are recorded below after observation;
registration alone is not a complete-target or CI PASS.

### Observed composition gate result

Root ran the new gate directly with the hash-pinned launcher and fresh
native-bootstrap candidate, under MSYS2 UCRT64 with a 300-second gate budget:

```bash
export PGY_BIN="D:/PergyraLang/.tmp/vision_owned_result_6e732613/pgy-pinned.exe"
export PGY_SELF_DRIVER_BIN="D:/PergyraLang/.tmp/vision_owned_result_6e732613/native-candidate-driver.exe"
timeout 300s bash tests/self_hosted/parity/direct_mir_owned_string_result_composition.sh
```

Observed exit0 and final PASS. Evidence directory:
`.tmp/self_hosted/owned_string_result_composition.AFR9QH/`.

- Six positive semantic/MIR producer controls succeeded.
- Six programs × two backends × direct/public routes = 24 exact-output executions.
  Root separately counted and checked all 24 runtime-output files.
- Two source producer refusals and five MIR mutations × two backends = 12
  refusal checks preserving the previous MIR/projected artifact.
- Borrowed/cyclic source × two public backends = four refusals invalidating
  the exact prior executable, following the existing binary owner policy.
- Valid-digest recursive-target and borrowed-branch mutations reached ownership
  readiness26 on both backends; root separately checked all four diagnostics.
- Native-forward controls still refused with the known summary-order defect;
  reordered controls executed. These observations are not self/native parity PASS.
- LLVM emitted its target-triple override warning. This is not warning-free evidence.

The existing compatibility script is run from a scratch copy with only its
root/work-directory plumbing changed: fixed verified repository root and a
unique `mktemp` directory replace destructive shared-workdir reuse. Its test
logic is unchanged. Root neither overwrites that externally edited script nor
deletes a concurrent task's shared output.

Root executed that isolated compatibility copy with the same launcher/candidate
packet and observed exit0, final `body proof + exact receipt source + C/LLVM
parity/negatives: PASS`. Evidence:
`.tmp/self_hosted/vision_owned_result_compatibility.u0vcbH/`.
This preserves the existing positive/native/direct checks, wrapper public
execution, borrowed refusal, branch transfer and wrong-source rejection. It is
the script's observed scope, not full native-forward declaration-order parity.

Static checks observed: owner cap160/180, new-gate Bash syntax and
`git diff --check`. The whole component inventory and Makefile integration
target were not executed; these narrow checks do not imply their success.

## Main-integration issues still open

1. Native oracle forward-wrapper ordering: baseline native C/LLVM refuse a
   wrapper declared before its fresh callee; reverse ordering executes. This is
   a separate summary-order defect, not a reason for native production retry.
2. GraphPlan readiness/digest and callee-body proof are repeated by admitted
   downstream consumers. A shared-callee two-return DAG can statically multiply
   proof work per depth; no benchmark improvement is claimed.
3. Semantic producer and native oracle retain a separate alias-depth8 guard.
   A longer alias path is a next bounded falsifier, not a proven new failure
   from this session or silently accepted support.
4. Whole-driver seed generation timed out; native emission exceeded the
   observed memory guard and root-only tracking missed detached workers.

The real four-route world is REACHABLE. Readiness-only stage topology remains
SURFACE. Registry ownership is still ACTIVE; whole-compiler SUBSTITUTING,
collection plan consolidation and the complete vision are not established.

No commit, push, installed publication, full matrix, full fixed point, sanitizer,
remote CI or performance benchmark is part of this session.

At final handoff HEAD remained `7a9fe09d`, the candidate source hash remained
`B2DB3A6E...`, and the source graph matched the build fingerprint. Existing
dirty vision, handoff, external compiler/gate/wrapper work and prior user-local
audit/conditional-push fixture were preserved. Root's added integration gate is
registered in Makefile; all scoped material remains uncommitted. Exact source
and runtime packet evidence must be rechecked if another task updates the tree.

# Assignment projection nominal-return counterexample

Revision: `5b97f2e10ffa7ecf9cfe932829a83ffffaa3ba12` (published main/origin/main).
Status: bounded physical-record implementation passes focused gates; the original
compiler-scale LLVM integration reaches the next owned Array<String> move refusal.
This audit is evidence, not an ABI owner, completion score or implementation plan.

## Current compiler and original gate

Candidate: `.tmp/self_hosted/compiler/receiver_literal_integration_20260906/pgy-self-driver.exe`.
SHA-256: `FC669FD242A7FE400BE0450C33BC991DD78B807CB2D949F4FC70D386B58F039E`.
Source graph: `d1571d6723654bbf4ee1f3fa3ddf12838a775eba89f732e9abd69163b84a8e98`.
The isolated sibling native launcher is `30F4130B`; shared `bin/` is unchanged.

`tests/self_hosted/parity/assignment_projection_probe_parity.sh` ran against
that candidate. C compilation, expected runtime output and all seven missing-fact
negatives completed before the LLVM leg. LLVM failed, exit 1 (`693e73`):

```text
owner=callable-route-envelope stage=return-type routine=588
name=SemanticAstEntrypointSelectionAccumulatorEmpty parameter=-1
type=SemanticAstEntrypointSelectionAccumulator carriage=
```

This reproduces the earlier weekly run `33992671114` on current source, not just
its older `16b2f894` logs. The local gate is terminal; do not restart its former
session `25512`. Logs remain under `.tmp/self_hosted/assignment_projection/`.
The previous 228-file gate output was hash-verified and copied first to
`.tmp/self_hosted/driver/assignment_projection_5b97f2e1/prior/` (`bf65d6`).

## Reduced owner-preserving counterexample

`.tmp/self_hosted/driver/assignment_projection_5b97f2e1/entrypoint_owner_probe.pgy`
imports the real `src/self_hosted/semantic/ast_entrypoint_selection_policy_owner.pgy`.
Its Main constructs the accumulator, observes one Main signature, finishes the
selection and logs `candidate_count`. No compiler owner was copied or modified.

- Public C: compile 0, exact output 1, runtime exit 0.
- Public LLVM: compile 1, no executable, same return-type rejection at routine 0.
- Receipt: `2546f6`; verified MIR producer receipt: `fa43cd`.

The emitted MIR contains two distinct struct identities. SelectionFact has two
Int fields and one Bool field, `abi_layout_required=false`, layout ID 0.
SelectionAccumulator has four Int fields, `abi_layout_required=true`, layout ID
577017080 (`47868e`). Both have complete positive declaration/field IDs.

The reached logical-record declaration owner explicitly requires physical ABI
absence (`direct_mir_scalar_program_logical_record_declaration_envelope_owner.pgy`).
It therefore cannot admit the accumulator's current declaration as a logical-only
record. Its last observed consumer is
`DirectMirScalarProgramCallableRouteEnvelopeAssessWithReferencedEnum`, through
`DirectMirScalarProgramZeroParameterReturnTypeSupportedWithReferencedEnum`.
This locates an admission boundary; it does not prove that relaxing that boundary
would provide a correct return/copy-out implementation.

## Disconfirming controls and next falsifier

Two further temporary four-Int Counts programs are retained beside that probe:
`value_record_probe.pgy` and `inout_record_probe.pgy`. Both receive a physical ABI
receipt (ID 631946432), including the version with no inout function (`00bd45`).
Their C outputs are respectively 0 and 1. LLVM rejects the two-routine value-flow
and three-routine structural families, respectively. Thus "inout alone creates
the physical receipt" is disproved; these other-family failures must not be
reported as the original callable-envelope failure.

Next: identify the existing physical nominal ABI fact and return/copy-out consumer
needed by the owner-preserving probe. Keep the original compiler-source gate as
the integration falsifier. Do not erase required ABI evidence, rename or flatten
the struct to fit another family, hard-code this compiler type, skip LLVM, or
retry the native backend. Any compiler change needs a fresh objective card,
current owner evidence and focused positive/missing-fact tests first.

## Physical receipt implementation and execution

The objective card is
`docs/agent_work_directives/physical_record_return_carriage_2026-09-06.md`.
The existing nominal declaration owner still admits required layout rows; the
record inventory now retains their exact identity and storage columns. Formal
parameters retain exact bounded receipt keys, including non-first parameters.
C/LLVM construction, read, nested member rebind and copy-in/out consume the same
record identity. Required Int fields use 4-byte storage; logical-only records
retain their prior representation. Self-consistent but unsupported physical
rows fail target admission instead of becoming a logical fallback.

Intermediate candidate `970D7619` executes the real-owner LLVM probe with output
`1` (`06790a`). The expanded fixture exposed a genuine dual-owner error in C:
the old two-Int scalar path and record path emitted incompatible C types for one
declaration (`d90e66`). The scalar GraphPlan's separate nominal carriage and its
four fact/target/preamble files were removed, not reordered behind a fallback.
The shared two-Int shape owner used by other bounded routes remains unchanged.

Final isolated candidate:
`.tmp/self_hosted/compiler/physical_record_registered_owner_20260906/pgy-self-driver.exe`.
SHA-256: `958AE778E28E5B3BB167089B4879F96307C6F32170014624D11124CD85250AB9`.
Source graph: `c415b3d8fc973ec1d4d5d7b4aa09c17de9b4b766a9f16dcf471f928f4ba947fe`.
Pergyra-seed build passed (`78a828`). Its generated C is byte-identical to the
preceding `E2EBC3E6` candidate, SHA-256
`8CB2DC541B21D53353A22CC2EDD74F7F6506169CFE92AF847521A92B95F8E562`
(`a8a27b`); the intervening change registered the derived fact's owner filename.
Shared installed binaries are unchanged.

- `direct_mir_physical_record_return_owner.sh`: PASS on `958AE778` (`523948`).
  One verified MIR input drives direct C/LLVM; public source C/LLVM also execute
  the exact ten expected lines. The fixture imports the real entrypoint owner
  and covers 1/2/3/8-field records, nested values, negative Ints, readonly refs,
  and non-first inout copy-out. Twelve receipt mutations on each backend fail
  without an artifact and with the reached fact owner's diagnostic. Evidence:
  `.tmp/self_hosted/physical-record-return.DIZc00/`.
- Existing two-Int and logical-record C/LLVM execution and negative gates pass
  on `E2EBC3E6` (`d0a51d`, `85916c`). The logical-record Option<Int> field gate
  passes on `958AE778`, including its Option<Long> negative (`512016`).
- SoT, responsibility/test-inc size and protocol checks pass (`188ca6`,
  `152606`, `93c65d`). Counts remain 88 authorities / 185 derived carriers,
  CLOSED55/BRIDGE32/ACTIVE1. Generated fixture-only inventory was refreshed;
  the 146-row keyword check passes (`5698b7`). Support bits did not change.

## Integration limit and preserved continuation

The original assignment gate ran on `E2EBC3E6`. C compiled, matched its owned
expected output and passed seven missing-fact modes, then entered LLVM. The
combined 300-second process limit terminated that LLVM child (code 143;
`c38434`), not a newly observed compiler semantic diagnostic. Old LLVM output
files in the fixed work directory are historical and do not prove this run.
The prior 238-file directory was hash-verified and preserved first under
`.tmp/self_hosted/physical_record_regression_prior_20260906/assignment_projection/`
(`a907fb`). Two-Int/logical/Option fixture directories are preserved there too.

The remaining LLVM leg is isolated in
`.tmp/self_hosted/physical_record_assignment_llvm_20260906/`; session `64735`
is now terminal, exit 1 (`b15c77`).
It uses `958AE778`, the original source/expected file, the existing normalization
and Pergyra artifact-comparison owner, and the original six LLVM negative modes.
The observed C output is hash-verified and reused instead of recompiling C.
This compiler-scale continuation has the directive's 30-minute integration
budget. It ended with `direct MIR scalar program extension is invalid: code=19`,
after passing the former return-type boundary. No native-pipeline compiler
fallback or LLVM skip was used, and no pass is claimed.

The full component inventory did not finish within its 60-second static budget
(`aaaa92`, observed tool exit 1 and no completion receipt); only its
checker-mechanics PASS was observed. The changed record-inventory slice passes
through that gate's actual functions and line caps (`9f062b`). Do not claim a
full component green from either result. Source/test/doc changes remain uncommitted;
regular remote CI 30/30 on `5b97f2e1` predates this candidate. Neither fixed point,
whole Platform/weekly green nor a new hard-substitution percentage is asserted.

## Reached failure and bounded runtime observation

At 09:56:15 local time, the LLVM child had used 732.70 CPU seconds and
3,286,740,992 working-set bytes (`f1d573`). Brief attached stack captures show
`DirectMirScalarCfgProgramAppendRoutine` / GraphPlan construction calling:

- parameter signature/ABI admission, including `pgy_substr -> strlen`
  (`0bc874`); `MirCapturedRequiredAbiLayoutRowAdmission` currently uses
  `Substring` for an ID span, while the adjacent ABI validator already uses
  the bounded `SubstringWithLen` form;
- payload-free enum readiness during expression marker admission (`a32139`);
- referenced-enum readiness during expression marker admission (`35f8c6`).

The last stack is saved as `graph-plan-stack.log` in the continuation directory.
These samples establish reached repeated operations, not their relative cost
or a complete memory profile. No compiler source was changed while it ran.
The log ended at 09:57:45 with code 19. An attempted bounded stop found the
identified child already gone; **no process was stopped** (`d898cc`).

Code 19 is the existing extension owner's
`DirectMirScalarProgramOwnedArrayStringMoveReadyForAbi` predicate. It combines
move-fact validity/coverage and the Array<String> ABI join; the failing branch
and row are not yet identified. The next falsifier must name that row and
consumer, not relax ownership/coverage or remove ABI. Follow-up review intake:
`docs/audits/2026-09-06_architecture_review_5b97_reconciliation.md`.
Docs and strict UTF-8 on all 63 changed live text paths pass (`311aa6`); the
worktree has 67 dirty entries and the index is empty. Shared binary hashes are
unchanged (`9c12cd`).

## Reached owned Array<String> move correction

This continues the same assignment-projection rung, not a separate ownership
cleanup program. Published HEAD remains `5b97f2e1`; no source change in this
section has been staged, committed or installed into shared `bin/`.

The first reduction preserved the consuming parameter and caller-local identity.
On `958AE778`, the first-callee control projected both backends, but moving only
that declaration behind another function made both reject code 19. An unrelated
String return after the move and a small import of the actual consuming
`CodegenJoinOwnedStringFragments` owner also rejected (`f5738c`). This was an
artifact-projection observation, not a runtime pass. Evidence is retained in
`.tmp/self_hosted/owned_array_move_20260906/`.

The existing move admission was requiring `target == 1` after already admitting
the exact target through its callable inventory. The existing last-use and final
GraphPlan checks also rejected every returned expression, including expressions
with no reference to the retired local. Both restrictions have been removed;
the same fact still requires exact local/callee/parameter/ABI identity and
complete last-use coverage. Return-expression absence and Void-return absence
are now distinguished through the existing Void sentinel owner; other absent or
out-of-range graph IDs do not count as a zero-use proof. The initial attempted
change incorrectly rejected that Void sentinel (`FEA3ED63`, `7841a9`), which the
positive control caught before publication. It is not retained as a green result.

The existing `DirectMirScalarProgramRoutinePartitionStart` function moved,
without renaming or changing its result, into the existing routine-partition
owner. The move-use owner consumes it there and retains its 100-line cap. No
new compiler owner file, general ResourceCarriage fact or cap increase was added
for this correction. The old definition and first-callee restriction have
structural residue ratchets; behavior is owned by the executable gate.

Final isolated Pergyra-seed candidate:
`.tmp/self_hosted/compiler/owned_array_callee_partition_owner_20260906/pgy-self-driver.exe`.
SHA-256: `19C20849DE03851F7CA5A03A8F14C0E5FE5F92267FE7AD6C72E58AC7D2A63E8D`.
Source graph: `7f96dbfe5824f7fdc355fa53e8f414a0e0b0353b2ec1dfee90756865a7cb0e0e`.
Build `62463` is terminal PASS (`c5a9e0`); all recorded source hashes match the
current inputs (`18ffa2`). Its sibling native launcher is copied only into that
isolated directory, not shared installation.

- `direct_mir_owned_array_string_callee_identity_owner.sh`: PASS (`ab2781`).
  Four sources produce verified MIR once each, then C and LLVM artifacts compile
  and execute the exact expected outputs. The real consuming join is imported,
  not imitated. A returned use of the moved local and five carriage/pass/layout/
  target mutations reject without artifacts and with compiler-owned diagnostics
  on both backends. Evidence: `.tmp/self_hosted/owned-array-callee-identity.yj3sAC/`.
  This gate is attached to the existing owned-parameter Make target, not a new
  CI job or an expected-rejection collector.
- Existing entrypoint, non-entrypoint, multiple-local and complete if/else
  owned-parameter C/LLVM gates: PASS (`70fa45`, `f040bb`). The alternative gate
  retains its LLVM target-triple override warning; no warning-free claim is made.
  Prior fixed-directory artifacts were copied and hash-verified before each
  replacement: 132 original files (`a2ac9c`) and 132 intermediate files (`547583`).
- Physical-record public/direct C/LLVM execution and all 24 receipt refusals
  remain PASS on this same final candidate (`fec034`), evidence
  `.tmp/self_hosted/physical-record-return.vKUiqO/`.
- The reached owned-move component slice passes the actual inventory functions,
  unchanged line caps and old-path checks (`b12191`). This is not a whole
  component-inventory PASS.

### Next falsifier, not whole-program closure

Three small forms import the same production string-join owner: a consuming
call used directly as the return expression, the call nested under Concat, and
the call returned after a loop. All produce verified MIR but still reject C and
LLVM with code 19 on the final candidate (`cc051f`, terminal `ba89ef`). Evidence:
`.tmp/self_hosted/owned-array-production-forms.NX1eSj/`. The collector's exit 0
means collection completed, not that these source programs passed.

The next missing carriage is now specific: a direct-return graph has no
operation row, while the move owner currently derives the caller only from an
operation partition; a nested call is not the graph root. Loop coverage is also
still restricted by the existing straight-line/complete-if-else proof. The real
`RewriteSemanticCall` and `RewriteSemanticMemberAccess` consumers return
`CodegenCExpressionTextCommitRoot(owned_fragments, value)` in this first form.
An operation-less return graph must acquire an explicit, unique caller/block
identity and terminal-use proof before it can retire cleanup. Do not reinterpret
a missing operation as permission, relax path coverage, or rewrite the real
consumer as a borrow. The exact first failing row of the original compiler-scale
execution is still unknown; these reductions establish reached unsupported
forms, not that original row's identity. The full leg has not been rerun.

No fixed point, full integration green, shared installation, new remote CI green
or hard-substitution/SoT percentage increase follows from these focused results.

Final navigation checks: SoT 88/185 and CLOSED55/BRIDGE32/ACTIVE1 pass (`bb0a58`),
Protocol 10 rows pass (`2adb2b`), and the regenerated fixture inventory retains
146 language-word rows and unchanged support bits (`de3126`). No generated
lexer source differs. Docs pass (`a6e341`), strict UTF-8/NUL and whitespace checks
pass across 73 changed live text paths (`4da3f1`), and the source-graph receipt
plus reached component slice recheck pass (`2b7b6c`). Shared binary hashes remain
unchanged (`99608c`). Worktree: 77 dirty entries, empty index, no live gate.

The current full test-inc-size check is **not** green evidence: its bounded
60-second invocation returned exit 1 without a completion message (`305439`).
The saved trace ends in `grep -RIn --include=*.h _IMPLEMENTATION src`, after the
size-policy checker itself passed. This does not establish a source-size
violation or the exact termination cause. The unchanged caps for the reached
owner slice are independently checked above; they do not substitute for the
unobserved whole-script result.

## Consuming graphs and covered continuations — current local implementation

HEAD is still `5b97f2e1`; this is the same uncommitted integration scope. The
first terminal-flow candidate `D953912A` passed the actual join/CommitRoot,
direct return, strict nested/nary calls, pre-call loops, pre-definition early
return and branch-merge cases through public and direct C/LLVM, with 20 refused
negative projections (`1cbf4e`). Its first run stopped at a test-only LLVM link
omission: the fixture reaches TextBuilder, so the harness now compiles and links
the existing `pgy_runtime_lib.c`, as the established runtime-value gate does.
The failed link is retained in `owned-array-terminal-flow.OMHuEX`; it was not
counted as LLVM runtime success.

The existing move fact now carries exact operation-or-return graph identity
and a coverage input digest. The new, cohesive `move_flow_owner` derives the
strict single-evaluation path, unique definition/dominance, exit coverage and
post-move absence of uses. Existing straight-line and complete-if/else coverage
remain. The final GraphPlan checker uses a borrowed storage view and the carried
input identity; its duplicate CFG proof functions are removed. No second cleanup
owner, native fallback, runtime flag, new language concept or registry authority
was added. The definition/call operation order is compared only within the same
block; numeric block order is not control-flow dominance.

The real `function_emit.pgy` consumes its join before subsequent branches.
`post_move_branch_owner.pgy` reduced this extra restriction to code 19 on both
targets (`15bd7f`); the separate pre-move process-fatal control projected both
(`ecf684`). The same proof now admits post-call branches/loops only when every
reachable later operation, condition and return avoids the retired local and
cannot re-enter its consuming block.

Current isolated Pergyra-seed candidate:
`.tmp/self_hosted/compiler/owned_array_exit_coverage_20260906/pgy-self-driver.exe`.
SHA-256: `613AE6CD26F5E3C39826B84921A9CA74FAD4C9D078A6E1FCE12F0DD0290D4A72`.
Source-graph input SHA-256:
`11aac0615466464f4c4995c377c5b2e0894ee92a8c132f80a62567ad7ea197d4`.
Official Pergyra-seed build `51656` completed PASS (`da8946`).

- Extended terminal/continuation gate: PASS (`e05d85`), 14 expected runtime
  lines on each public/direct C/LLVM path and 24 refused projections. Evidence:
  `.tmp/self_hosted/owned-array-terminal-flow.vwE5JO/`. Seven source negatives
  reach the owned-move refusal; malformed graph/identity/ABI cases may reject at
  an earlier owner. The test does not claim every malformed case reaches the
  move issuer. Runtime-object compilation retains existing deprecated-atomic
  warnings, and LLVM retains the target-triple override warning.
- Existing entrypoint, non-entrypoint, multiple-local and complete-if/else
  move gates: PASS (`51edb3`, `a1fce5`). Callee-order/independent-return/real-join
  execution and 12 refusals also pass (`a1fce5`), evidence
  `.tmp/self_hosted/owned-array-callee-identity.7Rw4zU/`.
- Before those fixed-directory regressions, all 132 previous artifacts were
  copied and SHA-256 verified under
  `.tmp/self_hosted/owned_array_exit_coverage_prior_20260906/` (`d0b307`).
- A gate-only Pergyra probe obtains a real issued GraphPlan and mutates its
  edges, return graph, routine partition, local use and move-operation identity.
  The standalone ABI receipt and repaired outer digest do not re-issue coverage;
  restoring the exact inputs restores readiness. The first probe's public
  source admission refused a dotted ArraySet receiver (`1c0676`); the probe now
  uses shared array-header bindings and checks their actual effect. This is
  test scaffolding, not a native-pipeline bypass. Its first whole 300-second run
  did not complete (`0df52f`): the 4.2 MB C harness was compiled, then its runtime
  continuation passed all five mutations (`0d0cd9`). Final-candidate whole-probe
  validation is tracked in the current handoff. This compiler-module integration
  harness is not a sub-minute static gate or a push-CI addition.

### Next exact production falsifiers

`EmitStmtList` has two distinct consuming exits for the same `result` array
(inside its loop and after the loop), and nests each call under the
`CodegenStatementBlockEmission` record constructor. These are not the earlier
complete four-block if/else control or a strict ordinary-call parent.

Two independent reductions are retained under
`.tmp/self_hosted/owned_array_move_20260906/`:
`two_consuming_exits_owner.pgy` and `consuming_record_constructor_owner.pgy`.
Both produce verified MIR; both C and LLVM reject code 19 on `613AE6CD`
(`c2ea7f`), evidence `.tmp/self_hosted/owned-array-multiple-exits.WLyfFc/`.
The collector's exit 0 is not execution success. The next change must prove
exactly-one consumption across the complete exit set and admit the constructor's
actual strict operand identity, without permitting bypass exits, duplicate
evaluation, repeated moves or later local use. Do not rerun the large original
LLVM leg while these known reduced production forms remain red.

This does not establish whole assignment projection, current-source bootstrap
fixed point, shared installation, exact remote CI or a SoT status increase.

Final checkpoint: the whole sealed-flow probe passes on `613AE6CD` (`0575e2`),
evidence `.tmp/self_hosted/owned-array-sealed-flow.I1sVkM/`. Physical-record
public/direct C/LLVM and 24 ABI refusals also pass (`c86f1d`), evidence
`.tmp/self_hosted/physical-record-return.saM3vs/`. The generated inventory was
refreshed through its owner; all recorded compiler source hashes still match
(`749f2a`), and the 146-row keyword gate passes (`d27b54`). Full SoT registry
validation completes under the 60-second native-process bound (`d9e28b`):
88 authorities / 185 derived, CLOSED55/BRIDGE32/ACTIVE1. The earlier bounded
MSYS wrapper returned without a completion receipt (`bc3b59`); that invocation
is not a PASS. Shared binaries are unchanged, index empty, 93 dirty paths and
20,865,323,008 free bytes on D: (`723f58`). All runtime/build processes are
terminal. No source scope was staged, committed, installed or published.
Final documentation quality, post-selfhost manifest and reached component
checks pass (`4de673`); strict UTF-8/NUL and whitespace checks pass on all 89
changed live text paths. The 93-entry dirty scope and empty index are unchanged.

## Complete consuming-exit sets — unified local implementation

The same primary integration scope replaces the straight/four-block/control-flow
coverage tags, their proof implementations and branch/merge arrays with one
complete consuming-exit-set proof. Caller/local identity groups the move rows;
the definition and its block are resolved once per local. Each member still
proves strict single evaluation and no reachable use after consumption. The
set proves definition dominance, excludes an ordinary unconsumed terminal path,
and requires every member to be reachable without passing another consumption.
Admitted callee/parameter/ABI identity remains per row, so alternative exits need
not share a callee or parameter ordinal. Existing cleanup owners are unchanged.

Record-constructor parents now consume the existing logical-record operand
owner, including its n-ary operand representation. No constructor spelling,
fixed field-count permission, arbitrary expression-parent permission, native
fallback or generalized ResourceCarriage was added. The same coverage fact now
holds only valid/digest/input identity and consuming-block rows. Those rows are
also bound into the input identity; final readiness does not re-prove the CFG.

The two `EmitStmtList` reductions were rechecked as RED on `613AE6CD` with valid
output arguments (`79161a`). An earlier command omitted `-o` and tested only CLI
usage (`428751`), not ownership. Owned-parameter forwarding is already rejected
on that baseline (`160205`); no parameter-origin authority was invented here.
Both reductions execute C/LLVM with exact `Print` output `row0` on intermediate
`A3AFBE8D` (`97bc4c`). The first harness expectation incorrectly included a
newline; it was preserved as `expected-with-newline.run` and corrected. That
expectation failure (`8a5b79`) is not a compiler failure. Evidence:
`.tmp/self_hosted/owned-array-complete-exit-controls.YAl8Z5/`.

Final isolated Pergyra-seed candidate:
`.tmp/self_hosted/compiler/owned_array_complete_exits_bound_20260906/pgy-self-driver.exe`.
SHA-256: `0CDAF01371A029AA606732807ACC742EF14272EEA5C73C6F5EC714F8C674BEF2`.
Source-graph input SHA-256:
`c2254aee7e9edaf7bf3dcab1e451d6942b9f257d1d874fb1e046c0a54ab82003`.
Official build PASS (`b3ff6c`); all source hashes match after the generated
keyword inventory refresh (`5e21f1`, `d9b61f`).

- Expanded terminal-flow gate PASS (`bebe72`), evidence
  `.tmp/self_hosted/owned-array-terminal-flow.m4T49k/`: 24 runtime lines on every
  direct/public C/LLVM path; 11 source negatives and six MIR mutations refuse
  on both backends, 34 projections total. New positives cover two/three exits,
  different admitted callee/parameter destinations and 2/3-field constructors.
  Missing exits, sequential/repeated consumption, sibling use, duplicated
  evaluation and missing constructor operands do not retire cleanup. Public
  checks unset the native override and reject its timing marker.
- Existing entrypoint/non-entrypoint/multiple-local/alternative gates PASS
  (`1298e1`, `e70396`, `1bb106`). The non-entrypoint source pin initially stopped
  at the deleted single-block implementation; its replacement checks the actual
  routine-owned partition and forbids restoring that restriction. Runtime and
  negative assertions were not weakened.
- Callee identity/order/independent-return/real-join execution and 12 refusals
  PASS; public/direct physical-record execution and 24 ABI refusals PASS
  (`fd7aa2`), evidence `owned-array-callee-identity.3dXoXB/` and
  `physical-record-return.b0XDyK/` under `.tmp/self_hosted/`.
- The actual issued-plan probe PASS (`a69466`), evidence
  `.tmp/self_hosted/owned-array-sealed-flow.pCVxPR/`: all six mutations refuse,
  including a consuming-block row changed after issue with coverage/move/outer
  digest repair. Exact restoration re-establishes readiness. This is a compiled
  compiler-module integration probe, not a cheap static or new push-CI gate.
- Before fixed-directory regressions, 379 files (247 assignment, 132 move)
  were copied and SHA-256 verified under
  `.tmp/self_hosted/complete_exit_set_prior_20260906/` (`46f48f`).

The exact original `assignment_projection_probe_parity.sh` now runs both
backends on the final candidate under 1,800 seconds, session `44755`. Its C
runtime and seven negatives have passed; LLVM is running (`929697`). Compiler
source is frozen. Observe this run; no restart, skipped LLVM, full-rung closure,
shared installation, current-source fixed point, commit/push, new remote CI or
substitution percentage is claimed before the actual terminal result.

Static reconciliation after this candidate: full SoT registry PASS (`b8dc48`),
88 authorities / 185 derived and CLOSED55/BRIDGE32/ACTIVE1; protocol 10 rows,
146-word inventory, reached component caps/residue slice, documentation and
post-selfhost manifest PASS (`62190d`, `cad11d`). Whole component/test-inc-size
completion is not inferred from this slice. Strict UTF-8/NUL passes across all
96 changed live text paths; 100 dirty entries, empty index and unchanged shared
binary hashes are observed (`018ce4`), with 20,766,851,072 bytes free on D:.
An in-flight LLVM process sample (`03fbce`) is about 2.12 GB working set and
2.22 GB private, not a peak or before/after memory-reduction result. MSYS's
zero subtree-RSS heartbeat is not a valid zero-memory measurement on Windows.

## Original integration passed code 19 and reached identity-hash work

The actual full run `44755` passed C runtime and all seven C negatives. A brief
attached stack (`df9b51`), saved in
`.tmp/self_hosted/owned_array_move_20260906/complete_exit_integration_stack.log`,
reached `DirectMirScalarCfgSealGraphPlan -> DirectMirCfgMirDigest ->
DirectMirCfgHashString -> pgy_strlen -> strlen`. The source control flow calls
and verifies `DirectMirScalarCfgProgramExtensionFromOwners` before this sealing
function, so the former code-19 boundary has been passed in the original
compiler-scale program. This is not final GraphPlan verification or LLVM runtime
completion. A later in-flight sample was 3,349,819,392 working-set bytes and
3,515,121,664 private bytes (`488ff7`), not a measured peak or reduction result.

The reached `air/mir_cfg_identity_owner.pgy` measured `StringLength(value)` for
the initial hash and twice on every byte iteration. The generated C confirms
both per-byte `pgy_strlen(value)` calls (`097afa`). The loop therefore repeatedly
scans the complete immutable MIR String. This evidence justifies one local
length binding, not a new cache or longer budget. The seed, 131 multiplier,
modulus, byte order and bounded-digest projection remain exactly unchanged.

Only the identified integration child, PID 73560 with its path/start time
verified, was deliberately stopped after this diagnosis (`d4ed5c`). The parent
gate is terminal exit 1 (`26517d`) and its log reports child code -1. Do not
classify that as a compiler rejection, timeout or LLVM PASS. No other process
was stopped. All 256 files / 27,368,922 bytes of this fixed-directory state
were subsequently copied and SHA-256 verified under
`.tmp/self_hosted/assignment_projection_complete_exit_at_hash_20260906/`
(`fc8e67`); the original evidence remains intact.

The new focused hash gate rejects the old repeated-scan body (`dab688`). It
imports the actual owner and checks the emitted C for one length computation,
then executes empty, ASCII, escaped and UTF-8 Strings, a modulus-edge seed, a
deterministic 1 MiB String and its bounded digest against an independent byte
recurrence. All seven values pass (`afd1d1`), evidence
`.tmp/self_hosted/cfg-identity-digest.SO2KIy/`. It is a manual Make target with
an existing component source-residue ratchet, not a new CI job.

Two harness corrections are not hash failures: the first probe omitted the
explicit Main return type and stopped in shared signature admission (`2245e5`);
the next produced all seven correct numeric values but Python wrote CRLF while
runtime normalization produced LF (`d012ea`, `8b59dc`). The generator now writes
canonical UTF-8 bytes. Both earlier evidence directories are preserved.

The hash owner is 106/120 lines, the gate 64/90, and the source fixture fits
30 lines (`70bb6c`). Isolated Pergyra-seed rebuild `97911` is now live under
`.tmp/self_hosted/compiler/owned_array_linear_identity_20260906/`; the compiler
source graph remains frozen while it runs. The seed hash still equals the prior
candidate's recorded `e3b71ad2` identity (`8b59dc`, `fb86d1`). The actual-owner
probe used the earlier driver to compile current source; it is not evidence that
the earlier driver already contains the hash fix. Verify the new candidate's
receipt and focused gates, then use the exact original assignment integration.

### Rebuilt linear-identity candidate

Official build `97911` is terminal PASS (`af0a9f`). Candidate:
`.tmp/self_hosted/compiler/owned_array_linear_identity_20260906/pgy-self-driver.exe`,
SHA-256 `908D9A4E24DD866BC750C9720481EBDECDD8AAA63CB1E257FC7750E304D6DEF1`.
Source-graph SHA-256:
`52cf237ea0b2ab113cb53c3480a1c1e22bad4b41883d6fe88b947fb9bd80e782`.
All source hashes match (`6cd0bc`); compared to `0CDAF013`, the only compiler
input difference is `air/mir_cfg_identity_owner.pgy` (`8e0794`). Generated C is
not raw byte-equal: the extra local shifts later private return/match temporary
names. Excluding only the changed hash body and reversing that monotonic +1
rename at 695 private-token occurrences makes the remainder identical
(`03d255`). This comparison is not a new semantic identity normalizer or gate.

On the rebuilt candidate, the actual-owner hash gate passes seven values;
terminal-flow passes 24 runtime lines per public/direct backend and 34 refusals
(`560c8d`); physical-record runtime and 24 refusals pass (`d2782c`); the actual
issued-plan probe passes six mutations and exact restoration (`731afd`). Evidence
under `.tmp/self_hosted/`: `cfg-identity-digest.Bugi0k/`,
`owned-array-terminal-flow.XAT1mb/`, `physical-record-return.XAFr9g/`, and
`owned-array-sealed-flow.sIYCQx/`.

Full SoT remains 88/185 and CLOSED55/BRIDGE32/ACTIVE1 (`1219fd`). Protocol,
keyword, reached component, documentation and post-selfhost manifest pass
(`eae367`); the added hash-owner caps/residue slice passes (`19bfa5`). MSYS did
not provide Git for the final whitespace command in that batch; the same
check passes through configured PowerShell Git (`f7fa15`). Strict UTF-8/NUL
passes on 99 live changed paths, with 103 dirty entries, empty index and
20,698,193,920 free bytes on D: (`1d5a2c`). No shared binary was replaced.

The exact original assignment integration completed PASS on the new candidate:
session `48391` (`b83a45`), unchanged 1,800-second budget and frozen compiler
source. Both runtimes produce the same five lines; C's seven and LLVM's six
missing-fact cases refuse. Artifact timestamps and exact output are checked
(`546f84`, `dd8521`). LLVM compile spanned about 21m25s. A mid-run stack reaches
the existing bounded JSON field reader (`d9a7b2`), but that one sample does not
establish a dominant cost or authorize a query/cache redesign. No current-source
fixed point, installation, publication or percentage increase is implied.

## Reached complete-driver native-oracle borrow boundary

The existing bootstrap gate's native C emission/compilation subset was run
without installing or using the result as the production driver. Emission
failed before any C artifact, with 100 reported errors (`191c00`, `dd6ac4`).
Evidence: `.tmp/self_hosted/owned_array_linear_identity_native_oracle_20260906/`.
The first eight errors forward borrowed arrays into the default-value
`DirectMirScalarCfgHashInts` parameter. The final plan consumer also constructs
and returns a storage aggregate containing borrowed plan arrays, then aliases
the borrowed move fact. Transitive diagnostics repeat these boundaries. Later
TextBuilder errors remain evidence, not independently diagnosed root causes.

The four source preimages are copied and hash-verified under
`.tmp/self_hosted/owned_array_move_20260906/native_boundary_preimage/`.
The correction makes the existing pure array hash readers explicit `ref`,
deletes the returned storage view and fact alias, and maps both admitted
storage and final plan fields into one input-digest function. The original
field sequence and byte recurrence are retained; no CFG proof is replayed.
The component slice rejects the old aliases and requires borrowed hash
parameters; all existing caps pass. The actual issued-plan gate now also
requires native-oracle C emission and six stale-input refusals/restoration.

Ref-only native retry `21782` ended RED (`d325f0`): its first root is now the
existing array hash reader's call to ArrayLength. A seven-line Count/ref-array
reduction independently reports a helper-escape summary. The existing native
non-escape vocabulary contains slot operations but omitted ArrayLength, whose
type owner returns Int without retaining an array. Local scalar-binding trials
still failed (`88f856`, `e6bb2c`) and were reverted, not kept as a workaround.

The existing vocabulary now includes that metadata READ. The escape collector
must first exclude a same-spelled user declaration before applying a builtin
permission. No unknown-call or ownership escape guard was relaxed. Isolated
native build `19290` passes (`e4b447`):
`.tmp/native_array_length_borrow_20260906/pgy.exe`, SHA-256
`A0F2FCDB58EDE960FA41C9DBB27B14AABF5E74E13E92CD4F1CC716EC626AF863`.
The actual imported hash-owner probe emits native C successfully. The complete
semantic battery passes 2,877 / 0 (`9d6f4b`), including borrowed array/slice
non-escape summaries and same-name user declaration refusal. Existing own
forwarding, store, return and alias refusals remain in the executed battery.

Complete-driver native retry `63708` completed C emission/compilation PASS
(`c70e3c`) within the original 300-second budget: zero errors/four prior warnings.
The later TextBuilder errors disappear with the borrow-summary correction.
Evidence is `current-native.*` under
`.tmp/self_hosted/owned_array_borrow_boundary_native_oracle_20260906/`.
`908D9A4E` predates these four Pergyra source changes; its original integration
PASS is not current-source evidence.

### Current borrow-boundary candidate and test-source reconciliation

Official Pergyra-seed build `73186` completed PASS (`dbb71e`). Current candidate:
`.tmp/self_hosted/compiler/owned_array_borrow_boundary_20260906/pgy-self-driver.exe`,
SHA-256 `B58EFE4E6836E01EF736EBA8AF4B88C67A5F8EC4D07BFB32BB4118D66F9241A3`;
source graph `c1090eed4640bb8fe80eb168e2a1ae4f0a8824894d234dd6996f0600e6e959f2`.
The source graph matches after the gate-source corrections (`609ab8`). A bounded
source-preimage comparison verifies all 27 digest fields, hash kinds/order and
validity/expression joins at both consumers; it is an audit, not a second owner.

On this binary, the public/native hash probe passes nine exact values, including
borrowed integer/String arrays (`af6f98`), and terminal-flow passes public/direct
C/LLVM 24-line runtime and 34 refusals. Physical-record runtime and 24 refusals
pass (`b174a2`). Evidence under `.tmp/self_hosted/`:
`cfg-identity-digest.aCha0P/`, `owned-array-terminal-flow.dYtmi3/`,
`physical-record-return.SodrY3/`. Reusing the existing two built drivers rather
than re-emitting/recompiling them, the bootstrap's canonical sample source-C,
MIR-producer and common-MIR-consumer comparisons pass (`2332ae`). Its existing
manifest and comparator own the sample and verdict; evidence is
`.tmp/self_hosted/owned_array_borrow_boundary_bootstrap_20260906/`.
This is bounded resumed comparison, not a whole-source fixed point.

The new native leg catches invalid gate-only source (`a46604`, `f108a8`):
reserved `local`, immutable `plan.digest` assignment, and an unnamed aggregate
argument at an own/ref boundary. The prior source and logs are preserved in
`owned-array-sealed-flow.L1Z6Qm/`. Historical six-mutation results remain
single-producer consumer execution, not native admission or source-language
parity evidence. Do not codify those unsupported source forms as language rules.

The test now binds a named route and reconstructs immutable receipts using the
existing coverage/move/program/GraphPlan digest owners. Its companion is a
gate-only counterfeit-state constructor, not a production fact or digest owner.
It repairs the intermediate program digest too, retaining the original six
mutations, standalone-receipt checks and exact restoration. The compiler fields
stay immutable. Native emission and six-mutation runtime pass (`ba4cd5`,
`f3e78a`); the exact dual-producer gate then completed PASS as session `9723`
(`8df5c9`), budget 900s, evidence `.tmp/self_hosted/owned-array-sealed-flow.9DaXo2/`.
Both probes execute six refusals and exact restoration. Native preflight runs
before expensive public emission, which took about five minutes for this module.
No source-language equivalence beyond these probes or performance closure follows.

### Next reduced target-carriage falsifier, not another full assignment run

The actual-owner hash fixture's public LLVM leg rejects on `B58EFE4E`
(`474453`), without producing an executable:
`callable-route-envelope / parameter-type-or-carriage`, routine 262
`DirectMirScalarCfgHashInts`, parameter 1, type `Array<Int>`, carriage
`readonly-ref`. Evidence: `cfg-identity-digest.aCha0P/llvm.compile.log`.
The seven-line `array_length_borrow_probe.pgy` under
`.tmp/self_hosted/owned_array_move_20260906/` reduces this to `Count`, routine 0,
parameter 0 (`445674`): public C executes `3`; public LLVM refuses. Its verified
MIR and both backend logs are preserved there. All runs are terminal.

Source confirms that the callable parameter policy admits Array<Int> value and
value-result, while readonly-array admission/target predicates cover String.
The C/LLVM signature, argument and parameter-read projections also consume
that String-only predicate. Merely adding readonly-ref to one acceptance test
would not establish target agreement or non-escape/cleanup correctness.
No fix is implemented for this newly reached seam yet. Continue with its
explicit objective card and reduced C/direct-MIR/LLVM cases before any full
assignment rerun. Current borrowed hash-C/native/bootstrap/probe results remain
valid, but they are not evidence that this LLVM gap is closed.

Final bounded verification: SoT remains 88/185 and CLOSED55/BRIDGE32/ACTIVE1
(`d04406`); protocol/keyword/component slice/post-selfhost manifest pass
(`1528c2`), and documentation reports PASS (`d873fe`). Strict UTF-8/NUL and
whitespace pass on 104 live changed paths; 108 dirty entries, empty index,
unchanged shared binaries and 20,536,872,960 free bytes on D: are observed
(`3d9e29`). No original-integration rerun, full component/test-inc-size PASS,
current-source fixed point, shared installation or publication is claimed.

### Readonly-array implementation and the next signed-literal falsifier

The readonly family now shares exact Int/String type/carriage admission and
C/LLVM pointer projection. Its four String-only source paths are retired,
not retained as a compatibility fallback. Int parameters reuse the already
captured Array<Int> ABI; readonly parameters do not enter the copy-out subset.
The Int target receipt now retains its layout ID and travels from program
emission to parameter reads. Nested member arguments project the typed member
path instead of reconstructing a standalone member snapshot.

Native expanded fixture execution passes eight values, and whole-driver native
C emission/compilation passes with zero errors/four prior warnings (`875680`,
`241f17`). Pergyra-seed candidate `F1FAE84E` / source graph `23c85437` builds and
matches all recorded inputs (`51c585`, `8d9868`). The original seven-line Count
now executes public LLVM and projects direct C/LLVM (`09a25e`). The broader
fixture still rejects a negative array literal at node 7/row 7, evidence
`.tmp/self_hosted/readonly-array-int.NGhh00/`. This is not a readonly parity PASS.

The reached literal owner only handled direct integer operands or two-leaf
subtraction. The same receipt now carries zero/unary/binary operator arity and
normalizes typed Negate into the existing expression arena. Its source/spine,
binding and Int-domain checks remain; tests keep negative values and add three
malformed-negate inputs. Static inventory/caps and independent nine-value hash
reference parity pass (`fe787b`). Pergyra-seed build `77166` later completed
as E909C73A (`158d3c`); its newly reached source gap is recorded below.

### Readonly source admission and bounded bootstrap, final 55445D18 checkpoint

E909C73A executed the eight expected readonly values on public/direct C/LLVM
and refused 28 malformed MIR inputs, then publicly accepted readonly ArrayPush
(`8fafd6`). The preserved `readonly-array-int.XyjMGu/` evidence also records
public acceptance of index write, return and inout forwarding while the native
oracle rejects all four (`248d8d`). The existing mutation owner checked only
default mode; ref was absent from its guard.

The same policy now rejects default/ref mutation. The existing named-value
boundary owner consumes resolved places, parameter modes and typed return facts
to reject owned-sequence escape through return/default/inout/own. Strictly
decreasing member paths preserve readonly-root provenance. No AST scan, native
recheck, borrowed-array copy or backend-specific policy is added. Local aliases
and general flow summaries remain outside this bounded claim. The new Pergyra
diagnostic reuses PGY_SEM_BORROW_ESCAPE, semantic:borrow_escape,
change-ref-to-own-or-stop-escape and the native resource layer. Its wording is
not the identity source.

Final candidate SHA-256 is
`55445D18A3DB6634280FA7C0E649324C1CBD2F5F9A4F0C2D8118D9F4FE9A0B3A`,
graph `b54c5081c4c43f86854305847ec8afc3c92f5ec80a8a0185b7f67f23ffc798ae`.
Both Pergyra-seed and whole-driver native emission/compilation pass (`3bdb89`,
`0a38ce`), with all 2,239 inputs matching (`cae9b9`). Final readonly gate:
eight values on four execution paths, 28 MIR refusals and 12 native/public-C/
public-LLVM source refusals PASS (`dfb05a`), evidence
`.tmp/self_hosted/readonly-array-int.tr5RXt/`.

Five earlier String/value/copy-out/owned-record/diagnostic gates pass on this
binary (`3751b8`, `6101af`). Two source pins pointed at a retired import seam;
they now inspect the actual call admission owner. Three no-op-parameter
mutations assumed any readonly carriage was unsupported; they now remove the
required carriage fact. The new gate separately rejects a truly mutating
readonly parameter. Old artifacts were copied and hash-verified before fixed
test paths were refreshed: 269, 172 and 224 files in the named readonly-source
preimage directories (`c6b04b`, `5eb8eb`, `30ab0d`). These are recoverable test
outputs, not deletion of user source.

Bounded bootstrap compares source-C, MIR producers and both consumers of the
same Pergyra-produced MIR through the original runners/manifest/comparator
(`9a24ed`). It reuses the two current-source driver builds and does not claim
the full stage2/gen3 fixed point. One temporary harness wrapper first lacked
its PGY variable (`0e11ad`); correction used a fresh output directory and no
compiler/comparator change.

The next falsifier is still the actual hash owner, now reduced to ToString(Long).
Its verified MIR has ToString around DirectMirCfgHashString at row 4485/node 8.
The two-line `long_to_string.pgy` under `readonly_source_boundary_20260906/`
prints 17 in public C and rejects LLVM at builtin-call node 4/row 1 (`fb13f7`).
The integer ToString signature/readiness owners currently require Int; no Long
fix has begun. Neither the actual hash direct-MIR execution nor the original
assignment integration was rerun to completion on 55445D18. No new SoT status,
installed replacement, commit, push or remote CI result is claimed.

Final static evidence: registry edges remain 88/185 and CLOSED55/BRIDGE32/ACTIVE1
(`04e85a`); keyword/protocol/changed inventory and full size gates pass
(`163367`, `fa1a20`). The separate Coq adequacy gate exits 1 because no rocq/coqc
is available in the current shell (`032151`). No missing-prover override is
used. This is not a formal adequacy PASS. Full component inventory and full
admission matrices were not rerun in this bounded continuation.
Documentation quality passes (`76bdd1`), and strict UTF-8/NUL validation covers
139 live dirty paths; all 2,239 compiler input hashes still match after inventory
generation (`548e1a`). Shared binaries are unchanged, 147 paths remain dirty,
index is empty, HEAD/origin/main is still 5b97f2e1, and D: has 20,279,492,608
free bytes. The final 55445D18 reproduces the two-line Long conversion failure
itself (`e07950`); this is the next implementation input, not an inferred RED.

### Signed-integer ToString closure, isolated 2A712317 checkpoint

The canonical C and LLVM materializations already accept signed 64-bit values
and use `%lld`; the signature-prefix and expression-readiness owners were the
Int-only restriction. Normalized kind 14 now has the signed-integer owner name,
without changing its identity or adding a runtime row. Call admission projects
the exact Int/Long operand type from the existing generic builtin signature.
Prefix validation consumes that type, rather than overriding it to Int. String
and Bool retain their existing kinds. The old Int-only owner name is removed
from every consumer and forbidden by the actual structural inventory.

Candidate SHA-256:
`2A712317CA918FB3646D58CB0C6450CE6D73C1792F4FB53725BE438DEF88E2CE`.
Source graph:
`5407813046B415D94E8C620AD75BBE389E72B15BFDFF2C43B43DE93722A88619`.
Official Pergyra-seed build and full native-driver emission/compilation pass
(`eac179`, `4b0e88`); all 2,239 inputs matched before the subsequent read-query
mode changes (`6cda70`). Compiler owners stayed within their existing caps.

The new gate passes eleven outputs through native/public/direct C and LLVM,
display-only artifact equality and 12 explicit input refusals (`3b30e7`):
`.tmp/self_hosted/signed-integer-tostr.7vxMxP/`. It covers literals, locals,
parameters, nested calls, values outside the stored Int range, signed extrema,
and Int/Bool/String controls. The prior 55445D18 rejects the new source at the
same Long builtin admission (`097cdc`). Readonly eight-value/28-MIR/12-source
regression passes on 2A712317 (`aaaf4d`, `readonly-array-int.KuRnTr/`).

An independent native large-literal defect remains open. The initial maximum
literal `9223372036854775807L` emitted `-9223372036854775808LL` before ToString
(`ba99bc`, `460e54`). Original fixture SHA-256
`8BF9FF6CBF8F5C27A3ACDC46267E96B45D71E8A3A9A21412FA33456815652E51`
is preserved in `signed_integer_to_string_20260906/native_long_literal_witness.pgy`
under `.tmp/self_hosted/`; logs/output remain in `signed-integer-tostr.BzPYz3/`.
The conversion fixture constructs the same maximum using exact Long arithmetic
and computes minimum as `0L - upper - 1L`; expected values are unchanged. This
proves conversion width, not large-literal source production or global numeric
semantics. No native literal fix or ignored expected failure is claimed.

The actual hash gate on 2A712317 next refuses public C source admission with
`borrow_boundary_escape`, default Array<String> (`42e327`), evidence
`cfg-identity-digest.XMzrQC/`. The preserved typed MIR identifies three edges
from readonly index fields into value parameters (`b3eda7`): receiver row's
source-ID input, and match-binding name/type inputs. Their existing consumers
only read elements and return scalar Bool/String. The receiver row, its unique-ID
query and the two match lookup parameters now preserve `ref`; no array copy,
owner replacement or guard relaxation is introduced. A fresh source-matched
Pergyra-seed/native build is in progress under `hash_readonly_consumers_20260906/`.
Actual hash execution and the original assignment integration are not yet
current-source PASS. This checkpoint does not change SoT/substitution metrics.

### Read-query source-matched F1A7B721 checkpoint and UTF-8 reduction

The fresh candidate is
`F1A7B721C90E72BE5324E95A5DD8201707EB956E452EE688391A943C525C491B`,
graph `02ff15d5e6ce50d3aa5578c5064e1b4581f14e065789a96ab2fdee6bf9f44684`.
Pergyra-seed/native full-driver builds pass (`6a08be`, `f33a27`), with zero
native errors/four existing warnings. Its 2,239 inputs match after inventory
generation (`2cd637`). Signed conversion passes all six paths and 12 refusals
(`f09af8`, `signed-integer-tostr.jpF3EU/`); readonly eight-value/28-MIR/12-source
regression passes (`7d85e2`, `readonly-array-int.KoeJhE/`). Bounded bootstrap
source-C/MIR-producer/common-MIR-consumer comparison passes (`70049f`) using
the existing gate runners/manifest/comparator and both current-source builds.
It is not a whole-driver fixed point.

Actual hash public/native C execute the same nine reference values (`6a2314`).
Public LLVM now rejects expression-kind node 6 / row 4489 (`9156d2`), the
literal `"한글🙂"`, after the earlier Long and source readonly restrictions.
Evidence: `.tmp/self_hosted/cfg-identity-digest.cYcV1o/`. The two-line
`hash_readonly_consumers_20260906/utf8_string_literal.pgy` reproduces public C
output and LLVM refusal at node 2 / row 1 (`b8d985`). String spelling admission,
decoded-payload embeddability and LLVM byte spelling are all currently ASCII-only.
The final objective card records this next boundary; no UTF-8 implementation,
actual direct-MIR hash result or original assignment rerun is claimed.

Registry edge status remains 88/185, CLOSED55/BRIDGE32/ACTIVE1 (`4c403a`).
The first MSYS invocation exited 1 without output; the explicit configured
Python invocation passed. Size/changed inventory pass (`a61a72`, `2bc48e`).
Keyword check found stale generated counts; the existing generator refreshed
its owned inventory, then keyword/protocol gates and all compiler hashes pass
(`2cd637`). There are 158 dirty entries, zero staged paths and unchanged shared
bin hashes; D: has 20,129,632,256 free bytes (`f844b6`). No commit/push/remote CI.

### UTF-8 projection and value-array readonly reborrow

The three existing literal owners now preserve admitted UTF-8 payload bytes.
The existing JSON decoder remains authoritative; byte spelling admits opaque
high bytes without changing delimiter/escape/control rules. LLVM emits those
bytes as two-digit hexadecimal escapes and accumulates through TextBuilder,
not repeated payload concatenation. docs/110 byte length and normalization-blind
equality remain unchanged; no general Unicode validator is claimed.

C100CF6E / graph EC270058 builds via Pergyra-seed and native full-driver paths
(`eb0073`, `b22156`), with all 2,239 inputs matching (`ed79dd`). The new six-path
literal gate passes multibyte/escape/length/normalization, semantic-vs-display
identity and 14 refusals (`c0c6f3`, `utf8-string-literal.0N6p7V/`). Old F1A7B721
rejects the same source (`a98153`). Long and readonly regressions pass (`cd719a`).
This fixes the reached literal projection, not all hash or Unicode semantics.

Actual hash LLVM next rejects routine 146's value Array<String> parameter 5
forwarding to readonly parameter 0 (`2a5a79`, `cfg-identity-digest.Hgt6Hj/`).
The eleven-output readonly fixture reproduces native success and old C100CF6E
self-host rejection (`517c3e`, `readonly-array-int.s8CK4E/`). The same source
form already worked for Int; changing this caller to ref would hide the gap.

The shared reborrow target now accepts value Int/String arrays. One LLVM
entry-frame storage owner consumes the existing exact ABI projections and
materializes a local header slot. Readonly calls borrow that slot; elements,
ownership, cleanup and copy-out authority are not copied or widened. The old
Int-only value storage function is deleted. Source/caps/old-path checks pass
(`ac349c`, `e1f470`); the registry only adds derived consumers, not an authority.

First builds rejected my nonexistent TextBuilderDestroy call (`e54e74`,
`76ee52`). Source SHA 3C597879 and logs remain in `array_value_reborrow_20260906/`;
this attempt is RED. Corrected code uses existing Finish/AllocatorDestroy.
The corrected candidate is
`87DCF19628254B1A04CCF86D261D42CBA138EFD66914DCE3056ABE9B2D6CE29C`,
graph `0e4a3d804ed7e5636c30ce2f68e17b65dc40fb8dab7a8d3edbd51afdffc65641`.
Pergyra-seed `79445` and native full-driver `32166` pass (`dc0e04`, `553606`),
with zero native errors/four prior warnings; all 2,240 inputs match (`9d8de4`).
Build/evidence directories are `array_value_parameter_frame_20260906/`.

The new candidate passes eleven native/public/direct outputs, including a
nonzero String parameter ordinal and caller contents after reborrow; all 28 MIR
and 12 source refusals remain (`94c752`, `readonly-array-int.YsrahC/`). UTF-8
six-path/14-refusal and Long eleven-value/12-refusal regressions pass (`67b5de`,
`e64a5b`). Bounded bootstrap reuses both already-built drivers and the existing
manifest/runners/comparator for source C, MIR production and one common MIR
consumer (`e7b25c`), not full-driver fixed point. The keyword inventory drifted
and was regenerated by its owner; keyword/protocol gates and all source input
hashes then pass (`b4e034`). The actual nine-value hash gate now passes native/
public C, public LLVM and same-MIR C/LLVM (`64b8fd`), evidence
`cfg-identity-digest.6be7Bd/`. Clang retains its host target-triple override
warning; no missing backend or native fallback is counted as success.
The original assignment integration ran once on 87DCF196, session `11117`,
within the existing 1,800-second bound. Its 266 prior files are
hash-verified in `array_value_parameter_frame_20260906/assignment_preimages/`
(`b004cc`). The older 908D9A4E assignment PASS is not this candidate's result.

The new run is terminal RED at public C source admission (`ae7327`), before
LLVM: readonly Array<String> crosses a default/value boundary. Verified MIR
production refuses the same source and emits no artifact (`e95f9b`). Native
MIR production passes with one unreachable-statement warning (`b2f084`); its
105,344,313-byte diagnostic artifact has SHA
`79D293E7EF1EACB9842608885BA10EF776317259DC2BB42AC08290041450B035`.

The read-only inspector consumes native routine/parameter/call/member binding
facts and reports six String edges (`3a0785`): StepActorFromFacts,
TransitionFactsFromArtifact and ExpressionSeedOutcome each pass borrowed
participant names/types into value parameters 1/2 of
SemanticAstIntentActionCallFromText. Source inspection confirms its Array<Bool>
value flags are also read-only query input. This is bounded diagnosis, not
complete alias analysis or permission to feed native MIR around public checks.

The three-line `intent_readonly_query_reduction.pgy` imports that existing owner:
public C rejects the same boundary (`4dc520`), while native emits C without
errors/warnings (`0d00ab`). No action query is executed by this reduction.
The next objective card preserves the participant fact's ref boundary and
existing alias/subject-action/ambiguity/range rules. No query-input fix, further
large rerun, installed-driver update or completion percentage is claimed.

Final observed verification: documentation quality and source-graph hash check
pass (`3bd4cb`). Strict UTF-8/NUL covers all 158 live dirty text files, with 166
dirty entries, zero staged paths and 19,804,196,864 bytes free on D: (`4428f9`).
All current build/test/diagnostic sessions are terminal. The next query change
remains a bounded implementation task, not an external blocker or goal completion.

## Intent participant query and DIR index carriage — current candidate

The preceding no-implementation checkpoint is superseded. The existing query
now borrows `SemanticAstIntentSignatureFacts` and derives its participant slice
from the intent index. It rejects invalid snapshot/index/column bounds before
reading. The old three array parameters and independently supplied start/count
are removed, without participant copies or whole-artifact readiness replay.

The initial whole-driver builds failed (`6dd4be`, `c436ec`): the three callers
identified from assignment's native MIR were not the full production inventory.
`SelfDirIntentStepFromArtifact` still used the old seven-argument call. The
TextBuilder failures followed the first arity error and were not patched as
independent cleanup defects. The existing DIR assembly loop already has the
semantic intent index; it now passes that identity to its step query. Derived
DIR columns remain for placement, transfer and provenance, not action lookup.
Four production calls are migrated across five existing files. No new fact
owner, fallback query, AST parent scan or independently reconstructed range.

Current Pergyra-seed driver:
`C681F4BF5549091A0FE15EDD4654A8A7293B4FB63C78C002E7DAFADC2966C036`.
Source manifest:
`2085431173A2AA441095518FBA53DACAA68A23E25D6F647C4D45F0796E5725E6`.
The installer prebuild key `4e069e53...` includes seed/runtime/compiler inputs;
it is not the source-manifest hash. Build and evidence directories respectively
are `.tmp/self_hosted/compiler/intent_participant_dir_carriage_20260906/` and
`.tmp/self_hosted/intent_participant_dir_carriage_20260906/`.
Native full-driver C emission/compilation passes (`37b004`), zero errors/four
existing warnings. Seed installation log, output receipt and artifact hash
agree (`5f5cb9`, `fc7678`); its expired terminal handle does not expose an exit
status. All 2,240 source inputs match, including after owned keyword inventory
generation (`a9325e`, `1b4a65`). Shared binaries remain outside this build.

Observed current-driver verification:

- The new actual query/actor-order fixture passes all eighteen results on
  native/public C/LLVM (`be8c2e`, `intent-participant-query.4Rudes/`). Synthetic
  malformed participant rows test the bounded query, not complete admission.
- A bounded slice of existing Checkout's gate passes source/MIR C equality,
  five runtime outputs with independent native C/LLVM, and six identity/type
  refusals (`d02e5c`, `intent_execution/`). The original gate's C command lacks
  `--native-pipeline`; the isolated runner adds it explicitly. No claim that
  the unmodified full gate or its chained observability suites ran.
- A second bounded slice uses the canonical multi-intent example and existing
  success/failure assertions (`2bc1af`, `nested_intent_execution/`). This checks
  several source intent indices, not only Checkout's first-index path.
- Readonly eleven outputs/28 MIR/12 source refusals pass (`239426`,
  `readonly-array-int.R6G25K/`). Actual nine-value identity hashing passes
  native/public C, public LLVM and direct C/LLVM (`04ab06`,
  `cfg-identity-digest.VDMixz/`). Those two directories are under
  `.tmp/self_hosted/`, like the query gate's evidence directory.
- Reused native/seed driver builds pass the owned bounded source-C, MIR
  producer and common-MIR consumer comparisons (`f5210d`, `bounded_bootstrap/`).
  This is not complete-driver stage2/gen3 fixed point.
- Changed structural inventory/caps, keyword/protocol and full size gates
  pass (`415024`, `b49428`, `330867`). Earlier size runs stopped during the
  header scan without a diagnostic; the unchanged 60-second gate passes with
  no compiler build running. The cause of those earlier exits is not proven.

The original assignment gate ran once as session `92648`, under 1,800 seconds,
and is now terminal PASS, exit 0 (`2f66c7`). C and LLVM produce the same five
output lines; all C seven/LLVM six missing-fact refusals and the owned artifact
comparison pass (`a8953e`, `969b3f`). No backend skip. The last LLVM compilation
heartbeat is 1,140 seconds; Clang reports the existing target-triple override.
Producer PID 92176 was the exact source-to-LLVM child and has exited. Its
120-second observation
has 124.6 CPU seconds, 1,688,764,416-byte working set and 1,762,336,768-byte
private memory (`a4c8f8`); these are snapshots, not peak or throughput claims.
All 270 previous fixed-directory files, 40,441,665 bytes, have hash-verified
copies in the current evidence directory's `assignment_preimages/` (`dff976`).
SoT remains 88/185, CLOSED55/BRIDGE32/ACTIVE1 (`a9325e`). No substitution
percentage, full-Intent proof, installed shared driver or dirty-source CI pass.
Documentation quality and strict UTF-8/NUL/whitespace checks pass (`ee9c08`,
`aa78b5`), 165 live text paths/173 dirty entries/zero staged paths. Shared
binary hashes remain unchanged (`62c2fa`). D: has 19,671,822,336 free bytes.

### Read-only observation during the same LLVM integration

The next goal turn confirms session 92648 and producer PID 92176 are live
(`4af530`, `51398a`); it does not restart the gate. One GDB batch attachment,
with function calls disabled and no source/process-memory edits, captures a
stack and explicitly detaches (`ac41e9`). The trace is preserved as
`intent_participant_dir_carriage_20260906/llvm-owner-stack.log`. Debugger wall
time is about 1.9 seconds; this run must not be called untouched timing data.

Observed stack: GraphPlanFromAdmitted -> AppendRoutine -> AppendExpression ->
PayloadEnumMemberMarkerReady -> PayloadEnumMemberFromGraph ->
ReferencedEnumFactReady -> pgy_as_get. This confirms that the original source
has reached GraphPlan expression admission, not a native Clang build or the
former Intent source refusal. It does not prove a hotspot percentage.

Current source explains a repeated operation worth measuring after this run:
expression admission calls the marker query inside its node loop; the marker
query scans later candidates; MemberFromGraph calls full referenced-enum
readiness before rejecting a non-member shape. Readiness recomputes the digest
and checks the complete enum/variant table, including uniqueness loops. Initial
route admission already validates the same fact. Sources are
`direct_mir_scalar_program_expression_admission_owner.pgy`,
`direct_mir_scalar_program_payload_enum_expression_owner.pgy`,
`direct_mir_scalar_program_referenced_enum_fact_owner.pgy` and
`direct_mir_scalar_program_route_admission_owner.pgy` (`99148f`, `fe4ed4`,
`33aac6`). The redundant call path is observed; its total runtime share and
whether existing receipt lifetime permits removing any check remain unproven.
No readiness check, source owner, memory allowance, cache or test is changed.
The original integration subsequently passes, so this observation is not a
functional blocker. It remains bounded performance evidence, not a parallel
optimization or general query-engine implementation queue. The outer goal now
continues with source-matched full-bootstrap session 73945 under its own
objective card. The already-green native/seed builds and bounded artifacts are
reused; the original full MIR producer/consumer phase owns the new falsifier.

### 2026-09-07: full-source readonly queries and native work accounting

Session 73945 fails before full MIR on readonly Array<Int> -> value admission.
The one diagnostic native-front dump has 7,619 routines and SHA-256
`7B170C5994A857BE884FCADDB16A5D3AB3E92D93131B67403B9044DCF20AAF70`.
The bounded streaming inspector, validated on the six old Intent edges, finds
136 exact-ID Int/String edges into 37 functions. It does not resolve 5,156
other edges and is not complete alias analysis. This 1.35 GB dump is never a
bootstrap input. Native construction of it succeeds; the Pergyra full-source
producer has not yet succeeded.

The existing read-only functions and their transitive readers now preserve
93 ref parameters across 51 functions/27 files. Preimages prove unchanged
function bodies, guards and result construction. Mutable/owned builders are
not converted. The source signature gate rejects the preimage declarations.
The generic owner-import reduction changes from public refusal to emitted C.
All 23 new query results match in native C/LLVM and public C on C681F4BF. Its
aggregate run and the new C7920411-only public LLVM run hit their 300-second
bounds with code 143 and no artifact. Neither is a parity pass or independent
semantic refusal; do not repeat the entire matrix without addressing that
compiler-shaped workload. Original assignment PASS remains historical.

The new native full-driver build initially fails at the summary owner's global
4,096-unit work count; 246 subsequent diagnostics are secondary. Two valid,
nonrecursive reductions distinguish scope from recursion: 4,096 independent
demands pass; 4,097 fail after exactly 4,096 body evaluations, zero recursion
hits. Invalid preliminary `--check` commands are not semantic evidence; the
observed reductions use native `--hir`.

The existing native owner now resets the work counter only when beginning a
new independent demand episode. Its cap, fixed-point guard, failure propagation
and completed-entry reuse stay; body-evaluation telemetry remains cumulative.
The expanded native gate proves independent demands pass, one oversized closure
still refuses, and existing recursive/escape controls retain their outcomes.
No general cache/query engine, bigger budget or swallowed failure.

Artifacts and evidence at the read-query checkpoint:

- Seed C7920411 / source-input manifest 435A49A7, isolated seed build PASS.
- Native frontend 4C7786E0, independent full driver FFFBF13F; build PASS with
  zero errors/four existing warnings (`3df9a8`).
- Current readonly eleven values/28 MIR/12 source refusals PASS (`e26813`).
- Expanded native summary gate PASS (`4e9f5b`), including oversized-closure
  refusal at the unchanged work cap.
- Source-C/MIR-producer/common-MIR-consumer bounded bootstrap PASS (`38b74c`).
- Owned inventory regeneration, keyword/protocol and 2,240 matching source
  hashes PASS (`91be97`); the initial generated inventory drift remains recorded.
- Full current-source bootstrap session 16470 is live at `full_mir_seed`
  (`ec9b8b`), not a completed fixed point. Source remains frozen for this job.

The active handoff owns navigation and full artifact hashes, not semantics.
Evidence is in `compiler_readonly_query_carriage_20260906/`; old failed builds,
public LLVM terminations and source preimages remain preserved. SoT counts and
completion percentages are unchanged. No shared install, commit/push or dirty-
source remote-CI claim. Rocq remains unavailable, with no formal skip override.

### 2026-09-07: call-spine projection and the next bounded LLVM frontier

The C7920411 full-source producer subsequently completes its 286,073,435-byte
MIR, SHA-256 `6C7993EA0DBD03BE924603D6DB097932214E9C940B0DAE691E68857582A80CE3`.
Full bootstrap 16470 nevertheless ends RED at its 1,800-second boundary: the
native oracle stops in generic analysis without an artifact or stderr diagnostic.
The original readonly refusal is passed, not the complete fixed-point gate.

A read-only stack and source show named-boundary admission scanning the
complete remaining expression arena for each call. The existing call-view
owner now derives a temporary call-root array in two node passes. Reverse
overwrites retain the forward scan's first-continuation behavior, including
branching DAG spines. Named-boundary and fresh-constructor queries consume
that view once; arguments/generic actuals still use the existing ordered view.
No new semantic owner, copied graph, serialized index or scan fallback.
No actual heap reclamation is claimed from this logical lifetime alone.

Current source-matched build is D56862B2 / manifest 657A0A7A, independent native
BC77A185, built with native frontend 4C7786E0. Both builds pass; the native build
has zero errors/four existing warnings. Current native/public C/LLVM projection
and readonly eleven-value/40-refusal gates pass (`0b063c`), as does independent
bounded bootstrap (`457def`). All source hashes, keyword/protocol, changed
component/caps and current full size pass. The source gate rejects the old
consumer. First test drafts failed on untyped empties, a native LLVM member
ArraySet limitation, and Args() excluding the executable name; logs are kept.
Their corrected fixture/gate does not change compiler semantics for those cases.

Three-run median process times, including startup and graph construction:

| Nodes | Calls | Old scan, seconds | Indexed, seconds |
| --- | --- | --- | --- |
| 1,408 | 256 | 0.022325 | 0.016097 |
| 2,816 | 512 | 0.039912 | 0.016666 |
| 5,632 | 1,024 | 0.110519 | 0.017436 |

This is a bounded owner-query measurement, not a full-compiler benchmark or
machine-independent timing threshold. `call-spine-current-projection.log` and
`call-spine-roots.FLNb7D/` retain the exact binaries, inputs and results.

The old missing public LLVM query leg alone is retried on D56862B2, not the
entire matrix. It still ends at 300 seconds, exit 124 / child 143, without an
artifact (`510f1a`). An identity-checked read-only stack, explicitly detached,
reaches LogicalRecordFactReady -> LogicalRecordMemberMarkerReady ->
AppendExpression in GraphPlanFromAdmitted (`1b29e1`). Source confirms full
record hashing/validation precedes node-kind classification. This is a reached
repeated-operation observation, not its time share. The first attach saw only
the debugger break-in thread and supplies no compiler-work evidence.

No logical-record validation is changed in this slice. The next evidence card
bounds shape-guard/validation-lifetime inspection before another build/run.
All jobs are terminal, source freeze released; full current-source producer
comparison/gen2/gen3 and the 23-result public LLVM leg remain open. SoT counts,
percentages, shared binaries and publication status do not advance.

The final affected-block inventory check also exposes the call-view owner's
old 160-line pin (`b11645`): the cohesive root projection adds 39 lines, for
189 total. Its local cap is deliberately updated to 200 with the objective
card's justification; shared policy is unchanged. The original ordered-view,
codegen-consumer and readonly-boundary blocks are included, not only the new
marker block. This is an inventory allowance, not semantic progress or evidence
that the entire component-contract script has been executed.

### 2026-09-07: bounded logical-record noncandidate guards

The reached expression owner now checks bounds and member/marker node shapes
before whole-record validation. Marker parent-kind classification also precedes
the table check; actual field and right-child reads remain after it. Matching
candidates retain FactReady, digest, declaration/field identity and ABI checks.
The fact, digest and ABI owners are unchanged by this slice. There is no new
admission receipt, trust flag, cache or semantic authority. Repeated validation
for actual record candidates remains; this does not claim a once-only admission.

The exact source preimage F73F39C4 is preserved in `record_query_preimages/`.
The real-owner fixture compares missing/bounds/nonmember queries, member
type/ordinal/row, unknown fields/types, stale digest, duplicate field identity,
invalid ABI absence and the canonical empty fact. Native C before/after produces
the same seventeen lines (`8c6f0b`, `b47d12`). The first test draft omitted the
expression-kind import and failed to compile; its log is kept separately. This
was a fixture correction, not a compiler import-rule change.
The preserved native probe binaries are B480113D (before) and CA501691
(after), with fixture AC2D8A8F (`b1aae7`); the full-driver candidates below
are separate artifacts, not these small measurement executables.

Fixed 2,048 noncandidate queries per process, three-run medians including
startup, construction and the single setup fact validation:

| Record fields | Before, seconds | Shape guards, seconds |
| --- | --- | --- |
| 32 | 0.070936 | 0.024593 |
| 64 | 0.154135 | 0.021871 |
| 128 | 0.465432 | 0.025636 |

These are bounded owner-query timings, not full-compiler performance, heap
measurements or portable timing thresholds. Original logs are
`record-shape-before-pressure.log` and `record-shape-after-pressure.log` in the
same evidence directory. The source-order gate rejects the actual preimage;
all three original expression-owner component checks and the added inventory
with its real queued caps pass (`ac3ab8`). This is not the whole component gate.

Current isolated Pergyra-seed build is 723F5707 / source-input manifest 6C95B095
(`8abef6`), independent native driver 234AAB7E (`0aec1b`), with unchanged native
frontend 4C7786E0. Source-C, MIR-producer and common-MIR-consumer bounded
bootstrap pass (`9eaf98`), not full fixed point. Owned keyword inventory,
keyword/protocol and all 2,240 source hashes pass (`335d3f`). The current full
size run is terminal at 60 seconds, exit 124 (`aa8837`), with only its initial
shared-policy checks observed; it is not replaced by an older green result.
Public query and existing direct-record evidence continue under the active
handoff. No shared install, publication, SoT count or percentage advance.

The original direct-record gate subsequently passes C/LLVM ten runtime outputs
and ten malformed-input refusals (`e8abf7`). New seventeen-result owner tests
pass native C/LLVM and public C; public LLVM expires at the aggregate
300-second matrix budget (`da97d6`, child 143), not a semantic refusal or
four-way success. Its read-only stack reaches payload-free enum marker
suffix scanning (`3543a2`); enum owners remain unedited.

The original twenty-three-result public LLVM leg, run once after the record
change with its unchanged standalone 300-second budget, is also terminal RED
(`70964f`, child 143/no executable in `bd2db1`). Its own read-only stack
`ae97e2` reaches logical-record digest/readiness through Row -> FieldType ->
ConstructorFromGraph. Source confirms repeated whole-table validation for
each constructor argument after an earlier validated row. This untouched
actual-candidate query is the next active falsifier; enum scanning is secondary,
not another implementation lane. Both stacks were identity checked and
explicitly detached; neither measures runtime share or provides clean timing.
All jobs are terminal and source freeze is released. Full fixed point and the
original LLVM query remain open. Current docs quality passes (`e1f7c6`).
Final affected component/caps, all source hashes and refreshed documentation
quality pass (`82a5bc`); no full-component or full-size PASS is inferred.

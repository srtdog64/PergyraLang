# Reached ArrayString index and mutation receiver admission

Status: DELIVERED (bounded admission seam, zero complete registry rows).
Base HEAD: `669d0b904f7d687dd8f99b2dad00ed45b6892daf`.
This is checkout-scoped executable evidence, not a new semantic authority.
The 69 pre-existing dirty entries remain outside the delivery scope.
Coordination: `docs/agent_work_directives/sot_sequential_closure_2026-10-01.md`.

## Objective and reached boundary

The installed source-to-MIR producer and direct C/LLVM projections reach the
general `DirectMirScalarCfgGraphPlan` route. The normalized
`DirectMirScalarProgramExpressionSet` and exact carried CFG/ValueId/LocalRef
identities own the facts. General admission is the last common consumer;
target materializers only project admitted storage and canonical runtime
bounds behavior. Do not read display text, reconstruct source/AST, retry a
legacy route, discard a receiver use or infer a length from capacity.

The installed baseline driver SHA-256 was
`35145D3FB9F19DEBEB23F9028528A888FE540A39032F4F68749AFC0E43F8DF33`.
Independent evidence: `.tmp/self_hosted/sot-index-gate-probe.UkM9FJ`.
All six existing positive variants projected and executed through C/LLVM.
Eleven existing negatives refused; six negatives were accepted by both
backends: `bad-while-init`, `bad-while-step`, `bad-set-receiver`,
`bad-set-index-oob`, `bad-set-index-negative`, `bad-post-read-oob`.
Those accepted negative artifacts were never executed. The general C/LLVM
get functions were unguarded; the old LLVM unsigned comparison belonged to
cleanup, not the read. The crossed receiver produced byte-identical output.

## Bounded implementation

- `direct_mir_scalar_program_array_string_extent_owner.pgy` derives immutable
  literal extents once. Multiple definitions, length-changing operations and
  call exposure preserve an explicit unknown/runtime obligation (`-1`).
- `direct_mir_scalar_program_array_index_induction_owner.pgy` rejects only a
  definite negative initializer or decreasing direct backedge. Incoming phi
  predecessor roles, definition blocks and read-before-update order matter.
- `direct_mir_scalar_program_array_index_execution_owner.pgy` derives known
  logical Bool outcomes and possible block execution from admitted facts.
  Unknown conditions retain both edges; every routine retains its entry.
- `direct_mir_scalar_program_array_index_static_bounds_owner.pgy` checks
  executed expression children, both operation lanes, conditions and returns.
  Visited nodes prevent repeated DAG traversal. Provably non-executed
  short-circuit RHSs and constant-dead blocks have no new bounds obligation.
  All original typed shape/identity checks still run before this consumer.
- `direct_mir_scalar_program_array_mutation_receiver_use_owner.pgy` joins the
  local receiver use to exact typed LocalRef and existing latest-dominating
  value authority. Parameter use offset zero is retained. Pop graph absence
  means explicit JSON null, never a missing or malformed field.
- General C/LLVM get projection now consumes canonical length-based panic
  contracts before address/load. LLVM declares the same owned panic export.

These are derived computation/admission consumers, not top-level fact owners.
The existing registry row retains all earlier consumers and forbidden paths.
SoT remains `CLOSED=70 BRIDGE=23 ACTIVE=2`: this delta closes zero complete
push/pop/program-plan registry rows. It is not a new C-owned compiler path
substitution or stage-level Pergyra-native dogfood.

## Verification history and falsifying cases

Development-only native-built probes are never installed as a self-host driver.
The final such probe at
`.tmp/self_hosted/sot-index-closure.Gn0gUk/validation.YgYV6I` passed 72
preserved-output refusals and 20 positive executions. Input/probe hashes were
preserved. Prior path/CRLF-invalid attempts are not verification evidence.

The first Pergyra-built private candidate (`candidate/` under that same root)
passed the existing String-array gate at
`.tmp/self_hosted/one-mir-string-array-mutation.Ybfsvp`: 34 preserved-output
refusals and exact C/LLVM execution. Its receiver gate reached four normal
programs, then refused a supported short-circuit source with readiness 29.
That candidate was not installed. The independent baseline had executed the
same `!true &&`, `!false ||` and `if false` source successfully. The final
execution-obligation consumer repairs this confirmed over-refusal.

Maintained executable gate:
`self-host-array-index-receiver-admission-test-smoke` in `Makefile`.
It is attached to the existing scalar graph-plan CI aggregate. The existing
17-negative String matrix is unchanged; the new producer-derived receiver
fixture supplies 19 mutations for both backends. The four normal regression
fixtures are source-issued, including preincrement-before-read, decrement-
then-break, literal-negative-then-break and decisive short-circuit/dead-body
behavior. Corrupted negative MIR is projected for refusal only, never run.

### Final private and installed evidence

The second private candidate (`candidate-final/`) passed the new receiver gate
at `.tmp/self_hosted/array-mutation-receiver-use.VBcAgr`: ten C/LLVM executions
and 38 preserved-output refusals. The extracted existing String gate passed
at `.tmp/self_hosted/one-mir-string-array-mutation.1CCRBJ`: ten executions and
34 preserved-output refusals. Public ArrayDrop passed at
`.tmp/self_hosted/public-array-drop.rImed9`. The adjacent ABI gate stopped at
`.tmp/self_hosted/array-string-layout-closure.Z8uJ4m`: DirWalk LLVM could not
compile because a materialized get referenced an undeclared bounds panic,
even though there was no source-level indexed read. This candidate was not
installed. The foreign declaration consumer now shares the admitted storage
readiness predicate; a development probe compiled all sixteen frozen ABI
bases at `.tmp/self_hosted/sot-index-closure.Gn0gUk/abi-declarations.Zb0oF7`.
This is compilation-only evidence, not their execution or installed evidence.

The final private candidate (`candidate-delivery/`) was built by the standard
Pergyra codegen seed in 13m31s, exiting 0 at 23:14:40 KST. The native compiler
was not used to compile the replacement driver source. All four gates passed
privately and then again with the default installed driver:

| Executable gate | Private evidence | Installed evidence |
| --- | --- | --- |
| String index/mutation | `one-mir-string-array-mutation.OIfN7h` | `one-mir-string-array-mutation.wn5L5Q` |
| Receiver and normal induction/short circuit | `array-mutation-receiver-use.NJiI02` | `array-mutation-receiver-use.IEycr3` |
| Complete existing ArrayString ABI | `array-string-layout-closure.7kWXcV` | `array-string-layout-closure.Bf51EU` |
| Public ArrayDrop, all stages | `public-array-drop.lZksHE` | `public-array-drop.qZIf0w` |

All directories are under `.tmp/self_hosted/`. In each run, the String gate
projected six variants and executed five semantic variants through C/LLVM,
reusing the byte-identical display-only result: ten executions and 34
preserved-output refusals. Receiver admission executed five source-issued
programs through C/LLVM and refused 38 corruptions with prior outputs intact.
The ABI gate executed sixteen unique bases (13 general, three legacy) through
C/LLVM and checked 102 coherent ABI plus 28 reached-route refusals. Public
ArrayDrop retained its native/public source and issued-MIR positive/negative
matrix. Corrupt negative artifacts were never executed.

Installed driver SHA-256:
`707DCD40049A1697A5827B2A7C8D3CF509573AA3C0031F2EEE338C9FA0D78EC7`.
The byte-identical companion manifest remains
`0A83B0DB5EFE3C00C6D9413C63045C4B17AFF079781213B280442C588E5A9C19`;
the unchanged native compiler remains
`A5DFE2CB3258216AB546EA39EEF65839D45100950D7DF7295D756DA9F72B3936`.
The frozen and independently current source graph is
`cb54d9e661afd669622029e305a34de2a43a82900f655205bb8a63afa8da6e4e`:
all 2,446 source hashes were checked with zero mismatches. Preserved unrelated
source comments are included; this is not a clean-HEAD-only binary build.
The prior driver/manifest are recoverable from
`.tmp/self_hosted/sot-index-closure.Gn0gUk/candidate-delivery/installed-backup/`.
The driver was atomically replaced after verification; its identity-only
`installed.output.receipt` is not fixed-point evidence.

All 366 shared scalar-owner comment-excluding caps passed without raising an
existing cap. The extracted String gate and mutation generator remain at
129/130 and 167/180 code lines. The emitted-contract and execution-contract
test owners separate structural projection checks from executable behavior;
the component smoke remains only an inventory/residue gate.

## Broader limits and next closure obligations

The authority-edge gate passed at 95 authorities, 196 derived carriers and the
unchanged status census. The lexical counter's 31 tests passed. The large
component inventory exited 1 after its match-pattern checkpoint without an
owned failure message; it is not green and did not certify this new inventory.
The unchanged native semantic size gate still fails for four C owners:
873 `collection_ownership_fact.c`, 611
`type_checker_builtins_ownership_nominal.c`, 655
`type_checker_call_generic_where.c`, 689 `type_checker_intent_step_sequence.c`
(comment-excluding limit 599). No full matrix, fixed point or green CI is
inferred from focused local gates.

Legacy Pop source/effect receipt generalization remains a proposal, not a
started successor rung: its source/effect counts and backend length authority
must be migrated before its full registry row can close. Push/ArrayInt and
general program-plan obligations also remain. Aggregate-formal deep lifetime
is still ACTIVE at `collection_parameter_member_move_deep_drop.pgy`.
This and the preceding ABI closure are at most two consecutive SoT-only
deliveries; a third needs a named real C-owned executable substitution, or an
explicit missing-fact/owner/last-consumer/falsifier BLOCKED card. Do not count
deleting an already-Pergyra-owned legacy emission path as that substitution.
Alrescha stays independent; no TypeSafe/Jev call was added to compiler code.

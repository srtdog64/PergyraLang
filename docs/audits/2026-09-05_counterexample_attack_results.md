# Counterexample attack results — 2026-09-05

Status: `READ-ONLY CAMPAIGN COMPLETE — SOURCE RUNG PUBLISHED; TWO CI RUNS REACHED 29/30; FINAL INTEGRATION EVIDENCE LOCALLY GREEN; SUCCESSOR REDS NOT REPAIRED`

Observed repository base:
`b4cfb0f2ef883d8f5b80ef96eafd89c1898c3d30`.

Completed direct-MIR repair checkpoint:
`123f088949a9d62095b0f181dadd387959b22064`.

Native-parser identifier repair:
`bbeb75d7bfeeeda1dc703699d127838ab8c21876`.

Static recurrence ratchet:
`4bf7d98881cc6bfb408aadeaefe8c6bcf42d6747`.

Local source-semantic active-variant repair checkpoint:
`95148fdc490cf8e859f4341e54a442d565de97dd`.

Published audit/resume checkpoint:
`67ab142efb74e9010ea57034e370dcacbfeb77e3`.

Local integration repair checkpoint:
`506c25272637203ee6bb8a0f6b9462d2a6f226fd`.

Local source-scan evidence checkpoint:
`63bed984af9f1b5324dc505b45fcebf6920697f7`.

Fresh candidate DRV-2:

- size: `6,710,592` bytes;
- SHA-256:
  `FB37EA36D92E9C28B6BB7162F87BA00E733255AD5E46B24A166578713DF75847`.

This document is adversarial scheduling evidence. It owns no compiler
semantics, SoT status, progress percentage, active implementation fact, or
repair priority. Temporary paths under `.tmp/` are ignored reproducer output,
not durable authorities. Three delegated agents performed read-only attacks
and left tracked repository state unchanged.

## Objective and separation

- Attack the one active direct-MIR tagged-enum payload rung with structurally
  valid counterexamples before treating its positive runtime as closure.
- Probe concurrency and imported fuzz corpora independently, without editing
  or publishing competing implementation rungs.
- Distinguish an accepted malformed or semantically invalid input from a
  backend divergence, runtime failure, diagnostic-relay failure, and a clean
  artifact-free refusal.
- Do not count seed volume, mutation count, agent count, or discovered defects
  as self-host replacement progress.

## Tagged-enum attack: repaired direct boundary

The original positive is the installed MIR for
`tests/cases/backend_compare/tagged_union/main.pgy`. One unchanged artifact now
executes exact `10`, `4`, `5`, `7` through direct C and LLVM. The attack found
three independent ways in which the prior candidate could still publish an
artifact with the wrong meaning.

### Named but inactive variant

The source semantic/MIR producer accepts both of these programs:

```pergyra
enum Pair { Left(Int), Right(Int), Empty }

func Main() -> Void {
    let value: Pair = Right(31);
    Log(value.Left._0);
}
```

```pergyra
enum Mixed { Number(Int), Text(String), Empty }

func Main() -> Void {
    let value: Mixed = Number(13);
    Log(value.Text._0);
}
```

Before checkpoint `123f0889`, direct C and LLVM both published artifacts. The
same-payload-type program printed `31` in C and `0` in LLVM. The mixed-type
program caused Windows access violation `0xC0000005` in C and printed `(null)`
in LLVM. Declaration membership and field type were proven, but the selected
member variant was never joined to the receiver SSA value's constructor.

The direct boundary now rejects both targets with exit 1, no artifact, and
`program_readiness=24`. The new plan readiness joins the member projection to
the nearest same-block definition of the receiver local and requires an exact
payload-enum constructor variant. This closes only the reached one-block
direct projection. The source semantic/MIR producer still emits verified MIR
for both invalid source programs; that earlier boundary remains open.

Ignored reproducer inputs:

- `.tmp/self_hosted/adversary_tagged_enum/audit/wrong_variant_same_type_source.pgy`,
  SHA-256
  `6A0CD0F35B03F220D203A98CCB84F23328B8988291DF60F434C6F9996C9F2ADE`;
- `.tmp/self_hosted/adversary_tagged_enum/audit/wrong_variant_different_type_source.pgy`,
  SHA-256
  `68694392DC35640B9941E2994A274FC621E03263B900124705EE602B02F95C1C`.

### Future SSA selection

The mutation changes the `c` receiver leaf and `uses=["c.1"]` of
`Log(c.Circle._0)` to `x` and `uses=["x.1"]`, where `x.1` is defined only
after the use. Before the repair, C and LLVM published artifacts and executed
`0`, `4`, `5`, `7`. The current direct boundary rejects both targets at
`stage=leaf-operand`, with no artifact.

Ignored MIR:
`.tmp/self_hosted/adversary_tagged_enum/audit/member-future-ssa-use.mir.json`,
SHA-256
`98F5C8812ED42EAF80E89209F7FB22D0EBE100D42AD1D2AC3D26E61BEEE177AC`.

### Stale SSA selection

The valid source defines `c.1 = Circle(10)`, then `c.2 = Circle(7)`, and logs
the current `c` with `uses=["c.2"]`. The mutation changes only that use to
`uses=["c.1"]`. Before the repair, both backends ignored the selected MIR SSA
generation, read the latest physical local, and printed `7` rather than `10`.
The current boundary consumes the canonical latest-dominating-local-value
fact and rejects both targets at `stage=leaf-operand`, with no artifact.

Ignored MIR:
`.tmp/self_hosted/adversary_tagged_enum/stale_local_generation/stale-c1.mir.json`,
SHA-256
`E0C74E79B3585DE70D620B41F57C8C06E6AF5F3980B47175F58ACC310141871C`.

The durable focused gate is
`tests/self_hosted/parity/one_mir_tagged_enum_payload_projection.sh`. It
executes the positive through both targets and requires nine mutations,
including the three adversarial families above, to fail without artifacts.

## Source-semantic active-variant attack: locally repaired candidate

The successor discovery became one bounded implementation lease at working
base `01f280a3566fb581d0f65d7783e678eee6c987d9`. The new semantic owner carries
only `Known(variant) | Unproven` until body admission. Stable local identity is
the tuple `(function node, declaration node, binding index)`; spelling is used
only while resolving that identity. Exact constructor assignment, equal branch
consensus, and an owned match arm may produce or preserve `Known`. Conflicting,
missing, loop-carried, or unsupported provenance becomes `Unproven`.

The first repair matrix found four further false admissions before integration:

- a match payload binder with the same spelling as an outer local inherited
  the outer local's constructor proof;
- a generic formal such as `T` was not yet a concrete enum during source
  checking and could become a payload enum only after specialization;
- a `ref` actual could be forwarded to an `inout` formal, but only the direct
  `inout` mode invalidated the reaching proof;
- a member call resolved to its canonical method but its stable declaration ID
  was not carried through native graph materialization and graph readiness.

The candidate now fails closed for all four. Direct, namespace, and member call
targets share one stable declaration-identity contract. Both `inout` and the
currently forwardable `ref` mode conservatively kill the proof. Match binders
without their own admitted lexical identity and generic receivers before
specialization cannot borrow an outer or future proof.

Source and external-MIR admission remain distinct. Source production may cross
DRV-2 only after source-semantic proof was consumed. External MIR has no such
receipt and must run the proof against its reconstructed semantic graph;
carried expression identity alone cannot grant admission.

That distinction exposed one valid-control failure: MIR match reconstruction
renders a typed arm as `subject == Enum.Variant` plus payload-binding
projections. Requiring proof without recognizing that graph equality rejected
the valid `enum_multi_payload` oracle. The repaired graph owner projects the
exact equality shape, and the flow owner refines only a stable matching subject
inside its then branch. The valid installed/public/native canonical forms now
agree again; the wrong-variant external-MIR mutation remains rejected.

Observed local candidate evidence:

- Pergyra-built DRV-2: 6,710,592 bytes, SHA-256
  `FB37EA36D92E9C28B6BB7162F87BA00E733255AD5E46B24A166578713DF75847`;
- the focused source gate accepts five positive flow families and rejects 21
  counterexamples with exact public diagnostic identity across MIR, C, and
  LLVM, with no failed artifact publication;
- the direct-MIR gate preserves exact `10`, `4`, `5`, `7` through general, C,
  and LLVM consumers and rejects nine mutations without artifacts;
- the existing statement-type and diagnostic-catalog parity gates are green;
- HIR routine identity, persisted expression-identity carriage, and installed
  C/native-LLVM prefix parity are green. The last two first exposed an old
  fixture/gate schema drift: the fixture still encoded a four-column identity
  row after `binding_syntax_id` became a separate fifth column, and the gate
  still named pre-split owner locations. The repair moves the same ID to its
  owned column and retargets checks; it does not mint a second identity fact.

Exact run `33911747694` at published head `67ab142e` completed 29/30. Full
self-host bootstrap, Rocq 9, Windows, macOS, TSan, sanitizers, codegen
bootstrap, and all twenty backend shards passed. `build-linux` found two exact
integration residues. Its role-override adjacent gate still required every
expression-graph identity to be absent, so it rejected the now-correct stable
member target ID. The language-word occurrence inventory also predated the new
owners. Repair `506c2527` makes the sealed role-override identity consume the
subject method's exact syntax ID, adds zero-ID and wrong-role-ID mutations, and
regenerates the occurrence inventory from the 146-row owner. The role gate now
passes eight C/LLVM runtime legs, three permutations, twelve MIR negatives,
and fourteen source negatives. The language registry, source 5/21 gate, hard
contract, and complete component inventory/removed-path ratchet are green
locally. Replacement exact-head CI remains pending; none of this changes a SoT
registry state or the 83% forecast.

Run `33917501829` at `47aaf032` was then concurrency-cancelled after seven
green jobs by independent proof-spine descendant `a7d0228e`. Exact descendant
run `33918274707` completed 29/30. It proved the updated Rocq corpus, full
self-host fixed point, every platform/sanitizer/codegen job, all twenty backend
shards, and both preceding Linux repairs. The sole failure was reached later in
the preparation contract: `self_host_source_scan_owner_smoke.sh` still pinned
the pre-change callable owner-set hash. Checkpoint `63bed984` updates that set
hash and the changed identity-resolution owner hash, names source revision
`95148fdc`, and explicitly says the historical performance figures were not
remeasured. The exact source-scan gate is green locally. Replacement CI remains
pending, so this is integration evidence rather than a SoT closure claim.

This evidence closes neither arbitrary alias analysis nor a whole-language CFG
theorem. Member/index/function-return provenance, loop projections, generic
specialization proof carriage, and post-match consensus remain conservative
rejections until their own stable facts and falsifiers exist. The wider rule
that currently permits `ref -> inout` forwarding is also a separate parameter-
mode contract question; this lease does not silently redefine it.

## Concurrency attack: successor reds, not repaired

These probes use the explicit native pipeline because the installed
self-host source surface does not yet carry the complete programs. They are
separate successor candidates and were not mixed into the tagged-enum repair.

### Affine `Future<T>` copied through `Array`

```pergyra
async func Worker() -> Int { return 7; }

async func Main() -> Void {
    let pending: Future<Int> = spawn Worker();
    let tasks: Array<Future<Int>> = [pending];
    let alias: Future<Int> = tasks[0];
    Log(await alias);
    Log(await pending);
}
```

Native C and LLVM emission and compilation all exit 0 and publish artifacts.
The generated C contains a raw identity copy for `Future<Int>`, then awaits the
alias and original. The C executable terminates with heap-corruption status
`0xC0000374`; the LLVM executable terminates with `0xC0000409` and
`await unknown execution lane`. The public self-host C request rejects without
an artifact but reports a malformed diagnostic receipt rather than owning the
semantic failure. This is an affine ownership hole, not a successful runtime
test.

Ignored fixture:
`.tmp/self_hosted/adversary_concurrency/array_future_named_alias_double_await.pgy`.

### `Zone` `own/ref` crosses a spawn boundary by value

Both an `own CounterZone` callee followed by parent reuse and a
`ref CounterZone` callee are accepted far enough for native C emission. The
spawn wrapper stores and forwards a `CounterZone` value while the callee ABI
requires `CounterZone *`.

- native C emit exits 0 and leaks an invalid `.c` artifact;
- native LLVM emit exits 1 at verifier time and publishes no `.ll`;
- C and LLVM full compilation exit 1 and publish no executable;
- the public self-host request rejects without an artifact, but its diagnostic
  receipt is absent.

The earlier exploratory claim that LLVM also leaked an artifact is withdrawn.
Only the C emitter publishes an invalid artifact for these fixtures.

Ignored fixtures:

- `.tmp/self_hosted/adversary_concurrency/zone_outstanding_task_use_after_move.pgy`;
- `.tmp/self_hosted/adversary_concurrency/zone_outstanding_task_ref.pgy`.

Bounded negative evidence remained green for mutable-Slot spawn/await, nested
cancellation, parallel Slot alias rejection, capability/TLS propagation,
join-any loser cancellation, pin suspension/parallel rejection, and aggregate
Slot escape rejection. Generation revalidation after automatic resume and
general panic-to-sibling cancellation remain design-only and were not scored
as implementation counterexamples.

## `F:\tex_bug` attack: successor reds, not repaired

The imported corpus root was `F:\tex_bug\corpus\pergyra\imported-local`, not
`F:\tex\_bug`. The current DRV-2 still accepts three minimized direct-MIR
documents that should fail closed:

- a root leading comma: `{,"schema"...`;
- one extra closing `}` after the root document;
- duplicate routine-local instruction IDs `0, 0`.

For each case, direct C and LLVM exit 0 with no diagnostic. C publishes a
465-byte artifact with SHA-256
`F5C0EB369EAD4E456C7BCD52E7DD670650E03B418C1B90FB50777CCE801D23EB`;
LLVM publishes a 1,320-byte artifact with SHA-256
`BBC56BBB49FA21908733EA404BF465BBBFF4D42F0D67B80730C02C0A87E8B745`.
Those hashes equal the respective valid-seed artifacts. GCC and Clang compile
them, execution exits 0, and stdout is exact `42`.

Input evidence:

- leading comma, 3,258 bytes, SHA-256
  `907A2FB3AE4FFFDDBF8907F3768880669641BD53AF37A50E7F4BB60F8A87D57E`;
- extra close, 3,258 bytes, SHA-256
  `2C81FFCB2583C321FDC669525948796E8E4ACFE3701656FDE492AD24A6030B10`;
- duplicate instruction ID, 3,257 bytes, SHA-256
  `5817505916E97E9942FE740858E02B68D679E04CB518C139E7D6CD97D167CD3E`;
- valid base, SHA-256
  `3996261D40B63B90F0870D3D601D4ADAD5F31525A601AFDE1D93E491C9DB2053`.

The command shape was:

```powershell
.\bin\pgy-self-driver.exe --mir-json-backend=c `
  .tmp\self_hosted\adversary_fuzz\direct_mir\let_log-<case>.json `
  -o .tmp\self_hosted\adversary_fuzz\followup_readonly_20260905\<case>.c
.\bin\pgy-self-driver.exe --mir-json-backend=llvm `
  .tmp\self_hosted\adversary_fuzz\direct_mir\let_log-<case>.json `
  -o .tmp\self_hosted\adversary_fuzz\followup_readonly_20260905\<case>.ll
```

Campaign totals were bounded as follows:

- native HIR: 120 mutations, zero crash/internal/timeout;
- self source-MIR: 120 mutations, zero crash/internal/timeout;
- native/self differential: 160 mutations; 145 both-reject, 15 native-only,
  zero self-only/both-accept/crash/internal/timeout;
- direct MIR: 83 candidates from four dual-backend-eligible seeds; 57 clean
  dual-reject and 26 dual-accept, comprising seven root-grammar and nineteen
  InstructionId mutations; zero backend divergence/crash/timeout.

The source campaign used the sorted first 100 corpus seeds, RNG seed
`20260904`, one to four mutation rounds, and six mutation operators. Its
ignored replay ledger is
`.tmp/self_hosted/adversary_fuzz/source_diff/summary.json`.

## Scheduling verdict

Checkpoint `123f0889` closes the active direct-MIR tagged-enum projection rung:
one target-neutral admitted plan now preserves the positive result, joins
payload member selection to constructor provenance, rejects future/stale SSA,
and keeps the payload-free real-match route green. It does not close the
source language's inactive-variant access.

The first published exact-head run `33888474407` found that the two new owners
used reserved word `local` as a variable name. The Pergyra-built seed accepted
that spelling, but the Linux native-oracle parser rejected it while emitting
the integrated driver. Checkpoint `bbeb75d7` renames only those variables to
`local_row`. A local replay of the exact native-oracle source emission then
completed with zero errors, and a fresh Pergyra-built DRV-2 passed the focused
and adjacent gates. Checkpoint `4bf7d988` makes the fast component contract
reject `let local:` in both reached owners. The cancellation request followed the decisive failure;
28 jobs finished successfully, the failing self-host job remained failed, and
the still-running `build-linux` job ended cancelled. Replacement CI is
separate publication evidence. Exact replacement run `33891240090` completed
30/30 green in 37m49s. Its full self-host job proved
`gen2 == gen3 (176108 lines)`, installed the Pergyra-built DRV-2, and censused
the production policy corpus as `3 in_subset / 0 out_of_subset`.

That bounded source discovery is no longer pending: it found that no existing
fact joined lexical binding identity to active-constructor flow, and the local
candidate described above now supplies the missing owner before MIR
publication. Publication and exact-head CI remain required before the active
lease can be retired. The declared enum/variant/payload-type projection remains
separate and cannot invent active-tag truth from spelling or delegate semantic
checking to the direct backend.

The affine-Future, Zone spawn ABI, root JSON grammar, and duplicate
InstructionId findings remain independent successor reds. Their order is not
decided by this audit, and none changes the canonical census or project
percentage.

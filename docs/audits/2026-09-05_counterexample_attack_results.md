# Counterexample attack results — 2026-09-05

Status: `READ-ONLY CAMPAIGN COMPLETE — ACTIVE DIRECT RUNG REPAIRED; SUCCESSOR REDS NOT REPAIRED`

Observed repository base:
`b4cfb0f2ef883d8f5b80ef96eafd89c1898c3d30`.

Completed direct-MIR repair checkpoint:
`123f088949a9d62095b0f181dadd387959b22064`.

Fresh candidate DRV-2:

- size: `6,675,205` bytes;
- SHA-256:
  `9AEE8FC490042BE8B22361F9284B257DDCDA5D68E16E1BC4A1F2E982376AF5EB`.

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

The next contiguous work item is a bounded read-only discovery at the source
semantic/MIR producer. It must determine whether an existing environment or
SSA fact owns active constructor provenance before opening an implementation
lease. `SemanticExpressionGraphEnumPayloadTypeName` currently proves only
declared enum/variant/payload type; it must not invent active-tag truth from
spelling or let the direct backend serve as the semantic checker.

The affine-Future, Zone spawn ABI, root JSON grammar, and duplicate
InstructionId findings remain independent successor reds. Their order is not
decided by this audit, and none changes the canonical census or project
percentage.

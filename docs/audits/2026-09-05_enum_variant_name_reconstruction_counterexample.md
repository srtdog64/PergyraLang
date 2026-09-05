# Enum variant-name reconstruction counterexample

Status: OBSERVED OPEN; not a semantic contract or a successor implementation
rung. Base `918a7c98e541771fab542713262b643d375d51c0` plus the reached CI repair.

The current isolated Pergyra driver has SHA-256
`1AAFC7D7F031424B75F7BE1737BA088B501A021154204864B1EB286B76642BA6`.
It successfully produced MIR for the following source, but general MIR-to-C
reconstruction returned exit 1 with
`MIR-LOWER ERROR: match enum variant declaration fact is missing`.
Observed run: `.tmp/self_hosted/mixed_arity_enum/run.GtNf1M/`; the original
`enum-local.mir.json` and diagnostic in `enum-local.general.c` are preserved.

```pergyra
enum Signal { Off, On }
enum OtherSignal { Off, On }
struct Reading { signal: Signal; weight: Int; }
func EnumMemberScore(reading: Reading) -> Int {
    match reading.signal {
        case Off: return 0;
        case On: return reading.weight;
    }
}
func OtherMark(signal: OtherSignal) -> Int {
    match signal {
        case Off: return 1;
        case On: return 3;
    }
}
func Main() -> Void {
    Log(EnumMemberScore(Reading(Signal.On, 22)));
    Log(EnumMemberScore(Reading(Signal.Off, 99)));
    Log(OtherMark(OtherSignal.On));
}
```

`structured_condition_emission_owner.pgy` asks
`MirProgramEnumVariantOwnerForName` for a variant's owner using only its name.
The enum index deliberately returns no owner when different enums share that
name. `expression_graph_tagged_enum_match_owner.pgy` has the same name-only
consumer. The materialized source local and the other routine's formal
parameter carry distinct nominal types; the observed error is not evidence
that these types are absent from MIR.

The reached payload-free local-admission regression keeps two declared,
referenced enums with equal ordinal shape, but gives the second enum distinct
variant names (`Disabled`, `Enabled`). Its `Signal -> OtherSignal` mutation
still falsifies nominal identity, independently of this reconstruction defect.
No duplicate-name success claim, compiler repair, skipped general-MIR route,
or execution of mutated output is implied by that bounded fixture choice.

A future repair must resolve both reconstruction consumers from the carried
scrutinee's type identity, reject missing/ambiguous identity, and pass this
source with exact output `22`, `0`, `3`. Guessing the first matching enum or
removing this finding is not a fix. Qualified-pattern behavior and whether the
same-name source passes direct C/LLVM remain untested in this observation.

## Separate bounded exhaustive-return observation

While adding an independent `Signal` formal parameter to `OtherMark`, the
probe briefly added `if expected == Signal.Off { return 0; }` immediately
before its exhaustive `match signal`. Source MIR production and general-MIR C
execution passed, but direct C returned exit 1:
`direct MIR scalar terminal return is invalid: routine=2 block=12 row=-1 expected=Int operation_start=4 operation_count=0 exhaustive_code=103 last_kind=-1`.
Artifacts remain in `.tmp/self_hosted/mixed_arity_enum/run.3rg5QY/`.
Removing only that preceding `if` restored the positive projections; the
independent formal parameter remains in the fixture and keeps `Signal`
referenced under local-type mutations. This is an OPEN positive-subset finding,
not a fixed exhaustive-return defect or evidence of an accepted invalid input.

## Read-only owner trace — 2026-09-06

The following narrows the two OPEN findings; it is not an implementation or
new executable PASS. The exhaustive-return review completed independently.
The duplicate-name agent returned a partial finding, then stopped because the
model service was at capacity; primary verified the retained MIR and the
remaining source boundaries directly. No compiler source or existing artifact
was changed during this trace.

### Duplicate names: carried identity exists, but the consumer loses the join

- In the retained MIR, `EnumMemberScore` branches on `__pgy_match_13` with
  exactly one use, `__pgy_match_13.1`. Its unique definition carries
  `abi_type_name=Signal`, and its source-local row also carries `Signal`.
- `OtherMark` instead branches on a formal-parameter graph leaf with
  `binding_syntax_id=22`, `binding_ordinal=0`. Header parameter zero has that
  syntax identity and type `OtherSignal`. The branch ABI field is null;
  absence of a type on that branch is not absence of all nominal evidence.
- `routine_inventory_owner.pgy` owns `MirRoutineHeaderFacts` and validates
  parameter-row identity. `routine_instruction_use_fact_owner.pgy` owns exact
  instruction uses; `routine_result_definition_fact_owner.pgy` owns unique SSA
  definitions. `routine_lower.pgy` already builds the routine index, use facts
  and header before body rendering, but `EmitRegionWithExpressionOrder` and
  `BlockCondWithExpressionOrder` do not receive the header/use joins.
- The other last consumer is reached through
  `expression_graph_fact_owner.pgy` -> `MirExpressionGraphSequenceAppendOccurrence`
  -> the match occurrence owners. It passes the enum inventory and instruction
  expression, not an admitted nominal subject fact. Both reconstruction
  consumers independently ask `MirProgramEnumVariantOwnerForName`.

Candidate correction boundary: admit the match subject's exact type at its
routine/instruction identity boundary and carry that same fact to both last
consumers. The existing `MirProgramEnumVariantRowForOwnerAndName` can join
owner plus variant without inventing a second declaration table. A local
definition must be the exact unique, valid dominating SSA use; a parameter
must agree on binding kind, ordinal and syntax identity. A routine-wide name
lookup or a guessed `.0`/`.1` suffix is not that proof. No new persisted wire
field is shown necessary by this bounded example, but general completeness of
this proposal has not been demonstrated.

Required falsifiers before calling this fixed: exact positive output for the
retained local/parameter example, same-spelled variants in distinct nominal
types, and controlled refusal of missing/duplicate SSA definitions, changed
parameter ordinal/syntax identity, cross-enum subject types and missing enum
declarations. Both structured text and ordered graph consumers must agree.

### Exhaustive return: routine entry is not the match-chain entry

The retained `OtherMark` routine has local blocks 0 (guard), 1 (return 0), 2
(first enum comparison), 3 (return 1), 4 (second comparison), 5 (return 3),
and 6 (empty terminal). The guard's false edge reaches block 2. Thus the enum
match head has one incoming edge; the routine head has zero.

`direct_mir_scalar_program_payload_free_enum_exhaustive_match_owner.pgy`
initializes `first` from `plan.routines.block_starts[routine]`, checks that
head's incoming count is zero, and reads its first enum match condition there.
Those are stronger assumptions than this valid CFG satisfies. The existing
routine CFG index owns structural merges, and the program routine admission
keeps successor, condition and return rows; this is not evidence that the
source failed to produce an enum identity. The structural-merge rule suggests
the guard's continuation is block 2; that specific derived fact was inspected
in source, not separately executed by the reviewer.

The common `direct_mir_scalar_program_enum_exhaustive_match_owner.pgy` tries
payload-free and then payload-bearing admission, returning `100 + payload`
after both fail. Consequently code 103 hides the earlier payload-free failure;
it must not be reported as that owner's exact rejection stage.

Candidate correction boundary: separate routine bounds from the exact
false-edge match chain ending at this fallthrough. Preserve stable scrutinee,
variant uniqueness/coverage and no-external-entry requirements. Choosing the
first enum comparison in the function or deleting incoming-edge checks would
not close the claim. Terminal readiness remains the last approval consumer;
LLVM `unreachable` emission must still depend on that approval.

Required falsifiers: existing unguarded match, the guarded example, guard-true
output 0 and guard-false outputs 1/3; then missing/duplicate variants, changed
scrutinee identity, outside edges into the chain/empty terminal, and invalid
returns. Rejected inputs must not publish a requested artifact. These proposed
new cases have not been run and do not replace the live CI integration gate.

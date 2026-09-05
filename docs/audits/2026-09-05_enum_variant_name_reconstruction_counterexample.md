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

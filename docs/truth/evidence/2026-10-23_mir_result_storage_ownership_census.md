# MIR result-storage ownership census

Recorded 2026-10-23 KST from local `main` commit
`4450c83fb652489d4277aee65033534183867d5b`. The semantic input was the
unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.

## Adopted change

- `DriverRung2MirProjection` publishes only `mir_facts`; it no longer creates
  a second owner for the admitted semantic analysis.
- Generic-specialization output Arrays are created independently and assembled
  once, so growing one output cannot leave stale descriptor aliases.
- `SelfMirAppendRoutine` grows the one `facts` functional-update owner instead
  of copying every Array descriptor through a second aggregate.
- Strings retained from borrowed DIR intent facts cross into MIR output through
  `ArrayPushOwnedString`.
- There is no compatibility read, graph clone, manual drop or weakened safety
  condition.

Measured production source SHA-256 values:

- `driver_rung2_owner.pgy`: `d381bd17e2867ab08e9ee113e379006f0cb32b9efb8648738e2cf93f3be0806f`
- `artifact_lower_owner.pgy`: `ab3e02113029b394849ef10578f6982a50f5da74bc004dd05c24043faafe1786`
- `generic_specialization_owner.pgy`: `96702cf15e7be74392fa1ccd24ffda1b55e3b18d57f611d48972255c7965cdc5`
- `intent_execution_source_plan_owner.pgy`: `ab2fc3353021ace5abd29116995b01c761d78f98a4a6f4b185508ec0679538d6`

## Rejected alternative

Adding broad `ref` modes to the whole analysis parameters reached 318 final
refusals, but it removed 22 rows while adding 8 new borrowed-boundary rows.
That candidate was rejected and is not in the commit. The adopted candidate
keeps the default aggregate contract and changes only the retained outputs and
their actual owner.

## Fixed final-owner census

The instrumented native compiler was unchanged, SHA-256
`b1a0e7d3c83c50755d93b771dc9ca83d2e7b569598aecf36683edae8838ae31c`.
It analyzed an exact snapshot of the committed source tree.

| Measure | Before | After |
|---|---:|---:|
| Final refusals | 332 | 319 |
| Outside `direct_mir_*` | 290 | 277 |
| `direct_mir_*` | 42 | 42 |
| Move-family refusals outside `direct_mir_*` | 84 | 83 |
| Hard census errors | 0 | 0 |

The semantic multiset comparison found exactly 13 removals and no additions.
The current 277 ordinary-route rows include 82 moved-aggregate uses, one
borrowed-parameter move, 52 borrowed-ref escapes, 51 local-copy changes, six
copy changes at function exit, 20 preserved-Array calls and 17 escaped
owned-String pushes. These categories describe diagnostics, not independent
defects.

The run performed three landing analyses and the final analysis in 942.63 s,
peaking at 3,980,072 KiB. `self_hosted_component_contract_smoke.sh` passed all
14 unit checks plus line caps and generator drift. `documentation_quality_smoke.sh`
passed all five unit checks and truth size caps.

V1 remains open: 319 ownership refusals remain, so the unchanged canonical
program is not admitted. Bootstrap, installed-driver C/LLVM execution and
exact-SHA CI were not run or claimed for this red commit.

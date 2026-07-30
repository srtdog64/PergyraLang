# Intent-step binding contract probe

**Status:** focused executable semantic contract.
**Parity owner:** `tests/self_hosted/parity/intent_step_binding_contract_parity.sh`

## Intent

Prove that self-host C intent emission binds `who`, `authorized by`, `using`,
and `where` through one admitted step-binding owner. Actor identity is allowed
to differ from authority, but the authority must be an authority-bearing
subject slot of the exact execution zone.

## Input Contract

The probe has no external input. Its typed fixtures cover distinct
actor/authority success, by-value and `inout` zone addresses, exact alias
preference, and unique type-based slot resolution. Negative fixtures include a
`where`/`using` mismatch, undeclared authority, ambiguous slot, and missing
slot.

## Output Contract

`CodegenIntentStepBindingContractReady()` returns true only when every positive
and negative witness has the required result. Any guessed participant, foreign
zone, or non-authority subject must make the probe exit nonzero.

## Oracle

`tests/self_hosted/parity/intent_step_binding_contract_parity.sh` enters the
focused owner gate, which compiles and runs this tool and requires the PASS
marker. Static string presence is not a substitute for the executable negative
witnesses.

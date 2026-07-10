# MIR Producer Track

This directory owns the bounded Pergyra-written `AstTreeArtifact -> MIR fact
graph -> pgy.mir.v1` producer used by DRV-2.

- `program_fact_owner.pgy`: flat declaration/routine/CFG/instruction rows.
- `routine_input_owner.pgy`: one immutable typed-artifact plus semantic-fact
  input bundle for routine lowering.
- `routine_lower_owner.pgy`: `SelfMirRoutineState -> SelfMirRoutineState`
  body lowering; unsupported shapes fail closed without multi-aggregate call
  state.
- `artifact_lower_owner.pgy`: declaration/routine assembly and deterministic
  instruction-ID canonicalization.
- `program_verify_owner.pgy`: structural range/topology/fact verifier.
- `json_projection_owner.pgy`: verified fact graph to MIR JSON projection.

The C compiler remains the whole-language oracle and default native producer.
For the bounded DRV-2 source frontier, however, C MIR is comparison evidence
only: the live replacement path produces and consumes MIR in Pergyra.

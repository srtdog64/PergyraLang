# MIR Producer Track

This directory owns the bounded Pergyra-written `AstTreeArtifact -> MIR fact
graph -> pgy.mir.v1` producer used by DRV-2.

- `program_fact_owner.pgy`: flat declaration/routine/CFG/instruction rows.
- `expression_graph_fact_owner.pgy`: instruction-owned normalized condition
  graph rows. A reachable semantic subtree is carried as one verified
  postorder interval; recursive large-aggregate copy returns are forbidden.
- `parallel_capture_fact_owner.pgy`: MIR-owned parallel capture boundary and
  disposition rows; the bounded non-parallel producer emits an explicit empty
  inventory instead of omitting the schema fact.
- `routine_input_owner.pgy`: one immutable typed-artifact plus semantic-fact
  input bundle for routine lowering.
- `routine_iteration_owner.pgy`: verified range/foreach binding, type, bound,
  and collection-use projection for routine lowering.
- `routine_statement_owner.pgy`: payload-only call-shaped statement lowering;
  control-flow ownership remains in `routine_lower_owner.pgy`.
- `routine_lower_owner.pgy`: `SelfMirRoutineState -> SelfMirRoutineState`
  body lowering; unsupported shapes fail closed without multi-aggregate call
  state.
- `artifact_lower_owner.pgy`: declaration/routine assembly and deterministic
  instruction-ID canonicalization.
- `program_verify_owner.pgy`: structural range/topology/fact verifier.
- `json_projection_owner.pgy`: verified fact graph to MIR JSON projection.

The C compiler remains the whole-language oracle and default native producer.
For the bounded DRV-2 source frontier, however, C MIR is comparison evidence
only: the live replacement path produces and consumes MIR in Pergyra. DRV-2
source and `--mir-json` compilation require `expr0_graph` on migrated branch,
definition, and value-return instructions and fail closed when it is absent.
Compact expression text is still retained for reconstructed source shape, but
it is not reparsed to recover a migrated expression graph in the hard consumer.

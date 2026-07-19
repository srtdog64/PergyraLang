# MIR Producer Track

This directory owns the bounded Pergyra-written `AstTreeArtifact -> MIR fact
graph -> pgy.mir.v1` producer used by DRV-2.

- `program_fact_owner.pgy`: flat declaration/routine/CFG/instruction rows.
- `expression_graph_fact_owner.pgy`: instruction-owned normalized condition
  graph rows. A reachable semantic subtree is carried as one verified
  postorder interval; recursive large-aggregate copy returns are forbidden.
- `match_fact_owner.pgy`: sparse instruction-keyed match pattern, variant, and
  binding rows.
- `match_json_projection_owner.pgy`: match fact projection into MIR JSON.
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
- `routine_match_owner.pgy`: bounded scalar case/default CFG lowering; arm
  exits and local-version rows are passed to the merge owner.
- `routine_match_pattern_owner.pgy`: consumes the canonical HIR match-pattern
  fact and projects integer or bounded `Some` / `None` variant and binding rows
  into MIR. It never reparses pattern text.
- `routine_match_merge_owner.pgy`: N-way live-predecessor SSA phi ownership for
  scalar match continuation blocks.
- `artifact_lower_owner.pgy`: declaration/routine assembly and deterministic
  instruction-ID canonicalization.
- `program_verify_owner.pgy`: structural range/topology/fact verifier.
- `json_projection_owner.pgy`: verified fact graph to MIR JSON projection.

The self-host producer also emits the `resource_flow_symbol_count` and
`resource_flow_symbols` routine fields explicitly.  This producer owns the
bounded typed-artifact graph, not the native semantic `ResourceFlowUniverse`,
so its current projection is an explicit empty inventory; the native compiler
MIR JSON path is the owner-directed HIR snapshot path.  Consumers must treat
missing fields as invalid rather than recovering resource identity from source
text or routine names.

The producer also emits explicit empty `loop_flow_summaries` and
`loop_flow_states` fields. Native loop transfer evidence is owned by semantic
analysis; HIR is only the adapter, MIR routines own the validated rows emitted
by native JSON, and self-host `mir_lower` consumes that projection. CFG shape
or source text is not a fallback owner.

At the self-host consumer boundary, `routine_lower.pgy` uses the native rows as
loop-projection admission facts: the rendered CFG loop headers must match the
summary count and `while`/`for` kinds. A mismatch is rejected rather than
recovered from CFG or source text. Each state snapshot stable index is also
resolved against the routine's ResourceFlowUniverse rows; it is not accepted
as an unrelated numeric array.

The C compiler remains the whole-language oracle and default native producer.
For the bounded DRV-2 source frontier, however, C MIR is comparison evidence
only: the live replacement path produces and consumes MIR in Pergyra. DRV-2
source and `--mir-json` compilation require `expr0_graph` on migrated branch,
definition, value-return, Log, and bare-call instructions and fail closed when
it is absent. Recursive member facts cover nested field reads and nested
receiver instance calls in the bounded DRV-2 fixture set.
Compact expression text is still retained for reconstructed source shape, but
it is not reparsed to recover a migrated expression graph in the hard consumer.
The explicitly named C-oracle canonicalization bridge may run the Pergyra
expression parser over legacy native MIR text before comparison; that bridge is
not reachable from hard source or direct `--mir-json` consumption.
Pipe syntax is normalized by the parser into the same direct-call and
call-argument graph consumed by ordinary calls; no pipe-specific codegen parser
or graph node kind exists.

# Self-host compiler Clone carriage through MIR (S5c, 2026-11-22)

Status: slice 1 implemented on `309301eeb` (run record:
[evidence](../truth/evidence/2026-11-22_selfhost_compiler_clone_carriage.md));
slices 2-3 open. One edit lane (Claude). User
decision 2026-11-22: S5c is the next rung after the region row. It is the
S5c step of the [one hand-off rule decision](../truth/decisions/2026-11-16_one_handoff_rule_for_both_front_ends.md)
and a condition of `semantic.hashmap_collection_ownership`. Integration gate:
a new `tests/self_hosted/parity/ownership/selfhost_compiler_clone_carriage_owner.sh`
run with the installed driver. Outputs are implementation candidates; source,
the SoT registry and gates stay authoritative.

## Objective card

- Objective: the direct-MIR C and LLVM routes emit every compiler `Clone`
  landing the self-host ownership analysis selects, from facts the self-host
  MIR carries. A landing of a type direct MIR cannot copy, or one no
  admission consumes, refuses the program. No route emits a landing program
  without its copy.
- Priority: correctness (today `--mir-json-backend` emits
  `local_copy_source_sorted_landed_valid.pgy` without its copy and prints
  `11` where native prints `31`, on C and LLVM), one fact owner, MIR
  carriage, fail-closed admission, backend parity, then patch size.
- Fact owner: `semantic/ast_expression/ast_expression_graph_compiler_clone_owner.pgy`
  (`compiler_clone_kinds`, one row per expression graph node).
- Last consumer: direct-MIR expression admission
  (`direct_mir_scalar_program_expression_admission_owner.pgy`).
- Forbidden: a direct emitter deriving landings again; dropping a listed
  site; the driver's landing count deciding admission; a per-program-shape
  route; copying in the emitter by expression shape.

## Chain (observed)

- The semantic analysis attaches `compiler_clone_kinds`
  (`ast_body_collection_ownership_stage_owner.pgy`). Source C wraps a landed
  node in the type's Clone (`expr_semantic_graph_emit_owner.pgy`,
  `RewriteExprFromSemanticGraphTracked`); the MIR to AST round trip derives
  the same landings again.
- MIR carries one kind only: the stored value of an owned String push/set
  (collection receipt). `driver/driver_compiler_clone_route_owner.pgy`
  counts the other landings from the program facts and the source C/LLVM
  routes refuse when the count is nonzero. `--mir-json` has no facts, so it
  admits and miscompiles.
- Direct MIR copies through the explicit-Clone kinds: String (151) and
  `Array<Int>`/`Array<String>` (144), resolved by
  `direct_mir_scalar_program_collection_clone_signature_owner.pgy`. There is
  no record or Map Clone.
- The 26 sweep programs refused for landings
  ([clusters](../truth/evidence/2026-11-22_selfhost_generality_failure_clusters.md))
  copy a record (about 13), a String (7), an Array (3) or a Map (2).
- Explicit-Clone versions of the Array programs run correctly on direct C
  and LLVM but leak the copy (the known `Array<Int>` local leak).

## Slices

1. Carriage and String/Array copies.
   - Producer: `mir/clone_site_projection_owner.pgy` projects
     `compiler_clone_kinds` into per-instruction lane lists; the writer emits
     `expr0_clone_sites` / `expr1_clone_sites` (window indices, only when
     non-empty), as `expr0_region_sites` does.
   - Admission: a listed node is appended as usual, then a Clone node of its
     type (the explicit-Clone signature) is appended over it and the node's
     consumers read the Clone. A list that is malformed, past the graph, or
     names a node whose type has no Clone kind is refused; every listed node
     must be converted (a skipped marker or the literal fast path refuses).
   - Program check: the lanes with sites must equal the lanes the admission
     converted; a site consumed by no admission refuses the program.
   - Old path: delete `driver_compiler_clone_route_owner.pgy`, its driver
     parameters and the receipt field.
   - Gate: Array and String landings on direct C/LLVM equal native;
     `local_copy_source_sorted` is the falsifier (previous driver prints
     `11`); forged sites (malformed, past the graph, on an `Int` node) and a
     record landing are refused on both backends.
2. Record copy: a direct-MIR deep Clone of a logical record (String and
   Array fields copied, scalars copied, nested records recursively).
   Found in slice 1: the MIR ownership row of a landed initializer still
   names the source it copies (a member move for `member_alias`), so the
   transition admission refuses it. The producer should write a `clone`
   row there; fix it in this slice.
3. Map copy, if direct MIR admits the Map programs otherwise.

## Unknowns

- Answered in slice 1: a Clone node over a call argument keeps the call's
  argument accounting; the three argument programs of the gate run as
  native on direct C and LLVM.
- Whether the owners that read expression graphs outside the admission
  (indexed assignment, member rebind, identity cell store, compound
  assignment) ever carry a landing; the program check refuses them until
  each reads the sites. No sweep program reached that refusal in slice 1.
- A MIR whose site list was deleted cannot be told from one with no
  landing; the direct routes trust the producer for absence, as they do
  for region sites.
- The self-host front end refuses an explicit `Clone(parts[i])` of a String
  element (`owned_element_escape`) yet lands the same copy itself; that is a
  checker gap outside this plan.

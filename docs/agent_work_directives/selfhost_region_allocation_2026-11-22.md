# Self-host region allocation (2026-11-22)

Status: slices 1 and 2 landed in `8be12fd7a`
([evidence](../truth/evidence/2026-11-22_selfhost_source_c_region_allocation.md));
slice 3 (direct MIR) landed in `c6c7ff395`
([evidence](../truth/evidence/2026-11-22_selfhost_direct_mir_region_allocation.md));
slice 4 closed the row
([evidence](../truth/evidence/2026-11-22_region_allocation_plan_closed.md)). One edit lane (Claude). Base revision `bcd9fbea0`. User decision 2026-11-22: close
`resource.region_allocation_plan` by implementing region allocation on the
self-host route. Integration gates: `region_user_callee_installed_self_host_owner.sh`,
`region_backend_wiring_smoke.sh`, `region_call_temporary_smoke.sh` and the
region backend-compare cases, run with the installed driver. Outputs are
implementation candidates; source, the SoT registry and gates stay
authoritative.

## Objective card

- Objective: the self-host route allocates the String concatenations the
  native region rule certifies in a function region and frees the region on
  every exit, on source C, direct-MIR C and LLVM; the unreached model
  `region_plan_owner.pgy` is replaced by the real owner; the row closes.
- Priority: one certification owner (self semantic), facts carried to every
  emitter, absent fact = heap (the plan's fail-closed asymmetry), no double
  release with the owned String temporary plan, behavioral gates.
- Forbidden: an emitter deciding region safety from the expression it is
  emitting; a region site outside its function scope; region allocation
  without a certified row; freeing a region site through the temporary plan.

## Native rule (observed, `src/semantic/region_escape_fact.c`)

- Site shapes: a `+` whose operand is a String literal or whose left child is
  such a `+` (every node of the left spine is a site: `Print("a"+"b"+"c")`
  is two sites); a `Concat`/`StringConcat` call (stdlib-selected, arity 2);
  a String `Clone`.
- Consumers: an argument the builtin retention registry marks borrowed
  (`Print` 0, `Log`, `StringLength`, `Concat`, ...), a stdlib-selected
  borrowed row, or a direct user callee whose `ref String` parameter is only
  passed directly to such rows in its body (no transitive forwarding).
  Nested borrowed calls certify recursively.
- Not visited: assignment, match, defer, with, unsafe, lambdas (heap).
- Ids: site = the concat/call node's stable syntax id; scope = one per
  function (a counter); owner = the function's syntax id.

## Self-host chain (observed)

- Source C: source -> semantic -> MIR JSON -> AST rebuilt from MIR ->
  semantic again -> `GenerateCFromVerifiedSemanticArtifact`. Concat at
  `expr_semantic_graph_emit_owner.pgy` (binary `+`),
  `runtime_call_rewrite_owner.pgy` (`Concat`), `expr_semantic_clone_call_emit_owner.pgy`;
  function prologue and every return in `function_emit.pgy` / `stmt_emit.pgy`
  (`routine_copyout` reaches every return).
- Direct MIR (`--mir-json-backend=c|llvm`, source LLVM): concat classified as
  `ExprConcatString`; C and LLVM string expression owners; routine entry and
  exits in the scalar CFG C/LLVM emission owners.
- MIR graph nodes carry no per-node syntax id; a site in MIR is (routine,
  instruction, graph slot, node index).
- `MirOwnedStringTemporaryPlanForCall` already frees `Concat` temporaries
  passed to builtin sinks: a region site must leave that plan.
- Self-host C includes `pgy_runtime.h`, which reaches
  `pgy_runtime_region_inline.h` through the inline core, so source C needs no
  new header; `pgy_region_create(0)` allocates nothing until the first
  concat. The self-host LLVM side has no `pgy_region_*_export` declarations
  or `PgyRegion` layout.
- The compiler's own source has many certified shapes (`Log("..." + x)`,
  `Log(Concat(...))`): gen2 output changes once slice 2 lands, so the
  fixed point (gen2 == gen3) and DRV-2 memory are part of slice 2's gate.

## Slices

1. Certifier: a self-semantic owner over the expression graph, call-target
   identity, the retention registry rows and signature parameter modes,
   producing per routine the certified graph nodes and the routine's scope.
   Gate: the native unit cases as executable self-host programs (chain = 2
   sites, non-borrowed consumer = 0, bare concat = 0, user callee good/bad).
2. Source C: the emitter reads the certifier for the rebuilt artifact;
   prologue creates the region when the routine has a site, certified nodes
   use `pgy_region_string_concat`, every return and the end destroy it; the
   temporary plan skips region sites. Gate: region fixtures on installed
   source C emit region calls, run like native, and pass ASan/LSan.
3. Direct MIR: the MIR producer carries the sites as a per-routine row family
   validated by `mir_lower`; scalar CFG C and LLVM emit region scope and
   sites; LLVM declares the region runtime. Gate: installed direct C/LLVM
   equal to source C/LLVM; a forged site row (outside its scope, on a
   non-concat node) is refused.
   Slice 3 design (survey 2026-11-22):
   - Carriage: an instruction's graph is a window `[start, start+count)` of
     the one semantic graph (`expression_graph_fact_owner.pgy`), JSON index =
     node - start. The producer writes `expr0_region_sites` /
     `expr1_region_sites` (JSON indices of certified nodes in that lane's
     window) only when non-empty, so programs without sites keep their MIR
     SHA; precedent `expr0_local_refs` (rows, append, validation, JSON and
     the generated memory projection, `scripts/render_mir_json_memory_projection.py`).
     Native MIR has no such field: absent means heap.
   - Consumer: direct-MIR admission (`expression_admission_owner.pgy`,
     `AppendExpressionNode`) has the instruction table and the JSON node
     index; a String `+` listed there gets a new expression kind (region
     concat). It must stay out of the fresh-String retirement kind lists
     (`expression_kind_id_owner.pgy`, `string_core_expression_kind_owner.pgy`).
     A routine with a region-concat node creates the region at entry and
     destroys it at every exit, beside `DirectMirOwnedLocalInitialization` /
     `DirectMirOwnedLocalExitCleanup` (C routine and block-exit owners; LLVM
     emission owner). The `?` early returns skip owned cleanup today
     (`*_option_int_try_owner.pgy`): destroy there too, or refuse the pair.
   - LLVM: define internal region helpers in the module, as `@pgy_concat`
     is defined (`string_llvm_materialization_owner.pgy`), so artifacts still
     link with bare `clang -x ir`; C includes the region runtime headers.
   - Gates that move: `region_user_callee_installed_self_host_owner.sh`
     (direct C pin, direct.ll == source.ll), `one_mir_string_builtin_program_projection.sh`
     (fixture `string_concat_op.pgy` has sites; MIR SHA pin), general emitter
     coverage, owned String result composition/reachable proof.
   Slice 3 result: program facts carry `region_sites`
   (`mir/region_site_projection_owner.pgy`, computed in
   `artifact_lower_owner.pgy` before the arena retires); the instruction
   writer emits `expr0/expr1_region_sites` only when non-empty. Direct-MIR
   admission reads the lane's list (`direct_mir_scalar_program_region_site_owner.pgy`, since renamed `..._lane_site_owner.pgy`),
   refuses a malformed list or an index past the graph before the node loop
   and a listed non-concat node inside it, and turns a listed String `+`
   into kind 159 `DirectMirScalarProgramExprRegionConcatString`. C and LLVM
   string emitters project kind 159 to the routine region; the retiring
   String observer takes the region pointer, so fresh operands (`ToString`)
   are still released. `scalar_cfg/direct_mir_scalar_cfg_region_scope_owner.pgy`
   decides per routine from its admitted expression rows; the region opens
   with the owned-local initialization and is freed with the owned-local exit
   cleanup and on the Option<Int> `?` early return (C and LLVM try owners).
   LLVM defines internal `@pgy.region.concat` / `@pgy.region.free` over
   malloc, so artifacts link without the runtime library.
4. Delete `region_plan_owner.pgy` and its manifest gate; update the
   installed self-host region gate from heap to region output; close the row.
   Slice 4 result: the model, its manifest, golden, gate, Makefile target,
   line caps, OWNERS entries and artifact kind `region_plan` are deleted
   (the installed gate moved to region output in slice 3); the row is
   `CLOSED` with `selfhost_unreached_region_plan_model` as a forbidden
   fallback.

## Unknowns

- Whether the rebuilt AST keeps every certified shape (literal operands,
  left spine) so the second semantic pass certifies the same sites as the
  first; slice 2 compares them.
- `region_user_callee_bad` is rejected by self-host admission ("ref argument
  must be addressable") but compiles as heap natively.

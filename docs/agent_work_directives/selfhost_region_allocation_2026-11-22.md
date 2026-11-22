# Self-host region allocation (2026-11-22)

Status: slices 1 and 2 landed in `8be12fd7a`
([evidence](../truth/evidence/2026-11-22_selfhost_source_c_region_allocation.md));
slices 3 and 4 open. One edit lane (Claude). Base revision `bcd9fbea0`. User decision 2026-11-22: close
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
4. Delete `region_plan_owner.pgy` and its manifest gate; update the
   installed self-host region gate from heap to region output; close the row.

## Unknowns

- Whether the rebuilt AST keeps every certified shape (literal operands,
  left spine) so the second semantic pass certifies the same sites as the
  first; slice 2 compares them.
- `region_user_callee_bad` is rejected by self-host admission ("ref argument
  must be addressable") but compiles as heap natively.

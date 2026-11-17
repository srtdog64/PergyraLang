# String temporary retirement on the installed seed path

Status: see [current gates](../truth/state/gates.md); this is a bounded step
of the self-compile memory plan, not a new track or full S1 closure.
Owner: GPT root integration. Parent checkpoint: `9049e547e`.

## Objective and boundary

- Objective: reduce the first-generation driver's cumulative String garbage
  while the general direct-MIR path is still being completed.
- Priority: storage identity and single ownership; explicit runtime contracts;
  no premature free; unchanged output; bounded measured memory; exact-SHA CI.
- This is the temporary AST-consuming C step explicitly allowed by decision
  `2026-11-16_compiler_temporaries_are_released_at_last_use`, point 3. The
  fixed full MIR passed role admission but stops at routine 1054 of 9623,
  `AstMatchCasePatternFactFromText`, in the collection-transition reader.
- It retires only fresh, independently allocated String expression operands
  immediately after an admitted synchronous nonretaining builtin call. It
  gives no release right to local bindings, parameters, user-call results,
  retaining calls, allocator-backed results, or compiler Clone overlays.
- This is not completion of S1, full OSSA, persisted `value-retire`, or the
  canonical direct-MIR admission obligation. Those remain on the same plan.

## Reached chain and owners

1. The existing semantic graph owns immutable node/call/binding identities.
   Its seal already issues first-consumer rows. Extend that same linear pass
   with use multiplicity; no per-call rescan of the whole graph.
2. `call_handoff_rule.def` owns builtin String result allocation contracts;
   generated native/Pergyra queries agree. The existing sealed builtin
   argument-retention registry continues to own nonretention. Do not invent
   a spelling-only grant in an emitter.
3. A MIR-layer temporary lifetime plan joins exact call identity, concrete
   type, result origin, the consuming argument, and single use. Unknown facts
   produce no retire. A supplied plan must match the complete eligible set;
   an extra, missing, repeated, or relocated retire is rejected.
4. The AST-consuming C ordered-call owner materializes operands once in source
   order, calls the admitted operation, saves its result when necessary,
   prints the planned frees, then yields the saved result. It owns C syntax,
   not freshness or retirement policy. The last consumer is the generated
   driver's runtime call, before the statement-expression result is yielded.
   Its existing 200-line cap remains fixed: the C call-temporary materializer
   owns type projection, collision-free temporary names and text assembly;
   the argument-order owner retains evaluation order and call orchestration.
5. The source-to-C and frozen-MIR-to-C paths both reconstruct the admitted
   semantic graph and reach that owner. The codegen seed builds the installed
   driver; a rebuilt gen2/gen3 pair and installed driver test that full chain.

## Forbidden paths and unresolved dependencies

- No runtime reference count, reachability scan, hidden ownership flag,
  function-wide String arena, raw-address guess, or manual source drop.
- No release based on source name alone or on a String result type alone.
- Keep the direct backend's storage-sharing guard. `alias_read_after` remains
  a counterexample to deleting it; this temporary step does not change it.
- Clone reconciliation: the AST emitter reads the selected graph overlay;
  the current MIR JSON writer does not serialize that overlay. Neither a
  selected-Clone count nor statement text alone proves transport. This step
  must not grant a fresh result from that missing transport.
- Existing direct-MIR temporary wrappers, full String/local/return lifetime
  planning, array transitions, and their deletion remain on S1/S2. Do not
  label this bounded seed-path bridge as their replacement.

## Execution and acceptance

1. Add checked generated result contracts and extend the existing graph seal.
2. Implement the lifetime plan and its complete-set validator; make the
   ordered-call emitter consume it without changing call evaluation order.
3. Exercise nested producers, borrowed/static values, retained constructor
   inputs, declared builtin shadows, a result that survives the call, loops,
   and short circuit. Mutate retire plans for duplicate, absent, premature,
   and borrowed/handed-off releases; the consumer must refuse all four.
4. Instrument generated artifacts at -O0 with ASan/LSan. A removed-free
   artifact must leak; an unchecked borrowed release must fail under ASan.
5. Rebuild the actual codegen fixed point and installed driver. Run the
   existing role, String allocation/OOM, and memory gates. Compare identical
   fixed inputs and artifact bytes, using the existing 14 GiB/1800 s bounds.
6. Publish the tested slice and observe its exact CI, including the actual
   bootstrap and installed-driver jobs. Then resume the remaining S1/S2
   direct-MIR chain; do not claim the full one-process DRV-2 bound here.

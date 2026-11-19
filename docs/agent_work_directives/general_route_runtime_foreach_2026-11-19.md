# General route: foreach over runtime arrays (2026-11-19)

Status: active, Claude lane. Base: main `f0d16adaf`. Output: an
implementation candidate on main, landed in slices.

## Objective card

- **Objective:** the general direct-MIR route compiles `for v in xs` where
  `xs` is any `Array<T>` local or parameter the route already stores. It must
  produce output equal to native on C and LLVM.
  - This is the prerequisite for retiring the three collection
    `projection.direct_mir_*` BRIDGE rows (String array push, collection pop,
    Array<Int> program). Each retires by deleting its route, as V1 requires;
    none is promoted to `CLOSED`.
- **Priority:** runtime equality on both backends, fail-closed admission, one
  fact owner, then patch size.
- **Forbidden:**
  - a per-program-shape claim;
  - literal-element replay in place of the runtime array;
  - a second cursor authority beside the local plan;
  - larger caps or budgets;
  - weakening the String element borrow.
- **Gates:**
  - a new push-core parity owner: foreach over a local and over a parameter,
    String elements, `break`/`continue`, nested loops, six legs where no
    runtime library is needed;
  - refusals with no artifact for a body that mutates or reassigns the
    iterable, and for a forged `loop-init` whose binding or iterable
    disagrees with the header;
  - the default-LLVM sweep (no program may move away from equal), component
    contract, SoT edge.

## Current chain (measured on `6f7126f72` and `f0d16adaf`)

- **Producer.** MIR names a foreach with a `loop-init` row in the init block
  and an `AST_FOR_LOOP` `branch` row ending the header. Both carry
  `arg0 = binding` and `expr0_graph = iterable`.
  - `iteration_type_facts` gives the binding and iterable types.
  - The index is implicit: no MIR row defines or advances it.
- **Graph input**
  (`scalar_cfg/direct_mir_scalar_cfg_graph_input_owner.pgy`):
  - `DirectMirScalarCfgForEachFactsFromOwners` must produce one fact per
    collection iteration, or the driver dies with "foreach facts are
    invalid".
  - It needs a local definition with literal elements (`fact.elements`), so
    a parameter or computed iterable dies here. This is 5 census programs.
- **Program route input**
  (`direct_mir_scalar_cfg_program_routine_admission_owner.pgy`,
  `DirectMirScalarCfgProgramInputReady`): it requires zero foreach facts and
  no collection plan, so the general route takes no foreach at all.
- **Emission.**
  - The only foreach emitters (`direct_mir_scalar_cfg_foreach_typed_{c,llvm}_emission_owner.pgy`)
    materialize the literal elements as a constant array and walk that.
  - Range loops reach the general route differently.
    `DirectMirScalarCfgAppendIterationLocals` adds the binding as an
    Iteration local row. `DirectMirScalarCfgProgramAppendRangeConditionExpression`
    synthesizes `Local < bound` as the header condition. Range block effects
    store the start and add the step.

## Intended chain

1. **Fact:** a program-route collection-iteration fact per foreach.
   - It carries the loop syntax id, init/header/body/exit/backedge blocks,
     the binding name and type, and the iterable type.
   - It is built from the `loop-init`/`branch` pair and checked like the
     range fact: same binding, same iterable text, header dominance.
   - It is built only when `program_expression_owner`; the literal foreach
     admission stays for one-routine scalar CFG until its routes are deleted.
2. **Locals:** the iteration-local owner appends two rows per foreach.
   - The binding gets the existing Iteration kind and its element type.
   - The cursor gets a new kind, keyed by the loop syntax id, as an Int.
   - Both are ordinary locals afterwards: declared, loaded and stored by the
     existing emitters.
3. **Header:** the condition is `Local(cursor) < ArrayLength(iterable)`.
   - The iterable is admitted from the branch row's `expr0_graph` at the
     iterable type.
   - The `ArrayLength` node is the existing length kind for that element
     type.
4. **Body entry:** `Local(binding) = Index(iterable, Local(cursor))` is one
   synthesized expression row, evaluated and stored at the top of the body
   block by a foreach block effect.
   - The index node is the existing kind for the element type.
   - The binding is a borrowed view: the lifetime owner gives it no release.
5. **Init and backedges:** block effects store 0 into the cursor at the end of
   the init block, and add 1 at the end of each backedge block, as range
   effects do.
6. **Admission guards:**
   - The body must not push, set or pop the iterable, and must not assign it.
   - The iterable must be a local or a parameter the route stores.
   - Anything else is refused with a typed stage, never a `Die`.
7. **Retirement (later slices):**
   - Let the general route claim one-routine programs: delete the
     `single-program-shape` guard and its control-flow/builtin claim owners.
   - Stop the collection plans from claiming programs the general route
     covers.
   - Delete each collection family and its registry row once the sweep shows
     its programs equal on the general route.

## Unknowns to settle before editing

- Whether wire `primary_refs` (when `wire.required`) name the foreach
  binding the way they name a range binding.
- Whether the C emitter's `Array<String>` length/index (`pgy_as` runtime
  calls) and `Array<Int>` element width (`i32` storage, `i64` local) need a
  conversion node.
- Which line caps the touched owners sit at; graph admission is at 450/450.

## Slices

1. Int and String elements, local or parameter iterable, LLVM and C; gate
   and sweep.
2. Bool, Long and record elements; break/continue/nested coverage.
3. Retirement steps above, one family per commit.

# Self-compile memory: role receiver dependency

Coordination plan for step 3 of
[the memory plan](selfhost_compile_memory_plan_2026-11-17.md).
Current status belongs to [state](../truth/state/blocker.md). This file
does not own language semantics or declare a SoT row closed.

## Objective card

- Objective: the general MIR emitter consumes the subject-target role
  receiver ABI needed by canonical DRV-2; delete the old role-override route.
- Priority: preserve represented safety/authority contracts and semantic
  callable identity, one admitted fact owner, remove the old route, run
  behavioral negatives and integration, then patch size.
- Fact owner: existing callable receiver carriage policy and
  `mir_lower/program_routine_receiver_target_owner.pgy`; the existing
  routine signature owns the carried erased-receiver fact.
- Last consumer: C signature/body receiver binding and LLVM's pointer ABI,
  through the general GraphPlan routine partition.
- Forbidden fallback: blanket role exclusion, source/display/name-only role
  selection, target layout guessed in an emitter, encoding erasure in an
  integer ID or carriage spelling, old override emission after migration.
- Shared gate: `role_override_mir_replacement.sh`, general multi-role
  execution/ABI probes, all retained MIR mutations and fixed full DRV-2 MIR.
  Follow with current-source codegen fixed point, installed driver and CI.

## Observed boundary

On published `1ff353e89`, source map and binaries are in
[the String record](../truth/evidence/2026-11-17_selfhost_string_allocation_boundary.md).
The complete current DRV-2 source emits MIR in the unchanged process budget:
peak 8,615,924 KiB, artifact SHA-256
`e7767d3059972797baf41654ececd912bfb91a0755547afea6336e6af8bbdf19`.
The artifact has 953 declarations, 9,623 routines, 15 subject-target roles
and 15 role routines. Each has an omitted source receiver and an explicit
`mutable-identity` MIR receiver. Direct C refuses at `role-declaration`
(155.77 seconds, 753,976 KiB); no C artifact is published.

The preserved role-override baseline passes all existing runtime legs,
three permutations, 16 MIR mutations and source receiver negatives.
Raw records are in WSL `.tmp/selfhost-memory-20261117/`.

## Full reached chain

1. Installed `--mir-json-backend=c|llvm` -> admitted MIR document, declaration
   and routine indexes -> existing role target capture. It resolves `for_type`
   and the exact target declaration, but currently drops receiver erasure.
2. Method signature normalizes explicit/omitted `self`, then
   `DirectMirRoutineSignatureFact` seals identity and parameter facts.
   Instance signatures and synthetic/fixture constructors must preserve the
   added fact; free functions and intents state false.
3. Main-first routine identity -> sealed flat routine partition -> GraphPlan.
   Carry erasure as a Bool array, include it in digest/shape checks, and
   migrate every constructor, empty plan, reseal and mutation producer.
4. C signatures use `void *` for an erased first parameter; body entry binds
   the existing concrete identity pointer before parameter uses. Its type
   comes from the existing admitted cell/type projection. No value copy.
   LLVM keeps its existing pointer representation and typed field owner.
   Mutable receiver field definitions also need the existing parameter mode
   consumed by the local-storage query and LocalRef plan. Bind the parameter
   identity without inventing an initial `.0` value; the existing identity-cell
   store admission, SSA predecessor checks and C/LLVM store consumers remain
   authoritative. Readonly parameters acquire no local mutable storage right.
   Both local initializers consume the existing identity-call parameter receipt
   and exact parameter type to initialize the pointer, with no object copy.
5. Replace the 20-line role-presence owner with role declaration assessment:
   exact role/method/routine joins, required `for_type`, known impl ability,
   method ranges and represented method/parameter contracts. Malformed
   claimed metadata ends dispatch. A genuinely unsupported ABI remains
   an explicit unsupported result, never an inferred contract.
6. Route admission reaches role routines through the ordinary callable
   path. Three-routine classification no longer claims role override.
   Delete its identity/plan/target-projection/emission files and all reached
   imports, registry consumers, ownership/cap entries.
7. The existing role gate stops assuming old C/LLVM symbol spelling.
   Compile and execute a slot with the emitted erased C signature, preserve
   subject dispatch and all negative/permutation cases. Add a multiple-role
   impl fixture and receiver pointer/parameter checks using emitted C types.

## Boundary choices and open facts

- This slice supports erased receivers whose concrete targets have an
  admitted noncopy identity cell, including the 15 actual compiler targets.
  Value-target role operators retain their existing separate route; they
  are not silently reinterpreted as identity pointers.
- `pgy.mir.v1` does not carry every source-level ability constraint or
  generic/default substitution. Validate the represented declaration and
  parameter contract. Missing evidence needed by this general ABI is
  refused or explicitly unsupported; never guess interface modes or defaults.
  This is not closure of the entire receiver/ability SoT family.
- An ordinary member/direct call may not select a role implementation merely
  by changing both target spelling and ID. It needs the existing typed role
  binding contract. Until consumed here, such calls stay refused; retain a
  falsifier that changes both fields, beyond the old ID-only mutation.
- Ordinary nominal method declaration rows omit `params`; direct overrides
  join their declaration ID to the existing routine signature for the complete
  prototype. A missing declaration parameter array is never an empty-arity fact.
- No new semantic family, user annotation, generic query/cache, new program
  shape or additional runtime policy is planned.

## Coherent edit scope and order

1. Target capture -> method/signature factories and all signature copies.
2. Routine identity/partition schemas, digests, constructors and graph probes.
3. C parameter declaration/binding, LLVM pointer parity and call admission.
4. Role declaration assessment -> delete override claim and four old owners.
5. Existing gate migration and multi-role behavioral/ABI negatives.
6. Owner/registry/inventory entries, complete fixed MIR next refusal census,
   bootstrap/installed integration, commit/push, exact-SHA CI.

The constructor census covers signature factories and instance copies, routine
identity/partition factories and reseals, graph/proof fixtures, and the existing
method/target factories. Recheck every actual constructor after editing.
The old override family budget is 1,250 lines. Replace its responsibility;
do not split this repair into micro-owner files. Rebudget only directly
changed component rows for real carried ABI fields; no timeout, memory or
universal 600-line limit increase.

The [validated candidate record](../truth/evidence/2026-11-17_selfhost_memory_role_receiver_migration.md)
contains constructor coverage and the fixed source-boundary mode census.
Private ref parameters borrow MIR spans/inventories within the admitted
generation; the final C/LLVM consumers need receiver erasure and the existing
identity-storage fact. No user own/ref/inout or Clone workaround was added.
Keep an explicit private ref only while it avoids whole-program copying;
remove it when inferred borrowing preserves that generation and cost boundary.

Do not start a second implementation lane. The next work after this
dependency is the existing owned-value/retire plan, not unrelated precision.

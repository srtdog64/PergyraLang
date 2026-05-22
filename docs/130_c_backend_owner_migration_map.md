# C Backend Owner Migration Map

This note records which C backend implementation headers can be promoted to
compiled owners immediately, and which ones are blocked by private static
seams. The goal is to avoid repeating unsafe A -> B -> A refactors.

## Rule

- Promote a header to a `.c` owner only when its dependencies are public owner
  seams or can be made public without pulling a large lowering layer with it.
- Do not promote a header when it depends on `transpiler.c`-local static seams.
  First extract the seam as a real owner API, then move the consumer.
- A 600 LOC signal means "review responsibilities"; it does not mean
  mechanical split.

## Safe Recent Promotions

- `transpiler_lambda_emit.c`
  - Moved lambda expression lowering out of an implementation header.
  - The old compatibility header was removed because `transpiler.h` already
    exposes the public emitter seam and no owner includes a lambda-specific
    header.
  - The owner is tracked by the Makefile source inventory.
  - Verification: `make pgy`, `build-source-inventory-test-smoke`,
    `test-inc-size-test-smoke`, `semantic-core-shape-test-smoke`,
    `perf-contract-test-smoke`, and `documentation-quality-test-smoke`.

- `transpiler_overlay_projection.c`
  - Moved overlay projection invalidation and world embedded sync out of
    implementation headers.
  - The headers are declaration-only.

- `transpiler_projection_method_invalidation.c`
  - Moved hosted-method projection invalidation traversal out of the helper
    include chain.
  - The header is declaration-only.

- `transpiler_domain_role_ability_names.c`
  - Moved role/ability bounded string copy, hosted-method symbol formatting,
    vtable typedef naming, operator alias naming, and surface diagnostic
    formatting out of `transpiler_domain_role_ability_emit.h`.
  - The remaining role/ability emission header still owns hosted-method MIR
    emission and vtable body emission, so only the stateless naming policy moved.
  - Verification: `make pgy`, `perf-contract-test-smoke`.

- `transpiler_enum_method_names.c`
  - Moved enum method emitted-name formatting, enum method surface diagnostic
    formatting, and enum generated-string-too-long diagnostics out of
    `transpiler_enum_decl_emit.h`.
  - The remaining enum declaration emission header still owns enum layout,
    constructor, and hosted-method body lowering; only the stateless naming and
    diagnostic formatting policy moved.
  - Verification: single owner compile, `transpiler.o` compile,
    `perf-contract-test-smoke`.

- `transpiler_call_subject_arg_policy.c`
  - Moved user-call subject-address decision and already-subject-ref detection
    out of `transpiler_expr_call_user_emit.c`.
  - This keeps `who`/subject receiver argument policy distinct from
    authorization policy: the owner only decides whether a generated C call
    needs `&arg` or `arg` for pointer-self host subjects.
  - Verification: single owner compile, user-call owner compile,
    `perf-contract-test-smoke`.

- `transpiler_mir_func_ssa_locals_emit.c`
  - Moved MIR function SSA local declaration emission out of an implementation
    header.
  - The header is declaration-only and the owner includes context, string,
    local-type, and SSA utility seams directly.
  - `transpiler_mir_local_type_lookup.h` now includes the slot builtin policy
    it consumes directly, so claim-shape lookup no longer depends on caller
    include order.
  - Verification: `make pgy`, `test-transpile`, `perf-contract-test-smoke`,
    `test-inc-size-test-smoke`, and `build-source-inventory-test-smoke`.

## Blocked Promotions

### `transpiler_enum_decl_emit.h`

Current blocker:

- Enum method emission calls `emit_func_decl_from_mir_named`.
- That function is still a `transpiler.c`/MIR emission static seam.
- Pulling `transpiler_mir_func_emit.h` into a new `.c` owner drags in many
  private static dependencies and breaks standalone compilation.

Required first step:

- Promote MIR function-body emission into a real owner API, or introduce a
  narrow hosted-method MIR emission API that enum/class/domain emitters can
  consume.

### `transpiler_call_constructor_result_emit.h`

Current blocker:

- Class constructor dispatch still depends on
  `ensure_generic_class_specialization`.
- Generic class detection itself is now a compiled owner query:
  `transpiler_class_has_generic_params(...)` in
  `transpiler_generic_param_query.c`.
- Effective generic argument naming is also a compiled owner query:
  `transpiler_generic_param_effective_arg_name(...)` in
  `transpiler_generic_param_query.c`.
- Bounded specialization names, method symbol names, and diagnostic surface
  names plus naming-length diagnostics live in
  `transpiler_generic_class_naming.c` instead of the specialization
  implementation header.
- Generic class specialization key construction also lives in
  `transpiler_generic_class_naming.c`, with type-name mangling consumed
  through `transpiler_mangled_name.h`. The specialization header now asks for
  the key instead of rebuilding it inline.
- Specialization ensure/emission remains a private C backend orchestration seam.

Required first step:

- Move generic class specialization lookup/ensure into a compiled owner seam
  before moving constructor dispatch.

### `transpiler_expr_call_spawn_emit.h`

Current blocker:

- `emit_call(...)` still routes builtin dispatch, domain constructors,
  Result/Option, stdlib, event, member-style calls, and user calls from one
  include-order body.
- The member-style path consumes slot policy, secure-token lookup, host-method
  lookup, overlay/world projection invalidation, and generic specialization
  seams that are not all declaration-only owner APIs yet.
- A direct `.c` promotion currently exposes those hidden dependencies as
  implicit declarations, so forcing this move would recreate the A -> B -> A
  refactor loop.

Required first step:

- Split `emit_call(...)` by dispatch family behind declaration-only owner APIs:
  builtin dispatch, member-style slot/host calls, and user/generic calls must
  stop depending on transitive implementation-header state before this header
  can become a compiled owner.

### `transpiler_relation_effect_emit.h`

Current blocker:

- Relation/effect declaration emission reaches hosted method emission through
  `emit_hosted_methods_from_mir_or_error_local`.
- That local hosted-method path still depends on the same private
  `emit_func_decl_from_mir_named` seam as enum method emission.

Required first step:

- Extract hosted-method MIR function emission as a narrow compiled owner API,
  then move enum and relation/effect declaration emission consumers.

### `transpiler_domain_role_methods_emit.h`

Current blocker:

- Domain role method emission also calls `emit_func_decl_from_mir_named`.
- Moving it alone would repeat the enum/relation-effect failure mode.

Required first step:

- Share the same hosted-method MIR function emission owner API required by enum
  and relation/effect emission.

### `transpiler_expr_call_user_emit.h`

Closed narrow owner:

- `transpiler_expr_call_user_emit.c` now owns direct user-call lowering.
- The header is declaration-only.
- The owner directly includes the host lookup, callable lookup, type rendering,
  slot-target, and generic specialization seams it consumes instead of relying
  on include-order body injection.

Remaining seam:

- User-call lowering depends on host/method lookup and generic specialization
  seams such as `current_host_method_decl`, `transpiler_current_host_decl_local`,
  `transpiler_decl_name_local`, `find_callable_decl`, and
  `is_pointer_self_host_type_name`.
- Generic function specialization ensure now lives in
  `transpiler_generic_specialization_emit.c`; user-call lowering still consumes
  it directly instead of a unified call-resolution policy.
- The remaining call-resolution seams are still implementation-header/static
  orchestration details in the broader `emit_call(...)` family.

Required first step:

- Extract a compiled call-resolution policy/API that returns the resolved call
  target, receiver behavior, specialization name, and subject-pointer policy.
  Then make spawn/member-style call lowering consume the same policy.

### `transpiler_statement_dispatch.h`

Current blocker:

- Statement dispatch is a root orchestration switch over many emitter owners.
- It directly coordinates defer/loop labels, bind emission, unsafe blocks, and
  fallback expression statements.

Required first step:

- Split policy from dispatch first: move bind emission and loop-control helper
  policy to named owners, then revisit whether dispatch itself should remain a
  single source-of-truth switch.

### `transpiler_mir_pending_uses.h`

Current blocker:

- Pending-use materialization consumes public SSA/name seams and now consumes
  effective local type rendering through
  `transpiler_mir_effective_type.c`.
- MIR local type lookup is consolidated under
  `transpiler_mir_local_type_lookup.h`; the old
  `transpiler_mir_ssa_emit.h` compatibility shim has been removed.
- Explicit local binding registration now lives in the compiled
  `transpiler_mir_local_binding.c` owner. Consumers now include the concrete
  local-binding and type-AST lookup owners directly instead of relying on an
  include-order shell.
- The remaining hidden dependency is the generic class specialization ensure
  seam (`ensure_generic_class_specialization`), which is public enough to link
  but still implemented by the specialization orchestration header.
- That ensure path emits generic hosted methods and still calls the same
  private MIR hosted-method body seam (`emit_func_decl_from_mir_named`) that
  blocks enum, relation/effect, and domain-role method promotion.
- Moving pending-use materialization before specialization ensure becomes a
  compiled owner would only shift that orchestration dependency.

Required first step:

- Extract hosted-method MIR function emission as a narrow compiled owner API.
  Then promote generic class specialization lookup/ensure into a compiled
  owner and move pending-use materialization to consume that seam.

### `transpiler_mir_emission_mapping_contract.h`

Current blocker:

- Mapping validation calls `transpiler_materialize_pending_inst_uses`.
- Promoting the mapping contract first would require pulling the pending-use
  implementation header into a new owner shell.

Required first step:

- Promote pending-use materialization only after effective local type rendering
  is public, then move the mapping contract to consume that compiled owner.

### `transpiler_destructure_emit.c`

Status:

- Closed. Destructuring emission is now a compiled owner and
  `transpiler_destructure_emit.h` is declaration-only.
- The owner consumes the public typed-var registry seam instead of the
  `transpiler_parallel_capture.h` current-local static helper.

Remaining note:

- A broader current-function local type query owner is still useful for other
  C backend headers, but destructuring is no longer blocked on that seam.

## Next Good Targets

- Headers whose implementation only uses already-compiled owner APIs.
- Headers included by one owner and not dependent on MIR function-body static
  helpers.
- Small lookup/policy helpers that can become data-driven compiled owners
  without changing lowering order.

## Do Not Do

- Do not include large implementation headers from a new `.c` owner just to make
  the build pass. That preserves the hidden dependency problem and adds another
  owner shell.
- Do not split a single responsibility only because it crossed a line-count
  threshold.
- Do not move codegen routines across owner boundaries before adding inventory
  smoke coverage for the new source file.

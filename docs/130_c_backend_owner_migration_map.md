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
  - Hosted-method MIR body emission later moved to
    `transpiler_hosted_method_body_emit.c`, so this owner now stays focused on
    stateless role/ability naming.
  - Verification: `make pgy`, `perf-contract-test-smoke`.

- `transpiler_domain_role_ability_emit.c`
  - Moved ability-vtable specialization tag rendering, typedef naming, and
    generic ability vtable declaration emission out of
    `transpiler_domain_role_ability_emit.h`.
  - The header is declaration-only and exposes the vtable-specialization API
    consumed by role method and nominal domain emission owners.
  - Verification: standalone owner compile, role-method owner compile,
    `transpiler.o` compile, `test-transpile`, `perf-contract-test-smoke`,
    `mir-declaration-inventory-test-smoke`, `memory-string-safety-test-smoke`,
    `semantic-core-shape-test-smoke`, `runtime-frontier-contract-test-smoke`,
    `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.

- `transpiler_hosted_method_body_emit.c`
  - Moved shared domain hosted-method MIR body emission out of
    `transpiler_domain_role_ability_emit.h`.
  - Relation, effect, party, roster, zone, and world hosted-method consumers now
    call a linked owner API instead of relying on a role/ability include-order
    helper.
  - The emitted C symbol policy is the generic `host_method` rule owned by this
    file, not the role/ability vtable naming layer.
  - Verification: standalone owner compile, `transpiler.o` compile,
    `perf-contract-test-smoke`, `mir-declaration-inventory-test-smoke`,
    `memory-string-safety-test-smoke`, `build-source-inventory-test-smoke`, and
    `test-inc-size-test-smoke`.

- `transpiler_zone_methods_emit.c`
  - Moved zone hosted-method forward/body bridge out of an implementation
    header and out of the late `transpiler.c` include-order bridge.
  - `transpiler_zone_decl_emit.c` now consumes a declaration-only zone method
    seam directly.
  - Verification: standalone owner compile, zone declaration owner compile, and
    `transpiler.o` compile.

- `transpiler_world_select_event_emit.c`
  - Moved world sync declaration, select lowering, and event
    declaration/subscription lowering out of
    `transpiler_world_select_event_emit.h`.
  - The header is declaration-only; the owner consumes projection lookup,
    hosted-method body emission, frontier policy, provenance, and type-require
    seams explicitly instead of relying on domain-role include order.
  - Verification: standalone owner compile and `transpiler.o` compile.

- `transpiler_domain_nominal_emit.c`
  - Moved ability, role, and party declaration emission out of
    `transpiler_domain_nominal_emit.h`.
  - The header is declaration-only and exposes the nominal declaration
    entrypoints plus the nominal surface-diagnostic helper used by roster
    declaration emission.
  - The domain-role shim now includes roster and relation/effect declaration
    seams directly instead of routing them through the nominal implementation
    header.
  - Verification: standalone owner compile, `transpiler.o` compile,
    `test-transpile`, `perf-contract-test-smoke`,
    `mir-declaration-inventory-test-smoke`, `memory-string-safety-test-smoke`,
    `semantic-core-shape-test-smoke`, `runtime-frontier-contract-test-smoke`,
    `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.

- `transpiler_enum_method_names.c`
  - Moved enum method emitted-name formatting, enum method surface diagnostic
    formatting, and enum generated-string-too-long diagnostics out of
    `transpiler_enum_decl_emit.h`.
  - Enum declaration emission later moved to `transpiler_enum_decl_emit.c`;
    the header is now declaration-only.
  - Verification: single owner compile, `transpiler.o` compile,
    `perf-contract-test-smoke`.

- `transpiler_enum_decl_emit.c`
  - Moved enum layout, tagged-union constructor, and enum hosted-method body
    lowering out of an implementation header.
  - The owner consumes the linked MIR function-body API
    `emit_func_decl_from_mir_named(...)` instead of relying on include-order
    static seams.
  - Verification: standalone owner compile, `transpiler.o` compile,
    `test-transpile`, `perf-contract-test-smoke`,
    `memory-string-safety-test-smoke`, `mir-declaration-inventory-test-smoke`,
    `semantic-core-shape-test-smoke`, `build-source-inventory-test-smoke`, and
    `test-inc-size-test-smoke`.

- `transpiler_call_subject_arg_policy.c`
  - Moved user-call subject-address decision and already-subject-ref detection
    out of `transpiler_expr_call_user_emit.c`.
  - This keeps `who`/subject receiver argument policy distinct from
    authorization policy: the owner only decides whether a generated C call
    needs `&arg` or `arg` for pointer-self host subjects.
  - Verification: single owner compile, user-call owner compile,
    `perf-contract-test-smoke`.

- `transpiler_class_decl_emit.c`
  - Moved non-generic class declaration lowering out of an implementation
    header.
  - The header is declaration-only; class fields, generated Slot/Box container
    scaffolding, class hosted-method forwards, and class method MIR body
    lowering live behind a compiled owner.
  - Verification: standalone owner compile, `transpiler.o` compile,
    `test-transpile`, `perf-contract-test-smoke`,
    `memory-string-safety-test-smoke`, `mir-declaration-inventory-test-smoke`,
    `semantic-core-shape-test-smoke`, `build-source-inventory-test-smoke`, and
    `test-inc-size-test-smoke`.

- `transpiler_func_flow_policy.c`
  - Moved function fallback policy helpers out of
    `transpiler_func_class_flow_emit.h`: current-return-type copy,
    function-parameter diagnostic surface formatting, too-long diagnostics, and
    Option return-constructor lookup.
  - This is not a full function-flow owner promotion; the remaining header still
    acts as the base-B include-order shim for function fallback, with-slot,
    return lowering, and downstream declaration emitters.
  - Verification: standalone owner compile and `transpiler.o` compile.

- `transpiler_mir_func_ssa_locals_emit.c`
  - Moved MIR function SSA local declaration emission out of an implementation
    header.
  - The header is declaration-only and the owner includes context, string,
    local-type, and SSA utility seams directly.
  - `transpiler_mir_local_type_lookup.c` consumes the slot builtin policy
    directly, so claim-shape lookup no longer depends on caller include order.
  - Verification: `make pgy`, `test-transpile`, `perf-contract-test-smoke`,
    `test-inc-size-test-smoke`, and `build-source-inventory-test-smoke`.

- `transpiler_mir_terminator_emit.c`
  - Moved MIR branch/return/fallthrough terminator emission out of an
    implementation header.
  - The header is declaration-only and exposes only explicit and fallthrough
    terminator APIs; branch/return/pin-cleanup error details are private to the
    owner.
  - This keeps CFG cleanup/pin-exit emission near the MIR body-safety
    source-of-truth without relying on caller include order.

- `transpiler_async_parallel_emit.c`
  - Moved C backend `parallel` block and `async` block emission out of an
    implementation header.
  - The header is declaration-only and exposes `emit_parallel_block(...)` and
    `emit_async_block(...)`.
  - The owner consumes `transpiler_parallel_capture.h` as a declaration-only
    surface; capture walking now lives in `transpiler_parallel_capture.c`.
  - Verification: standalone owner compile, `transpiler.o` compile,
    `test-transpile`, `perf-contract-test-smoke`,
    `memory-string-safety-test-smoke`, `semantic-core-shape-test-smoke`,
    `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.

- `transpiler_mir_destructure_emit.c`
  - Moved MIR destructuring statement emission out of an implementation
    header.
  - The header is declaration-only and exposes the MIR let-destructure
    emission API consumed by MIR block scheduling.
  - The owner still consumes the current local-type lookup implementation
    API, now exposed by `transpiler_mir_local_type_lookup.c`.
  - Verification: standalone owner compile and `transpiler.o` compile.

- `transpiler_mir_local_type_lookup.c`
  - Moved MIR/source local type lookup out of an implementation header.
  - The header is declaration-only and exposes arena-backed type-name copy,
    arena-backed type-name rendering, expression-local inference, and
    current-function local lookup APIs.
  - This removes the static `transpiler_find_local_type_name(...)` seam that
    blocked parallel capture and MIR destructuring owner work.
  - Verification: standalone owner compile plus dependent async/parallel,
    destructuring, and `transpiler.o` compiles.

- `transpiler_generic_class_specialization_emit.c`
  - Moved generic class specialization lookup/ensure and helper emission out
    of an implementation header.
  - `transpiler_func_class_flow_emit.h` no longer injects the specialization
    body through include order; call sites consume the existing
    `ensure_generic_class_specialization(...)` declaration.
  - The owner still emits into `ctx->helpers`, but the owner boundary is now a
    linked C source file and the declaration header is body-free.
  - Verification: standalone owner compile, `transpiler.o` compile,
    `build-source-inventory-test-smoke`, `mir-declaration-inventory-test-smoke`,
    `perf-contract-test-smoke`, and `semantic-core-shape-test-smoke`.

- `transpiler_func_class_flow_emit.c`
  - Moved function declaration fallback emission, with-slot lowering, and
    return statement lowering out of an implementation header.
  - The previous include-order dependency on class/async/enum/match bodies is
    gone; `transpiler_statement_dispatch.h` now consumes match and enum
    declaration seams explicitly.
  - The owner still depends on MIR emission state, function flow policy,
    host-self policy, and type rendering/require APIs, but those are linked
    owner seams rather than local static header state.
  - Verification: standalone owner compile, `transpiler.o` compile,
    `test-transpile`, `build-source-inventory-test-smoke`,
    `test-inc-size-test-smoke`, `perf-contract-test-smoke`,
    `mir-declaration-inventory-test-smoke`, and `semantic-core-shape-test-smoke`.

- `transpiler_expr_call_spawn_emit.c`
  - Moved the top-level `emit_call(...)` dispatcher shim out of an
    implementation header.
  - The owner is intentionally thin: it routes builtin, domain constructor,
    Result/Option, stdlib/event, member-style, and user-call families to their
    dedicated linked owners.
  - Verification: standalone owner compile and `transpiler.o` compile.

- `transpiler_spawn_channel_emit.c`
  - Moved spawn wrapper emission plus channel send/receive expression lowering
    out of an implementation header.
  - The declaration header is body-free; the owner directly consumes future
    type inference, generic binding/specialization, expression type inference,
    symbol lookup, type rendering, and diagnostic context APIs.
  - Verification: standalone owner compile and `transpiler.o` compile.

- `transpiler_expr_dispatch_emit.c`
  - Moved the root C expression dispatch switch out of an implementation
    header.
  - The owner still routes expression families, but it now links as one
    compiled source and consumes literal, call, spawn/channel, array-access,
    lambda, slot-target, projection, type-mapping, and diagnostic seams
    explicitly.

- `transpiler_let_emit.c`
  - Moved the root C let-declaration orchestration body out of an
    implementation header.
  - The owner now links as a compiled source and consumes slot/view/box/channel,
    Option/Result/collection, generic class specialization, symbol registration,
    type-rendering, collection runtime suffix, and scalar-zero seams explicitly.

## Blocked Promotions

### `transpiler_call_constructor_result_emit.c`

Status:

- Closed for the constructor dispatch wrapper. The implementation now lives in
  `transpiler_call_constructor_result_emit.c`, and
  `transpiler_call_constructor_result_emit.h` is declaration-only.
- Class constructor dispatch still depends on
  `ensure_generic_class_specialization`, but that is now an explicit linked
  dependency owned by `transpiler_generic_class_specialization_emit.c`, not an
  include-order implementation-header dependency.
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
- Specialization ensure/emission remains a private C backend orchestration seam,
  but it is now a compiled owner rather than a header body.

### `transpiler_expr_dispatch_emit.c`

Status:

- Closed as an implementation-header migration. `emit_expression(...)` now
  lives in `transpiler_expr_dispatch_emit.c`, and the matching header is
  declaration-only.
- The owner still has broad expression-family routing responsibility; future
  work should continue moving family-specific policy to named owners instead of
  putting implementation back into the header.

Remaining seam:

- Identifier/member fallback policy, await result rendering, and event invoke
  expression formatting are still directly coordinated by the root dispatcher.

### `transpiler_relation_effect_emit.c`

Status:

- The shared hosted-method MIR body path now lives in
  `transpiler_hosted_method_body_emit.c`, so the old role/ability-local helper
  blocker is closed.
- Relation/effect surface formatting, declaration emission, projection sync,
  hosted-method forward declarations, and hosted-method MIR body calls now live
  behind the compiled owner `transpiler_relation_effect_emit.c`.
- The header is declaration-only.

Remaining note:

- This owner still consumes domain provenance/projection sync APIs; those APIs
  are already declaration-backed and should remain the shared runtime
  propagation boundary.

### `transpiler_roster_decl_emit.c`

Current status:

- Roster declaration emission now has its own compiled owner.
- The header is declaration-only and exposes `emit_roster_decl(...)`.
- The owner consumes shared nominal surface formatting, declaration inventory,
  hosted-method metadata view, forward declarations, and hosted-method body
  emission through explicit APIs instead of relying on the domain-role include
  chain.
- Verification: standalone owner compile, `transpiler.o` compile,
  `build-source-inventory-test-smoke`, `mir-declaration-inventory-test-smoke`,
  and `test-inc-size-test-smoke`.

- `transpiler_domain_role_methods_emit.c`
  - Moved role method MIR lowering, role vtable instance emission, and role
    operator alias emission out of an implementation header.
  - The header is declaration-only.
  - The ability-vtable helper dependency is now declaration-only through
    `transpiler_domain_role_ability_emit.h`.
  - Verification: standalone owner compile, `transpiler.o` compile,
    `test-transpile`, `perf-contract-test-smoke`,
    `mir-declaration-inventory-test-smoke`, `semantic-core-shape-test-smoke`,
    `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.

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

### `transpiler_statement_dispatch.c`

Status:

- Closed as an implementation-header migration. `emit_statement(...)` now lives
  in `transpiler_statement_dispatch.c`, and `transpiler_statement_dispatch.h`
  is declaration-only.
- The owner remains the single root statement-dispatch switch and now appears
  in the Makefile source inventory instead of depending on include-order body
  injection.

Remaining seam:

- Statement dispatch still directly coordinates bind emission, unsafe blocks,
  defer registration, loop-control labels, and fallback expression statements.
  Future cleanup should split those policies into responsibility-named owners
  without moving the dispatch body back into a header.

### `transpiler_mir_pending_uses.c`

Status:

- Closed. Pending-use materialization now lives in
  `transpiler_mir_pending_uses.c`, and `transpiler_mir_pending_uses.h` is
  declaration-only.
- The owner consumes public SSA/name, local type-AST, effective local type,
  expression-type inference, symbol, context, and MIR reason APIs directly.
- The older generic-specialization blocker no longer applies to this owner.

### `transpiler_mir_emission_mapping_contract.c`

Status:

- Closed. Mapping validation now lives in
  `transpiler_mir_emission_mapping_contract.c`, and the header is
  declaration-only.
- The owner consumes `transpiler_mir_pending_uses.h` plus public SSA,
  local-binding, pin-resource-alias, and AST accessor seams instead of
  relying on the MIR emission contract include body.

### `transpiler_destructure_emit.c`

Status:

- Closed. Destructuring emission is now a compiled owner and
  `transpiler_destructure_emit.h` is declaration-only.
- The owner consumes the public typed-var registry seam instead of the
  `transpiler_parallel_capture.h` current-local static helper.

Remaining note:

- A broader current-function local type query owner is still useful for other
  C backend headers, but destructuring is no longer blocked on that seam.

### `transpiler_parallel_capture.c`

Current status:

- Capture walking is a compiled owner. `transpiler_parallel_capture.h` is now
  declaration-only and depends on the local type lookup compiled owner API.

Remaining note:

- Keep future capture inference in this owner; do not grow async/parallel emit
  with another local AST walker.

### `transpiler_mir_emission_contract.c`

Current status:

- MIR emission contract validation is a compiled owner. The header now exposes
  only the function/intent MIR emitability query seam consumed by function and
  intent lowering.
- CFG/body-safety smoke contracts now check the compiled owner for topology,
  branch-condition, cleanup-edge, pin-cleanup, and emission-fact validation.

Closed note:

- `transpiler_mir_block_emit.c` now owns block statement emission. The header is
  declaration-only and exposes `transpiler_emit_mir_block_statements(...)` to
  the function-body emission stack without keeping the implementation in an
  include surface.

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

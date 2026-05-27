# Beta Closure Readiness Report (Historical Snapshot)

Date: 2026-04-26

Status: historical snapshot. The live beta-readiness source of truth is
`docs/100_beta_readiness_checklist.md`.

Anti-hype correction (2026-04-29): this report is a readiness audit, not a
marketing snapshot. Current source-of-truth docs (`docs/100` and `TODO.md`)
now separate feature-completeness from strict beta readiness: feature feel is
about **70%**, and strict beta readiness is currently **about 72-74%**. This
report is older than that policy update, so
older progress-log entries below may mention the previous 50% line or older
`.inc` inventories. The current `.inc` source of truth is the zero-inventory
note immediately below plus `docs/115_inc_cleanup_status.md`.

This document summarizes the current codebase state, the remaining improvement opportunities, and the concrete work needed to close PergyraLang for beta. It is based on the current README/TODO/status docs, the C/LLVM backend paths, the IR pipeline tests, the ABI smoke matrix, and backend-compare coverage.

2026-04-27 include cleanup update: `.inc` debt is now closed as a
zero-inventory gate for the full `src` tree. `src/runtime`, `src/codegen`,
`src/compiler`, `src/semantic`, and `src/tests` have **0 `.inc` files /
0 LOC**. Former pass-through seams now live in named private owner headers,
test fragments use `.cases.h`, and compiler runtime cache freshness tracks the
renamed runtime owner headers instead of stale `.inc` paths.

2026-05-19 owner cleanup update: the follow-up debt is now implementation-header
cohesion, not `.inc` inventory. Generated C backend specialization helpers have
moved from `transpiler_specialization_helpers.h` into the compiled owner
`src/codegen/transpiler_specialization_helpers.c`; Result suffix parsing and
`Result<T,E>` specialization discovery moved from
`transpiler_type_result_mapping_helpers.h` into
`src/codegen/transpiler_type_result_mapping_helpers.c`. HashMap stdlib builtin
dispatch and lowering moved from `transpiler_expr_stdlib_map_builtin.h` into
`src/codegen/transpiler_expr_stdlib_map_builtin.c`, with HashMap metadata
validation owned by the shared collection support owner. Queue stdlib dispatch
and lowering also moved from `transpiler_expr_stdlib_queue_builtin.h` into
`src/codegen/transpiler_expr_stdlib_queue_builtin.c`, with unary collection
metadata validation owned by the same support owner. Result/Option builtin
dispatch and lowering moved from
`transpiler_call_result_option_builtin_emit.h` into
`src/codegen/transpiler_call_result_option_builtin_emit.c`; linked owners now
consume `src/codegen/transpiler_option_context.h` for Option context
declarations instead of pulling in the broad helper shim. Intent observability
builtin lowering moved from `transpiler_intent_observability_builtin_emit.h`
into `src/codegen/transpiler_intent_observability_builtin_emit.c`.
Projection/world lookup seams also moved into
`src/codegen/transpiler_projection.c`: overlay domain-slot lookup,
projection-target detection, and world-state lookup are linked owner APIs
instead of implementation-header static helpers, and source inventory smoke
rejects reopening the old local helper names. Domain query builtin lowering for
`HasProjection`, `HasLayer`, `HasState`, `HasZone`, `HasZoneProjection`,
`HasZoneLayer`, and `HasZoneState` now lives in
`src/codegen/transpiler_expr_domain_query_builtin.c` instead of the builtin
dispatch header. I/O and time builtin lowering moved to
`src/codegen/transpiler_expr_io_builtin.c`, so file/runtime call bodies no
longer live in the builtin dispatch header either. Domain constructor emission
moved from the constructor-result call dispatcher to
`src/codegen/transpiler_domain_constructor_emit.c`, so class compound literals,
party/roster/relation/effect/zone/world designated initializers, projection
dirty defaults, world dirty defaults, and enum variant constructor call strings
now share a compiled owner. The remaining constructor header is a thin dispatch
wrapper around the local generic-class specialization seam. Expression core
lowering moved to `src/codegen/transpiler_expr_core_emit.c`, tuple/Array
literal lowering moved to
`src/codegen/transpiler_expr_composite_literal_emit.c`, and Array/Slice checked
access lowering moved to `src/codegen/transpiler_expr_array_access_emit.c`; all
three headers are now declaration-only. Channel let lowering also moved to
`src/codegen/transpiler_let_channel_emit.c`, leaving
`transpiler_let_channel_emit.h` declaration-only. Future/RemoteFuture type
queries moved to `src/codegen/transpiler_future_type_query.c`, and post-let
type registration moved to `src/codegen/transpiler_let_type_register_emit.c`,
removing another pair of static include-order bodies from spawn/let lowering.
Box/Rc let lowering moved to
`src/codegen/transpiler_let_box_emit.c`, leaving
`transpiler_let_box_emit.h` declaration-only for `Box<T>`, `Box<Array<T>>`, and
`Rc<T>` constructor let paths. Collection let lowering moved to
`src/codegen/transpiler_let_collection_emit.c`, leaving
`transpiler_let_collection_emit.h` declaration-only for `Option<T>` and stable
collection constructor let paths. Slot/View let lowering moved to
`src/codegen/transpiler_let_slot_emit.c`, leaving
`transpiler_let_slot_emit.h` declaration-only for
ClaimSlot/ClaimSecureSlot/ClaimDeviceSlot, ReadView/WriteView/MoveToken, and
Slot/SecureSlot sugar paths. Zone specialization emission is now linked
through `src/codegen/transpiler_zone_specialization_emit.c` instead of relying
on a declaration-only header without a source inventory entry. Zone struct and
layer accessor emission also moved from `transpiler_zone_struct_emit.h` into
the compiled owner `src/codegen/transpiler_zone_struct_emit.c`, so generated
zone fields and accessors no longer live in an implementation header. The redundant
`transpiler_mir_emit_predicates.h` wrapper header is gone; function and intent
emitters now call the reason-capable MIR contract APIs directly.
MIR match condition lowering moved from `transpiler_mir_match_condition_emit.h`
to `src/codegen/transpiler_mir_match_condition_emit.c`, so Option/Result
destructor pattern conditions and payload binding no longer live in a static
implementation header.
Expression builtin dispatch moved to
`src/codegen/transpiler_expr_builtin_dispatch.c`, so the `BuiltinKind` routing
switch no longer lives in the expression-emitter include chain.
Control-flow statement lowering moved to
`src/codegen/transpiler_control_flow_emit.c`, leaving
`transpiler_control_flow_emit.h` declaration-only for `if`/`for`/`while`,
loop-label lookup, and the condition-head formatter shared with MIR branch
terminator emission.
MIR CFG control lowering moved to
`src/codegen/transpiler_mir_cfg_control_emit.c`, leaving
`transpiler_mir_cfg_control_emit.h` declaration-only for MIR loop init, for-in
binding, backedge increment, branch-condition rendering, and select readiness
rendering.
`pergyra_ast_type_to_c_copy(...)` moved to
`src/codegen/transpiler_type_render.c`, so shared AST type-to-C conversion no
longer lives in the forward-helper include. Dependent
emitters now include the type-mapping, collection-support, Option-context,
intent-observability, or role-ability declarations they consume directly instead
of relying on hidden include-order bodies.

## Current Verdict

PergyraLang is no longer blocked by broad surface absence. The remaining beta risk is concentrated in a small number of deep implementation contracts:

- runtime propagation must be generalized beyond the closed slices;
- runtime recoverable failure needs a richer queryable surface;
- declaration-side MIR inventory still carries AST-shaped metadata;
- function/action/intent body safety is not yet fully CFG/dataflow-backed, even though HIR/MIR CFG infrastructure exists;
- type-resolution DAG exists and is now much more visible, but is not yet the
  full semantic execution truth;
- arena/lifetime rules are mostly settled but a few owner/runtime ABI boundaries remain.

Current beta readiness for this historical report was approximately **50%**.
Current live policy is **about 72-74% strict beta readiness**; see
`docs/100_beta_readiness_checklist.md`.

This is intentionally lower than a feature-count reading. Many core and foundation surfaces are already implemented, tested, and documented, but strict beta readiness depends on the trust of the underlying closure mechanisms. Until function body safety is CFG/dataflow-backed, until type-resolution DAG becomes the source of truth for frozen-subset dependency ordering, and until long-term modularization reaches stable owner boundaries, the project should not be described as 90%+ beta-ready.

The current beta posture is best described as:

> Narrow beta is close, but strict beta still needs CFG-backed body safety plus the remaining propagation, failure, MIR inventory, DAG, and lifetime closure work to be either completed or explicitly downgraded from the beta contract.

2026-04-26 correction: at the time of this report the strict readiness number
remained **50%** because CFG and AIR/body-dataflow source-of-truth were still not
closed, but two structural risks improved materially. The type-resolution DAG fallback cap is now
`materializer_fallbacks<=1296` with exact family accounting, and the production
runtime/codegen/compiler `.inc` size gate is green again with
`src/compiler/mir_public_part_a.inc=959` and
`src/compiler/mir_public_part_b.inc=800`. The first lean debt-slice after the
process change moved C backend type-alias declaration emission out of a near-cap
include body into `src/codegen/transpiler_type_alias.c`, reducing
`src/codegen/transpiler_emitters_base_b_part_c.inc` to 976 LOC without adding a
new `.inc` split. The next debt-slice deleted
`src/codegen/transpiler_emitters_type_require.inc` and moved type requirement
checks into `src/codegen/transpiler_type_require.c`, reducing the source `.inc`
cap to 159 while keeping `src/codegen/transpiler_emitters_base_a_part_a.inc` at
905 LOC. The extern declaration pass now has its own
`src/codegen/transpiler_extern.c` owner, reducing
`src/codegen/transpiler_emitters_base_b_part_b.inc` from 998 LOC to 957 LOC.
Declarator rendering for event-handler/function types now lives in
`src/codegen/transpiler_type_declarator.c`, reducing
`src/codegen/transpiler_helpers_core_b_part_c.inc` from 992 LOC to 849 LOC.
LogBanner indentation normalization now lives in
`src/codegen/transpiler_log_normalize.c`, reducing
`src/codegen/transpiler_expr_emitters_part_a.inc` from 991 LOC to 878 LOC.
Generated-C runtime intent exit cleanup now lives in
`src/runtime/pgy_runtime_intent_exit.h`, preserving the
`pgy_intent_exit_export(...)` inline ABI while reducing
`src/runtime/pgy_runtime_part_ba_part_b.inc` from 996 LOC to 894 LOC.
Generated-C DeviceSlot/SecureSlot macro bodies now live in
`src/runtime/pgy_runtime_slot_macros.h`, preserving built-in instantiation order
while reducing `src/runtime/pgy_runtime_part_ba_part_c.inc` from 996 LOC to
808 LOC. Generated-C intent last-history step accessors now live in
`src/runtime/pgy_runtime_intent_history.h`, preserving borrowed string ABI
accessor names while reducing `src/runtime/pgy_runtime_part_ba_part_a.inc` from
989 LOC to 867 LOC. Generated-C intent last/active borrowed exports now live in
`src/runtime/pgy_runtime_intent_active_exports.h`, reducing
`src/runtime/pgy_runtime_part_ba_part_a.inc` again from 867 LOC to 558 LOC and
making active/recent ABI owner checks explicit. LLVM-linkable intent borrowed
exports now live in `src/runtime/pgy_runtime_lib_intent_exports.h`, reducing
`src/runtime/pgy_runtime_lib_part_b_part_c.inc` from 852 LOC to 315 LOC and
keeping C/LLVM intent ABI pipeline cases green. LLVM method-call projection
sync helpers now live in `src/codegen/llvm_expr_call_projection_sync.h`,
reducing `src/codegen/llvm_expr_call_methods_part_a.inc` from 880 LOC to
671 LOC while keeping world/zone projection backend compare green.
LLVM method-call domain action sync and slice/member-call helpers now live in
`src/codegen/llvm_expr_call_methods_domain_slice.h`, removing the remaining
`src/codegen/llvm_expr_call_methods_part_a.inc` body while preserving
`llvm_expr.c` include order.
The LLVM call dispatcher now lives in `src/codegen/llvm_expr_call_dispatch.h`,
removing the former `src/codegen/llvm_expr_calls_main.inc` body while preserving
the call-family helper shim order.
LLVM expression host/self, projection binding, spawn expression, operator
suffix, enum lookup, and number/string literal helpers now live in
`src/codegen/llvm_expr_host_spawn_literal_helpers.h`, removing the former
`src/codegen/llvm_expr_helpers_part_b.inc` body while preserving `llvm_expr.c`
helper include order.
LLVM expression boundary call argument helpers, projection field helpers,
world/zone lookup helpers, and host-class lookup helpers now live in
`src/codegen/llvm_expr_boundary_projection_helpers.h`, removing the former
`src/codegen/llvm_expr_helpers_part_a.inc` body while preserving `llvm_expr.c`
helper include order.
C backend MIR SSA identifier contract helpers now live in
`src/codegen/transpiler_mir_ssa_contract.h`, reducing
`src/codegen/transpiler_emitters_base_a_part_d.inc` from 849 LOC to 677 LOC
while keeping `test-transpile` green.
C backend MIR routine lookup, active SSA name resolution/rendering,
token-local filtering, and local type-name lookup now live in
`src/codegen/transpiler_mir_ssa_names.h`, removing the former
`src/codegen/transpiler_emitters_mir_inventory_ssa_names.inc` body while
preserving the MIR inventory/SSA shim order.
C backend primitive, slot/channel, constructed generic, and local type-name
rendering now live in `src/codegen/transpiler_type_mapping_helpers.h`, removing
the former `src/codegen/transpiler_helpers_core_types.inc` body while preserving
the helper-core shim order.
C backend world sync declaration, select lowering, and event
declaration/subscription lowering now live in
`src/codegen/transpiler_world_select_event_emit.c`; the remaining header is
declaration-only. This removes the former
`src/codegen/transpiler_domain_role_part_d.inc` body and the later
implementation-header owner while preserving the domain-role shim order.
LLVM expression assignment, member lvalue/member access, projection
invalidation, and embedded world projection assignment sync now live in
`src/codegen/llvm_expr_assignment_member_projection.h`, removing the former
`src/codegen/llvm_expr_values.inc` body while preserving `llvm_expr.c` include
order.
LLVM-linkable runtime authority rejection state, checked arithmetic exports,
panic invariant export, and file-path normalization helpers now live in
`src/runtime/pgy_runtime_lib_authority_file_core.h`, removing the former
`src/runtime/pgy_runtime_lib_part_a.inc` body while preserving
`pgy_runtime_lib.c` include order.
LLVM-linkable raw set tail exports, intent active/recent registry helpers,
intent trace mutation, and MIR trace hooks now live in
`src/runtime/pgy_runtime_lib_set_intent_trace_exports.h`, removing the former
`src/runtime/pgy_runtime_lib_part_b_part_b.inc` body while preserving
`pgy_runtime_lib.c` include order.
RIR flow semantic flags, state merge rules, and HIR CFG enrichment now live in
`src/compiler/rir_flow.h`, removing the former `src/compiler/rir_flow.inc` body
while preserving `rir.c` include order.
C backend MIR local type lookup, explicit binding registration, effective local
type rendering, MIR function signature support checks, SSA expression emission,
phi copy emission, and exit-SSA lookup now live behind concrete MIR owners. The
former `src/codegen/transpiler_mir_ssa_emit.h` compatibility shell has been
retired after removing `src/codegen/transpiler_emitters_mir_inventory_ssa_emit.inc`.
Generated-C threaded channel and SPSC channel inline macro definitions plus the
stable `Int`/`String` instantiations now live in
`src/runtime/pgy_runtime_channel_inline.h`, removing the former
`src/runtime/pgy_runtime_part_bb.inc` body while preserving `pgy_runtime.h`
include order.
C backend zone sync, projection readiness/dirty fields, layer/state frontier
sync, bounded recompute, and the MIR hosted-method metadata guard now live in
`src/codegen/transpiler_zone_decl_emit.c`; `transpiler_zone_decl_emit.h` is
declaration-only. This removes the former implementation-header owner created
from `src/codegen/transpiler_domain_role_part_c.inc` while preserving the
domain-role shim order through a hosted-method bridge.
C backend block auto-release emission, intent participant/action lookup,
inferred causes lookup, and effective-zone sync helpers now live in
`src/codegen/transpiler_block_intent_helpers.c`, removing the former
`src/codegen/transpiler_emitters_base_b_part_c.inc` body while preserving the
base-B shim order and reducing the production `.inc` inventory to
77 files / 19,652 LOC.
C backend intent cleanup/rollback/invalidation tail emission now lives in
`src/codegen/transpiler_intent_cleanup_emit.c`; the matching header exposes
only the cleanup-tail seam and the MIR carrier-missing diagnostic path remains
shared through `transpiler_set_mir_intent_carrier_missing(...)`.
C backend intent signature/runtime-entry emission now lives in
`src/codegen/transpiler_intent_prologue_emit.c`; the matching header exposes
only the prologue seam.
Generated-C inline file/string helpers, `StringSplit` allocation, and the toy
Qubit runtime now live in `src/runtime/pgy_runtime_io_qubit_inline.h`, removing
the former `src/runtime/pgy_runtime_part_c.inc` body while preserving
`pgy_runtime.h` include order. Compiler runtime cache freshness dependencies
were also updated to reference the current named runtime owners instead of
deleted include paths. The production `.inc` inventory is now
76 files / 19,110 LOC.
C backend domain/party constructor dispatch and Result/Option builtin call
lowering now live behind compiled owners, with the remaining constructor
dispatch wrapper in `src/codegen/transpiler_call_constructor_result_emit.c`.
This removed the former `src/codegen/transpiler_expr_emitters_part_c.inc` body
while preserving the expression emitter shim order. The production `.inc`
inventory was 75 files / 18,573 LOC at that extraction point.
Generated-C slot/device-slot/secure-slot instantiations, Box/Array/Rc builtins,
and inline HashMap helpers now live in
`src/runtime/pgy_runtime_builtin_storage_inline.h`, removing the former
`src/runtime/pgy_runtime_part_ba_part_c.inc` body while preserving
`pgy_runtime_part_ba.inc` include order. Runtime ABI/panic tests and compiler
runtime cache freshness now point at the new named owner. The production `.inc`
inventory is now 74 files / 18,038 LOC.
Semantic overlay/host method typing, nominal boundary classification, zone
effect-layer checks, and movable resource predicates now live in
`src/semantic/type_checker_host_helpers.h`, removing the former
`src/semantic/type_checker_helpers_host.inc` body while preserving
`type_checker.c` include order. The production `.inc` inventory is now
73 files / 17,448 LOC.
Semantic builtin name resolution and Slot/SecureSlot/DeviceSlot operation
validation now live in `src/semantic/type_checker_builtins_slotops.h`, removing
the former `src/semantic/type_checker_builtins_slotops.inc` body while
preserving `type_checker_builtins.c` include order. The production `.inc`
inventory is now 72 files / 16,923 LOC.
Generated-C parallel macros, zone lock/generation/authority validation, Result
helpers, remote Result helpers, and Option helpers now live in
`src/runtime/pgy_runtime_zone_result_option_inline.h`, removing the former
`src/runtime/pgy_runtime_part_ba_part_e.inc` body while preserving
`pgy_runtime_part_ba.inc` include order. Runtime ABI/panic/authority tests and
compiler runtime cache freshness now point at the new named owner. The
production `.inc` inventory is now 71 files / 16,402 LOC.
C backend overlay projection invalidation, zone/effect propagation snippets,
and world-state lookup helpers now live in
`src/codegen/transpiler_projection_sync.h`, removing the former
`src/codegen/transpiler_helpers_core_a_part_c.inc` body while preserving the
helper-core-A shim order. The production `.inc` inventory is now
70 files / 15,883 LOC.
Generated-C intent trace storage/registry helpers, active/recent trace updates,
step ok/fail tracing, and MIR resource trace hooks now live in
`src/runtime/pgy_runtime_intent_trace_inline.h`, removing the former
`src/runtime/pgy_runtime_part_ba_part_a.inc` body while preserving
`pgy_runtime_part_ba.inc` include order. Runtime ABI lifetime smoke and compiler
runtime cache freshness now point at the new named owner. The production `.inc`
inventory is now 69 files / 15,370 LOC.
LLVM extended List/Set/HashMap raw-call lowering now lives in
`src/codegen/llvm_expr_call_collections_extended.h`, removing the former
`src/codegen/llvm_expr_call_collections_extended.inc` body while preserving
`llvm_expr_calls.inc` dispatcher include order. The production `.inc` inventory
is now 68 files / 14,862 LOC.
C backend helper-root string helpers, MIR resource-op/DEF helper emission, and
the expression-emitter include root now live in
`src/codegen/transpiler_helpers.h`, removing the former
`src/codegen/transpiler_helpers.inc` body while preserving `transpiler.c`
top-level include order. The production `.inc` inventory is now
67 files / 14,356 LOC.
C backend Log/LogRaw/LogBanner lowering and core binary expression lowering now
live in `src/codegen/transpiler_expr_core_emit.h`, removing the former
`src/codegen/transpiler_expr_emitters_part_a.inc` body while preserving
`transpiler_expr_emitters.inc` include order. The production `.inc` inventory is
now 66 files / 13,869 LOC.
C backend role ability/method lookup and Result/Option/collection
specialization collection now live in
`src/codegen/transpiler_specialization_helpers.h`, removing the former
`src/codegen/transpiler_helpers_core_b_part_b.inc` body while preserving
`transpiler_helpers_core_b.inc` include order. The production `.inc` inventory
is now 65 files / 13,402 LOC.
C backend ability, role, party, roster, relation, and effect declaration
emission now lives in `src/codegen/transpiler_domain_nominal_emit.h`, removing
the former `src/codegen/transpiler_domain_role_part_b.inc` body while
preserving `transpiler_domain_role.inc` include order. The production `.inc`
inventory is now 64 files / 12,937 LOC.
C backend `emit_expression()` dispatch now lives in
`src/codegen/transpiler_expr_dispatch_emit.c`, removing the former
`src/codegen/transpiler_expr_emitters_part_f.inc` body while preserving
`transpiler_expr_emitters.inc` include order. Runtime panic contract smoke now
reads the new owner path for checked array/slice lowering. The production
`.inc` inventory is now 63 files / 12,486 LOC.
MIR public validation/inventory surface now lives in
`src/compiler/mir_public_surface.c`; `src/compiler/mir_public_surface.h` is
declaration-only and no longer carries validation bodies through `mir.c` include
order.
Semantic generic parameter lookup, default-bound validation, and
class-specialization where-bound validation now live in
`src/semantic/type_checker_generic_contracts.c`, removing the former
`src/semantic/type_checker_generic_contracts.inc` body and the temporary
implementation-header dependency. The production `.inc` inventory was
61 files / 11,663 LOC at that extraction point.
LLVM member-call dispatch and nominal hosted-method self argument lowering now
live in `src/codegen/llvm_member_call_emit.h`, removing the former
`src/codegen/llvm_expr_call_methods_part_b.inc` body while preserving
`llvm_expr.c` include order. The production `.inc` inventory is now
60 files / 11,262 LOC.
Generated-C runtime platform includes, contract headers, warning helpers, path
normalization, and IO sandbox checks now live in
`src/runtime/pgy_runtime_platform_io_core.h`, removing the former
`src/runtime/pgy_runtime_part_a.inc` body while preserving `pgy_runtime.h`
include order. Runtime authority and panic contract smokes now read the named
owner path. The production `.inc` inventory is now 59 files / 10,879 LOC.
Semantic alias resolution stack handling, alias materialization, function-type
formatting, and embedded world-zone mutation rejection now live in
`src/semantic/type_checker_resolution_helpers.h`, removing the former
`src/semantic/type_checker_helpers_resolution.inc` body while preserving
`type_checker.c` include order. The production `.inc` inventory is now
58 files / 10,500 LOC.
Generated-C List and Set inline runtime definitions now live in
`src/runtime/pgy_runtime_list_set_inline.h`, removing the former
`src/runtime/pgy_runtime_part_ba_part_d.inc` body while preserving
`pgy_runtime_part_ba.inc` include order. Runtime ABI and panic contract smokes
now read the named owner path. The production `.inc` inventory is now
57 files / 10,123 LOC.
LLVM identifier emission, direct Slot/SecureSlot fallback lowering, slot target
resolution, and banner literal normalization now live in
`src/codegen/llvm_expr_identifier_slot_helpers.h`, removing the former
`src/codegen/llvm_expr_helpers_part_c.inc` body while preserving `llvm_expr.c`
include order. The production `.inc` inventory is now 56 files / 9,753 LOC.
Semantic spawn token boundary checks and channel send/recv ownership
diagnostics now live in `src/semantic/type_checker_async_channel.c`, removing
the former `src/semantic/type_checker_async_channel.inc` body and superseding
the temporary implementation header. The production `.inc` inventory at that
extraction point was 55 files / 9,384 LOC.
LLVM callable/event signature helpers, scalar string coercion, binary lowering,
unary lowering, and `?` propagation lowering now live in
`src/codegen/llvm_expr_scalar_core.h`, removing the former
`src/codegen/llvm_expr_core.inc` body while preserving `llvm_expr.c` include
order. Runtime panic contract smoke now reads the named owner path. The
production `.inc` inventory is now 54 files / 9,024 LOC.
Semantic generic subject signature formatting and effective default generic
argument derivation now live in
`src/semantic/type_checker_generic_support.c`, removing the former
`src/semantic/type_checker_generic_support.inc` body and the temporary
`type_checker.c` include-order dependency. The production `.inc` inventory was
53 files / 8,666 LOC at that extraction point.
LLVM projection field-copy lowering and bounded projection sync loop generation
now live in `src/codegen/llvm_domain_projection_sync_helpers.h`, removing the
former `src/codegen/llvm_domain_helpers_part_b.inc` body while preserving
`llvm_domain.c` include order. Runtime frontier contract smoke now reads the
named owner path. The production `.inc` inventory is now 52 files / 8,333 LOC.
C backend parallel block emission and async block spawning now live in
`src/codegen/transpiler_async_parallel_emit.c` behind a declaration-only
header, retiring another implementation-header owner from the execution
family surface.
Semantic type resolution now lives in `src/semantic/type_checker_resolve.c`,
removing the former `src/semantic/type_checker_resolve.inc` body. The memoized
`resolve_type_node(...)` wrapper is TU-local, the obsolete
`type_checker_resolve.h` compatibility header is deleted, and metadata-first
public APIs replace direct resolver entry. The production `.inc` inventory is
now 50 files / 7,701 LOC.
Semantic domain-query builtin validation now lives in
`src/semantic/type_checker_builtins_query_domain.h`, removing the former
`src/semantic/type_checker_builtins_query_domain.inc` body while preserving
`type_checker_builtins.c` include order. HasProjection/HasZoneProjection
source-field lookup and zone/world query validation now have a named private
owner. The production `.inc` inventory is now 49 files / 7,387 LOC.
CFG/body-flow loop analysis now lives in
`src/semantic/type_checker_flow_loops.h`, removing the former
`src/semantic/type_checker_flow_loops.inc` body while preserving
`type_checker_flow.c` include order. Loop resource snapshots, bounded loop
analysis, and effect merge logic now have a named private owner. The production
`.inc` inventory is now 48 files / 7,086 LOC.
C backend function-forward helpers now live in
`src/codegen/transpiler_func_forward_helpers.h`, removing the former
`src/codegen/transpiler_helpers_core_b_part_c.inc` body while preserving
`transpiler_helpers_core_b.inc` include order. Spawn/future return inference,
early type forward-declaration checks, generic call binding inference, and
hosted-method forward declarations now have a named private owner. The
production `.inc` inventory is now 47 files / 6,790 LOC.
MIR lowering public API now lives in `src/compiler/mir_lower_public_api.h`,
removing the former `src/compiler/mir_public_part_a.inc` body while preserving
`mir.c` include order. `mir_lower(...)`, MIR routine lookup, declaration header
lookup, liveness pass entry, and DCE pass entry now have a named private owner.
The production `.inc` inventory is now 46 files / 6,500 LOC.
LLVM-linkable runtime intent/slot-core exports now live in
`src/runtime/pgy_runtime_lib_intent_slot_core_exports.h`, removing the former
`src/runtime/pgy_runtime_lib_part_b_part_c.inc` body while preserving
`pgy_runtime_lib.c` include order. `pgy_intent_exit_export(...)`, the runtime
deadline helper, and primitive `Slot<Int/Long/Float>` exports now have a named
private owner. The production `.inc` inventory is now 45 files / 6,212 LOC.
C backend match lowering moved out of the old
`src/codegen/transpiler_emitters_match.inc` body and now lives in the compiled
owner `src/codegen/transpiler_match_emit.c`; the header is declaration-only.
Result, Option, and enum destructor pattern lowering now have a named private
owner outside the include chain. The production `.inc` inventory is now 44
files / 5,932 LOC.
LLVM domain query call lowering now lives in
`src/codegen/llvm_expr_domain_query_calls.h`, removing the former
`src/codegen/llvm_expr_call_domain_queries.inc` body while preserving
`llvm_expr_calls.inc` include order. `HasProjection`, `HasLayer`, `HasState`,
`HasZone`, and zone-detail query lowering now have a named private owner. The
production `.inc` inventory is now 43 files / 5,657 LOC.
MIR base helpers now live in `src/compiler/mir_base_helpers.h`, removing the
former `src/compiler/mir_base.inc` body. RIR public dump/destroy surface now
lives in `src/compiler/rir_public_surface.h`, removing
`src/compiler/rir_public.inc` and keeping the RIR include chain owner-named. The
production `.inc` inventory is now 41 files / 5,119 LOC.
Runtime quantum exports now live in
`src/runtime/pgy_runtime_lib_quantum_exports.h`, removing the former
`src/runtime/pgy_runtime_lib_part_b_part_f.inc` body while keeping runtime
source packaging and ABI lifetime inventory pointed at a named owner. The
production `.inc` inventory is now 40 files / 4,866 LOC.
LLVM slot/device call lowering now lives in
`src/codegen/llvm_expr_slot_device_calls.h`, removing the former
`src/codegen/llvm_expr_call_slots.inc` body while preserving
`llvm_expr_calls.inc` dispatcher order. The production `.inc` inventory is now
39 files / 4,621 LOC.
C backend intent-zone binding emit helpers now live in
`src/codegen/transpiler_intent_zone_binding_emit.c`; the header is
declaration-only. LLVM constructor, RC, and
task/channel call lowering now live in `src/codegen/llvm_expr_constructor_calls.h`,
`src/codegen/llvm_expr_rc_calls.h`, and
`src/codegen/llvm_expr_task_channel_calls.h`. This removes four more anonymous
`.inc` bodies while preserving backend include order. The production `.inc`
inventory is now 35 files / 3,689 LOC.
C backend control-flow emission, semantic resource-flow dataflow, LLVM
collection-base calls, and LLVM Result/Option calls now live in
`src/codegen/transpiler_control_flow_emit.h`,
`src/semantic/type_checker_flow_resources.h`,
`src/codegen/llvm_expr_collection_base_calls.h`, and
`src/codegen/llvm_expr_result_option_calls.h`. The production `.inc` inventory
is now 31 files / 2,814 LOC.
Semantic channel query builtins, operator expression checking, LLVM MIR
local/block emission, DAG graph core, and enum declaration emission now live in
`src/semantic/type_checker_builtins_query_channel.h`,
`src/semantic/type_checker_operator_expr.h`,
`src/codegen/llvm_mir_local_emit.h`, `src/codegen/llvm_mir_block_emit.h`,
`src/semantic/type_checker_resolution_graph_core.c`, and
`src/codegen/transpiler_enum_decl_emit.c`. The production `.inc` inventory is
now 25 files / 1,675 LOC.
Semantic context helpers, LLVM log/array calls, and C backend MIR/base emitter
tails now live in `src/semantic/type_checker_context_helpers.c`,
`src/codegen/llvm_expr_log_calls.h`, `src/codegen/llvm_expr_array_calls.h`,
`src/codegen/transpiler_mir_emit_state.h`,
`src/codegen/transpiler_mir_emit_decls.h`, and
`src/codegen/transpiler_mir_pending_uses.c`. The production `.inc` inventory is
now 19 files / 835 LOC.
Formatter layout/io, semantic flow effects, LLVM intent observability calls,
and assignment checking now live in `src/compiler/fmt_layout.h`,
`src/compiler/fmt_io.h`, `src/semantic/type_checker_flow_effects.h`,
`src/codegen/llvm_expr_intent_observability_calls.h`, and
`src/semantic/type_checker_assignment.h`. The production `.inc` inventory is now
13 files / 297 LOC.
The former semantic flow parallel implementation header has since moved to the
compiled owner `src/semantic/type_checker_flow_parallel.c`.
C backend MIR emission contract/resource-hook helpers now live in
`src/codegen/transpiler_mir_emission_contract.c`, removing the remaining
`src/codegen/transpiler_emitters_base_a_part_d.inc` body while preserving the
base-A include order.
C backend slot/device builtin expression emitters now live in
`src/codegen/transpiler_slot_builtin_emit.h`, reducing
`src/codegen/transpiler_expr_emitters_part_a.inc` from 797 LOC to 531 LOC while
keeping slot sugar, secure slot token, and panic codegen smoke green.
C backend expression type inference now lives in
`src/codegen/transpiler_expr_type_infer.h`, reducing
`src/codegen/transpiler_helpers_core_b_part_c.inc` from 797 LOC to 296 LOC
while keeping the C transpile suite green.
C backend statement dispatch now lives in
`src/codegen/transpiler_statement_dispatch.c`, reducing
`src/codegen/transpiler_emitters_base_b_part_c.inc` from 803 LOC to 546 LOC
while keeping representative control-flow, parallel, and intent backend
compare paths green.
C backend role method emission, ability/vtable emission, hidden provenance
helpers, and role operator aliases now live in
`src/codegen/transpiler_domain_role_ability_emit.h`, removing the former
`src/codegen/transpiler_domain_role_part_a.inc` body while preserving
domain-role shim order.
The shared domain hosted-method MIR body path has since been lifted out to
`src/codegen/transpiler_hosted_method_body_emit.c`, so party/roster/relation/
effect/zone/world method bodies no longer depend on a role/ability-local
include-order helper.
Relation/effect declaration emission has also moved into
`src/codegen/transpiler_relation_effect_emit.c`, leaving its header as a
declaration-only seam.
Zone hosted-method forwarding/body emission now lives in
`src/codegen/transpiler_zone_methods_emit.c`, and `transpiler.c` no longer owns
a late include-order bridge for zone methods.
Ability-vtable specialization emission now lives in
`src/codegen/transpiler_domain_role_ability_emit.c`; the matching header is
declaration-only.
Ability/role/party nominal declaration emission now lives in
`src/codegen/transpiler_domain_nominal_emit.c`; the matching header is
declaration-only and no longer carries roster/relation implementation includes.
Generated-C `HashMap<String>` and map-keys inline runtime now lives in
`src/runtime/pgy_runtime_map_string_inline.h`, reducing
`src/runtime/pgy_runtime_part_ba_part_d.inc` from 767 LOC to 377 LOC while
keeping runtime ABI, panic codegen, and representative collection backend
compare paths green.
C backend MIR function emission now lives in
`src/codegen/transpiler_mir_func_emit.c`, reducing
`src/codegen/transpiler_emitters_base_b_part_a.inc` from 766 LOC to 162 LOC.
Generated-C runtime array sort kernels and scalar std/log/math helpers now live
in `src/runtime/pgy_runtime_array_sort_inline.h` and
`src/runtime/pgy_runtime_scalar_std_inline.h`, reducing
`src/runtime/pgy_runtime_part_ba_part_c.inc` from 759 LOC to 535 LOC while
keeping C transpile, ABI, panic codegen, and representative C/LLVM parity green.
MIR ABI layout lookup now lives in `src/compiler/mir_abi_layout.h`, reducing
`src/compiler/mir_public_part_b.inc` from 753 LOC to 420 LOC while keeping DAG,
AIR drift, and ABI smoke green. CFG contract validation now lives in compiled
owners: `src/compiler/mir_cfg_contract_validate.c` handles non-cleanup CFG
shape/source/fallback checks, and
`src/compiler/mir_cfg_contract_validate_cleanup.c` handles cleanup-edge,
rollback/invalidation, and cleanup-convergence checks. RIR validation now lives in
`src/compiler/rir_validation.h`, reducing `src/compiler/rir_public.inc` from
741 LOC to 269 LOC while keeping DAG, CFG body-dataflow, AIR drift, and ABI
smoke green. RIR lowering/enrichment now lives in
`src/compiler/rir_builder.h`, removing the former `src/compiler/rir_builder.inc`
body while preserving the `rir.c` flow -> build -> names -> validation include
order. C backend MIR intent inventory helpers now live in
`src/codegen/transpiler_mir_inventory_intent.h`, removing the old
`transpiler_emitters_mir_inventory_intent.inc` body while preserving the
existing SSA include-order shim. C backend call/spawn/channel expression
emission now lives in `src/codegen/transpiler_expr_call_spawn_emit.h`, removing
the old `transpiler_expr_emitters_part_e.inc` body while preserving the
expression emitter include order. Spawn wrapper and channel send/receive
lowering now live in `src/codegen/transpiler_spawn_channel_emit.c`, with the
matching header declaration-only. Member-style slot/host/slice call lowering
now lives in `src/codegen/transpiler_expr_call_member_emit.c`, reducing that
header to the call-dispatch shim. C backend builtin-call dispatch now lives in
`src/codegen/transpiler_expr_builtin_dispatch.h`, removing the old
`transpiler_expr_emitters_part_b.inc` body while preserving builtin-call
lowering order. Semantic builtin query and nominal contracts now live in
`src/semantic/type_checker_builtins_query.h` and
`src/semantic/type_checker_builtins_nominal.h`, removing two more production
`.inc` bodies and closing the former split `BuiltinKind builtin_resolve(...)`
return-type boundary. Generated-C runtime pool/FSM/timer helpers now live in
`src/runtime/pgy_runtime_pool_fsm_timer_inline.h`, so runtime part E is focused
on parallel/zone authority/effect-pool/unsafe/result/option helpers and runtime
cache freshness tracks the new owner directly. Semantic expression checking now
lives in `src/semantic/type_checker_expr.h`, and CFG body-dataflow smoke
follows that owner path instead of the former `.inc` body. C backend
function/class/control-flow emission now lives in
`src/codegen/transpiler_func_class_flow_emit.h`, removing the former
`transpiler_emitters_base_b_part_b.inc` body while preserving base-B include
order. Function fallback policy helpers now live in
`src/codegen/transpiler_func_flow_policy.c`, so the remaining function-flow
header keeps emission orchestration instead of owning policy lookup/formatting
statics. Generated-C runtime Box/Arena/Allocator/Array/Rc/primitive-slot helpers
now live in `src/runtime/pgy_runtime_memory_array_slot_inline.h`, and runtime
panic/ABI/cache freshness checks follow that owner path instead of the former
part-B `.inc`. LLVM domain core helpers now live in
`src/codegen/llvm_domain_core_helpers.h`, removing the old
`llvm_domain_helpers_part_a.inc` body while preserving LLVM domain lowering
order. Semantic relation/effect/projection helpers now live in
`src/semantic/type_checker_helpers_effects.h`, and CFG body-dataflow smoke
tracks that owner path instead of the former `.inc` body. Semantic function-body
checking now lives in `src/semantic/type_checker_program.h`, removing the former
`src/semantic/type_checker_program.inc` body while preserving the top-level
semantic TU include order. LLVM-linkable runtime
channel/qubit exports now live in
`src/runtime/pgy_runtime_lib_channel_quantum_exports.h`, removing the old
`pgy_runtime_lib_part_b_part_e.inc` body while preserving runtime ABI lifetime
checks. LLVM-linkable raw Queue/Map/Set exports now live in
`src/runtime/pgy_runtime_lib_raw_collection_exports.h`, and secure/device slot,
array, file IO, and string helper exports now live in
`src/runtime/pgy_runtime_lib_slot_array_io_string_exports.h`; runtime panic,
ABI lifetime, and compiler runtime-cache freshness checks now track those owner
headers directly. The remaining largest structural blockers are now broader
CFG/body dataflow source-of-truth, DAG source-of-truth completion, AIR negative
expansion, runtime frontier generalization, ABI ownership/pinning parity, and
cross-platform matrix enforcement.
LLVM-linkable runtime core exports now live in
`src/runtime/pgy_runtime_lib_core_exports.h`, reducing
`src/runtime/pgy_runtime_lib_part_b_part_a.inc` from 986 LOC to 909 LOC while
keeping exported symbol names unchanged. LLVM-linkable raw `List<T>` exports
now live in `src/runtime/pgy_runtime_lib_list_raw_exports.h`, reducing
`src/runtime/pgy_runtime_lib_part_b_part_a.inc` further from 909 LOC to 759 LOC
while keeping raw collection ABI smoke green. C backend `let` destructuring lowering
now lives in `src/codegen/transpiler_destructure_emit.c`, superseding the
temporary implementation header and keeping `transpiler_destructure_emit.h`
declaration-only. The original extraction reduced
`src/codegen/transpiler_emitters_base_b_part_c.inc` from 976 LOC to 873 LOC and
kept destructure array/tuple C/LLVM parity green. Generated-C queue inline
runtime now lives in `src/runtime/pgy_runtime_queue_inline.h`, reducing
`src/runtime/pgy_runtime_part_ba_part_e.inc` from 969 LOC to 773 LOC while
keeping queue/channel smoke parity green. Generated-C `HashMap<Int>` key
adapters now live in `src/runtime/pgy_runtime_map_int_key_inline.h`, reducing
`src/runtime/pgy_runtime_part_ba_part_d.inc` from 963 LOC to 815 LOC while
keeping map backend-compare cases green. LLVM-linkable primitive slot exports
for `Slot<Double>`, `Slot<Bool>`, and `Slot<String>` now live in
`src/runtime/pgy_runtime_lib_slot_exports.h`, reducing
`src/runtime/pgy_runtime_lib_part_b_part_d.inc` from 947 LOC to 790 LOC while
keeping runtime panic ABI/codegen and full ABI smoke green. LLVM-linkable
standard string/conversion/math/random exports now live in
`src/runtime/pgy_runtime_lib_std_exports.h`, reducing
`src/runtime/pgy_runtime_lib_part_b_part_e.inc` from 817 LOC to 761 LOC while
keeping runtime ABI lifetime and ABI pipeline smoke green. MIR declaration
header inventory helpers now live in `src/compiler/mir_decl_headers.h`, reducing
`src/compiler/mir_public_part_a.inc` from 959 LOC to 789 LOC while keeping DAG,
AIR drift, and ABI smoke green. RIR public vocabulary name helpers now live in
`src/compiler/rir_names.h`, reducing `src/compiler/rir_public.inc` from 911 LOC
to 804 LOC while keeping RIR validation/dump consumers on the same vocabulary.
C backend parallel capture analysis now lives in
`src/codegen/transpiler_parallel_capture.c`, reducing
`src/codegen/transpiler_emitters_base_b_part_b.inc` from 957 LOC to 730 LOC
while keeping the parallel channel-sum backend-compare path green.
C backend stdlib call lowering now lives in
`src/codegen/transpiler_expr_stdlib_builtin.h`, reducing
`src/codegen/transpiler_expr_emitters_part_d.inc` from 946 LOC to 26 LOC while
keeping representative stdlib/string/collection backend-compare paths green.
C backend overlay/projection invalidation and zone-layer bind helpers now live
in `src/codegen/transpiler_overlay_projection.h`; the old
`transpiler_helpers_core_a_part_b.inc` include body was removed and the source
`.inc` count is now 158/159. The runtime frontier contract smoke also now reads
the real world frontier owner in `transpiler_domain_role_part_d.inc`.
C backend `let` declaration lowering now lives in
`src/codegen/transpiler_let_emit.c`, reducing
`src/codegen/transpiler_emitters_base_a_part_a.inc` from 905 LOC to 138 LOC
while keeping the C transpile suite and representative let-heavy backend
compare cases green.
C backend MIR block statement emission now lives in
`src/codegen/transpiler_mir_block_emit.c`; the old
`transpiler_emitters_base_a_part_c.inc` include body was removed, dropping
source `.inc` total to 49,911 LOC while keeping MIR/DAG/AIR smoke and
representative MIR-heavy backend compare paths green.
C backend intent declaration emission now lives in
`src/codegen/transpiler_intent_emit.c`; the old
`transpiler_emitters_intent.inc` include body was removed, dropping source
`.inc` total to 48,949 LOC and leaving no production `.inc` above 900 LOC.
The header is now declaration-only, and intent cleanup MIR eligibility is read
through `transpiler_active_can_emit_intent_cleanup_from_mir(...)` rather than a
direct `ctx->mir` probe in the emitter owner.
Generated-C runtime intent active-step/recent query accessors now live in
`src/runtime/pgy_runtime_intent_query_inline.h`; panic helpers and checked
arithmetic exports live in `src/runtime/pgy_runtime_panic_checked_inline.h`,
reducing `src/runtime/pgy_runtime_part_ba_part_b.inc` from 894 LOC to 705 LOC
while keeping panic codegen, panic ABI, runtime lifetime, and full ABI smoke
green.

## Closed Or Mostly Closed

### Stable Surface

The frozen beta subset is now fairly clear:

- generics: exact, ability, multi-bound, and implemented default type argument resolution;
- own/ref: anchored slot-handle and boundary-visible aggregate subset;
- collections: `List<T>`, `Set<T>`, `HashMap<String|Int|Long|Bool, T>`;
- runtime observability: `last / history / active / recent`;
- C/LLVM parity for the currently smoke-covered stable paths.

The main surface trust risk is no longer "what should exist", but whether every parser-accepted surface is either fully closed or explicitly rejected.

### Runtime Propagation Slices

The following propagation slices are now locked by tests:

- world derived-state bounded recompute: `world_fixpoint_abi`;
- relation/effect/zone projection-chain bounded recompute: `projection_chain_abi`;
- zone lifecycle bounded frontier loop: `zone_frontier_abi`;
- v1 handoff materialization projection freshness: `handoff_projection_frontier_abi`;
- active world-owned zone handoff to projection-backed world-state freshness: `handoff_world_state_frontier_abi`;
- action-caused layer/state handoff to active world-derived aliases: `handoff_layer_state_frontier_abi`;
- embedded world-zone projection freshness after direct assignment: `world_embedded_projection_abi`;
- embedded world-zone projection freshness after subject method call: `world_embedded_method_projection_abi`;
- embedded world-zone projection freshness across a simple branch/join: `world_embedded_branch_projection_abi`;
- embedded world-zone action-caused layer/state freshness after subject action call: `world_embedded_action_frontier_abi`;
- embedded world-zone action-caused layer/state freshness after subject action call with fixed-capacity effect pool: `world_embedded_action_pool_frontier_abi`;
- direct C/LLVM stdout parity for branch/join embedded projection visibility: `world_embedded_branch_projection_visibility`.
- direct C/LLVM stdout parity for handoff materialization projection visibility: `handoff_projection_frontier`.
- direct C/LLVM stdout parity for active world-owned handoff state visibility: `handoff_world_state_frontier`.
- direct C/LLVM stdout parity for action-caused handoff layer/state visibility: `handoff_layer_state_frontier`.
- direct C/LLVM stdout parity for embedded world-zone action-caused layer/state visibility: `world_embedded_action_frontier`.
- direct C/LLVM stdout parity for embedded world-zone action-caused fixed-capacity effect pool visibility: `world_embedded_action_pool_frontier`.

This is meaningful progress: the remaining propagation problem is no longer the absence of bounded loops. It is now the generalization of those loops across handoff and broader world-zone propagation families.

### Backend Parity

The current parity baseline is strong for the covered subset:

- `make test-abi` covers C and LLVM ABI smoke cases.
- `make llvm-test-backend-compare` now includes 52 backend-compare cases.
- authority failure bool/string/code drift was closed through `authority_failure_surface`.
- intent authority snapshot propagation was closed through `intent_authority_snapshot_abi` and `intent_authority_snapshot`.
- embedded branch projection visibility is now a direct backend-compare case, not only ABI smoke.

### MIR Path

Routine body emission is largely MIR-driven. Missing ordinary function, method, and intent carriers are hard errors instead of silent partial output. The remaining issue is structural: declaration inventory is still carried as AST-shaped data inside the MIR program instead of a dedicated declaration IR.

## Remaining Beta Blockers

### 0. Function CFG And Body Dataflow Source Of Truth

Status: highest semantic architecture blocker.

Already closed:

- HIR has function CFG v0 with predecessor/reachability, dominator/frontier, loop-depth, local-def, and phi-candidate skeleton facts.
- MIR has routine/block/instruction/cleanup blocks, SSA version maps, def/use summaries, rollback/invalidation exceptional CFG, liveness/DCE slices, and backend vertical slices.
- RIR carries flow-block summaries for resource/projection/world-handoff/invalidation/authority-loss style facts.

Remaining work:

- promote all-path return, reachability, the general branch/join assignment
  lattice beyond the sealed local-`let` surface, move/use-after-move, borrow/ref
  lifetime, drop/cleanup, zone/effect transition, projection freshness, and
  parallel/channel task-boundary checks to CFG/dataflow facts;
- add interprocedural body summaries for return, escape, move, borrow, drop, effect, zone, task, and channel behavior;
- require diagnostics to include path provenance, previous state, `Reason`, and `Fix`;
- make C and LLVM consume the same facts for the frozen subset.

Concrete next work:

- write a CFG/body dataflow inventory test that compares HIR/RIR/MIR facts for representative bodies;
- all-path return is now migrated, direct/terminating-if/exhaustive-match
  unreachable warnings are emitted as `PGY_SEM_UNREACHABLE_CODE`, and stable local `let`
  use-before-init is sealed by syntax plus `PGY_SEM_UNINIT_LOCAL`;
- `QubitSlot` loop move/join now has source-level regression for break-exit
  consumption and continue-backedge consumed-resource detection; migrate richer
  nested/exceptional reachability provenance and the wider branch/join
  assignment lattice next if delayed assignment becomes part of the beta-stable
  surface;
- `defer` cleanup-body terminators are now isolated from the surrounding CFG
  path, and cleanup-body resource moves/releases are snapshot-restored so they
  do not consume the current path's live resources;
- the direct `type_check_statement()` fallback now uses the same defer cleanup
  snapshot helper as CFG body flow; full drop insertion/validation remains a
  beta blocker;
- resource snapshots now include anchored slot state, so terminating-branch
  releases no longer poison reachable fallthrough paths and fallthrough releases
  remain conservatively joined;
- parallel task bodies now use CFG/resource snapshots: task-local terminators do
  not terminate the outer path, resource moves/releases are joined after the
  parallel block, and duplicate cross-task consumption reports
  `PGY_SEM_PARALLEL_SLOT_CONFLICT`. Blocking channel send of a movable resource
  in a parallel task is now covered by the same consume/join regression;
- then migrate remaining ownership borrow/drop, followed by zone/effect and
  channel receive/backpressure/cancellation facts.

Beta closure condition:

- function/action/intent body safety is no longer AST traversal policy; it is a CFG/dataflow contract with semantic diagnostics and backend parity coverage.

### 1. Handoff And Broader World-Zone Propagation

Status: highest runtime blocker.

Already closed:

- bounded loops exist for world derived state, zone lifecycle, and projection chains;
- embedded projection freshness works for direct assignment, method call, and a simple branch/join;
- v1 handoff materialization keeps source and target projection freshness aligned on C and LLVM.
- active world-owned zone handoff keeps projection-backed world state and composed `all` state fresh on C and LLVM.
- embedded world-owned zone subject action calls now propagate action-caused effect layer/state freshness to active world-derived aliases on C and LLVM for single effect slots and fixed-capacity effect pools.
- action-caused effect handoff keeps target zone layer/state and active world-derived layer/state aliases fresh on C and LLVM.

Remaining work:

- define handoff propagation as a runtime contract, not just a semantic trace;
- ensure C and LLVM use the same recompute order, pass limit, hard-fail boundary, and provenance stamp vocabulary;
- document whether handoff is materialization, identity move, snapshot, or another explicit beta rule.

Concrete next tests:

- `handoff_authority_rejection_abi`;
- broader real authority rejection path beyond intent step-local `authorized by` validation.

Beta closure condition:

- handoff mutation/query results are identical on C and LLVM;
- stale projection or stale world-state reads after handoff are covered;
- pass-limit overflow remains hard-fail;
- docs describe the beta handoff rule without implying a richer v1 ownership model than exists.

### 2. Recoverable Runtime Failure Surface

Status: baseline exists, richer query surface remains.

Runtime panic contract progress:

- `src/runtime/pgy_runtime_panic_contract.h` now provides the shared hard-fail panic vocabulary.
- Exported typed slot and secure-slot runtime paths no longer silently fallback for released-slot or invalid-token hard failures.
- Inline typed slot and secure-slot runtime paths use the same released-slot, invalid-secure-token, and double-release classes.
- `runtime-panic-contract-test-smoke` gates the implementation contract, and `runtime-panic-abi-test-smoke` provides executable evidence for released-slot, invalid-token, and double-release aborts.

Already closed:

- generated C and LLVM runtime-lib share authority validation vocabulary;
- non-aborting authority validation exposes `last_ok`, `last_zone`, `last_participant`, and `last_reason`;
- `authority_failure_abi`, `authority_failure_surface`, `intent_authority_snapshot_abi`, and `intent_authority_snapshot` are green.
- semantic `authorized by` validation now resolves participants to concrete zone subject slots, so same-type non-authority slots and ambiguous same-type authority mappings are rejected before lowering.

Remaining work:

- decide the minimum stable query surface for zone/world/authority boundary failures;
- expose the same reason/state model for the major recoverable paths, not just authority validation;
- keep invariant breaks as hard-fail and do not blur them into recoverable errors.

Concrete next tests:

- authority missing-zone query;
- authority missing-participant query;
- boundary mismatch query after world handoff;
- intent step failure reason tied to runtime state.

Beta closure condition:

- recoverable failures return `Bool`, `Result<T>`, or queryable runtime state consistently;
- hard-fail cases remain hard-fail;
- docs, diagnostics, and runtime accessors use the same reason vocabulary.

### 3. Declaration-Side MIR Inventory Debt

Status: functional but structurally incomplete.

Already closed:

- backend entrypoints receive MIR bundle data;
- routine body paths use MIR routines and fail hard on missing carriers;
- intent step check/eval/meta carrier absence is no longer silently tolerated.

Remaining work:

- split declaration inventory into a dedicated IR shape instead of AST-carried inventory;
- remove raw host-name state and duplicated named-decl lookup from helper paths where possible;
- keep declaration emission errors structured when inventory is incomplete.

Concrete next work:

- introduce a small `MIRDeclInventory`/`MIRDeclHeader` view that contains only the metadata backends need;
- migrate zone/world/relation/effect declaration emitters to that view first;
- add regression that a missing declaration inventory item fails with a backend error, not partial output.

Beta closure condition:

- remaining AST references are documented as declaration metadata carriers or removed;
- backend users can tell which declaration metadata is required;
- LLVM and C consume the same declaration inventory truth for the frozen subset.

### 4. Type-Resolution DAG Source Of Truth

Status: graph infrastructure exists and the retired recursive/materializer
fallback counters are now gated at zero for the current stable path. This is
still a structural closure axis, but the remaining work is evidence/modeling
completion and owner-local simplification rather than a broad fallback rewrite.

Already closed:

- graph inventory, cycle diagnostics, and topo derivation exist;
- provider-first staged worklist is active for top-level declarations and synthetic local/projection nodes;
- generic default type, constraints, where-bound, and ability consumers run through staged DAG paths;
- cycle diagnostics use `Contract source`, `Reason`, and `Fix` vocabulary.
- non-generic nominal class type references, known non-class scope symbols,
  generic consumer paths, and alias-cycle diagnostics are represented by
  graph-backed metadata/evidence rather than recursive resolver calls.
- current local evidence (2026-05-24): retired resolver and zero-only stage
  materializer telemetry have been removed; active DAG evidence reports
  `metadata_dead_ends=0`, `metadata_entries=3735`, `metadata_owned=261`, and
  `metadata_hits=8771`.

Remaining work:

- widen module/generic/ability provenance so diagnostics and AIR evidence can
  point at the graph fact that proved or rejected the dependency;
- keep the graph as the default source for provider/consumer ordering in the
  semantic paths that already have inventory edges;
- keep module import DFS and type-resolution DAG responsibilities separate.

Concrete next work:

- keep `type-resolution-dag-test-smoke` and
  `type-resolution-resolver-inventory-test-smoke` as the hard cap for fallback
  resurrection: resolver calls, body fallbacks, stage materializer calls, and
  metadata dead-ends must remain zero;
- add targeted regressions where declaration order would fail without
  provider-first topo scheduling;
- promote local contract and projection path handlers from "covered node family" to "semantic source of truth" where possible.

Beta closure condition:

- no known frozen-subset type dependency relies only on declaration order;
- graph-backed errors include stable provenance;
- the docs stop calling for a full DAG rewrite and instead name the exact remaining migration paths.

### 4b. Long-Term Modularization Boundary

Status: necessary structural work is underway, but the stop condition is not met.

Already closed:

- several semantic leaf/helper families now live in real translation units;
- module contract include-order debt was removed;
- type-resolution graph primitive, collector, label, domain, and declaration helper seams have started moving out of `.inc`;
- speed baseline and `perf-summary` are available to catch modularization regressions.
- production runtime/codegen/compiler include files are below the 1,000 LOC
  gate; the current MIR public split is `part_a=959` and `part_b=800`, guarded
  by `make backend-inc-size-test-smoke` and `make inc-sentinel-test-smoke`.
- tooling conformance is green locally; `tests/tooling_conformance_smoke.sh`
  invokes formatter smoke through `bash`, avoiding Linux execute-bit drift on
  mounted worktrees.

Remaining work:

- reduce semantic `.inc` files above 800 LOC;
- continue converting behavior-heavy codegen/runtime/compiler `.inc` families
  into real owner translation units instead of adding new split fragments;
- extract the files currently closest to the cap (`964`, `962`, `959`, `957`,
  `947`, `946`, `925`, `911`, `909` LOC) into owner `.c`/`.h` seams before adding new
  behavior to those families;
- make `type_checker.c` orchestration-only rather than an include aggregator;
- split backend/runtime owners so future core features do not require editing multi-thousand-line include fragments.

Beta closure condition:

- DAG/stage/declaration/backend/runtime owner boundaries are explicit enough that a frozen-subset feature can be changed without relying on include-order side effects;
- remaining `.inc` usage is zero under `src`; generated tables, local macro tables, and private test fixtures must use named `.h` / `.c` owners or `.cases.h` test fragments;
- any remaining representation debt is documented as internal and non-user-visible.

### 5. Arena And Lifetime Boundaries

Status: direction is correct; remaining boundaries are narrow.

Already closed:

- arena/index discipline is documented;
- semantic scratch arena is used across several diagnostic and coverage paths;
- LLVM has clearer scratch, persistent, and result-owned lanes;
- memory tests cover hard-fail unwrap and pointer lifetime guards.

Remaining work:

- finish owner shell boundaries such as context/registry/result outer shell ownership;
- clarify runtime ABI ownership for returned strings and helper-produced payloads;
- prevent caches from retaining arena-owned transient pointers.

Concrete next work:

- audit helper names that return `char *`, `const char *`, or grow-array payloads;
- classify each as borrowed, scratch-owned, result-owned, persistent-owned, or runtime-owned;
- add one focused regression for a returned diagnostic/runtime string that survives scratch teardown.

Beta closure condition:

- helper ownership is mechanically reviewable;
- runtime ABI return ownership is documented and tested;
- no frozen-subset diagnostic or runtime query relies on a scratch pointer after the producing phase ends.

## Secondary Improvement Opportunities

### Contract Clause Density

The language is semantically stronger than it is comfortable to write. `requires / within / authorized by / causes / refresh / publish / bind` still creates repetition across actions, intent steps, and zones.

Recommended beta approach:

- do not add new keywords before beta;
- keep the explicit form as the canonical source of truth;
- keep compressed examples only when they are already semantically equivalent and smoke-covered;
- treat intent compression as a sequence of fail-closed inference rules. The
  first implemented rule is single-subject participant `who` inference:
  omitted `who` derives from the enclosing intent only when exactly one
  subject participant exists. Multi-subject intents remain explicit;
- expand `docs/69_authoring_pair_examples.md` with examples that are actually tested.

### Projection Diagnostics

Projection diagnostics have improved, but beta should keep pushing toward a single diagnostic shape:

- target;
- source;
- projection kind;
- field path or map;
- `Reason:`;
- `Fix:`.

Recommended next work:

- add one semantic regression each for missing source field, ambiguous path, wrong projection kind, and duplicate field map;
- make those cases appear in the beta support board as completed only when the wording is stable.

### Documentation Hygiene

The docs are useful but large, and some older docs still describe broader ambitions beside current beta facts.

Recommended next work:

- keep `README.md`, `TODO.md`, `docs/17_development_status.md`, `docs/18_language_status.md`, and `docs/70_beta_closure_master_board.md` as the live truth set;
- avoid broad edits to old roadmap docs unless they contradict beta support;
- add a beta release checklist that points to exact make targets and exact smoke cases.

## Recommended Execution Order

Operating change for the next sprint:

- Use a lean debt-slice loop rather than a test-first loop. Pick one owner,
  finish the implementation slice, run the local gate, then batch the wide
  regression.
- This is not a reduction in rigor. Full regression remains required before
  closure, but running it after every small edit has been slowing actual debt
  removal.
- The next sprint should not be "add more tests"; it should be "remove one
  source-of-truth duplication or fallback seam, then prove it with the smallest
  meaningful gate."

1. Promote DAG staged resolution.
   Audit remaining recursive type-resolution consumers and move frozen-subset dependency ordering behind graph-backed paths. This is now the highest-value beta blocker because it defines whether the language can keep module/generic/authority contracts stable as the surface grows.

2. Continue long-term modularization around the DAG boundary.
   Split graph inventory/stage/declaration helpers until semantic ownership is explicit enough to avoid include-order side effects. Prioritize `.inc` seams that directly affect type resolution, module contracts, and backend declaration inventory.

3. Close the remaining handoff propagation tails.
   The active projection-backed world-state and action-caused layer/state slices are now covered; next add handoff authority/failure cases and implement any missing queryable rejection path.

4. Complete recoverable failure state.
   Expand queryable runtime failure beyond the current authority baseline while preserving hard-fail boundaries.

5. Shrink declaration inventory debt.
   Build a dedicated declaration metadata view for frozen domain declarations and migrate C/LLVM consumers.

6. Finish arena/lifetime ownership review.
   Classify returned helper ownership and add a small regression that proves result-owned data outlives scratch formatting.

7. Freeze docs and examples.
   Update the live truth set, stable examples, and release checklist only after the code paths are covered.

## Practical Beta Exit Criteria

PergyraLang can be called beta when the following are simultaneously true:

- `make test-all` is green;
- `make test-abi` is green for C and LLVM smoke;
- `make llvm-test-backend-compare` is green with the frozen subset cases;
- Linux support is C+LLVM. Current live policy is Windows C regression as the
  official beta line, with Windows LLVM/backend compare only when executable
  toolchain evidence is present; see `docs/100_beta_readiness_checklist.md`;
- runtime propagation has handoff/general world-zone coverage, not only projection-chain and embedded slices;
- recoverable runtime failures expose queryable state for the stable failure surface;
- declaration-side MIR debt is either closed for the frozen subset or explicitly documented as non-user-visible representation debt;
- function/action/intent body safety is CFG/dataflow-backed for reachability, all-path return, init, move/borrow, cleanup, effect/zone, and parallel/channel facts;
- type-resolution DAG is the source of truth for frozen-subset dependency ordering;
- arena/runtime ABI ownership rules are reviewable and tested;
- stable subset, explicit reject, and beta-out-of-scope wording match across README, TODO, status docs, diagnostics, and smoke examples.

## Current Risk Summary

The project is close enough that broad new design should stop. The remaining value is in making the existing language truthful:

- every accepted beta surface should either run through semantic/runtime/C/LLVM/tests/docs or be rejected explicitly;
- every runtime state transition should be observable enough to debug;
- every parity case should fail loudly when C and LLVM drift;
- every structural debt item should be described as either user-visible beta risk or internal representation debt.

The next most valuable implementation target is the remaining handoff propagation tail: authority/failure visibility after transfer. Projection, active world-state, and action-caused layer/state handoff slices are now covered on both backends.

## Progress Note - 2026-05-02 Intent On-Receiver Compression

- Implemented the second narrow Intent-Compress `who` rule:
  `on: receiver.Action(...)` derives omitted `who` only when `receiver` is an
  intent subject participant and the receiver's subject declares `Action`.
- The rule is fail-closed. Multiple distinct matching receivers do not infer,
  so ambiguous business authority stays explicit instead of hidden in the
  compression layer.
- Provenance now reaches AST print, semantic contract summary, DIR, AIR, and
  `pgy.air.graph.v1` JSON as `who_from_on_receiver`.
- This improves the intent verbosity pain point without claiming full intent
  inference. `where`, `using`, `requires`, and `authorized by` compression are
  still open design/implementation work.

## Progress Note - 2026-05-02 Intent On-Receiver Where/Using Compression

- Extended the `on` evidence path so `on: receiver.Action(...)` can derive the
  step `where` from the resolved action's `within <Zone>` clause.
- Existing unique zone binding inference then derives `using` when exactly one
  intent binding has that zone type.
- Explicit `where` remains authoritative, and conflicting `on` action zones do
  not infer. This keeps the compression rule narrow and avoids hiding authority
  or effect decisions inside syntax sugar.

## Progress Note - 2026-05-02 Intent On-Receiver Action Contract

- A single resolved `on: receiver.Action(...)` now inherits the action
  contract's `requires` and `causes` clauses when the step leaves them omitted.
- `authorized by self` maps to the receiver alias. `authorized by
  <action-param>` now maps through a single `on` call argument when the
  argument is a declared intent participant identifier. Expression-valued
  arguments, missing bindings, and multiple `on` calls remain explicit rather
  than silently inferred.
- The authority provenance now survives lowering: DIR records
  `authorized_by_inherited_from_action`, AIR records `authority_from_action`,
  and AIR drift diagnostics can report `authority_provenance=action-inherited`.
  The action-derived zone source is also preserved as
  `where_inherited_from_action` / `source_from_action` in DIR/AIR JSON.
  Action-derived `requires` and `causes` are preserved as
  `requires_inherited_from_action` / `requires_from_action` and
  `causes_inherited_from_action` / `causes_from_action`.
- `causes` is no longer AIR-only provenance. Intent RIR lowering materializes
  step causes as `RIR_RESOURCE_EFFECT_INSTANCE` plus `RIR_OP_ATTACH_EFFECT`,
  preferring the unique zone effect-slot anchor over the effect type name when
  the current zone makes that anchor unambiguous. AIR strict evidence observes
  it as `AIR_EVIDENCE_RIR_EFFECT_PROPAGATION`.
- Action-derived `authorized by` is also no longer accepted as a boundary flag
  alone in the parsed on-receiver regression. The fixture now requires matching
  `AIR_EVIDENCE_RIR_AUTHORITY`, `has_rir_authority_evidence`, and
  `rir_authority_evidence_name` for the inherited authority participant.
- Multiple `on` actions do not merge contracts. This keeps the compressed
  surface predictable and fail-closed.

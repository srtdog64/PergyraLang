# Include Cleanup Status

Last updated: 2026-04-27

This note records the current state of the beta include-cleanup track. It is a
progress ledger, not a new language surface.

## Closed In This Slice

- Production `.inc` cleanup is closed for `src/runtime`, `src/codegen`,
  `src/compiler`, and `src/semantic`: there are now **0 production `.inc`
  files / 0 LOC** under `src`, excluding `src/tests/**/*.inc` fixtures.
- Owner-size policy is now stricter than the historical `.inc` cleanup target:
  600 LOC is the default split-review threshold for any production `.c` or
  private owner `.h`; 1,000 LOC is only the hard stop / temporary risk line.
  A production owner above 600 LOC must either be split in the current sprint
  or be listed here with a named follow-up owner seam. New owners should aim
  below 600 LOC unless the file is a compact table, generated ABI surface, or a
  deliberately single-entry orchestration layer with no mixed responsibility.
- The final pass-through and leaf helper shims were renamed to named private
  owner headers, including `pgy_runtime_inline_core.h`,
  `transpiler_base_a_emitters.h`, `transpiler_base_b_emitters.h`,
  `transpiler_expr_emitters.h`, `transpiler_helpers_core_{a,b}.h`,
  `transpiler_domain_role_emit.h`, and `llvm_expr_call_owners.h`.
- Compiler runtime cache freshness now tracks the renamed runtime owner
  headers instead of stale `.inc` dependency paths.
- `inc-sentinel-test-smoke` now treats `src/tests/**/*.inc` as the only
  tolerated fixture lane and caps it at the current 47 files; production
  `.inc` reintroduction is a hard failure.
- C backend scalar/math/string stdlib call lowering now lives in
  `src/codegen/transpiler_expr_stdlib_scalar_builtin.h`. The main
  `transpiler_expr_stdlib_builtin.h` dispatcher drops from 917 LOC to 751 LOC
  while preserving dispatch order.
- C backend Map/List/Set/Queue stdlib call lowering now lives in
  `src/codegen/transpiler_expr_stdlib_collection_builtin.h`. The main
  dispatcher drops further to 432 LOC while preserving collection runtime
  specialization order.
- `production-header-size-test-smoke` caps production owner headers at 1,000
  LOC by default, with no temporary per-header allowance. LLVM declaration
  inventory helpers now live in `src/codegen/llvm_inventory_internal.h`, so
  `src/codegen/llvm_internal.h` stays under the same production cap.
- LLVM statement parallel/async/select lowering now lives in
  `src/codegen/llvm_stmt_parallel_async.c`. `src/codegen/llvm_stmt.c` drops to
  3,078 LOC, and the full `llvm-test-backend-compare` suite remains green.
- LLVM statement loop/match lowering now lives in
  `src/codegen/llvm_stmt_loop_match.c`. `src/codegen/llvm_stmt.c` drops to
  2,582 LOC, while the control-flow owner keeps `while`, `for`, and `match`
  parity covered by the full backend compare suite.
- LLVM statement ownership now has separate real TUs for type inference,
  let helpers, let lowering, with lowering, loop/match lowering, and
  parallel/async/select lowering:
  `src/codegen/llvm_stmt_type_infer.c`,
  `src/codegen/llvm_stmt_let_helpers.c`,
  `src/codegen/llvm_stmt_let_with.c`, `src/codegen/llvm_stmt_with.c`,
  `src/codegen/llvm_stmt_loop_match.c`, and
  `src/codegen/llvm_stmt_parallel_async.c`. `src/codegen/llvm_stmt.c` is now
  914 LOC, and every statement owner TU is below 1,000 LOC while backend
  compare remains green.
- LLVM domain method lookup, implicit-self classification, operator alias
  helpers, and propagation provenance stamping now live in
  `src/codegen/llvm_domain_method_helpers.c`. `src/codegen/llvm_domain.c`
  drops to 3,340 LOC.
- LLVM world sync emission now lives in
  `src/codegen/llvm_domain_world_sync.c`. `src/codegen/llvm_domain.c` drops to
  2,663 LOC, and the helper include width was reduced so the build remains
  warning-clean under the current `-Wall -Wextra` gate.
- LLVM zone sync emission now lives in
  `src/codegen/llvm_domain_zone_sync.c`. `src/codegen/llvm_domain.c` drops to
  1,649 LOC, and the former `llvm_domain_core_helpers.h` mega-header is split
  into focused owner headers for role lookup, declaration parts, projection
  count/value/sync body, and zone-layer binding. This keeps the extracted zone
  TU warning-clean without adding unused attributes or new `.inc` files.
- MIR slot/claim type helper extraction now lives in
  `src/compiler/mir_type_helpers.c` / `.h`. `src/compiler/mir.c` drops from
  2,927 LOC to 2,742 LOC without changing MIR lowering behavior, and
  `make test-mir` remains green.

- `src/codegen/transpiler_context.c` now owns the C backend output/context
  primitives that used to live in include bodies:
  - `CodeBuf` allocation, growth, raw/formatted writes, and file dump
  - `TranspilerCtx` create/destroy ownership
  - indentation emission helpers
  - backend error message/hint allocation
  - transpiler scratch arena string helpers
- `src/codegen/transpiler_context.h` is the private C backend seam consumed by
  `transpiler.c`; these helpers are no longer behavior owned by
  `transpiler_helpers_core_a_part_a.inc`.
- `src/codegen/transpiler_symbols.c` now owns C backend local symbol
  bookkeeping that used to live in include bodies:
  - slot variable registration and token lookup
  - typed local registration and lookup
  - alias expression registration and lookup
  - view-like and projection-borrow local metadata
- `src/codegen/transpiler_symbols.h` is the private C backend seam consumed by
  `transpiler.c` and the included emitter fragments. This keeps ownership ABI
  tracking out of `transpiler_helpers_core_a_part_a.inc`.
- `src/codegen/transpiler_decl_lookup.c` now owns C backend declaration
  lookup helpers that used to live in include bodies:
  - MIR-backed named declaration lookup
  - function/intent/callable lookup including extern functions
  - nominal/domain lookup for class, zone, world, relation, effect, party, and
    roster declarations
  - private C backend type-alias target resolution
  - declaration inventory cache lookup
  - current host binding lookup
  - nominal host declaration cache lookup
  - declaration method list projection
  - current-host method lookup
  - nominal-host method lookup and cache update
  - ability and event declaration lookup
- `src/codegen/transpiler_decl_lookup.h` is the private C backend seam consumed
  by `transpiler.c` and the included emitter fragments. This removes another
  AST-carried declaration inventory helper family from `.inc` ownership.
- `src/codegen/transpiler_projection.c` now owns C backend projection
  provenance, world/zone query lookup, and nominal type predicates:
  - C-backend-prefixed zone domain slot lookup
  - current world field predicate lookup
  - zone state and layer slot lookup
  - world zone slot lookup
  - world-zone declaration resolution
  - nested vessel-backed projection source path resolution
  - projection literal lowering shared by `ToObject`, `ToTObject`, and zone
    refresh-map emission
  - subject type predicate lookup
  - nominal host type predicate lookup
- `src/codegen/transpiler_projection.h` is the private C backend seam consumed
  by `transpiler.c` and the included emitter fragments. This removes the last
  behavior-heavy helper body from `transpiler_helpers_core_a_part_a.inc`.
- `src/codegen/transpiler_nominal.c` now owns C backend nominal member type
  lookup and host-expression type resolution:
  - current-host field type lookup
  - class/subject/zone/world/relation/effect member type lookup
  - nominal host expression type resolution for lowered member access
- `src/codegen/transpiler_type_render.h` exposes the private
  `transpiler_render_type_name_local()` seam so real translation units can
  reuse the existing generic-binding-aware type renderer without static buffer
  ownership.
- `src/codegen/transpiler_enum.c` now owns enum variant qualification lookup.
- `src/codegen/transpiler_operator.c` now owns C backend operator-overload
  lookup, operator method alias matching, and role operator method traversal.
- `src/codegen/transpiler_type_alias.c` now owns C backend type-alias
  declaration emission. The old `emit_type_alias_decl(...)` body was removed
  from `transpiler_emitters_base_b_part_c.inc`, and the implementation now uses
  the existing private type-render/type-specialization seams from a real
  translation unit.
- `src/codegen/transpiler_type_require.c` now owns C backend type requirement
  checks for AST type nodes and resolved type names. The old
  `src/codegen/transpiler_emitters_type_require.inc` include body was deleted,
  `transpiler_emitters_base_a_part_a.inc` no longer includes it, and the helper
  is now a real translation-unit seam shared by extern, declaration, and domain
  emitters.
- `src/codegen/transpiler_extern.c` now owns C backend extern declaration
  emission. `emit_extern_block(...)` was removed from
  `transpiler_emitters_base_b_part_b.inc`, keeping the extern pass as a real
  declaration owner instead of another include-body function.
- `src/codegen/transpiler_type_declarator.c` now owns C backend declarator
  rendering for ordinary types, event-handler function pointers, function
  pointer values, and function signatures. This removes another shared helper
  family from `transpiler_helpers_core_b_part_c.inc`.
- `src/codegen/transpiler_log_normalize.c` now owns C backend LogBanner
  indentation normalization. The remaining expression-core lowering later moved
  to `transpiler_expr_core_emit.h`.
- `src/runtime/pgy_runtime_intent_exit.h` now owns the generated-C inline
  intent exit implementation. The ABI surface remains `static inline
  pgy_intent_exit_export(...)`, but the large observability snapshot/cleanup
  body no longer lives in `pgy_runtime_part_ba_part_b.inc`.
- `src/runtime/pgy_runtime_slot_macros.h` now owns the generated-C inline
  `DeviceSlot<T>` and `SecureSlot<T>` macro families. The built-in slot
  instantiation order now stays in `pgy_runtime_builtin_storage_inline.h`, but the macro
  bodies no longer inflate that split include.
- `src/runtime/pgy_runtime_intent_history.h` now owns the generated-C inline
  last-intent history step accessors. The exported inline ABI names remain
  unchanged, while `pgy_runtime_intent_trace_inline.h` now carries the
  per-field borrowed string accessor block.
- `src/runtime/pgy_runtime_lib_core_exports.h` now owns LLVM-linkable runtime
  core exports for logging, wall-clock sleep/time, and integer string
  conversion. `pgy_runtime_lib_part_b_part_a.inc` is now focused on collection
  raw export bodies instead of carrying unrelated core runtime functions.
- `src/runtime/pgy_runtime_lib_list_raw_exports.h` now owns LLVM-linkable raw
  `List<T>` collection exports. Runtime lib part A is now focused on raw queue
  and map exports instead of carrying all raw collection families together.
- `src/codegen/transpiler_destructure_emit.h` now owns C backend
  `let`-destructuring statement lowering. The base-B statement dispatcher keeps
  the same switch surface but no longer carries tuple/array destructuring
  lowering inline.
- `src/runtime/pgy_runtime_queue_inline.h` now owns generated-C inline queue
  macro and built-in `Int`/`String` queue implementations. Runtime part E is no
  longer a mixed queue/pool/FSM/authority/result include body.
- `src/runtime/pgy_runtime_map_int_key_inline.h` now owns generated-C inline
  `HashMap<Int>` key adapters for `Int`, `Long`, and `Bool` keys. Runtime part
  D is now focused on string map/list/set bodies instead of carrying those
  adapter wrappers inline.
- `src/runtime/pgy_runtime_lib_slot_exports.h` now owns LLVM-linkable primitive
  slot exports for `Slot<Double>`, `Slot<Bool>`, and `Slot<String>`. Runtime
  lib part D now starts at secure-slot exports instead of carrying primitive
  slot ABI bodies inline.
- `src/runtime/pgy_runtime_lib_std_exports.h` now owns LLVM-linkable standard
  string/conversion/math/random exports. Runtime lib part E now starts at the
  channel runtime section, and the lifetime smoke inventory reads these private
  owner headers in runtime-lib include order.
- `src/compiler/mir_decl_headers.h` now owns MIR declaration-header inventory
  helpers and method-routine linking. `mir_public_part_a.inc` now starts at
  `mir_lower(...)` instead of carrying declaration inventory helper bodies.
- `src/compiler/rir_names.h` now owns public RIR vocabulary name helpers for
  scope, fact, resource, state, and op kinds. `rir_public_surface.h` now focuses
  on dump surfaces instead of carrying name vocabulary bodies.
- `src/codegen/transpiler_parallel_capture.h` now owns C backend parallel
  capture discovery and capture-list deduplication. The async/parallel emitter
  keeps the same lowering surface, but `transpiler_emitters_base_b_part_b.inc`
  no longer carries the capture-analysis helper family inline.
- `src/codegen/transpiler_expr_stdlib_builtin.h` now owns C backend stdlib call
  lowering. The expression emitter shim still preserves include order, but
  `transpiler_expr_emitters_part_d.inc` now only carries the event-call builtin
  helper instead of the full stdlib dispatch body.
- `src/codegen/transpiler_overlay_projection.h` now owns C backend
  overlay/projection invalidation and zone-layer bind helpers. The
  `transpiler_helpers_core_a_part_b.inc` include body was removed, so this
  cleanup reduces the source `.inc` count instead of creating another split.
- `src/codegen/transpiler_let_emit.h` now owns C backend `let` declaration
  lowering. The base-A part keeps MIR inventory/SSA helper declarations, but no
  longer carries the entire `emit_let_decl(...)` body.
- `src/codegen/transpiler_mir_block_emit.h` now owns C backend MIR block
  statement emission and MIR emission eligibility wrappers. The
  `transpiler_emitters_base_a_part_c.inc` include body was removed instead of
  split further.
- `src/codegen/transpiler_intent_emit.h` now owns C backend intent declaration
  emission. The `transpiler_emitters_intent.inc` include body was removed, so no
  production `.inc` file remains above 900 LOC.
- `src/runtime/pgy_runtime_panic_checked_inline.h` now owns generated-C inline
  intent-recent accessors, panic helpers, and checked arithmetic exports.
  Runtime part B now starts at stack/box/arena/allocator helpers, and runtime
  ABI lifetime inventory reads this private header in generated-runtime include
  order.
- The empty C backend tail include
  `src/codegen/transpiler_helpers_core_a_part_d.inc` was removed from the
  `transpiler_helpers_core_a.inc` shim.
- The empty compiler/runtime tail sentinels were removed from the current split
  families:
  - `src/compiler/mir_public_part_c.inc`
  - `src/runtime/pgy_runtime_part_ba_part_f.inc`
  Their shims now include only files that carry real implementation content,
  and contract tests point at the implementation-owning parts instead of empty
  tail placeholders.
- `src/codegen/transpiler_expr_emitters.inc` is now a shim over named private
  expression owners such as `transpiler_expr_core_emit.h`,
  `transpiler_expr_dispatch_emit.h`, and the remaining focused helper parts.
- The old expression-emitter split that crossed `emit_call`,
  `emit_binary`, and helper function bodies was removed.
- `emit_call` was reduced from a multi-thousand-line mixed dispatcher into
  dedicated helpers:
  - builtin dispatch
  - domain constructor lowering
  - `Result` / `Option` lowering
  - stable stdlib lowering
  - event call lowering
  - member-style call lowering
  - final user-call lowering
- `src/codegen/transpiler_intent_zone_binding_emit.h` no longer leaves
  dangling `static void` return-type fragments for the intent emitter.
- `src/codegen/transpiler_emitters_intent.inc` now owns the full
  `emit_intent_decl` signature at its file boundary.
- Runtime ABI lifetime smoke was updated so runtime split-file checks read the
  whole split family instead of assuming a symbol remains in a fixed old part.

## Current Gate

The production include debt gate is green:

```sh
make backend-inc-size-test-smoke
make semantic-inc-size-test-smoke
find src -path src/tests -prune -o -name '*.inc' -print
```

The contract is now:

```text
production .inc under src/runtime  = 0
production .inc under src/codegen  = 0
production .inc under src/compiler = 0
production .inc under src/semantic = 0
test fixture .inc under src/tests  = 47 files, capped by inc-sentinel
```

Empty include sentinels are rejected:

```sh
make inc-sentinel-test-smoke
```

This gate rejects any production `.inc` file, rejects any zero-byte `.inc`, and
rejects any increase above the current `src/tests/**/*.inc <= 47` fixture cap.
There is no empty-sentinel allowlist. New behavior-owning `.inc` splits are
blocked by default.

Owner-size policy is separate from the `.inc` gate:

```text
600 LOC  = split-review threshold for production .c and private owner .h
1000 LOC = hard cap for new owner headers and active risk line for owner TUs
```

The current large-owner snapshot was last refreshed on 2026-04-27. The leading
production split candidates are:

```text
2742 src/compiler/mir.c
2445 src/compiler/hir.c
2394 src/codegen/llvm_intent.c
1774 src/parser/parser_domain.c
1751 src/runtime/slot_security.c
1736 src/runtime/slot_manager.c
1658 src/semantic/type_checker_decls_domain_helpers.c
1649 src/codegen/llvm_domain.c
1633 src/parser/parser.c
1580 src/compiler/driver_app.c
1555 src/semantic/type_checker_intent_helpers.c
1513 src/compiler/dir.c
1395 src/compiler/compiler.c
1279 src/compiler/air.c
1232 src/parser/ast.h
1199 src/semantic/slot_analyzer.c
1168 src/codegen/llvm_backend.c
1146 src/semantic/type_checker_builtins_stdlib_body.c
1092 src/semantic/type_checker_zone_decl.c
1027 src/codegen/llvm_domain_zone_sync.c
```

Test harness files are intentionally excluded from the first owner-split queue
even when they exceed 600 LOC; they should be reduced after the production
compiler/runtime/codegen seams are stable.

The MIR public implementation split is also below the production cap after
moving the public lowering entry points into a named private owner and the
public name helpers / `mir_destroy(...)` into the second public owner:

```text
src/compiler/mir_lower_public_api.h 290
src/compiler/mir_public_surface.h 420
```

The production `.inc` gate is now stricter than the previous 1,000 LOC cap:
no production `.inc` files remain under `src` outside test fixtures.

```text
production_inc_count=0
production_inc_loc=0
```

After the local symbol/slot tracking extraction,
`src/codegen/transpiler_helpers_core_a_part_a.inc` is down to 422 LOC.
After the declaration lookup extraction, it is down further to the remaining
projection/type predicate helpers at 169 LOC.
After the projection/type predicate extraction, it is now a forward-declaration
shim only at 14 LOC.
That forward-declaration part file has now been folded into
`src/codegen/transpiler_helpers_core_a.inc`, deleting
`src/codegen/transpiler_helpers_core_a_part_a.inc`.
After the declaration cache/current-host extraction,
`src/codegen/transpiler_helpers_core_b_part_a.inc` is down to 687 LOC.
After the current-host/nominal-host method lookup extraction and world/zone
query extraction, `src/codegen/transpiler_helpers_core_b_part_a.inc` is down to
578 LOC, while `src/codegen/transpiler_helpers_core_a_part_c.inc` is down to
577 LOC.
After the ability/event declaration lookup extraction,
`src/codegen/transpiler_helpers_core_b_part_a.inc` is down to 565 LOC.
After the nominal member type lookup extraction, it is down to 298 LOC.
After the enum/operator lookup extraction, it is down to 161 LOC.
After the projection literal lowering extraction, it is down to 70 LOC.
That remaining small part file has now been folded into
`src/codegen/transpiler_helpers_core_b.inc`, deleting
`src/codegen/transpiler_helpers_core_b_part_a.inc`.

The codegen helper shim now has no empty tail part:

```text
src/codegen/transpiler_helpers_core_a.inc
  -> inline declarations, part_b, part_c
src/codegen/transpiler_helpers_core_b.inc
  -> inline small bridge helpers, part_b, part_c, part_d
```

## Verification

The cleanup was verified with:

```sh
make pgy test-transpile backend-inc-size-test-smoke
make test-semantic test-transpile test-inc-size-test-smoke
make inc-sentinel-test-smoke
make LLVM_ENABLED=1 llvm-test-smoke test-abi runtime-abi-lifetime-test-smoke
git diff --check -- src/codegen src/runtime src/compiler src/semantic tests
```

Observed results:

- `test-transpile`: 673 passed, 0 failed.
- `test-semantic`: 2337 passed, 0 failed.
- `llvm-test-smoke`: all listed LLVM smoke cases passed.
- Latest local slice reran `make pgy`,
  `make test-transpile backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make test-semantic`, and `make LLVM_ENABLED=1 llvm-test-smoke`.
- Latest follow-up reran `make pgy`,
  `make test-transpile backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  and `make LLVM_ENABLED=1 llvm-test-smoke`.
- Latest enum/operator extraction reran `make pgy`,
  `make test-transpile backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  and `make LLVM_ENABLED=1 llvm-test-smoke`.
- Latest projection-literal extraction reran `make pgy`,
  `make test-transpile backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  and `make LLVM_ENABLED=1 llvm-test-smoke`.
- Latest `.inc` count reduction deleted
  `transpiler_helpers_core_a_part_a.inc` and
  `transpiler_helpers_core_b_part_a.inc`, reran `make pgy`, and tightened
  `make inc-sentinel-test-smoke` with a source `.inc` file-count cap.
- Latest pass-through shim reduction deleted `transpiler_emitters.inc`,
  `transpiler_emitters_base.inc`, `transpiler_helpers_core.inc`,
  `llvm_expr_helpers.inc`, `llvm_expr_call_methods.inc`,
  `llvm_domain_helpers.inc`, `mir_public.inc`, `pgy_runtime_part_b.inc`, and
  `pgy_runtime_lib_part_b.inc`. Their owning `.c` / `.h` files now include the
  concrete implementation chunks directly.
- Latest MIR public split cleanup keeps `src/compiler/mir_public_part_a.inc`
  at 959 LOC and `src/compiler/mir_public_part_b.inc` at 800 LOC. Verified by
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make mir-declaration-inventory-test-smoke`, and `make pgy`.
- Latest lean debt-slice extraction moved C backend type-alias declaration
  emission into `src/codegen/transpiler_type_alias.c`, reducing
  `src/codegen/transpiler_emitters_base_b_part_c.inc` to 976 LOC. Verified by
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`.
- Latest type-require extraction deleted
  `src/codegen/transpiler_emitters_type_require.inc`, moved the checks into
  `src/codegen/transpiler_type_require.c`, and reduced
  `src/codegen/transpiler_emitters_base_a_part_a.inc` to 905 LOC. Verified by
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` and touched
  path `git diff --check`.
- Latest extern-emitter extraction moved `emit_extern_block(...)` into
  `src/codegen/transpiler_extern.c`, reducing
  `src/codegen/transpiler_emitters_base_b_part_b.inc` from 998 LOC to 957 LOC.
  The source `.inc` sentinel now uses the current 159-file cap by default.
  Verified by `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`
  and touched path `git diff --check`.
- Latest type-declarator extraction moved event-handler/function declarator
  rendering into `src/codegen/transpiler_type_declarator.c`, reducing
  `src/codegen/transpiler_helpers_core_b_part_c.inc` from 992 LOC to 849 LOC.
  Verified by `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`
  and touched path `git diff --check`.
- Latest LogBanner normalization extraction moved multiline string
  normalization into `src/codegen/transpiler_log_normalize.c`, reducing
  `src/codegen/transpiler_expr_emitters_part_a.inc` from 991 LOC to 878 LOC.
  Verified by `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`
  and touched path `git diff --check`.
- Latest runtime intent-exit extraction moved generated-C inline intent exit
  cleanup/snapshot logic into `src/runtime/pgy_runtime_intent_exit.h`, reducing
  `src/runtime/pgy_runtime_part_ba_part_b.inc` from 996 LOC to 894 LOC without
  changing the exported inline ABI name. Verified by
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, and touched path
  `git diff --check`.
- Latest runtime slot-macro extraction moved generated-C inline DeviceSlot and
  SecureSlot macro bodies into `src/runtime/pgy_runtime_slot_macros.h`, reducing
  `src/runtime/pgy_runtime_part_ba_part_c.inc` from 996 LOC to 808 LOC while
  preserving the built-in slot instantiation order. Verified by
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, and touched path
  `git diff --check`.
- Latest runtime intent-history extraction moved generated-C inline last-history
  step accessors into `src/runtime/pgy_runtime_intent_history.h`, reducing
  `src/runtime/pgy_runtime_part_ba_part_a.inc` from 989 LOC to 867 LOC while
  preserving the borrowed string ABI. `runtime_abi_lifetime_smoke.sh` now reads
  the private runtime inline headers as part of the generated-C runtime family.
  Verified by `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, and touched path
  `git diff --check`.
- Latest LLVM runtime-lib core export extraction moved logging/time/sleep and
  `pgy_int_to_string(...)` into `src/runtime/pgy_runtime_lib_core_exports.h`,
  reducing `src/runtime/pgy_runtime_lib_part_b_part_a.inc` from 986 LOC to
  909 LOC while preserving exported runtime symbol names. Verified by
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, and touched path
  `git diff --check`.
- Latest C backend destructuring extraction moved `AST_LET_DESTRUCTURE`
  lowering into `src/codegen/transpiler_destructure_emit.h`, reducing
  `src/codegen/transpiler_emitters_base_b_part_c.inc` from 976 LOC to 873 LOC.
  Verified by `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  targeted backend compare for `destructure_array` and
  `destructure_tuple_return`, and touched path `git diff --check`.
- Latest generated-C queue extraction moved queue macro and built-in queue
  implementations into `src/runtime/pgy_runtime_queue_inline.h`, reducing
  `src/runtime/pgy_runtime_part_ba_part_e.inc` from 969 LOC to 773 LOC.
  Verified by `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-panic-codegen-test-smoke runtime-abi-lifetime-test-smoke
  test-abi`, targeted backend compare for `queue_pop_string` and
  `parallel_channel_sum`, and touched path `git diff --check`.
- Latest generated-C map key-adapter extraction moved `HashMap<Int>` adapters
  for `Int`/`Long`/`Bool` keys into
  `src/runtime/pgy_runtime_map_int_key_inline.h`, reducing
  `src/runtime/pgy_runtime_part_ba_part_d.inc` from 963 LOC to 815 LOC.
  Verified by `make -B pgy`, `make backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-panic-codegen-test-smoke`, targeted backend
  compare for `map_keys` and `map_get_string`, and touched path
  `git diff --check`.
- Latest LLVM runtime-lib primitive slot export extraction moved `Slot<Double>`,
  `Slot<Bool>`, and `Slot<String>` exported bodies into
  `src/runtime/pgy_runtime_lib_slot_exports.h`, reducing
  `src/runtime/pgy_runtime_lib_part_b_part_d.inc` from 947 LOC to 790 LOC while
  preserving exported ABI names. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-abi-test-smoke runtime-panic-codegen-test-smoke
  runtime-abi-lifetime-test-smoke test-abi`.
- Latest LLVM runtime-lib std export extraction moved `StringJoin`, `ToInt`,
  `ToFloat`, math functions, and random seeding into
  `src/runtime/pgy_runtime_lib_std_exports.h`, reducing
  `src/runtime/pgy_runtime_lib_part_b_part_e.inc` from 817 LOC to 761 LOC.
  Verified by `make runtime-abi-lifetime-test-smoke test-abi
  backend-inc-size-test-smoke inc-sentinel-test-smoke`.
- Latest LLVM runtime-lib raw list export extraction moved raw `List<T>`
  collection exports into `src/runtime/pgy_runtime_lib_list_raw_exports.h`,
  reducing `src/runtime/pgy_runtime_lib_part_b_part_a.inc` from 909 LOC to
  759 LOC. Verified by `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-panic-codegen-test-smoke
  runtime-abi-lifetime-test-smoke test-abi`.
- Latest MIR declaration-header extraction moved declaration inventory helper
  bodies into `src/compiler/mir_decl_headers.h`, reducing
  `src/compiler/mir_public_part_a.inc` from 959 LOC to 789 LOC. Verified by
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke air-drift-test-smoke test-abi`.
- Latest RIR vocabulary extraction moved public name helpers into
  `src/compiler/rir_names.h`, reducing `src/compiler/rir_public.inc` from
  911 LOC to 804 LOC. Verified by `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke type-resolution-dag-test-smoke air-drift-test-smoke
  test-abi`.
- Latest parallel-capture extraction moved C backend capture discovery and
  capture-list deduplication into `src/codegen/transpiler_parallel_capture.h`,
  reducing `src/codegen/transpiler_emitters_base_b_part_b.inc` from 957 LOC to
  730 LOC. Verified by `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke parallel-core-contract-test-smoke
  runtime-panic-codegen-test-smoke` and targeted backend compare for
  `parallel_channel_sum`.
- Latest stdlib-call extraction moved C backend stdlib call lowering into
  `src/codegen/transpiler_expr_stdlib_builtin.h`, reducing
  `src/codegen/transpiler_expr_emitters_part_d.inc` from 946 LOC to 26 LOC.
  Verified by `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-codegen-test-smoke` and targeted backend compare for
  `string_io`, `array_builtins`, `list_get_string`, and `map_get_string`.
- Latest overlay/projection extraction moved C backend overlay invalidation and
  zone-layer bind helpers into `src/codegen/transpiler_overlay_projection.h`
  and removed `src/codegen/transpiler_helpers_core_a_part_b.inc`, lowering the
  source `.inc` count to 158/159. `runtime_frontier_contract_smoke.sh` was also
  corrected to read the real world frontier owner in
  `transpiler_domain_role_part_d.inc`. Verified by
  `make runtime-frontier-contract-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke` and targeted backend compare for
  `world_embedded_branch_projection_visibility` and
  `world_embedded_action_frontier`.
- Latest let-emitter extraction moved C backend `let` declaration lowering into
  `src/codegen/transpiler_let_emit.h`, reducing
  `src/codegen/transpiler_emitters_base_a_part_a.inc` from 905 LOC to 138 LOC.
  Verified by `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  test-transpile` and targeted backend compare for `destructure_array`,
  `array_builtins`, and `map_keys`.
- Latest MIR block-emitter extraction moved C backend MIR block statement
  emission into `src/codegen/transpiler_mir_block_emit.h` and removed
  `src/codegen/transpiler_emitters_base_a_part_c.inc`. The source `.inc` total
  is now 49,911 LOC. Verified by `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke test-transpile type-resolution-dag-test-smoke
  air-drift-test-smoke` and targeted backend compare for `destructure_array`,
  `destructure_tuple_return`, `host_method_class_return`, and
  `world_embedded_branch_projection_visibility`.
- Latest intent-emitter extraction moved C backend intent declaration emission
  into `src/codegen/transpiler_intent_emit.h` and removed
  `src/codegen/transpiler_emitters_intent.inc`. The source `.inc` total is now
  48,949 LOC, and no production `.inc` remains above 900 LOC. Verified by
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  test-transpile runtime-panic-codegen-test-smoke` and targeted backend compare
  for `intent_authority_snapshot` and `intent_failure_observability_strings`.
- Latest runtime panic/checked extraction moved generated-C inline
  intent-recent accessors, panic helpers, and checked arithmetic exports into
  `src/runtime/pgy_runtime_panic_checked_inline.h`, reducing
  `src/runtime/pgy_runtime_part_ba_part_b.inc` from 894 LOC to 705 LOC and
  source `.inc` total to 48,761 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-codegen-test-smoke runtime-panic-abi-test-smoke
  runtime-abi-lifetime-test-smoke test-abi`.
- Latest runtime intent-active extraction moved generated-C inline last/active
  borrowed exports into `src/runtime/pgy_runtime_intent_active_exports.h`,
  reducing `src/runtime/pgy_runtime_part_ba_part_a.inc` from 867 LOC to
  558 LOC and source `.inc` total to 48,453 LOC. The active and recent ABI
  smoke groups now point at their real owners instead of relying on one broad
  concatenated source family. Verified by `make runtime-abi-lifetime-test-smoke
  backend-inc-size-test-smoke inc-sentinel-test-smoke` and `make -B pgy
  runtime-panic-codegen-test-smoke runtime-panic-abi-test-smoke test-abi`.
- Latest LLVM runtime intent-export extraction moved non-inline intent
  borrowed exports into `src/runtime/pgy_runtime_lib_intent_exports.h`,
  reducing `src/runtime/pgy_runtime_lib_part_b_part_c.inc` from 852 LOC to
  315 LOC and source `.inc` total to 47,916 LOC. This makes generated-C inline
  and LLVM-linkable intent export ownership symmetric. Verified by `make
  runtime-abi-lifetime-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke` and `make -B pgy runtime-panic-codegen-test-smoke
  runtime-panic-abi-test-smoke test-abi`.
- Latest LLVM method-call projection extraction moved world/zone projection
  sync helpers into `src/codegen/llvm_expr_call_projection_sync.h`, reducing
  `src/codegen/llvm_expr_call_methods_part_a.inc` from 880 LOC to 671 LOC and
  source `.inc` total to 43,918 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke` and targeted backend
  compare for `world_embedded_branch_projection_visibility`,
  `world_embedded_action_frontier`, `world_embedded_action_pool_frontier`, and
  `world_zone_projection_visibility`.
- Latest C backend MIR SSA contract extraction moved identifier mapping and
  verification helpers into `src/codegen/transpiler_mir_ssa_contract.h`,
  reducing `src/codegen/transpiler_emitters_base_a_part_d.inc` from 849 LOC to
  677 LOC and source `.inc` total to 43,715 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile`.
- Latest C backend slot builtin extraction moved slot/device expression
  emitters into `src/codegen/transpiler_slot_builtin_emit.h`, reducing
  `src/codegen/transpiler_expr_emitters_part_a.inc` from 797 LOC to 531 LOC and
  source `.inc` total to 43,406 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile
  runtime-panic-codegen-test-smoke`.
- Latest C backend expression type-inference extraction moved
  `infer_expression_type_name(...)` into
  `src/codegen/transpiler_expr_type_infer.h`, reducing
  `src/codegen/transpiler_helpers_core_b_part_c.inc` from 797 LOC to 296 LOC
  and source `.inc` total to 42,906 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile`.
- Latest C backend statement-dispatch extraction moved `emit_statement(...)`
  into `src/codegen/transpiler_statement_dispatch.h`, reducing
  `src/codegen/transpiler_emitters_base_b_part_c.inc` from 803 LOC to 546 LOC
  and source `.inc` total to 42,650 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile` and
  targeted backend compare for `break_continue`, `parallel_channel_sum`, and
  `intent_header_interleaved`.
- Latest generated-C runtime string-map extraction moved `HashMap<String>` and
  map-key inline runtime into `src/runtime/pgy_runtime_map_string_inline.h`,
  reducing `src/runtime/pgy_runtime_part_ba_part_d.inc` from 767 LOC to 377 LOC
  and source `.inc` total to 42,261 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-codegen-test-smoke test-abi`
  and targeted backend compare for `map_get_string`, `map_keys`,
  `list_get_string`, `queue_pop_string`, and
  `intent_failure_observability_strings`.
- Latest lean debt batch moved C backend MIR function emission into
  `src/codegen/transpiler_mir_func_emit.h`, reducing
  `src/codegen/transpiler_emitters_base_b_part_a.inc` from 766 LOC to 162 LOC.
  The same batch moved generated-C runtime array sort kernels and scalar
  std/log/math helpers into `src/runtime/pgy_runtime_array_sort_inline.h` and
  `src/runtime/pgy_runtime_scalar_std_inline.h`, reducing
  `src/runtime/pgy_runtime_part_ba_part_c.inc` from 759 LOC to 535 LOC and
  source `.inc` total to 41,436 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile
  runtime-abi-lifetime-test-smoke runtime-panic-codegen-test-smoke test-abi`
  and targeted backend compare for `intent_header_interleaved`,
  `destructure_tuple_return`, `host_method_class_return`,
  `world_embedded_branch_projection_visibility`, `map_get_string`, `map_keys`,
  and `string_io`.
- Latest MIR ABI owner extraction moved ABI layout table/lookup into
  `src/compiler/mir_abi_layout.h`, reducing
  `src/compiler/mir_public_part_b.inc` from 753 LOC to 420 LOC and source
  `.inc` total to 41,103 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke air-drift-test-smoke test-abi`.
- Latest CFG contract owner extraction moved cleanup/rollback/invalidation MIR
  validation into `src/compiler/mir_cfg_contract_validate.h`, reducing
  `src/compiler/mir_public_part_a.inc` from 743 LOC to 290 LOC and source
  `.inc` total to 40,650 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- Latest RIR validation owner extraction moved `rir_validate`,
  `rir_validate_against_dir`, and projection-kind validation helpers into
  `src/compiler/rir_validation.h`, reducing `src/compiler/rir_public.inc` from
  741 LOC to 269 LOC and source `.inc` total to 40,178 LOC. Verified by
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- Latest C backend MIR intent inventory cleanup moved the former
  `src/codegen/transpiler_emitters_mir_inventory_intent.inc` body into
  `src/codegen/transpiler_mir_inventory_intent.h` and made the existing SSA
  shim include that owner header directly. This removes one production `.inc`
  body and reduces source `.inc` total to 39,485 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- Latest C backend call/spawn/channel emitter cleanup moved the former
  `src/codegen/transpiler_expr_emitters_part_e.inc` body into
  `src/codegen/transpiler_expr_call_spawn_emit.h` and made the expression
  emitter shim include that owner header directly. This removes another
  production `.inc` body and reduces source `.inc` total to 38,763 LOC.
  Verified by `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke type-resolution-dag-test-smoke
  cfg-body-dataflow-test-smoke air-drift-test-smoke test-abi`.
- Latest LLVM domain helper cleanup moved the former
  `src/codegen/llvm_domain_helpers_part_a.inc` body into
  `src/codegen/llvm_domain_core_helpers.h` and made `llvm_domain.c` include the
  owner header directly. This removes another production `.inc` body and
  reduces source `.inc` total to 38,041 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- Latest runtime channel/qubit export cleanup moved the former
  `src/runtime/pgy_runtime_lib_part_b_part_e.inc` body into
  `src/runtime/pgy_runtime_lib_channel_quantum_exports.h` and made
  `pgy_runtime_lib.c` include the owner header directly. The runtime ABI
  lifetime smoke now reads the new owner header as the split continuation for
  generated-runtime checks. This removes another production `.inc` body and
  reduces source `.inc` total to 37,327 LOC. Verified by `make
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke test-abi`.
- Latest runtime raw collection and slot/array/io/string export cleanup moved
  the former `src/runtime/pgy_runtime_lib_part_b_part_a.inc` body into
  `src/runtime/pgy_runtime_lib_raw_collection_exports.h` and the former
  `src/runtime/pgy_runtime_lib_part_b_part_d.inc` body into
  `src/runtime/pgy_runtime_lib_slot_array_io_string_exports.h`. Runtime panic
  and ABI lifetime smokes now read the new owner headers, and compiler runtime
  cache freshness tracks them directly. This removes two more production
  `.inc` bodies and reduces source `.inc` total to 35,901 LOC. Verified by
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-contract-test-smoke
  runtime-panic-codegen-test-smoke test-abi`.
- Latest C backend builtin-call dispatch cleanup moved the former
  `src/codegen/transpiler_expr_emitters_part_b.inc` body into
  `src/codegen/transpiler_expr_builtin_dispatch.h` and made the expression
  emitter shim include the owner header directly. This removes another
  production `.inc` body, leaves builtin-call lowering in the original include
  order, and reduces the current source `.inc` inventory to 102 files / 35,191
  LOC. Verified by `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke type-resolution-dag-test-smoke
  cfg-body-dataflow-test-smoke air-drift-test-smoke test-abi`.
- Latest semantic builtin-query cleanup moved the former
  `src/semantic/type_checker_builtins_query.inc` body into
  `src/semantic/type_checker_builtins_query.h` and fixed the chained
  `BuiltinKind builtin_resolve(...)` signature so
  `type_checker_builtins_slotops.inc` owns a complete function boundary. This
  removes another production `.inc` body and reduces the current source `.inc`
  inventory to 101 files / 34,490 LOC.
- Latest semantic builtin-nominal cleanup moved the former
  `src/semantic/type_checker_builtins_nominal.inc` body into
  `src/semantic/type_checker_builtins_nominal.h`. This keeps the
  `Rc`/`Weak`/`Box`/allocator and intent-observability builtin type contract in
  the original dispatch order while removing another production `.inc` body;
  the current source `.inc` inventory is now 100 files / 33,809 LOC.
- Latest generated-C runtime pool/FSM/timer cleanup moved object-pool,
  finite-state-machine, timer, and cooldown inline helpers into
  `src/runtime/pgy_runtime_pool_fsm_timer_inline.h`. Runtime include order,
  ABI lifetime inventory, and compiler runtime-cache freshness now track that
  owner header directly; `pgy_runtime_part_ba_part_e.inc` is reduced to
  parallel/zone authority/effect-pool/unsafe/result/option helpers and the
  current source `.inc` total is 33,653 LOC.
- Latest semantic expression owner cleanup moved the former
  `src/semantic/type_checker_expr.inc` body into
  `src/semantic/type_checker_expr.h`. The CFG body-dataflow smoke now points at
  the new expression owner, and the current source `.inc` inventory is 99 files
  / 32,983 LOC.
- Latest C backend function/class/control-flow owner cleanup moved the former
  `src/codegen/transpiler_emitters_base_b_part_b.inc` body into
  `src/codegen/transpiler_func_class_flow_emit.h`. This preserves the existing
  base-B include order while removing another production `.inc` body; the
  current source `.inc` inventory is 98 files / 32,322 LOC.
- Latest generated-C runtime memory/array/slot cleanup moved the former
  `src/runtime/pgy_runtime_part_ba_part_b.inc` body into
  `src/runtime/pgy_runtime_memory_array_slot_inline.h`. Runtime include order,
  panic-contract smoke, ABI lifetime inventory, and compiler runtime-cache
  freshness now track that owner header directly; the current source `.inc`
  inventory is 97 files / 31,662 LOC.
- Latest semantic relation/effect/projection helper cleanup moved the former
  `src/semantic/type_checker_helpers_effects.inc` body into
  `src/semantic/type_checker_helpers_effects.h`. CFG body-dataflow smoke now
  tracks the new helper owner path, and the current source `.inc` inventory is
  96 files / 31,013 LOC.
- Latest C backend MIR emission contract cleanup moved the former
  `src/codegen/transpiler_emitters_base_a_part_d.inc` body into
  `src/codegen/transpiler_mir_emission_contract.h`. The base-A shim still
  preserves include order, but the remaining MIR emission/resource-hook owner
  is no longer an anonymous part file; the current production source `.inc`
  inventory is 95 files / 30,368 LOC.
- Latest RIR lowering/enrichment cleanup moved the former
  `src/compiler/rir_builder.inc` body into `src/compiler/rir_builder.h`.
  `rir.c` still includes it in the same position between flow and name/validation
  owners, but RIR construction is no longer an anonymous include body; the
  current production source `.inc` inventory is 94 files / 29,733 LOC.
- Latest semantic function-body/program owner cleanup moved the former
  `src/semantic/type_checker_program.inc` body into
  `src/semantic/type_checker_program.h`. The top-level semantic TU still
  includes it after helper/orchestration definitions, but function body
  checking is no longer carried by an anonymous `.inc`; the current production
  source `.inc` inventory is 93 files / 29,099 LOC.
- Latest LLVM method-call domain/slice cleanup moved the former
  `src/codegen/llvm_expr_call_methods_part_a.inc` body into
  `src/codegen/llvm_expr_call_methods_domain_slice.h`. `llvm_expr.c` still
  includes it before the remaining method-call tail, but domain action sync and
  slice/member-call helpers are no longer anonymous part-A code; the current
  production source `.inc` inventory is 92 files / 28,467 LOC.
- Latest LLVM call dispatcher cleanup moved the former
  `src/codegen/llvm_expr_calls_main.inc` body into
  `src/codegen/llvm_expr_call_dispatch.h`. The call-family shim still includes
  constructor/collection/domain/event/log/slot/task helpers before the final
  dispatcher, but `llvm_emit_call` now has a named owner; the current production
  source `.inc` inventory is 91 files / 27,842 LOC.
- Latest LLVM expression helper cleanup moved the former
  `src/codegen/llvm_expr_helpers_part_b.inc` body into
  `src/codegen/llvm_expr_host_spawn_literal_helpers.h`. Host/self, projection
  binding, spawn expression, operator suffix, enum lookup, and number/string
  literal helpers now have a named owner; the current production source `.inc`
  inventory is 90 files / 27,221 LOC.
- Latest C backend role/ability cleanup moved the former
  `src/codegen/transpiler_domain_role_part_a.inc` body into
  `src/codegen/transpiler_domain_role_ability_emit.h`. Role method emission,
  ability/vtable emission, hidden provenance helpers, and operator aliases now
  have a named owner while the domain-role shim preserves include order; the
  current production source `.inc` inventory is 89 files / 26,601 LOC.
- Latest LLVM expression boundary/projection helper cleanup moved the former
  `src/codegen/llvm_expr_helpers_part_a.inc` body into
  `src/codegen/llvm_expr_boundary_projection_helpers.h`. Boundary call argument
  helpers, projection field helpers, world/zone lookup helpers, and host-class
  lookup helpers now have a named owner; the current production source `.inc`
  inventory is 88 files / 25,996 LOC.
- Latest C backend MIR SSA naming cleanup moved the former
  `src/codegen/transpiler_emitters_mir_inventory_ssa_names.inc` body into
  `src/codegen/transpiler_mir_ssa_names.h`. MIR routine lookup, active SSA name
  resolution/rendering, token-local filtering, and local type-name lookup now
  have a named owner; the current production source `.inc` inventory is
  87 files / 25,395 LOC.
- Latest C backend type mapping cleanup moved the former
  `src/codegen/transpiler_helpers_core_types.inc` body into
  `src/codegen/transpiler_type_mapping_helpers.h`. Primitive, slot/channel,
  constructed generic, and local type-name rendering now have a named owner
  while the helper-core shim preserves include order; the current production
  source `.inc` inventory is 86 files / 24,796 LOC.
- Latest C backend world/select/event cleanup moved the former
  `src/codegen/transpiler_domain_role_part_d.inc` body into
  `src/codegen/transpiler_world_select_event_emit.h`. World sync declaration,
  select lowering, and event declaration/subscription lowering now have a named
  owner while the domain-role shim preserves include order; the current
  production source `.inc` inventory is 85 files / 24,198 LOC.
- Latest LLVM expression assignment/member/projection cleanup moved the former
  `src/codegen/llvm_expr_values.inc` body into
  `src/codegen/llvm_expr_assignment_member_projection.h`. Member lvalue/member
  access, projection invalidation, embedded world projection assignment sync,
  and assignment emission now have a named owner while `llvm_expr.c` preserves
  include order; the current production source `.inc` inventory is 84 files /
  23,617 LOC.
- Latest LLVM-linkable runtime authority/file/path bootstrap cleanup moved the
  former `src/runtime/pgy_runtime_lib_part_a.inc` body into
  `src/runtime/pgy_runtime_lib_authority_file_core.h`. Runtime authority
  rejection state, checked arithmetic exports, panic invariant export, and
  file-path normalization helpers now have a named owner while
  `pgy_runtime_lib.c` preserves include order; the current production source
  `.inc` inventory is 83 files / 23,031 LOC.
- Latest LLVM-linkable runtime set/intent trace cleanup moved the former
  `src/runtime/pgy_runtime_lib_part_b_part_b.inc` body into
  `src/runtime/pgy_runtime_lib_set_intent_trace_exports.h`. Raw set tail
  exports, intent active/recent registry helpers, intent trace mutation, and
  MIR trace hooks now have a named owner while `pgy_runtime_lib.c` preserves
  include order; the current production source `.inc` inventory is 82 files /
  22,449 LOC.
- Latest RIR flow cleanup moved the former `src/compiler/rir_flow.inc` body
  into `src/compiler/rir_flow.h`. RIR flow semantic flags, state merge rules,
  and HIR CFG enrichment now have a named owner while `rir.c` preserves include
  order; the current production source `.inc` inventory is 81 files /
  21,877 LOC.
- Latest C backend MIR SSA emit cleanup moved the former
  `src/codegen/transpiler_emitters_mir_inventory_ssa_emit.inc` body into
  `src/codegen/transpiler_mir_ssa_emit.h`. MIR local type lookup, explicit
  binding registration, MIR function signature support checks, SSA expression
  emission, phi copy emission, and exit-SSA lookup now have a named owner while
  the MIR inventory/SSA shim preserves include order; the current production
  source `.inc` inventory is 80 files / 21,313 LOC.
- Latest generated-C channel runtime cleanup moved the former
  `src/runtime/pgy_runtime_part_bb.inc` body into
  `src/runtime/pgy_runtime_channel_inline.h`. Threaded channel and SPSC channel
  inline macro definitions plus stable `Int`/`String` instantiations now have a
  named owner while `pgy_runtime.h` preserves include order; the current
  production source `.inc` inventory is 79 files / 20,752 LOC.
- Latest C backend zone declaration cleanup moved the former
  `src/codegen/transpiler_domain_role_part_c.inc` body into
  `src/codegen/transpiler_zone_decl_emit.h`. Zone struct emission, projection
  readiness/dirty fields, layer/state frontier sync, bounded recompute, and
  hosted zone method lowering now have a named owner while the domain-role shim
  preserves include order; the current production source `.inc` inventory is
  78 files / 20,198 LOC.
- Latest C backend block/intent helper cleanup moved the former
  `src/codegen/transpiler_emitters_base_b_part_c.inc` body into
  `src/codegen/transpiler_block_intent_helpers.h`. Block auto-release emission,
  intent participant/action lookup, inferred causes lookup, and effective-zone
  sync helpers now have a named owner while the base-B shim preserves include
  order; the current production source `.inc` inventory is 77 files /
  19,652 LOC.
- Latest generated-C IO/Qubit runtime cleanup moved the former
  `src/runtime/pgy_runtime_part_c.inc` body into
  `src/runtime/pgy_runtime_io_qubit_inline.h`. Inline file/string helpers,
  `StringSplit` allocation, and the toy Qubit runtime now have a named owner
  while `pgy_runtime.h` preserves include order; compiler runtime cache
  freshness dependencies were also updated to stop pointing at deleted runtime
  include paths. The current production source `.inc` inventory is 76 files /
  19,110 LOC.
- Latest C backend constructor/Result-Option call cleanup moved the former
  `src/codegen/transpiler_expr_emitters_part_c.inc` body into
  `src/codegen/transpiler_call_constructor_result_emit.h`. Domain/party
  constructor lowering and Result/Option builtin call lowering now have a named
  owner while the expression emitter shim preserves include order; the current
  production source `.inc` inventory is 75 files / 18,573 LOC.
- Latest generated-C builtin storage cleanup moved the former
  `src/runtime/pgy_runtime_part_ba_part_c.inc` body into
  `src/runtime/pgy_runtime_builtin_storage_inline.h`. Slot/device-slot/
  secure-slot instantiations, Box/Array/Rc builtins, and inline HashMap helpers
  now have a named owner while `pgy_runtime_part_ba.inc` preserves include
  order; compiler cache freshness and runtime panic/ABI smoke tests now read
  the new owner path. The current production source `.inc` inventory is
  74 files / 18,038 LOC.
- Latest semantic host-helper cleanup moved the former
  `src/semantic/type_checker_helpers_host.inc` body into
  `src/semantic/type_checker_host_helpers.h`. Overlay field lookup, host method
  call typing, subject/nominal boundary classification, zone effect-layer
  checks, and movable resource predicates now have a named owner while
  `type_checker.c` preserves include order; the current production source
  `.inc` inventory is 73 files / 17,448 LOC.
- Latest semantic builtin slotops cleanup moved the former
  `src/semantic/type_checker_builtins_slotops.inc` body into
  `src/semantic/type_checker_builtins_slotops.h`. Builtin name resolution,
  Slot/SecureSlot/DeviceSlot semantic validation, release diagnostics, and
  device handle argument checks now have a named owner while
  `type_checker_builtins.c` preserves include order; the current production
  source `.inc` inventory is 72 files / 16,923 LOC.
- Latest generated-C zone/result-option runtime cleanup moved the former
  `src/runtime/pgy_runtime_part_ba_part_e.inc` body into
  `src/runtime/pgy_runtime_zone_result_option_inline.h`. Parallel section
  macros, zone lock/generation/authority validation, Result helpers, remote
  Result helpers, and Option helpers now have a named owner while
  `pgy_runtime_part_ba.inc` preserves include order; compiler cache freshness
  and runtime ABI/panic/authority smoke tests now read the new owner path. The
  current production source `.inc` inventory is 71 files / 16,402 LOC.
- Latest C backend projection/sync helper cleanup moved the former
  `src/codegen/transpiler_helpers_core_a_part_c.inc` body into
  `src/codegen/transpiler_projection_sync_helpers.h`. Overlay projection
  invalidation scanning, zone/effect relation propagation snippets, and
  world-state lookup helpers now have a named owner while the helper-core-A
  shim preserves include order; the current production source `.inc` inventory
  is 70 files / 15,883 LOC.
- Latest generated-C intent trace runtime cleanup moved the former
  `src/runtime/pgy_runtime_part_ba_part_a.inc` body into
  `src/runtime/pgy_runtime_intent_trace_inline.h`. `pgy_runtime_strdup`,
  active/recent intent registry storage, trace append helpers, step ok/fail
  tracing, and MIR resource trace hooks now have a named owner while
  `pgy_runtime_part_ba.inc` preserves include order; compiler cache freshness
  and runtime ABI lifetime smoke now read the new owner path. The current
  production source `.inc` inventory is 69 files / 15,370 LOC.
- Latest LLVM collection-call cleanup moved the former
  `src/codegen/llvm_expr_call_collections_extended.inc` body into
  `src/codegen/llvm_expr_call_collections_extended.h`. Extended List/Set/
  HashMap raw-call lowering now has a named private owner while
  `llvm_expr_calls.inc` preserves dispatcher include order. The current
  production source `.inc` inventory is 68 files / 14,862 LOC.
- Latest C backend helper-root cleanup moved the former
  `src/codegen/transpiler_helpers.inc` body into
  `src/codegen/transpiler_helpers.h`. C string escaping/formatting, MIR
  resource-op/DEF helper emission, and the expression-emitter include root now
  have a named private owner while `transpiler.c` preserves top-level include
  order. The current production source `.inc` inventory is 67 files /
  14,356 LOC.
- Latest C backend expression-core cleanup moved the former
  `src/codegen/transpiler_expr_emitters_part_a.inc` body into
  `src/codegen/transpiler_expr_core_emit.h`. Log/LogRaw/LogBanner lowering and
  core binary expression lowering now have a named private owner while
  `transpiler_expr_emitters.inc` preserves include order. The current
  production source `.inc` inventory is 66 files / 13,869 LOC.
- Latest C backend specialization-helper cleanup moved the former
  `src/codegen/transpiler_helpers_core_b_part_b.inc` body into
  `src/codegen/transpiler_specialization_helpers.h`. Role ability/method
  lookup and Result/Option/collection specialization collection now have a
  named private owner while `transpiler_helpers_core_b.inc` preserves include
  order. The current production source `.inc` inventory is 65 files /
  13,402 LOC.
- Latest C backend domain nominal cleanup moved the former
  `src/codegen/transpiler_domain_role_part_b.inc` body into
  `src/codegen/transpiler_domain_nominal_emit.h`. Ability, role, party, roster,
  relation, and effect declaration emission now have a named private owner
  while `transpiler_domain_role.inc` preserves include order. The current
  production source `.inc` inventory is 64 files / 12,937 LOC.
- Latest C backend expression-dispatch cleanup moved the former
  `src/codegen/transpiler_expr_emitters_part_f.inc` body into
  `src/codegen/transpiler_expr_dispatch_emit.h`. The `emit_expression()`
  dispatcher now has a named private owner while
  `transpiler_expr_emitters.inc` preserves include order; runtime panic
  contract smoke now reads the new owner path for checked array/slice lowering.
  The current production source `.inc` inventory is 63 files / 12,486 LOC.
- Latest MIR public-surface cleanup moved the former
  `src/compiler/mir_public_part_b.inc` body into
  `src/compiler/mir_public_surface.h`. MIR kind names, destroy, validation,
  emission-topology validation, and dump now have a named private owner while
  `mir.c` preserves include order. The current production source `.inc`
  inventory is 62 files / 12,066 LOC.
- Latest semantic generic-contract cleanup moved the former
  `src/semantic/type_checker_generic_contracts.inc` body into
  `src/semantic/type_checker_generic_contracts.h`. Generic parameter lookup,
  default-bound validation, and class-specialization where-bound validation now
  have a named private owner while `type_checker_generic_support.h` preserves
  include order. The current production source `.inc` inventory is 61 files /
  11,663 LOC.
- Latest LLVM member-call cleanup moved the former
  `src/codegen/llvm_expr_call_methods_part_b.inc` body into
  `src/codegen/llvm_member_call_emit.h`. `llvm_emit_member_call()` and nominal
  hosted-method dispatch now have a named private owner while `llvm_expr.c`
  preserves include order. The current production source `.inc` inventory is 60
  files / 11,262 LOC.
- Latest generated-C runtime root cleanup moved the former
  `src/runtime/pgy_runtime_part_a.inc` body into
  `src/runtime/pgy_runtime_platform_io_core.h`. Platform includes, contract
  headers, warning helpers, path normalization, and IO sandbox checks now have a
  named private owner while `pgy_runtime.h` preserves include order. The current
  production source `.inc` inventory is 59 files / 10,879 LOC.
- Latest semantic resolution-helper cleanup moved the former
  `src/semantic/type_checker_helpers_resolution.inc` body into
  `src/semantic/type_checker_resolution_helpers.h`. Alias resolution stack
  handling, alias materialization, function-type formatting, and embedded
  world-zone mutation rejection now have a named private owner while
  `type_checker.c` preserves include order. The current production source
  `.inc` inventory is 58 files / 10,500 LOC.
- Latest generated-C collection-runtime cleanup moved the former
  `src/runtime/pgy_runtime_part_ba_part_d.inc` body into
  `src/runtime/pgy_runtime_list_set_inline.h`. List and Set inline runtime
  definitions now have a named private owner while `pgy_runtime_part_ba.inc`
  preserves include order. The current production source `.inc` inventory is 57
  files / 10,123 LOC.
- Latest LLVM identifier/slot-helper cleanup moved the former
  `src/codegen/llvm_expr_helpers_part_c.inc` body into
  `src/codegen/llvm_expr_identifier_slot_helpers.h`. Identifier emission,
  direct Slot/SecureSlot fallbacks, slot target resolution, and banner literal
  normalization now have a named private owner while `llvm_expr.c` preserves
  include order. The current production source `.inc` inventory is 56 files /
  9,753 LOC.
- Latest semantic async/channel cleanup moved the former
  `src/semantic/type_checker_async_channel.inc` body into
  `src/semantic/type_checker_async_channel.h`. Spawn token boundary checks and
  channel send/recv ownership diagnostics now have a named private owner while
  `type_checker.c` preserves include order. The current production source
  `.inc` inventory is 55 files / 9,384 LOC.
- Latest LLVM scalar-expression cleanup moved the former
  `src/codegen/llvm_expr_core.inc` body into
  `src/codegen/llvm_expr_scalar_core.h`. Callable/event signature helpers,
  scalar string coercion, binary lowering, unary lowering, and `?` propagation
  lowering now have a named private owner while `llvm_expr.c` preserves include
  order. The current production source `.inc` inventory is 54 files / 9,024
  LOC.
- Latest semantic generic-support cleanup moved the former
  `src/semantic/type_checker_generic_support.inc` body into
  `src/semantic/type_checker_generic_support.h`. Generic subject signature
  formatting and effective default generic argument derivation now have a named
  private owner while `type_checker.c` preserves include order. The current
  production source `.inc` inventory is 53 files / 8,666 LOC.
- Latest LLVM domain projection-sync cleanup moved the former
  `src/codegen/llvm_domain_helpers_part_b.inc` body into
  `src/codegen/llvm_domain_projection_sync_helpers.h`. Projection field-copy
  lowering and bounded projection sync loop generation now have a named private
  owner while `llvm_domain.c` preserves include order. The current production
  source `.inc` inventory is 52 files / 8,333 LOC.
- Latest C backend async/parallel cleanup moved the former
  `src/codegen/transpiler_emitters_async_parallel.inc` body into
  `src/codegen/transpiler_async_parallel_emit.h`. Parallel block emission and
  async block spawning now have a named private owner while
  `transpiler_func_class_flow_emit.h` preserves include order. The current
  production source `.inc` inventory is 51 files / 8,016 LOC.
- Latest semantic type resolver cleanup moved the former
  `src/semantic/type_checker_resolve.inc` body into
  `src/semantic/type_checker_resolve.h`. The memoized `resolve_type_node(...)`
  wrapper, uncached resolver body, assignment compatibility gate, and
  constructed-type wrapper now have a named private owner while
  `type_checker_expr.h` preserves include order. The current production source
  `.inc` inventory is 50 files / 7,701 LOC.
- Latest semantic domain-query builtin cleanup moved the former
  `src/semantic/type_checker_builtins_query_domain.inc` body into
  `src/semantic/type_checker_builtins_query_domain.h`. HasProjection/
  HasZoneProjection source-field lookup, zone/world slot lookup, and domain
  projection query validation now have a named private owner while
  `type_checker_builtins.c` preserves include order. The current production
  source `.inc` inventory is 49 files / 7,387 LOC.
- Latest CFG/body-flow loop cleanup moved the former
  `src/semantic/type_checker_flow_loops.inc` body into
  `src/semantic/type_checker_flow_loops.h`. Loop resource snapshot comparison,
  bounded loop analysis, and loop effect merge logic now have a named private
  owner while `type_checker_flow.c` preserves include order. The CFG/body
  dataflow smoke now reads the named owner path. The current production source
  `.inc` inventory is 48 files / 7,086 LOC.
- Latest C backend function-forward helper cleanup moved the former
  `src/codegen/transpiler_helpers_core_b_part_c.inc` body into
  `src/codegen/transpiler_func_forward_helpers.h`. Spawn/future return type
  inference, early type forward-declaration checks, generic call binding
  inference, and hosted-method forward declarations now have a named private
  owner while `transpiler_helpers_core_b.inc` preserves include order. The
  current production source `.inc` inventory is 47 files / 6,790 LOC.
- Latest MIR lowering public API cleanup moved the former
  `src/compiler/mir_public_part_a.inc` body into
  `src/compiler/mir_lower_public_api.h`. `mir_lower(...)`, MIR routine lookup,
  declaration header lookup, liveness pass entry, and DCE pass entry now have a
  named private owner while `mir.c` preserves include order. MIR declaration
  inventory smoke now reads the named owner path. The current production source
  `.inc` inventory is 46 files / 6,500 LOC.
- Latest LLVM-linkable runtime intent/slot-core export cleanup moved the former
  `src/runtime/pgy_runtime_lib_part_b_part_c.inc` body into
  `src/runtime/pgy_runtime_lib_intent_slot_core_exports.h`.
  `pgy_intent_exit_export(...)`, the runtime deadline helper, and primitive
  `Slot<Int/Long/Float>` exports now have a named private owner while
  `pgy_runtime_lib.c` preserves include order. Runtime ABI lifetime and panic
  contract smokes now read the named owner path. The current production source
  `.inc` inventory is 45 files / 6,212 LOC.
- Latest C backend match lowering cleanup moved the former
  `src/codegen/transpiler_emitters_match.inc` body into
  `src/codegen/transpiler_match_emit.h`. Result/Option/enum destructor pattern
  helpers and `emit_match_stmt(...)` now have a named private owner while
  `transpiler_func_class_flow_emit.h` preserves include order. The current
  production source `.inc` inventory is 44 files / 5,932 LOC.
- Latest LLVM domain query call cleanup moved the former
  `src/codegen/llvm_expr_call_domain_queries.inc` body into
  `src/codegen/llvm_expr_domain_query_calls.h`. `HasProjection`, `HasLayer`,
  `HasState`, `HasZone`, and zone-detail query lowering now have a named
  private owner while `llvm_expr_calls.inc` preserves include order. The current
  production source `.inc` inventory is 43 files / 5,657 LOC.
- Latest MIR/RIR owner cleanup moved the former `src/compiler/mir_base.inc`
  body into `src/compiler/mir_base_helpers.h` and the former
  `src/compiler/rir_public.inc` body into `src/compiler/rir_public_surface.h`.
  MIR low-level helpers and RIR dump/destroy surfaces now have explicit owners
  while preserving the existing include order. The current production source
  `.inc` inventory is 41 files / 5,119 LOC before the next runtime/codegen
  cleanup slice.
- Latest runtime quantum export cleanup moved the former
  `src/runtime/pgy_runtime_lib_part_b_part_f.inc` body into
  `src/runtime/pgy_runtime_lib_quantum_exports.h`. Runtime source packaging,
  LLVM runtime library include order, and ABI lifetime smoke now refer to the
  named quantum export owner. The current production source `.inc` inventory is
  40 files / 4,866 LOC.
- Latest LLVM slot/device call cleanup moved the former
  `src/codegen/llvm_expr_call_slots.inc` body into
  `src/codegen/llvm_expr_slot_device_calls.h`. `ClaimSlot`, `Write`, `Read`,
  `Release`, and `Device*` lowering now have a named owner while
  `llvm_expr_calls.inc` remains the dispatcher-order shim. The current
  production source `.inc` inventory is 39 files / 4,621 LOC.
- Latest C/LLVM call-owner cleanup moved the former
  `src/codegen/transpiler_emitters_base_b_part_d.inc` body into
  `src/codegen/transpiler_intent_zone_binding_emit.h`, the former
  `src/codegen/llvm_expr_call_constructors.inc` body into
  `src/codegen/llvm_expr_constructor_calls.h`, the former
  `src/codegen/llvm_expr_call_rc.inc` body into
  `src/codegen/llvm_expr_rc_calls.h`, and the former
  `src/codegen/llvm_expr_call_task_channel.inc` body into
  `src/codegen/llvm_expr_task_channel_calls.h`. The current production source
  `.inc` inventory is 35 files / 3,689 LOC.
- Latest flow/resource and LLVM collection/result cleanup moved
  `src/codegen/transpiler_emitters_control_flow_loops.inc` into
  `src/codegen/transpiler_control_flow_emit.h`,
  `src/semantic/type_checker_flow_resources.inc` into
  `src/semantic/type_checker_flow_resources.h`,
  `src/codegen/llvm_expr_call_collections_base.inc` into
  `src/codegen/llvm_expr_collection_base_calls.h`, and
  `src/codegen/llvm_expr_call_result_option.inc` into
  `src/codegen/llvm_expr_result_option_calls.h`. The current production source
  `.inc` inventory is 31 files / 2,814 LOC.
- Latest semantic/LLVM MIR owner cleanup moved
  `src/semantic/type_checker_builtins_query_channel.inc` into
  `src/semantic/type_checker_builtins_query_channel.h`,
  `src/semantic/type_checker_operator_expr.inc` into
  `src/semantic/type_checker_operator_expr.h`,
  `src/codegen/llvm_mir_locals.inc` into
  `src/codegen/llvm_mir_local_emit.h`,
  `src/codegen/llvm_mir_blocks.inc` into
  `src/codegen/llvm_mir_block_emit.h`,
  `src/semantic/type_checker_resolution_graph_core.inc` into
  `src/semantic/type_checker_resolution_graph_core.h`, and
  `src/codegen/transpiler_emitters_enum_decl.inc` into
  `src/codegen/transpiler_enum_decl_emit.h`. The current production source
  `.inc` inventory is 25 files / 1,675 LOC.
- Latest helper/call owner cleanup moved
  `src/semantic/type_checker_helpers_context.inc` into
  `src/semantic/type_checker_context_helpers.h`,
  `src/codegen/llvm_expr_call_log.inc` into
  `src/codegen/llvm_expr_log_calls.h`,
  `src/codegen/llvm_expr_call_arrays.inc` into
  `src/codegen/llvm_expr_array_calls.h`,
  `src/codegen/transpiler_emitters_base_b_part_a.inc` into
  `src/codegen/transpiler_mir_emit_state.h`,
  `src/codegen/transpiler_emitters_base_a_part_a.inc` into
  `src/codegen/transpiler_mir_emit_decls.h`, and
  `src/codegen/transpiler_emitters_base_a_part_b.inc` into
  `src/codegen/transpiler_mir_pending_uses.h`. The current production source
  `.inc` inventory is 19 files / 835 LOC.
- Latest formatter/flow/observability cleanup moved `src/compiler/fmt_layout.inc`
  into `src/compiler/fmt_layout.h`, `src/compiler/fmt_io.inc` into
  `src/compiler/fmt_io.h`, `src/semantic/type_checker_flow_effects.inc` into
  `src/semantic/type_checker_flow_effects.h`,
  `src/semantic/type_checker_flow_parallel.inc` into
  `src/semantic/type_checker_flow_parallel.h`,
  `src/codegen/llvm_expr_call_intent_observability.inc` into
  `src/codegen/llvm_expr_intent_observability_calls.h`, and
  `src/semantic/type_checker_assignment.inc` into
  `src/semantic/type_checker_assignment.h`. The current production source `.inc`
  inventory is 13 files / 297 LOC.
- Latest overall audit also reran `make tooling-conformance-test-smoke`; the
  formatter smoke is invoked through `bash`, so Linux execute-bit drift no
  longer blocks the tooling conformance gate.
- `test-abi`: ABI spec 49 passed, 0 failed, plus C/LLVM ABI pipeline smoke.
- `runtime-abi-lifetime-test-smoke`: passed.
- `test-inc-size-test-smoke`: all `src/tests/**/*.inc` files are below
  990 LOC.
- `inc-sentinel-test-smoke`: no empty `.inc` files are allowed, and the source
  `.inc` count must not increase above 159.
- `git diff --check`: passed; only line-ending warnings were reported.

## Remaining Include Debt

- Test fixture `.inc` files are now under the 990 LOC cap and are guarded by
  `make test-inc-size-test-smoke`.
- The next structural cleanup queue is no longer `.inc` removal; it is
  600-plus production owner reduction. Current high-priority examples include
  `src/compiler/mir.c`, `src/compiler/hir.c`, `src/codegen/llvm_intent.c`,
  `src/runtime/slot_security.c`, `src/runtime/slot_manager.c`,
  `src/semantic/type_checker_decls_domain_helpers.c`,
  `src/codegen/llvm_domain.c`, `src/compiler/air.c`, and
  `src/codegen/llvm_domain_zone_sync.c`. Each one needs a named semantic owner
  split, not blind line-count sharding.
- The long-term target remains real `.c` / `.h` ownership for behavior-heavy
  families. The current state removes the worst function-boundary and size
  debt, but `.inc` should continue shrinking toward generated tables, private
  macro tables, or test fixtures only.
- Runtime inline owners may live in private `.h` files when generated C must
  include the implementation directly. This is still preferable to behavior
  growth inside split `.inc` files, but those headers must keep ABI names and
  include order explicit.
- New `.inc` split files are not allowed by default. Raising the file-count cap
  must be treated as beta debt and justified in this ledger.
- C backend context and local symbol tracking now have real TU owners:
  `transpiler_context.c`, `transpiler_symbols.c`, and
  `transpiler_decl_lookup.c`, `transpiler_projection.c`,
  `transpiler_nominal.c`, `transpiler_enum.c`, `transpiler_operator.c`, and
  `transpiler_type_alias.c`, `transpiler_type_require.c`, and
  `transpiler_extern.c`, `transpiler_type_declarator.c`,
  `transpiler_log_normalize.c`, `transpiler_parallel_capture.h`,
  `transpiler_expr_builtin_dispatch.h`, and
  `transpiler_expr_stdlib_builtin.h`, `transpiler_overlay_projection.h`, and
  `transpiler_let_emit.h`, `transpiler_mir_block_emit.h`,
  `transpiler_intent_emit.h`, `pgy_runtime_intent_active_exports.h`, and
  `pgy_runtime_lib_intent_exports.h`, and
  `llvm_expr_call_projection_sync.h`, `transpiler_mir_ssa_contract.h`, and
  `transpiler_slot_builtin_emit.h`, `transpiler_type_mapping_helpers.h`,
  `transpiler_world_select_event_emit.h`, and
  `llvm_expr_assignment_member_projection.h`, plus
  `pgy_runtime_lib_authority_file_core.h` and
  `pgy_runtime_lib_set_intent_trace_exports.h`, plus `rir_flow.h`,
  `llvm_domain_world_sync.c`, and `llvm_domain_zone_sync.c`. The next
  high-value extraction candidate is not more blind line-count splitting; it is
  choosing a real owner seam for `compiler/mir.c` or the 1,027 LOC zone
  frontier body.
- Empty `.inc` tails are no longer allowed. A split-order shim may include only
  real implementation chunks; if a tail becomes empty, remove it and update the
  shim, dependency list, tests, and this ledger in the same change.
- Future splits must not move dangling return-type fragments across include
  boundaries. The build now enforces this through
  `-Werror=implicit-function-declaration` and `-Werror=implicit-int`.

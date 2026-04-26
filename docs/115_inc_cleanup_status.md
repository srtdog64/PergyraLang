# Include Cleanup Status

Last updated: 2026-04-26

This note records the current state of the beta include-cleanup track. It is a
progress ledger, not a new language surface.

## Closed In This Slice

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
  indentation normalization. This keeps `transpiler_expr_emitters_part_a.inc`
  focused on expression lowering instead of carrying multiline string
  normalization helpers.
- `src/runtime/pgy_runtime_intent_exit.h` now owns the generated-C inline
  intent exit implementation. The ABI surface remains `static inline
  pgy_intent_exit_export(...)`, but the large observability snapshot/cleanup
  body no longer lives in `pgy_runtime_part_ba_part_b.inc`.
- `src/runtime/pgy_runtime_slot_macros.h` now owns the generated-C inline
  `DeviceSlot<T>` and `SecureSlot<T>` macro families. The built-in slot
  instantiation order stays in `pgy_runtime_part_ba_part_c.inc`, but the macro
  bodies no longer inflate that split include.
- `src/runtime/pgy_runtime_intent_history.h` now owns the generated-C inline
  last-intent history step accessors. The exported inline ABI names remain
  unchanged, while `pgy_runtime_part_ba_part_a.inc` no longer carries the
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
  scope, fact, resource, state, and op kinds. `rir_public.inc` now focuses on
  validation and dump surfaces instead of carrying name vocabulary bodies.
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
- `src/codegen/transpiler_expr_emitters.inc` is now a shim over top-level
  function-boundary parts:
  - `transpiler_expr_emitters_part_a.inc` through
    `transpiler_expr_emitters_part_f.inc`
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
- `src/codegen/transpiler_emitters_base_b_part_d.inc` no longer leaves
  dangling `static void` return-type fragments for the intent emitter.
- `src/codegen/transpiler_emitters_intent.inc` now owns the full
  `emit_intent_decl` signature at its file boundary.
- Runtime ABI lifetime smoke was updated so runtime split-file checks read the
  whole split family instead of assuming a symbol remains in a fixed old part.

## Current Gate

The production runtime/codegen/compiler include-size gate is green:

```sh
make backend-inc-size-test-smoke
```

The test fixture include-size gate is also green:

```sh
make test-inc-size-test-smoke
```

Empty include sentinels are now rejected:

```sh
make inc-sentinel-test-smoke
```

This gate rejects any zero-byte `.inc` file and rejects any increase above the
current `src/**/*.inc` file cap of 159, so new `.inc` splits are blocked by
default. There is no empty-sentinel allowlist.

The expression emitter parts are currently below the beta 1,000 LOC cap:

```text
src/codegen/transpiler_expr_emitters_part_a.inc 878
src/codegen/transpiler_expr_emitters_part_b.inc 711
src/codegen/transpiler_expr_emitters_part_c.inc 544
src/codegen/transpiler_expr_emitters_part_d.inc 26
src/codegen/transpiler_expr_emitters_part_e.inc 778
src/codegen/transpiler_expr_emitters_part_f.inc 472
src/codegen/transpiler_expr_stdlib_builtin.h 920
```

The related intent/base emitter seams are also below the cap:

```text
src/codegen/transpiler_helpers_core_a.inc 29
src/codegen/transpiler_helpers_core_a_part_c.inc 577
src/codegen/transpiler_helpers_core_b.inc 82
src/codegen/transpiler_helpers_core_b_part_b.inc 519
src/codegen/transpiler_helpers_core_b_part_c.inc 849
src/codegen/transpiler_context.c 284
src/codegen/transpiler_context.h 36
src/codegen/transpiler_symbols.c 349
src/codegen/transpiler_symbols.h 42
src/codegen/transpiler_decl_lookup.c 618
src/codegen/transpiler_decl_lookup.h 58
src/codegen/transpiler_enum.c 42
src/codegen/transpiler_enum.h 16
src/codegen/transpiler_extern.c 53
src/codegen/transpiler_extern.h 8
src/codegen/transpiler_expr_stdlib_builtin.h 920
src/codegen/transpiler_intent_emit.h 965
src/codegen/transpiler_log_normalize.c 127
src/codegen/transpiler_log_normalize.h 6
src/codegen/transpiler_let_emit.h 767
src/codegen/transpiler_mir_block_emit.h 967
src/codegen/transpiler_nominal.c 254
src/codegen/transpiler_nominal.h 20
src/codegen/transpiler_operator.c 148
src/codegen/transpiler_operator.h 23
src/codegen/transpiler_overlay_projection.h 928
src/codegen/transpiler_parallel_capture.h 229
src/codegen/transpiler_projection.c 374
src/codegen/transpiler_projection.h 42
src/codegen/transpiler_type_declarator.c 188
src/codegen/transpiler_type_declarator.h 13
src/codegen/transpiler_type_require.c 64
src/codegen/transpiler_type_require.h 20
src/codegen/transpiler_type_render.h 16
src/codegen/transpiler_helpers_core_types.inc 657
src/codegen/transpiler_emitters_base_b_part_d.inc 257
src/runtime/pgy_runtime_intent_exit.h 106
src/runtime/pgy_runtime_panic_checked_inline.h 191
src/runtime/pgy_runtime_slot_macros.h 190
```

The MIR public implementation split is also below the production cap after
moving the public name helpers and `mir_destroy(...)` into the second public
part:

```text
src/compiler/mir_public_part_a.inc 959
src/compiler/mir_public_part_b.inc 800
```

The gate is green, but several production include files are still close enough
to the 1,000 LOC cap that they should be treated as owner-extraction
candidates rather than targets for more split-file growth:

```text
src/codegen/transpiler_emitters_base_a_part_d.inc 885
src/codegen/llvm_expr_call_methods_part_a.inc 880
src/codegen/transpiler_expr_emitters_part_a.inc 878
src/codegen/transpiler_emitters_base_b_part_c.inc 873
src/runtime/pgy_runtime_lib_part_b_part_c.inc 852
src/codegen/transpiler_helpers_core_b_part_c.inc 849
src/runtime/pgy_runtime_part_ba_part_d.inc 814
src/codegen/transpiler_emitters_base_b_part_a.inc 811
src/runtime/pgy_runtime_part_ba_part_c.inc 808
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
- Latest overall audit also reran `make tooling-conformance-test-smoke`; the
  formatter smoke is invoked through `bash`, so Linux execute-bit drift no
  longer blocks the tooling conformance gate.
- `test-abi`: ABI spec 49 passed, 0 failed, plus C/LLVM ABI pipeline smoke.
- `runtime-abi-lifetime-test-smoke`: passed.
- `test-inc-size-test-smoke`: all `src/tests/**/*.inc` files are below
  1,000 LOC.
- `inc-sentinel-test-smoke`: no empty `.inc` files are allowed, and the source
  `.inc` count must not increase above 159.
- `git diff --check`: passed; only line-ending warnings were reported.

## Remaining Include Debt

- Test fixture `.inc` files are now under the 1,000 LOC cap and are guarded by
  `make test-inc-size-test-smoke`.
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
  `transpiler_log_normalize.c`, `transpiler_parallel_capture.h`, and
  `transpiler_expr_stdlib_builtin.h`, `transpiler_overlay_projection.h`, and
  `transpiler_let_emit.h`, `transpiler_mir_block_emit.h`,
  `transpiler_intent_emit.h`, and `pgy_runtime_intent_active_exports.h`. The
  next high-value extraction candidate is not
  more blind line-count splitting; it is choosing a real owner seam for generic
  binding/type-specialization helpers, LLVM method-call helpers, or remaining
  runtime near-cap owners such as intent history/runtime-lib export groups.
- Empty `.inc` tails are no longer allowed. A split-order shim may include only
  real implementation chunks; if a tail becomes empty, remove it and update the
  shim, dependency list, tests, and this ledger in the same change.
- Future splits must not move dangling return-type fragments across include
  boundaries. The build now enforces this through
  `-Werror=implicit-function-declaration` and `-Werror=implicit-int`.

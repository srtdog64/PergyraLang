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
- The empty C backend tail include
  `src/codegen/transpiler_helpers_core_a_part_d.inc` was removed from the
  `transpiler_helpers_core_a.inc` shim.
- Empty compiler/runtime tail sentinels remain in place for the current split
  families:
  - `src/compiler/mir_public_part_c.inc`
  - `src/runtime/pgy_runtime_part_ba_part_f.inc`
  These must be removed only with their shim/order contract and tests in the
  same change; restoring old HEAD content is wrong because those bodies have
  already moved into earlier split parts in the current working tree.
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

Empty include sentinels are now guarded explicitly:

```sh
make inc-sentinel-test-smoke
```

This gate rejects any zero-byte `.inc` file unless it is listed as an
intentional split-family sentinel and verified against its owning shim. The
current allowlist is deliberately narrow:

```text
src/compiler/mir_public_part_c.inc -> src/compiler/mir_public.inc
src/runtime/pgy_runtime_part_ba_part_f.inc -> src/runtime/pgy_runtime_part_ba.inc
```

The expression emitter parts are currently below the beta 1,000 LOC cap:

```text
src/codegen/transpiler_expr_emitters_part_a.inc 991
src/codegen/transpiler_expr_emitters_part_b.inc 711
src/codegen/transpiler_expr_emitters_part_c.inc 544
src/codegen/transpiler_expr_emitters_part_d.inc 946
src/codegen/transpiler_expr_emitters_part_e.inc 778
src/codegen/transpiler_expr_emitters_part_f.inc 472
```

The related intent/base emitter seams are also below the cap:

```text
src/codegen/transpiler_helpers_core_a_part_a.inc 14
src/codegen/transpiler_helpers_core_a_part_c.inc 577
src/codegen/transpiler_helpers_core_b_part_a.inc 70
src/codegen/transpiler_helpers_core_b_part_b.inc 519
src/codegen/transpiler_context.c 284
src/codegen/transpiler_context.h 36
src/codegen/transpiler_symbols.c 349
src/codegen/transpiler_symbols.h 42
src/codegen/transpiler_decl_lookup.c 618
src/codegen/transpiler_decl_lookup.h 58
src/codegen/transpiler_enum.c 42
src/codegen/transpiler_enum.h 16
src/codegen/transpiler_nominal.c 254
src/codegen/transpiler_nominal.h 20
src/codegen/transpiler_operator.c 148
src/codegen/transpiler_operator.h 23
src/codegen/transpiler_projection.c 374
src/codegen/transpiler_projection.h 42
src/codegen/transpiler_type_render.h 16
src/codegen/transpiler_helpers_core_types.inc 657
src/codegen/transpiler_emitters_intent.inc 962
src/codegen/transpiler_emitters_base_b_part_d.inc 257
```

After the local symbol/slot tracking extraction,
`src/codegen/transpiler_helpers_core_a_part_a.inc` is down to 422 LOC.
After the declaration lookup extraction, it is down further to the remaining
projection/type predicate helpers at 169 LOC.
After the projection/type predicate extraction, it is now a forward-declaration
shim only at 14 LOC.
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

The codegen helper shim now has no empty tail part:

```text
src/codegen/transpiler_helpers_core_a.inc
  -> part_a, part_b, part_c
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
- `test-abi`: ABI spec 49 passed, 0 failed, plus C/LLVM ABI pipeline smoke.
- `runtime-abi-lifetime-test-smoke`: passed.
- `test-inc-size-test-smoke`: all `src/tests/**/*.inc` files are below
  1,000 LOC.
- `inc-sentinel-test-smoke`: only the two documented empty split-family
  sentinels are allowed, and both are still included by their shims.
- `git diff --check`: passed; only line-ending warnings were reported.

## Remaining Include Debt

- Test fixture `.inc` files are now under the 1,000 LOC cap and are guarded by
  `make test-inc-size-test-smoke`.
- The long-term target remains real `.c` / `.h` ownership for behavior-heavy
  families. The current state removes the worst function-boundary and size
  debt, but `.inc` should continue shrinking toward generated tables, private
  macro tables, or test fixtures only.
- C backend context and local symbol tracking now have real TU owners:
  `transpiler_context.c`, `transpiler_symbols.c`, and
  `transpiler_decl_lookup.c`, `transpiler_projection.c`,
  `transpiler_nominal.c`, `transpiler_enum.c`, and
  `transpiler_operator.c`. The next high-value extraction candidate is not more
  blind line-count splitting; it is choosing a real owner seam for generic
  binding/type-specialization helpers.
- Empty `.inc` tails are not a general pattern. They are allowed only as a
  temporary split-order sentinel when removing them would also require a shim
  contract change; each such file must be named in
  `tests/inc_sentinel_smoke.sh`.
- Future splits must not move dangling return-type fragments across include
  boundaries. The build now enforces this through
  `-Werror=implicit-function-declaration` and `-Werror=implicit-int`.

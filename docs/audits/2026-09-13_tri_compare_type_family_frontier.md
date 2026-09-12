# Backend tri-compare: the type families the bounded route cannot represent

Revision: `cb7c8149ad0f4dc15c3fd2b0dfe9b61b09eb4f0d` (published main/origin/main).
Status: `ci.yml` and `platform_full.yml` are green on this revision;
`self_host_parity.yml` stops at `backend_output_tri_compare_parity.sh` with four
of fourteen cases passing. This audit is evidence, not an ABI owner, completion
score or implementation plan.

## Why the step cannot close with owner fixes

Three registries decide whether the bounded direct-MIR route can represent a
type: `compiler/abi_layout_row_owner.pgy` supplies the ABI row,
`compiler/direct_mir_scalar_cfg_type_family_owner.pgy` admits it as a source
local through `DirectMirScalarCfgSourceLocalTypeSupported`, and
`compiler/runtime_value_representation_owner.pgy` gives it a runtime
representation. Counting occurrences of each family the nine failing cases need:

```
                abi_row  local_admit  runtime_repr
Slice                 0            0             0
Box                   0            0             0
Future                0            0             0
Channel               0            0             0
WriteView             0            0             0
ReadView              0            0             0
SecureSlot            0            0             0
Zone                  0            0             0
Slot                  0            0             0
Allocator             1            0             3
```

One of the ten is representable. So the remaining cases are not stale pins or
single-owner gaps: each family needs a row, admission, a representation and
emission on both legs before any case that uses it compiles.

That ordering is measured, not assumed. The checker side of `slice_copy` is now
complete -- `SliceCopy` is registered and the chained-call mis-parse is fixed --
and the case passes the checker tool while the driver still refuses it at
`SemanticAstInitializerTypeFacts`. Repairing the typing alone moves the wall
from the semantic stage to the route stage.

## Family to case

| family | unlocks | also needs |
| --- | --- | --- |
| `Slice<T>` | `slice_copy`, `slice_surface` | tuple destructuring binding for `slice_surface` |
| `Box<T>` | `allocator_lane_boxarray`, `allocator_defer_cleanup` | `defer`; `Box<Array<Int>>` arrives as `Box<Unknown>` |
| `Slot<T>`, `ReadView<T>`, `WriteView<T>`, `SecureSlot<T>` | `pin_write_view_block`, `secure_slot_view` | a `pin` statement rule and four block attributes |
| `Channel<T>` | `channel_send_recv_basic` | an expression node kind for channel receive |
| `Future<T>` | `async_spawn_await` | `spawn`, `await`, `async func` |
| `Zone` | `intent_zone_binding` | observe with `--observe-mir-consumer-stages` |

`class_factory_field_method` needs no new family; it refuses with
`direct MIR three-routine structural shape is unsupported`.

## The first family's target bytes

`Slice<T>` is the cheapest entry and the native producer already carries its
rows (`src/compiler/mir_abi_layout.c` around line 268), so the self-hosted row
has an exact target rather than a design question. Dumped from
`PGY_NATIVE_PIPELINE=1 pgy --mir-json tests/cases/backend_compare/slice_copy/main.pgy`:

```
Slice<Int>     size=16 align=8 fields=[data@0/8/8 length@8/8/8]
               runtime_fn=pgy_slice_get_Int    inner_c_type=int32_t  representation=0
Slice<String>  size=16 align=8 fields=[data@0/8/8 length@8/8/8]
               runtime_fn=pgy_slice_get_String inner_c_type=char*    representation=0
```

The emitted `abi_layout` object names no field-order term, only the field list,
so the self-hosted row's field-order classification is internal and does not
need a vocabulary agreed with the native producer. The runtime layout is
`{ data: ptr, length: i64 }` on both legs: C `PgySlice_<Suffix>`
(`src/runtime/pgy_runtime_memory_array_slot_inline.h` around line 209) and LLVM
`%PgySlice_<Suffix>` (`src/codegen/llvm_backend.c` around line 350).

Its two source operations are `Array<T>.Slice(start: Int, len: Int) -> Slice<T>`
(`src/semantic/type_checker_expr_call.c` around line 278) and
`SliceCopy(Slice<T>) -> Array<T>`
(`src/semantic/type_checker_builtins_stdlib_array.c` around line 300).

## What this revision closed

Six commits took the set from three of fourteen to four and repaired two
regressions this work introduced. The bounded gaps within reach are closed: the
`f"{expr}"` parse form, the channel constructor with its `Channel<Unknown>`
reconciliation and the `<-` operator no longer read as a comparison, the
chained-call argument span, and `SliceCopy`'s registration. What is left is
family construction.

Two traps cost real time and are worth carrying forward. A new refusal must not
pre-empt a more specific diagnostic a negative gate pins: that broke
`platform_full` twice, once from comparing a whole call rendering and once from
skipping a refusal by operand spelling rather than by whether the type resolved.
And several owner files have their bytes pinned, so editing
`semantic/expression_operator_fact_owner.pgy` required re-pinning
`benchmarks/selfhost_source_scan_owner_evidence.json`.

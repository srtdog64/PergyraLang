# Pointer Risk Register

This document is the current source of truth for pointer and lifetime risks in
the beta-closure sprint. It does not claim that Pergyra is pointer-free. It
classifies where pointers are allowed, who owns them, and which seams still need
evidence before beta.

## 0. Rule

Pergyra source code should expose semantic handles, views, and domain
boundaries. C/LLVM lowering may use raw pointers internally, but every pointer
that crosses a stable ABI boundary must be classified as one of:

- `borrowed`: caller must not free; valid only for the documented owner window.
- `result-owned`: caller/runtime consumer owns and must eventually release.
- `runtime-owned`: runtime table/registry owns the backing object.
- `container-owned`: collection/channel owns the stored payload until removal,
  receive, overwrite, or destroy.
- `scratch`: temporary helper storage; must not be cached or returned.

Any pointer not fitting one of these classes is beta debt.

## 1. Closed And Gated

| Risk | Current contract | Gate |
|---|---|---|
| Intent observability string exports | `runtime-borrowed string` copied into thread-local snapshots; consume/copy before a later borrowed string query on the same thread reuses the slot | `runtime-abi-lifetime-test-smoke` |
| String helpers | `result-owned string` | `runtime-abi-lifetime-test-smoke` |
| `Substring` / `StringReplace` sizing | reject strings beyond the `Int` index range and count replacements with `size_t` before result allocation | `runtime-abi-lifetime-test-smoke` + `test-abi` |
| `StringSplit` / `MapKeys` arrays | `result-owned array` with copied string payloads | `runtime-abi-lifetime-test-smoke` |
| File descriptors | `runtime-owned handle` table, released by close | `runtime-abi-lifetime-test-smoke` |
| Exported slices | no pointer arithmetic for zero-length slices; subtract-form range checks | `runtime-abi-lifetime-test-smoke` |
| `Queue<String>` | `container-owned` copied payloads on C inline and LLVM raw paths | `runtime-abi-lifetime-test-smoke` |
| `Channel<String>` | `container-owned` copied payload on inline C and LLVM export paths; receive transfers; destroy frees pending payloads | `runtime-abi-lifetime-test-smoke` |
| Inline `Channel<T>` boundary | status/ready/length/capacity/full/space/closed helpers reject null or uninitialized channels before locking, and public `Int` size views clamp oversized counters | `runtime-abi-lifetime-test-smoke` + `test-abi` |
| Exported `Channel<Int/String>` boundary | destroy/close/query helpers reject null or uninitialized exported channels before locking, and public `Int` size views clamp oversized counters | `runtime-abi-lifetime-test-smoke` + `test-abi` |
| `List<String>` | push/set copy, get borrows, remove frees | `runtime-abi-lifetime-test-smoke` |
| `Set<String>` | add copies, has borrows probe, remove frees and tombstones | `runtime-abi-lifetime-test-smoke` |
| Inline `List`/`Queue` capacity | capacity growth is bounded by both element allocation size and the `Int` size API range | `runtime-abi-lifetime-test-smoke` |
| Inline/raw `Array`/`Slice`/`Rc` handles | array, slice, box, BoxArray, Rc, and Weak helpers reject null handles, missing backing storage, refcount overflow, and BoxArray drop-size overflow before dereference or pointer arithmetic | `runtime-abi-lifetime-test-smoke` + `test-abi` |
| Raw/inline collection size API range | raw List/Queue/HashMap/Set and inline HashMap capacity growth stays within `Int` size-return range | object compile + ABI smoke term |
| Raw/inline collection initialization guards | raw List/Queue/HashMap/Set and inline List/Queue/Set/HashMap operations reject uninitialized or oversized backing storage and null string keys before count, modulo, `strcmp`, or pointer access | `runtime-abi-lifetime-test-smoke` + `test-abi` |
| Raw `Set<T>` pointer-sized values | generic set hashes raw bytes; String has separate hash/equality/rehash path | `runtime-abi-lifetime-test-smoke` |
| LLVM raw `HashMap<K,String>` | set copies values, get borrows, remove frees key/value and tombstones | `runtime-abi-lifetime-test-smoke` |
| Raw/inline hash deletion | tombstone or backward-shift preserves probe chains | `runtime-abi-lifetime-test-smoke` |
| Pool/arena/slice pointer arithmetic | overflow checked before deriving pointer; Pool/FSM/Timer helpers reject null handles, invalid FSM inputs, and oversized pool capacity before pointer/state access; allocator tracing counters saturate instead of wrapping and debug fill skips zero-size null allocations | direct runtime probes + ABI smoke terms |
| Generated C/LLVM `Slice<T>.Slice` | borrowed view only; subtract-form range check; non-empty null backing rejects; zero-length slices do not derive a pointer; panic class is `out-of-bounds` | `runtime-abi-lifetime-test-smoke` + `runtime-panic-contract-test-smoke` + `perf-contract-smoke` |
| `SliceCopy(Slice<T>)` | explicit owned snapshot escape hatch; copies slice elements into a new `Array<T>`; `Slice<String>` duplicates payloads to satisfy result-owned string-array policy; borrowed slice transport stays blocked unless copied first | `runtime-abi-lifetime-test-smoke` + `test-semantic` + `test-transpile` + backend compare `slice_copy` |
| `Slice<T>` boundary transport | `Slice<T>` is `BORROW_TRACKED`; ref-spawn, blocking channel send/receive, and non-blocking channel helper send/receive attempts are rejected with `borrowed Slice view` / backing-owner diagnostics until a future valid boundary form carries explicit owner/provenance evidence | `test-semantic` + `cfg-body-dataflow-test-smoke` |
| Secure scope destroy while pinned | checked destroy returns `SLOT_ERROR_PINNED`; void destroy hard-fails | `test-security` fixture coverage |
| Slot manager/scope allocation sizes | slot table and secure scope handle/token arrays are size-checked before allocation and token wiping | object compile + ABI smoke term |
| Slot pool API boundary | alloc/free/get/is-valid reject uninitialized backing arrays and invalid free-list cursors; stats avoids divide-by-zero on corrupted zero-capacity pools | `runtime-abi-lifetime-test-smoke` + `test-abi` |
| Async scheduler worker/queue boundary | worker array size is checked before scheduler `calloc`; start/stop/destroy/steal/spawn/enqueue/unblock reject missing worker arrays or queues before dereference | object compile + ABI smoke term |
| Async concurrent queue boundary | queue push/pop reject missing head/tail sentinels and the queue size counter saturates/decrements only within valid range | `runtime-abi-lifetime-test-smoke` + `test-abi` |
| Parallel task array boundary | `parallel_run` rejects null task arrays and checks task/argument array sizes before `calloc`; worker arrays share the same overflow guard | `runtime-abi-lifetime-test-smoke` |
| Party runtime context/stat boundary | nonzero role/shared-field/ability counts reject null backing arrays before lookup; fiber-stat counters saturate and all stat readers/writers use the registry mutex | `runtime-abi-lifetime-test-smoke` |
| World/roster async completion | async roster worker publishes result before a release-store completion flag; waiters acquire-load completion before reading the result | `runtime-abi-lifetime-test-smoke` + `test-abi` |
| LLVM argument scratch arrays | call/spawn/event/callable lowering guards `size_t` arena sizing and LLVM `unsigned` arity before building argument arrays | object compile + ABI smoke term |
| LLVM constructed type args | LLVM type mapping and other public callers use `llvm_constructed_arg_name_copy(...)`; static scratch helper was removed | object compile + ABI/perf smoke terms |
| LLVM Rc expected inner type | Rc lowering copies expected inner type into caller-owned storage instead of returning static scratch | object compile |
| LLVM expected type context | return/let expected type annotations are copied into `LLVMGenCtx.scratch` before expression lowering | object compile + perf smoke |
| LLVM spawn future inner type | generic spawned return inference stores inferred inner type in `LLVMGenCtx.scratch` before later future-var registration | object compile + perf smoke |
| LLVM temporary names | small ring buffer avoids same-expression static-name clobber before LLVM consumes builder names | object compile + ABI smoke term |
| C backend constructed type args | expression inference and match payload lowering copy constructed args before further type rendering | `test-transpile` + perf smoke |
| C backend enum variant lookup | qualified enum variant names are written to caller-owned buffers | `test-transpile` + perf smoke |
| C backend match enum destructuring | enum payload binding arrays are caller stack-owned, not static shared storage | `test-transpile` + perf smoke |
| C backend MIR intent collector arrays | MIR intent metadata collector uses one checked capacity helper before scratch-array growth | object compile |
| C backend enum constructor arguments | enum-constructor lowering guards argument pointer-array sizing and generated call-buffer growth | `test-transpile` |
| World/roster dynamic arrays | count growth, result arrays, execution-plan/stat arrays, and visualization buffer sizing are checked before `realloc`/`calloc`; nonzero roster/party counts reject null backing arrays before iteration; frame and estimate counters saturate instead of wrapping | object compile + ABI smoke term |
| World/roster copied names | copied roster/world names guard `strlen + 1` before allocation | object compile + ABI smoke term |

## 2. Beta Policies And Open Risks

### 2.1 Beta Policy: `Array<String>` Ownership

Current generic `PgyArray_String` stores `char *` values. Known stable producers
such as `StringSplit` and `MapKeys` duplicate payloads before pushing, so the
array becomes the result owner in those paths. The generic `ArrayPush` /
`ArraySet` surface should not be globally changed to deep-copy until the source
contract is decided, because doing so can create double-copy or leak behavior in
producer-owned paths.

- Option A: `Array<String>` is pointer-storage; producers must copy when the
  array owns payloads.
- Option B: `Array<String>` owns payloads; push/set/drop become string-specific
  and every producer passes borrowed inputs.

Beta decision: keep Option A and gate every stable producer that returns
`Array<String>` as result-owned. This keeps generic `Array<String>` as
pointer-storage while requiring producer functions such as `StringSplit` and
`MapKeys` to duplicate payloads before pushing.

This item is closed for beta as a policy decision, not as a universal
ownership model. The beta contract is intentionally narrow: stable
`Array<String>` producers own their returned payloads, while generic
`Array<String>` mutation APIs remain pointer-storage and must not silently
deep-copy.

Option B remains a beta+ ABI proposal, not a silent runtime tweak. To make
`Array<String>` globally own payloads, the language would need string-specific
`ArrayPush` / `ArraySet` / `ArrayDrop` semantics, generated C/LLVM parity,
double-copy prevention for producers that already allocate, and a migration rule
for borrowed string views. Until those are designed together, global deep-copy
would trade one dangling-pointer risk for double-free/leak ambiguity.

### 2.2 Open Risk: Static Scratch Pointers In Codegen Helpers

Several C helpers return pointers to static local buffers, especially type-name
rendering and C type mapping helpers. These are acceptable only if the caller
uses the pointer immediately and never stores it across another helper call.
LLVM constructed-type parsing now has `llvm_constructed_arg_name_copy(...)` for
callers that need stable data across further lowering. The old static-return
`llvm_constructed_arg_name_at(...)` compatibility seam was removed, so
recursive type lowering cannot retain that scratch pointer.

Known examples:

- `llvm_tmp_name(...)`: small ring of static temp-name buffers for immediate
  LLVM builder calls; callers must still not cache the returned pointer.
- `llvm_stmt_render_type_annotation_copy(...)` renders type annotations through
  a stack buffer and stores the result in the LLVM scratch arena. The old
  static-return annotation helper was removed.
- LLVM constructed generic argument parsing is copy-only through
  `llvm_constructed_arg_name_copy(...)`. The old static-return
  `llvm_constructed_arg_name_at(...)` helper was removed.
- C backend type mapping is now copy-only. The static-return
  `pergyra_type_to_c(...)`, `slot_inner_type_name(...)`,
  `constructed_arg_name_at(...)`, and `generic_args_to_c_suffix(...)`
  entrypoints were removed; stable callers must use
  `pergyra_type_to_c_copy(...)`, `slot_inner_type_name_copy(...)`,
  `copy_constructed_arg_name_at(...)`, and
  `generic_args_to_c_suffix_copy(...)`.
  `collection_runtime_suffix_copy(...)` serves the same role for collection
  runtime helper suffixes. The old static-return `collection_runtime_suffix(...)`
  helper was removed after List/Set/Queue/Map call emission moved to stack-owned
  suffix buffers.
- C backend slot target helpers: `lookup_slot_type_copy(...)`,
  `transpiler_resolve_slot_target_copy(...)`, and
  `transpiler_resolve_device_slot_inner_copy_or_error(...)` are the stable
  caller-owned seams. The older pointer-returning lookup/resolver functions
  were removed instead of kept as compatibility surfaces.
- C backend contextual `Option<T>` helpers now expose
  `transpiler_contextual_option_inner_type_copy(...)` for `Some` / `None` /
  `UnwrapOption` inference and emission. The pointer-returning contextual inner
  helper was removed.
- C backend future/channel/select inner-type helpers now use caller-owned copy
  seams (`lookup_future_inner_type_copy(...)`,
  `channel_inner_type_name_copy(...)`, and
  `select_channel_inner_type_copy(...)`) instead of returning the static
  `slot_inner_type_name(...)` scratch pointer through intermediate helpers.
- C backend function signature and slot-parameter metadata emission now copies
  slot payload names before generating secure token parameters or registering
  slot variables. Forward declarations, AST fallback function emission, and
  MIR-backed function emission must not pass `slot_inner_type_name(...)`
  directly into `register_slot_var(...)`.
- C backend expression type inference now routes nested `Slot<T>` /
  `Array<T>` / `Channel<T>` / `DeviceSlot<T>` inner-type extraction through
  `transpiler_infer_slot_inner_type_name(...)`, which copies the result into
  the transpiler arena before returning it from the inference API.
- C backend expression/codegen emission no longer calls
  `slot_inner_type_name(...)`; that static-return API was removed. Core
  builtins, stdlib collections, for-in/destructuring, match payload binding,
  channel send/receive, MIR local type lookup, MIR resource emission, and MIR
  SSA slot registration now use `slot_inner_type_name_copy(...)` or an
  arena-owned wrapper before preserving the value across formatting,
  registration, or secondary type rendering.
- `pergyra_ast_type_to_c_copy(...)` is the only AST type-node C mapping seam.
  The old static-return `pergyra_ast_type_to_c(...)` compatibility helper was
  removed. Event declaration emission, type
  alias emission, hosted-method metadata forward declarations, MIR function
  parameter emission, spawn wrapper parameter emission, lambda emission, and
  class/enum fallback method return emission now follow the same caller-owned
  rule. Domain role/ability fallback emission, generic class specialization
  emission, match destructor payload emission, and the requirement compatibility
  wrapper also render through the copy seam. Direct
  `pergyra_ast_type_to_c(...)` use must not be reintroduced.
- `pergyra_type_to_c_copy(...)` is the only concrete type-name mapper. The old
  `pergyra_type_to_c(...)` compatibility entrypoint was removed after
  production callers and tests moved to the fail-closed copy contract.
- `lookup_enum_variant_qualified_name_copy(...)` is the only C enum variant
  qualified-name lookup seam. The old static-return
  `lookup_enum_variant_qualified_name(...)` helper was removed after expression
  dispatch, constructor-call lowering, type inference, and MIR SSA contract
  checks moved to stack-owned buffers.
- C match enum destructuring uses caller-owned binding arrays when asking
  `is_enum_variant_destructor(...)` for enum payload bindings. The previous
  static binding-array scratch storage was removed.
- `transpiler_require_type_name_c_type_copy(...)` is now the only type-name
  requirement helper. The old static-return `transpiler_require_type_name_c_type`
  compatibility wrapper was removed after parallel/async capture emission moved
  to caller-owned buffers. `transpiler_require_ast_c_type_copy(...)` is likewise
  the only AST-type requirement helper; the old static-return
  `transpiler_require_ast_c_type(...)` wrapper was removed after class, enum,
  domain nominal, relation/effect, zone/world, roster, generic class, hosted
  metadata, and function fallback emitters moved to caller-owned buffers. Role
  operator alias emission also uses the copy helper for both lhs and rhs so one
  required type cannot overwrite the other before the wrapper signature is
  emitted.
- `transpiler_try_emit_box_array_let(...)` uses a stack-owned inner-type buffer
  for the local `Box<Array<T>>` lowering window; it must not reintroduce a
  static inner buffer because the value does not escape the function.

Required direction:

- New code should prefer caller-provided output buffers, arena-owned copies, or
  result-owned strings.
- Any function returning a static scratch pointer must say so in its name,
  comment, or nearby contract.
- Cached compiler metadata must not store static scratch pointers.

### 2.3 Arena And Cache Pointers

The compiler has scratch, persistent, result-owned, and runtime-owned lanes.
The unsafe pattern is caching a pointer from a shorter-lived lane in a
longer-lived structure.

Required direction:

- Long-lived inventories store stable indexes or owned copies.
- Diagnostics store result-owned payload snapshots, not scratch formatting
  pointers.
- Backend registries store persistent copies, not render scratch.

### 2.4 System-Tier Raw Pointer Escape

`unsafe {}` exists as a boundary marker, but a beta-stable raw pointer / MMIO /
inline-assembly operand surface is not closed. It must not be implied by Slot,
Pin, or view documentation.

Required direction:

- Keep raw pointer escape out of beta-stable claims.
- If introduced, require a dedicated system-tier contract, diagnostics, and
  runtime-none lowering story.

## 3. Audit Procedure

When touching code that returns or stores a pointer:

1. Classify the pointer with the vocabulary in section 0.
2. If it crosses C/LLVM ABI, add or extend a smoke gate.
3. If it is stored in a collection/channel/map, state overwrite/remove/destroy
   ownership.
4. If it is a scratch pointer, prove it cannot be cached or returned.
5. If it depends on pointer arithmetic, use subtract-form range checks or
   overflow-checked size arithmetic before deriving the pointer.

## 4. Current Priority

1. Keep `runtime-abi-lifetime-test-smoke` as the gate for stable ABI pointer
   ownership.
2. Preserve the beta `Array<String>` Option A contract unless a beta+ ABI
   migration explicitly replaces it.
3. Replace static scratch return helpers opportunistically when their callers
   already need stable data.
4. Do not add system-tier raw pointer syntax before beta closure.

# HashMap deep Clone: backend request (2026-10-22)

Status: HANDOFF REQUEST, Claude to GPT. On 2026-10-22 the user assigned
the backend part to GPT under the producer/consumer split. Base: branch
`claude/p0-native-checker` at the commit that adds
[the collection change rule](../truth/decisions/2026-10-22_collection_change_through_a_copy.md).
The outputs here are an implementation request and the measured gain; the
checker change waits for the backend.

## Objective and boundary

`Clone` of a `HashMap<K, V>` with `K` one of `String`, `Int`, `Long`,
`Bool` and `V` a `String` or a physical scalar copies the map into new
storage on C and LLVM, wherever the map sits: the `Clone` argument, a
struct or class field, an Option payload. After that lands on main, the
Claude lane makes `clone_copies_deeply`
(`src/semantic/function_param_publication_result_relation.c`) call such a
`Clone` independent of its argument, and the landing driver then copies
a shared map where it is copied.

Order matters. The checker change without the backend admits two owners
of one bucket array: `let b = Clone(a)` grown past its capacity frees the
buckets `a` still reads (measured: ASan heap-use-after-free, C and LLVM
panic `map key not found`).

Out of scope: maps whose values are Arrays, structs, Options or maps;
List, Set and Queue `Clone`. They stay copied by value and the checker
keeps them dependent. Do not copy them partly.

## Measured gain (checker-only counterfactual, canonical driver input)

On top of the collection change rule, calling such a `Clone` independent
in the checker alone (no backend emits it, reverted) takes final refusals
from 379 to 327: local copies whose other side changed 51 -> 17, moves
102 -> 85, borrowed storage handed on 1 -> 0
([census](../truth/evidence/2026-10-22_native_census_collection_change_through_copy.md)).
In `src/self_hosted`, 476 of the 501 `HashMap<` spellings are
`HashMap<String, Int>`, and 482 have a String or scalar key and value.

## Contract

- The copy owns its own `keys`, `values` and `occupied` arrays with the
  same `capacity`, the same marker in every slot (empty, live, deleted),
  and the same `count`, `deleted_count` and `key_storage_kind`. Copy slot
  by slot, do not rehash: `MapKeys` order must be equal on the original
  and the copy, and C and LLVM must stay runtime-equal.
- A live slot's String key and String value are duplicated. A non-live
  slot holds a NULL key (as `MapRemove` leaves it) and zeroed value bytes.
- A zero (never initialized) map clones to a zero map.
- An allocation failure panics through the existing collection OOM path;
  no partial copy is returned.

## Where it lands today (survey at base)

- C: `transpiler_clone_value_at` (`src/codegen/transpiler_clone_value_emit.c`)
  has no HashMap branch and returns the value. Map types come from
  `PGY_HASHMAP_DEFINE` (`src/runtime/pgy_runtime_builtin_hashmap_inline.h`),
  emitted in `transpiler_specialization_registry.c`.
- LLVM: `llvm_clone_value_at` (`src/codegen/llvm_expr_clone_value.c`) has
  no HashMap branch. LLVM maps use the raw exports over `PgyHashMapRaw`
  (`src/runtime/pgy_runtime_lib_raw_map_exports.h`), registered in
  `llvm_runtime_raw_collections.c`.
- Self-host C emitter: `RewriteSemanticCloneCall`
  (`src/self_hosted/codegen/emission/expr_semantic_clone_call_emit_owner.pgy`)
  copies only Arrays and returns every other argument unchanged, so its
  String, struct and Option `Clone` is by value where native C and LLVM
  copy deeply ([decision](../truth/decisions/2026-10-19_struct_clone_is_deep.md)).
  Read in source, not run. It belongs to self-host generality: the
  installed driver must give the same output, for example a struct with
  an `Array<Int>` field cloned, the copy's field set, the original read.

## Falsifiers

Each program below is refused by the checker until the Claude change, so
test the backend first with that change applied locally (the scratch
counterfactual, a HashMap branch in `clone_copies_deeply` for the types
above). Expected output on C and LLVM, ASan and UBSan clean on C:

| Program | Expected |
|---|---|
| `a: HashMap<String, Int>` with `k=1`; `b = Clone(a)`; `MapSet(b, "k", 2)`; log `MapGet(a, "k")`, `MapGet(b, "k")` | `1 2` (today `2 2`) |
| as above, then 40 new keys into `b`; log `MapGet(a, "k")`, `MapHas(a, "key7")`, `MapGet(b, "key39")` | `1 false 39` (today ASan heap-use-after-free) |
| struct `Bag { label: String; counts: HashMap<String, Int>; }` cloned, `MapSet(b.counts, "k", 2)` | `1 2` |
| `HashMap<String, String>` with `k="one"`, clone set to `"two"` | `one two` |
| 12 keys, one removed, `Clone`; compare `MapKeys` of both element by element | `true 11` |

Add them under `tests/cases/backend_compare/` once the checker admits
them (the Claude change), with a runtime test of the copy itself before.

## Edit scopes

- GPT: the runtime map exports and inline macro, the two native Clone
  emitters, map specialization and LLVM registration, the self-host Clone
  emitter, backend and runtime tests.
- Claude: `clone_copies_deeply`, the ownership fixtures and the census.
- Neither edits the other's files. The checker change lands after the
  backend commit is on main, with the census run on the canonical input.

## Integration gate

The falsifiers above on C and LLVM with ASan; the existing map
`backend_compare` cases; `test-transpile`; then, with the checker change,
`function-param-flow-summary` and the unchanged canonical census.

Emitted code frees nothing yet, so every landed map copy stays allocated:
measure the self-hosted compiler's memory once the canonical program is
admitted.

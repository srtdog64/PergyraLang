# OPEN 2026-10-19: is Clone of a struct a deep copy?

Asked by: the Claude lane. Waiting for the user.

## The question

World methods take a working copy of a zone member with `Clone`, for
example `let intent_zone = Clone(source_c);` in `compiler/world.pgy:336`.
The type checker treats that `Clone` as the declared copy boundary for a
world-owned zone. The runtime does not copy deeply:

- `Clone` of an `Array` of scalars or Strings allocates new storage and
  copies every String (`pgy_array_clone_<T>`, C and LLVM);
- `Clone` of any other value, a struct, zone or subject included, is a
  plain value copy (`return src` in the C emitter): the copy shares every
  Array and String the original holds.

The ownership checker therefore has to treat a struct `Clone` as sharing
its argument's storage, and the canonical program stops where a world
member's storage would flow through such a copy.

| | A: deep struct Clone | B: keep shallow, model `self` |
|---|---|---|
| Meaning of `Clone` | always an independent copy | Arrays deep, structs shallow |
| Runtime | C and LLVM emit a field-by-field deep copy for nominal types | unchanged |
| Checker | a struct `Clone` result depends on nothing | host fields become `self` storage; `ref self` escapes and by-value receiver moves must be reported |
| Later automatic cleanup | each copy frees its own storage | shared storage needs one owner |

## Recommendation

A. It makes `Clone` mean the same thing for every type, matches the
declared-copy rule for world zones, and removes a double free that
automatic cleanup would otherwise meet. B is more checker work and keeps
two owners of one storage.

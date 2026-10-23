# 2026-10-24: Clone of a String or scalar map is a deep copy

Decided by: the Claude lane's
[request](../../agent_work_directives/hashmap_deep_clone_backend_2026-10-22.md)
of 2026-10-22, whose backend the user assigned to GPT; on 2026-10-23 the
user let the Claude lane take the backend over ("이어서해" to the question
whether Claude may take the map deep Clone).

## Rule

`Clone` of a `HashMap<K, V>` whose key is `String`, `Int`, `Long` or `Bool`
and whose value is a `String` or a physical scalar (`Int`, `Long`, `Float`,
`Double`, `Bool`, `Duration`) copies the map into new storage on C and
LLVM, wherever the map sits: the `Clone` argument, a struct or class field,
an Option payload. The copy has the same capacity and the same marker in
every slot, and String keys and values are duplicated. Nothing is
rehashed, so `MapKeys` gives the same order on both. The ownership checker
(`clone_copies_deeply`) then treats such a `Clone` as independent of its
argument, so the landing copies a shared map where the rules select a
copy.

A map with any other value (Arrays, structs, Options, maps), and List, Set
and Queue, are still copied by value, and the checker keeps their `Clone`
dependent on its argument.

## Why

- 482 of the 501 `HashMap<` spellings in `src/self_hosted` have a String or
  scalar key and value. Their local copies, moves and two-part values had
  no copy that could stand in, so the checker refused them.
- A partial copy is not an option: two owners of one bucket array let a
  copy grown past its capacity free the buckets the original still reads.

## Not covered

The self-host C emitter (`RewriteSemanticCloneCall`) still copies only
Arrays whole. Its String, Option, struct and map `Clone` is by value, so
the installed driver can differ from native C and LLVM on a program that
relies on a deep copy. That is a self-host generality obligation, recorded
in the request above.

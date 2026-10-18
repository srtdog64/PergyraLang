# 2026-10-19: Clone of a struct is a deep copy

Decided by: the Claude lane, after the user asked for the option that
"uses less memory and pays off long term" on the
[open question](2026-10-19_open_struct_clone_depth.md).

## Rule

`Clone` of a struct, zone or subject copies every field it owns: Strings
and Arrays are copied as `Clone` of an Array already is, nested values
recursively. The copy shares no storage with the original, on C and LLVM.
The ownership checker then treats a struct `Clone` result as independent
of its argument.

## Why

- Memory: the self-hosted compiler has six non-Array `Clone` calls, all in
  `compiler/world.pgy`, each copying an execution subject that holds an
  identity String or two and an empty outcome, once per driver route
  call. The deep copy costs a few short allocations there.
- `Clone` then means an independent copy for every type, which is what the
  type checker's declared-copy rule for world zones already assumes.
- Shared storage under a shallow copy would have two owners once cleanup is
  automatic, and the checker would have to model `self` storage, `ref
  self` escapes and receiver moves to keep it safe.

Replaces: the OPEN question of 2026-10-19.

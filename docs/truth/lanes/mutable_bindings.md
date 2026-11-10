# Mutable binding implementation lane

- Owner: this user-authorized syntax and immutability support chat.
- Decision: [mutable bindings and compound assignment](../decisions/2026-11-08_mutable_bindings_and_compound_assignment.md).
- State owner: [mutable_bindings.md](../state/mutable_bindings.md).
- Plan: [whole-chain directive](../../agent_work_directives/mutable_bindings_and_compound_assignment_2026-11-08.md).
- Edit scope: declaration/assignment parser facts; exact write permission;
  compound MIR and C/LLVM consumers; reached ownership inference seams;
  compiler-resolved declaration migrations and their generators; behavioral
  binding/Slice/indexed-store gates and this lane's evidence.
- Shared semantic seams include the builtin argument-retention registry and
  existing collection provenance owners. Notify the main chat about changes.
- Main owns the frozen V.001 candidate, installed binaries, active driver ABI
  work, staging, publication, bootstrap and CI. This lane does not change those.
- Preserve unrelated dirty and staged work. A migration file can also contain
  main-owned edits; a full-file patch is not permission to overwrite them.

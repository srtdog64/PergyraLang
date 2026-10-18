# Decisions

One decision per file. A file is never rewritten; a later decision names
the one it replaces. OPEN files wait for the user.

| Date | Decision | Status |
|---|---|---|
| 2026-10-14 | [V1 is two goals](2026-10-14_v1_scope.md) | in force |
| 2026-10-16 | [One main, integrated by the GPT root lane](2026-10-16_main_only_integration.md) | in force |
| 2026-10-17 | [State and decisions live in docs/truth/](2026-10-17_docs_truth_facade.md) | in force |
| 2026-10-17 | [Is the new native ownership checker part of V1?](2026-10-17_open_native_checker_scope.md) | answered 2026-10-18 |
| 2026-10-18 | [The native ownership checker is part of V1](2026-10-18_native_checker_in_v1.md) | in force |
| 2026-10-18 | [A call moves a default-mode aggregate the callee changes](2026-10-18_default_aggregate_move.md) | in force |
| 2026-10-18 | [The flow work cap counts closure entries, not frame nodes](2026-10-18_flow_budget_counts_closure_entries.md) | in force |
| 2026-10-18 | [Copy or move a String the proof route cannot alias?](2026-10-18_open_string_push_landing.md) | answered 2026-10-19 |
| 2026-10-19 | [Is Clone of a struct a deep copy?](2026-10-19_open_struct_clone_depth.md) | answered 2026-10-19 |
| 2026-10-19 | [An unprovable String store lands as an owned copy](2026-10-19_string_landing_copy_then_move.md) | in force |
| 2026-10-19 | [Clone of a struct is a deep copy](2026-10-19_struct_clone_is_deep.md) | in force |

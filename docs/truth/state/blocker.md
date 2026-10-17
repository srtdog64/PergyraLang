# Current blocker

Owner: GPT root lane. As of local main `7dda8a0c2` (2026-10-17).

**The native compiler refuses the unchanged canonical program.** This stops
the codegen bootstrap at its first step, and with it every later CI job.

- First error: `CodegenAstTextTypedArenaFromNodes` at
  `src/self_hosted/hir/ast_text_arena_projection_owner.pgy:185:44`,
  `ArrayPush(atom_table, nodes[i].type_name)` (BORROW). The line did not
  change; the native checker became stricter.
- The other ~18,435 errors are mostly cascades of the first one, not
  independent failures. The count of independent refusals is not known for
  this revision. The last census, at `39a8fc1db`, counted 202.
- Input: the fixed canonical program has 4273 routines and 3188 keys. One
  native run takes about 28 s and 1.37 GB.
- Resolved before this one: the `work4096` budget refusal. A repeated
  finalization went from over 30 s to 1.74 s; the cap was not raised.

Next step, from the GPT card: a read-only chain survey. Its parts are typed
nested field and index publication, flow and caller payload lifetime and
the last retirement, and scratch falsifiers. No production edit is to be
made before that map exists.

Open decision for the user: [decisions/2026-10-17_open_native_checker_scope.md](../decisions/2026-10-17_open_native_checker_scope.md).

Detail: `docs/agent_work_directives/main_takeover_integration_2026-10-16.md`
(frozen evidence; read its first section for the plan).

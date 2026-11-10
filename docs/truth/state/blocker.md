# Current blocker

Owner: GPT root lane. Checked 2026-11-10 KST at local main `60fd1e26d`
plus the preserved dirty candidate in
`/root/pgy-current-all-20261109.gOvxcs` (`Ubuntu-C-WSL`).

## Objective card

- Objective: publish one commit containing the preserved candidate with the
  canonical fixed point, installed C/LLVM driver and required CI green on that
  exact SHA.
- Priority: semantic identity and one ownership fact; fail-closed admission;
  C/LLVM parity; fixed point; installed execution; exact-SHA CI.
- Active slice: `driver.current26.test` passes the fresh-String C/LLVM focused
  artifact, but source-to-MIR rejects the first returned-aggregate `own
  Array<String>` transfer in `CodegenAstTextTypedArenaProjectionReady`.
- Fact owner: the existing collection call argument and descriptor-retention
  owners decide whether the caller descriptor is borrowed, retained or moved.
- Last consumers: the semantic collection verdict, MIR collection state table,
  canonical MIR producer, direct C/LLVM emitters and installed drivers.
- Forbidden fallback: source `Clone` or manual drop, routine/program-shape
  exception, admitting `(owned-literal, unknown, live)`, reduced canonical
  input, skipped backend or weakened negative.
- Falsifier: an `own Array<String>` whose descriptor is returned inside an
  aggregate must retire the caller binding; a later caller use must still be
  refused.

## Exact observed seam

- Minimal input:
  `tests/self_hosted/fixtures/ast_text_arena_projection_ownership_probe.pgy`.
- Failing local: `no_expression_graph_texts`, syntax `5320` in the probe.
- Before the call: ownership `owned-elements` (`2`), disposition `live` (`0`),
  origin `owned-literal` (`10`), transition `owned` (`3`).
- Formal identity is intact: syntax `5240`, inventory row `3`, mode `own` (`2`).
  The argument permission effect is consuming and not unknown.
- The callee returns the descriptor in `AstArena.atom_table`, so the existing
  retention summary correctly marks the definition retained.
- The reached bug is the final application: descriptor retention calls
  `SemanticAstCollectionRemoveCallPermission`, producing
  `(owned-literal, unknown, live)`. The caller binding was moved, so its
  disposition must become retired while element ownership remains attached to
  the moved storage. The MIR state table must remain unchanged.

## Closure order

1. Make the existing retention application distinguish an `own` move from a
   continuing borrow; retire only the moved caller descriptor.
2. Prove the minimal positive and use-after-move negative, rebuild the current
   driver and rerun the full canonical MIR producer.
3. Continue only from the next full-input failure, then prove direct C/LLVM,
   `gen2 == gen3`, installed C/LLVM execution and local required CI.
4. Review the complete diff, remove exact scratch artifacts, commit and push
   main, then observe required remote jobs on that exact SHA.

The task is incomplete until exact-SHA required CI is green.

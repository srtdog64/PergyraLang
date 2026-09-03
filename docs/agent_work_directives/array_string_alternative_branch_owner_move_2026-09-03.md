# ArrayString all-path alternative owner move — 2026-09-03

Status: `ACTIVE — LOCAL EXECUTABLE GREEN; PUBLICATION/EXACT CI PENDING`

Exact base: `f8913fcedf6c5012ccf7edcad996c252ae913955` on
`origin/main`.

This directive coordinates one executable CFG/cleanup rung inside
`abi.mir_array_string_layout_projection`. It does not admit loop moves,
one-sided conditional moves, general ownership flow, or close the registry row.

## Shared objective card

- Objective: allow one named caller-local `Array<String>` to move into the same
  admitted `owner-handle` parameter in both arms of an admitted `if/else`, so
  every reachable normal continuation has retired the local exactly once.
- Priority: path-alternative semantics; exact caller/block/operation/expression/
  local/callable/parameter and ABI identity; all-path proof; target-neutral
  cleanup consumption; negative fallback ratchet; then patch size.
- Fact owner: `DirectMirScalarProgramOwnedArrayStringMoveFact` remains the sole
  digest-sealed owner. Any extension must record enough CFG identity to prove
  alternative rows cover every normal predecessor; row duplication alone is
  not evidence of all-path retirement.
- Last legitimate consumer: the shared ArrayString cleanup policy may suppress
  routine-exit cleanup only for a local whose move fact proves retirement on
  every reachable normal path. C and LLVM may not infer branch ownership or
  maintain independent state.
- Forbidden fallback: branch-OR closure, accepting a move in only one arm,
  suppressing cleanup after any one move row, runtime moved flags, backend-
  local analysis, source/AST/MIR text rescan, callable/local name matching,
  implicit cloning, double free, or a second ownership owner.
- Verification gate: a focused C/LLVM gate must execute true and false calls of
  the all-path fixture with exact output and no caller cleanup, while a
  one-sided move, later use after merge, duplicated same-arm move, missing CFG
  edge, and forged callable/parameter/ABI identity publish no artifact.

## Discovery and edit boundary

- Production entrypoint: current installed Pergyra-built DRV-2 source-to-MIR,
  then direct MIR C/LLVM projection.
- Direct bypass to delete: the current last-use owner rejects every routine with
  more than one block and the move-fact readiness rejects duplicate
  `(caller, local)` rows, even when those rows are mutually exclusive and cover
  all normal alternatives.
- Initial falsifier: produce verified MIR for a Bool-parameterized routine that
  creates one `Array<String>` and calls the owner-handle callee in both `if/else`
  arms. The expected current result is MIR success followed by projection code
  19; if failure occurs earlier, record that exact missing owner instead of
  widening the planned patch.
- Allowed edits after reproduction: the move fact/admission/use owner, existing
  CFG plan facts needed to prove exact alternative coverage, shared cleanup
  policy, bounded fixtures/gate, structural ratchets, and current handoff/
  progress records.
- Out of scope: loops, early return, break/continue, member/formal/literal/fresh
  result moves, unnamed values, general dataflow, syntax/runtime changes, and
  unrelated SoT rows.
- Static owner gates have a 60-second budget and focused parity has a five-
  minute budget. Full CI runs only after the bounded executable slice is green.

## Observed falsifier and candidate

- The positive source produced 16,171-byte verified MIR. Before the change,
  C and LLVM projection both failed with extension code 19 and no artifact.
  The one-sided source produced 14,623-byte verified MIR and failed identically.
- The sole move fact now carries digest-sealed straight/alternative coverage,
  call block, branch block, and merge block rows. A bounded coverage admission
  proves the exact two-arm diamond; final GraphPlan readiness cross-checks the
  sealed rows against CFG and local-use identity. ABI and cleanup queries were
  split out of the fact file without widening its 120-line ceiling.
- Fresh Pergyra-built DRV-2 is 6,575,689 bytes with SHA-256
  `BE96C2C9AAA2412CEAE079D3E9E39CC94053F5A86CCC60F3B6AD997524F6E99E`.
  The positive C/LLVM artifacts compile and execute with exact output
  `branch-released` twice, caller cleanup absent, and callee cleanup present.
- Focused gate is green in 6.3 seconds. It rejects one-sided transfer, later
  use after merge, duplicate same-arm transfer, missing CFG edge, and forged
  carriage/pass/ABI in both backends without publishing an artifact. Existing
  straight entrypoint, straight non-entrypoint, and two-independent-local gates
  are green in a 7.4-second parallel run.
- `tests/self_hosted_component_contract_smoke.sh` is green, but its observed
  run exceeded the declared 60-second static budget because it also invokes an
  executable driver-source MIR action. That gate-shape performance debt does
  not weaken the focused executable verdict and is not repaired in this rung.
- The new Pergyra source/fixture inventory changed only the generated language-
  word implementation counts. The canonical generator rewrote that inventory;
  the 146-row registry check and typed parser-selector parity are green.
- First publication `c12a03d4`, run `33734309099`, reached the full self-host
  native oracle and rejected a local `fact` binding derived from borrowed
  `ref plan`; later TextBuilder diagnostics were cascades. The repair consumes
  `plan.program.owned_array_string_move` directly, as the diagnostic requires.
  Local native oracle compilation now reports 0 errors and the four pre-existing
  warnings. Rebuilt DRV-2 is 6,575,177 bytes, SHA-256
  `7AA5F5371B654438368F235D90F129BD16FE9BC652975764C1804C052F17E0D0`;
  focused and prior owner-move regressions are green. The already-red run was
  cancelled after useful logs were captured; repair exact-head CI is pending.

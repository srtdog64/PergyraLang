# Current Work Handoff

Updated: 2026-07-24 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. Verify it against the
current source, `git status --short --branch`, the SoT registry, the active
owner, and the named executable gate.

## Resume checkpoint

- Current local HEAD is `aa449bd2` (`Close Set literal graph and runtime SoT`).
  `origin/main` was `cb25eb92` when this snapshot was written, so local `main`
  was ahead by two commits: `6574f89f` and `aa449bd2`.
- The Pergyra-built DRV-2 manifest contains 280 MIR fixtures. The latest added
  executable row is `set_literal_basic`.
- `6574f89f` closes the full-matrix `random_inferred_let` blocker. Native MIR
  now carries the routine-owned inferred `Int` fact directly on the
  `event.1` DEF as `abi_type_name`. The shared MIR renderer still rejects a
  missing instruction fact; no `source_locals` compatibility read was added.
- `aa449bd2` closes Set literals through a distinct parser graph spine,
  semantic declared-type owner, MIR graph carriage, the existing Set runtime
  ABI owner, and expected-type C emission. It does not treat Set literals as
  arrays or structs and does not infer the element ABI in codegen.
- The previous indexed Set argument closure remains `23c2f0cb`; its handoff
  refresh is `cb25eb92`.

## Exact remaining dirty state at handoff

The following work was present but was not included in the two commits above:

- modified `Makefile`;
- modified `src/compiler/mir_fact_surface_validate.c`;
- modified `src/compiler/mir_fact_validate_internal.h`;
- modified `src/compiler/mir_json_dump.c`;
- untracked `src/compiler/mir_fact_surface_validate_resource.c`;
- untracked `src/compiler/mir_json_dump_decl.c`;
- untracked `src/compiler/mir_json_dump_decl.h`;
- modified `tests/self_hosted/parity/driver_bootstrap.sh`;
- untracked `docs/198_market_safety_positioning.md`;
- untracked `docs/self_hosted/22_full_matrix_inferred_let_blocker.md`.

The native C files are a concurrent file-split change. Do not discard, stage,
or fold them into a self-host rung without reviewing their owner and gate.
The raw full-matrix blocker note predates `6574f89f`; its observed failure is
resolved, but the untracked file remains user-owned. The driver-bootstrap
runtime-header classifier change is plausible follow-up work but has not been
included in an executable closure commit here.

## Last closed executable rung: Set literal runtime surface

Objective card:

- Objective: carry `{...}` and `{}` from parser-owned syntax identity through
  semantic `Set<T>` compatibility and the existing Set runtime ABI into C.
- Priority: parser identity, ordered element graph, contextual empty literal,
  homogeneous element evidence, runtime symbol ownership, negative ratchet,
  then patch size.
- Fact owner:
  `src/self_hosted/semantic/ast_expression_graph_set_literal_owner.pgy`.
- Parser construction owner:
  `src/self_hosted/parser/expression_set_literal_graph_owner.pgy`.
- Parser contract owner:
  `src/self_hosted/parser/expression_set_literal_contract_owner.pgy`.
- Last legitimate consumer:
  `RewriteSemanticSetLiteralValue` in
  `src/self_hosted/codegen/emission/expr_semantic_composite_literal_emit_owner.pgy`.
- Forbidden fallback: Set-as-array/struct dispatch, source reparse, source
  spelling as ABI, backend element-type guessing, untyped empty-literal
  success, and missing Set ABI success.
- Verification gate:
  `tests/self_hosted/parity/driver_rung2_set_literal_parity_owner.sh`.

Observed closure:

- Parser and MIR carry distinct `set_literal` and `set_element` graph nodes.
- Declared `Set<Int>` and `Set<String>` literals emit the existing owned
  `PgySet_int`/`PgySet_String`, `pgy_set_new_*`, and `pgy_set_add_*` symbols.
- Duplicate insertion remains runtime Set behavior. The positive fixture runs
  as `3`, `true`, `false`, `0`, `2`, `true`.
- Removing the declaration ABI fact fails with `local declaration is missing
  its MIR ABI type fact`.
- A mixed element literal and a dedicated untyped empty literal both fail
  closed with `initializer_type_unresolved`.
- Parser responsibilities were split by owner; the generic expression graph
  owner is 597 lines and the Set graph/contract owners are 30/25 lines.

## Last observed verification

Green:

- Native compiler rebuild: `make -s compiler`, exit 0. Existing warnings were
  observed; no success was inferred from silence.
- Native `random_inferred_let` MIR probe: `event.1` owns
  `abi_type_name: Int` and `source_type: AST_LET_DECL`.
- Focused hard `random_inferred_let` producer-first parity:
  `backends=1 body_fixtures=20 mir_fixtures=1`.
- Existing lexical List instruction-type strip remains fail-closed, proving no
  routine-local compatibility fallback was introduced.
- Official Pergyra parser/gen2/host DRV-2 build and bounded source smoke, with
  manifest count 280.
- Focused hard `set_literal_basic` producer-first parity after the parser owner
  split: `backends=1 body_fixtures=20 mir_fixtures=1`.
- `tests/self_host_hard_contract_smoke.sh`.
- `tests/sot_authority_edge_smoke.sh`: 48 authorities, 40 derived fact
  carriers, `CLOSED=28 BRIDGE=20 ACTIVE=0`.
- `PGY_ALLOW_MISSING_COQ=1 tests/sot_authority_adequacy_smoke.sh`: live
  binding and negative mutations passed. Coq/Rocq itself was a declared skip
  because no prover is installed.
- `git diff --cached --check` before both commits.

Blocked by unrelated dirty work:

- `tests/self_hosted_component_contract_smoke.sh` passes the Set owner and
  parser line-cap checks, then stops because the concurrent native JSON split
  removed `mir_json_emit_decl_generic_params(out, header);` from
  `src/compiler/mir_json_dump.c` without yet updating that component
  assertion. Do not report the full component gate as green.

Not run:

- Full unfiltered 280-row hard DRV-2 matrix.
- LLVM parity for the new Set literal rung.
- Actual Coq/Rocq proof execution.
- Full driver bootstrap/fixpoint for the separate dirty
  `driver_bootstrap.sh` change.

## Next executable work

The next semantic seam is intentionally `Unknown` until executable evidence
selects it. At the next scheduled/merge boundary:

1. Verify HEAD/origin and preserve the dirty native C split and user-owned
   untracked documents.
2. Resolve or isolate the native JSON split component assertion under its own
   owner; do not absorb it into a Pergyra language rung by convenience.
3. Run the unfiltered 280-row hard DRV-2 matrix with the current Pergyra-built
   driver. `random_inferred_let` must remain green. The first observed red row,
   not fixture ordering or an AI guess, chooses the next rung.
4. For that row, record objective, owner, last consumer, forbidden fallback,
   and falsifier before editing.
5. Land another executable Pergyra replacement before spending multiple
   commits on documentation or SoT-only cleanup.

## Workstation and recovery facts

- Global rules: `C:/Users/user/.codex/AGENTS.md`; repository `AGENTS.md` is
  more specific.
- Repository `core.autocrlf=false`. Use
  `C:/msys64/usr/bin/bash.exe` with `/ucrt64/bin:/usr/bin:/bin` for gates and
  Windows `bin/*.exe` artifacts.
- `.vscode` remains configured for `bin/pgy.exe`, `bin/pgy-lsp.exe`, MSYS2
  UCRT64, focused build/self-host/doc tasks, and Extension Host launchers.
- Session-specific inferred-let/Set-literal probe and verification directories
  were sent to the Windows Recycle Bin. Canonical parser/codegen/compiler
  bootstrap caches were retained.
- Never copy configured credentials into source, documentation, or logs.

## Resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, `src/self_hosted/OWNERS.md`,
   and the relevant row in `docs/semantics/sot_owner_spine_registry.md`.
2. Verify `git status --short --branch`, HEAD/origin, the named owner, and the
   focused Set literal gate.
3. Treat the current source/registry/gates as authoritative when this snapshot
   disagrees.
4. Run the full hard matrix only at the scheduled boundary, then follow its
   first observed falsifier.
5. Refresh exact revision, dirty state, last green gate, next falsifier, and
   blockers after the next material session.

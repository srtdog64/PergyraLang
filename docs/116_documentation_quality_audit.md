# Documentation Quality Audit

Last updated: 2026-04-26

Status: beta-closure support note.

This document records the documentation quality issues found while reviewing
the beta docs and the async/concurrency surface. It is not a new language
contract; the source-of-truth contracts remain:

- `docs/107_beta_stable_subset.md`
- `docs/100_beta_readiness_checklist.md`
- `docs/113_memory_concurrency_model.md`
- `docs/114_async_model_positioning.md`
- `docs/05_async_concurrency.md`

## Current Findings

1. The old `docs/INDEX.md` had mojibake and stale paths, so it was no longer a
   trustworthy entry point. It has been rewritten as a UTF-8 beta-closure index.
2. Async docs had the correct thesis, but the sequential-trap example could be
   misread as if `parallel` implicitly creates future values. It now uses named
   `spawn` handles plus explicit `await`.
3. `examples/async_demo.pgy` used a capture-bearing anonymous `async { ... }`
   block. That runtime path exists, but detached local capture lifetime is not
   the beta-stable task-creation model. The example now uses named `spawn`.
4. `RemoteFuture<T>` user-facing docs used `(await pending)?`; the executable
   examples use `let result: Result<T> = await pending;` followed by `Unwrap`.
   The guide now follows the executable pattern.
5. Several older design documents still contain historical alpha-era wording.
   They are useful as design history, but beta-stable claims should be read from
   the 100-series contract docs unless an older doc explicitly links a current
   regression gate.
6. Grammar docs now separate parser-accepted detached `async { ... }` from the
   beta-stable task surface. Named `spawn Worker(args...)` is the stable creation
   form; capture-bearing detached async blocks remain outside the beta contract.
7. `documentation-quality-test-smoke` now scans all `docs/**/*.md` and
   `examples/**/*.pgy` for invalid UTF-8 / replacement characters, and rejects
   anonymous `async { ... }` in executable examples unless the file is explicitly
   marked as a design sketch.
8. `campaign_graph_fsm` exposed an LLVM-only projection freshness drift for
   current-zone subject method calls. The LLVM backend now syncs zone projection
   targets after those calls, and `llvm-campaign-projection-test-smoke` locks the
   exact C/LLVM-visible output for that example.
9. `dnd_tavern_campaign` exposed two LLVM parity risks that are now regression
   gates: MIR `with slot` body fallback could execute a flattened body twice,
   and large zone/class layouts could truncate hidden projection fields in the
   LLVM field registry. `llvm-dnd-campaign-test-smoke` now compares C and LLVM
   stdout exactly, requires one epilogue, requires five choice lines, and checks
   the final `ready=true/true` projection state.

## Async Documentation Position

The beta docs now use this distinction consistently:

- Stable: `parallel`, named `spawn Worker(args...)`, `async func`, checked
  `await`, `Future<T>`, `RemoteFuture<T> -> await -> Result<T>`, `Channel<T>`,
  `select`, copy-only cancellation/status helpers, and pin/view boundary
  diagnostics.
- Explicitly out-of-beta: `spawn async () { ... }`, anonymous async closure
  capture/lifetime analysis, and capture-bearing detached async block stability.
- Rationale: Pergyra is not hiding suspension. It decomposes coloring so
  lifetime, cancellation, fallibility, streaming, and parallel structure do not
  collapse into one overloaded `async` keyword.

## Remaining Documentation Cleanup Priorities

1. Rewrite or supersede older mojibake-heavy files if they remain linked from
   user-facing docs.
2. Extend the documentation smoke gate with targeted stale-reference checks when
   a deleted `.inc` shim or superseded syntax surface appears outside a debt
   ledger / historical design note.
3. Mark historical design docs with a short banner when a 100-series document
   supersedes their beta claims.
4. Keep example docs executable-first: if a code sample is beta-stable, it
   should either appear in `examples/` or be covered by a focused smoke test.
5. Avoid using "experimental" as a dumping ground. If a parser-accepted surface
   is not beta-stable, the docs must name the exact missing closure item:
   semantic rejection, runtime contract, backend parity, diagnostics, or
   regression evidence.

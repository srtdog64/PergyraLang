# Raw/View and Join bridge

Owner: this chat's bounded bridge; main owns integration and publication.
Checked 2026-11-06 KST at HEAD `60fd1e26d` plus preserved dirty work.

Latest executable assessment:
[functional gaps and full observations](../../audits/compiler_functional_gaps_2026-11-06.md).
Pinned native `b25581e17a` and driver13 `c931c8b5f5` were tested independently;
their full hashes, input/runtime ledgers and 116 observations are linked there.
Ordinary String and multiple collection lifetimes still have physical leaks;
the installed paths also retain generic-Array, Join and View support gaps.
SoT registry cleanup is deferred; actual missing type/lifetime facts are not.

The latest native passes focused read/lend and scalar literal cleanup. Its
unchanged full R8 canonical run exceeded the 300 s focused budget, with no
artifact or final diagnostic array. Current whole-input error count is unknown;
the old 207 must not be reused as this binary's result. Full driver fixedpoint,
installed whole-corpus parity and same-published-commit CI are unestablished.

Earlier bounded evidence remains in the
[Join replacement run record](../evidence/2026-11-06_join_shadow_replacement_cleanup_and_fixedpoint.md),
[raw/View native proof](../evidence/2026-11-05_raw_view_memory_bridge_compiler_runtime.md)
and [fixed-R8 207 diagnostic audit](../../audits/native_driver_207_errors_2026-11-06.md).
Their scoped successes do not close general value cleanup.

Next falsifiers: no-drop String/local/factory-container sanitizer cases and
native-versus-installed Array/View paths in the latest audit, followed by the
unchanged canonical input and whole-driver C/LLVM/fixedpoint/CI integration.
Main owns production implementation, integration and publication. This lane
changed audit data/documents only and preserved other main work.

No staging, commit or push was performed by this lane.

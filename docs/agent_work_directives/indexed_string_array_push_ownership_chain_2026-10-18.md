# Indexed String into ArrayPush: canonical ownership chain

Status: PLAN / read-only owner survey. Current blocker and gate results are in
[truth/state](../truth/state/blocker.md). This card fixes the reached chain
and acceptance boundary; it is not a new semantic owner or a permission to
change the canonical program.

## Objective card

- Objective: admit the unchanged `SemanticSourceDiagnosticCallCodes` program,
  including `ArrayPush(codes, found[j])`, with compiler-owned String lifetime
  and cleanup on C and LLVM. Preserve all borrowed-source refusals.
- Priority: exact source/owner identity, one typed decision, fail-closed
  emission and cleanup, negative behavior, then copy cost and patch size.
- Entrypoints: native `--mir-json` canonical admission first; the same MIR
  then reaches installed self-host C/LLVM and bootstrap/gen2==gen3/CI.
- Source producer: `SemanticDiagnosticCallCodesFromContent(ReadFile(path))`
  returns `found: Array<String>`; `found[j]` enters the local `codes` array in
  a loop. The current first refusal is `PGY_SEM_BORROW_ESCAPE` at
  `src/self_hosted/semantic/diagnostic_owner.pgy:573:39`.
- Current fact chain: `type_checker_builtins_stdlib_array.c` calls
  `semantic_indexed_string_borrow_call_argument`; the indexed String owner,
  array element lifetime owner and collection ownership fact/result owner
  determine source and mutation. The immutable collection effect snapshot
  carries the issued fact into MIR validation and backend emission.
- Last consumers: C `transpiler_expr_stdlib_builtin.c`, LLVM
  `llvm_expr_array_calls.c`, their runtime Array<String> insert and release
  ABI, and the caller/return cleanup boundary. The current ordinary
  `pgy_array_push_String` stores its pointer; explicit
  `pgy_array_push_owned_String` duplicates it. Existing deep-drop is not yet
  proof of automatic cleanup for this whole chain.
- Forbidden fallback: user source rewrite to `ArrayPushOwnedString`, manual
  deep-drop, accepting an indexed borrowed pointer by source name, shallow
  alias despite a proved deep release, C/LLVM-specific ownership decisions,
  new syntax/annotation, old/new dual-read or smaller canonical input.

## Complete obligation set before editing

1. Determine the exact producer fact for the returned `found` elements and
   its live storage/cleanup authority across the declared call and loop.
   Identify whether the indexed value is borrowable, copyable or movable,
   and which later uses rule out a move. Do not infer ownership from `Array`
   type alone.
2. Select one compiler-owned operation contract for this site. If copying,
   issue a typed, current copy receipt and one receiver element-ownership
   transition; if moving, prove source-slot invalidation and no later use.
   The existing array-return and collection owners retain their identities.
3. Carry the decision once through the existing immutable effect/source
   snapshot and MIR/ABI row. Missing, stale, aliased, mixed or duplicated
   evidence must refuse before C/LLVM output or mutation. Remove any old
   interpretation that could independently choose shallow insertion.
4. Make C and LLVM implement the same selected insert postcondition and
   recoverable-failure behavior. Emit automatic cleanup for all owned
   elements and backing storage on normal/early/error exits, including
   `found` and returned `codes`; no double free or leak under reuse/return.
5. Check the installed self-host general MIR route against the same row.
   A native-only green result is a prerequisite, not substitution. No new
   `direct_mir_*` shape route.

## Falsifiers and shared gate

- Positive: a minimal source-derived Array<String> indexed loop and the
  unchanged canonical program; C/LLVM outputs and ownership effects agree.
- Negative: borrowed formal elements, mixed static/heap elements, duplicate
  or later index use, alias/rebind/retire of source or receiver, changed
  source/ABI row, missing fact, and a failed copy/allocation. Each refuses
  without partial output or cleans constructed state exactly once.
- Run the current semantic/phase/MIR/AIR and strict C/LLVM gates, focused
  ASan+UBSan with leak detection for the new operation, then full canonical,
  bootstrap, installed driver and required exact-SHA CI. Document actual
  memory/time on the unchanged input; do not relax budgets.

## Bounded read-only surveys

- A: semantic source/array-return/indexed/collection owner and every reached
  fact consumer; determine copy/move feasibility and missing current fact.
- B: C/LLVM runtime ABI and cleanup, including normal/early/error exits and
  backend divergences. Report the exact last consumer and candidate contract.
- C: production canonical, current self-host route and executable positive/
  negative fixtures; identify the smallest falsifier and integration gate.
- Root: reconcile A/B/C into one edit map with disjoint implementation
  scopes. No production edit or parallel implementation begins while the
  copy/move and cleanup unknowns could change the ownership decision.

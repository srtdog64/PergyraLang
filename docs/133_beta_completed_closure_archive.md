# Beta Completed Closure Archive

Status: completed-evidence archive.

This document holds completed beta-closure evidence that no longer needs to
inflate the active execution checklist. The active checklist remains
`docs/100_beta_readiness_checklist.md`; the source-of-truth ownership contract
remains `docs/125_source_of_truth_spine.md`.

## 2026-05-29 Backend Parity Refresh

Local MinGW/Git Bash `tests/compare_backends.sh` passed the frozen
backend-compare suite at `181/181` after promoting `select_fairness`,
registering `if_else_chain`, adding the `device_slot_remote` C/LLVM path, and
registering loop-defer `continue` cleanup parity.

The runtime-panic follow-up rechecked `device_slot_remote`, `select_fairness`,
and `try_operator_result` at `3/3`. `runtime-panic-codegen-test-smoke` covers
`?` on `Err` in a non-`Result`-returning function for both C and LLVM.
`backend-compare-llvm-coverage-test-smoke` keeps non-experimental
`llvm_smoke.sh` cases from staying LLVM-only; the only current allowlisted
LLVM-only surface is `qubit_slot`, which remains out-of-beta.

This is strong parity evidence for the current frozen subset. It does not close
the remaining 80% blockers by itself: CFG/AIR consumer completeness,
declaration bootstrap shape, and ABI/Slot/Pin freeze remain open.

## 2026-05-29 Executable Wrapper And Select Tightening

Executable wrappers now fail closed when MIR entrypoint inventory and emitted
declarations disagree. LLVM resolves user `Main` through
`llvm_lookup_function(...)` and must not synthesize it with
`lookup_or_declare_function(ctx, "Main", ...)`. C and LLVM both reject a MIR
claim for user `Main` or `__pgy_top_level_exec` when the matching registered
function/source wrapper is missing. LLVM also no longer treats a registered
`Main` symbol as sufficient to create an executable wrapper unless MIR set
`has_main_function`. `perf-contract-test-smoke` gates this shape while keeping
generated C `main` / LLVM `main` wrapper creation explicit.

C MIR select readiness rendering now sends identifier channels through the
regular expression path with SSA disabled. This keeps implicit field and
captured channel lvalues in the correct C shape while select readiness avoids
reading an uninitialized SSA shadow. `test-transpile` covers this with `858/0`.

## 2026-05-29 Channel Constructor And Type-Family Classifier Closure

Semantic constructor validation plus C/LLVM constructor guards fail-close
`Channel<T>` field initialization inside class and domain-host aggregate
constructors. Channel runtime storage currently carries mutex/condvar state, so
constructor fields must wait for movable channel-handle lowering instead of
copying channel storage by value or default-zeroing channel runtime state.

C constructor lowering consumes the shared
`transpiler_constructor_channel_guard` owner so class/domain emitters do not
drift into separate channel-field policies. C channel-kind checks route through
`transpiler_type_name_is_channel(...)`, while LLVM constructor and receive
inference use `pgy_classify_type(...) == PGY_TK_CHANNEL`. The same C classifier
seam covers `Future<T>`, `RemoteFuture<T>`, `Result<T,E>`, `Option<T>`, and
common collection spelling for match destructuring, try-let lowering, Option
context, array access, for-in lowering, MIR for-in/destructuring type lookup,
`BoxArray` let lowering, channel type queries, and collection builtin
inference. `perf-contract-test-smoke` rejects new C `transpiler_*.c` direct
type-family prefix checks outside the classifier owner.

LLVM channel send/receive/select resolves both registered local channels and
current-host `Channel<T>` fields through the shared `LLVMChannelTarget` owner.
Task/channel builtins consume the same target seam for `TrySend`, `TryRecv`,
`ChannelReady`, `ChannelLength`, `ChannelClose`, and related query calls.
Field-channel receive type inference also consumes the target owner for the
`Channel<T>` inner type, so `let value = <-ch` does not fall back to poison
`i32` when `ch` is a current-host field.

## 2026-05-29 Hosted Declaration Compatibility Closure

Party, role, and roster host declarations are part of the shared host
compatibility type set, not ad-hoc fallback cases.
`llvm_find_host_decl_in_active_inventory(...)` iterates
`pgy_host_decl_compat_types(...)`, and `mir-declaration-inventory-test-smoke`
gates `AST_PARTY_DECL`, `AST_ROLE_DECL`, and `AST_ROSTER_DECL` in that set.
C/LLVM host method lookup must not regress to a partial class/enum/domain-only
chain.

`src/codegen/host_decl_compat.c` owns the class/enum/party/roster/role/world/
relation/effect/zone type set, host declaration-name accessor, pointer-self
host policy, C nominal-host lookup order, known-nominal forwarding,
compatibility method view, shared-field compatibility view, and name-based
field lookup helpers.

The C/LLVM constructor Channel guards, LLVM current-host Channel target
resolution, LLVM current field class lookup, C nominal/overlay current-field
helpers, and C/LLVM projection-path helpers consume those owner seams instead
of reopening class/shared field arrays locally. This is not a full
declaration-field metadata model yet; it removes duplicated backend field
traversal families that were safe to close without introducing the dedicated
declaration-field IR.

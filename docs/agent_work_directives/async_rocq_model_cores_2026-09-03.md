# Async Rocq model cores — 2026-09-03

Status: `ACTIVE — ROCQ 9 KERNEL GREEN; FINAL EXACT-HEAD MATRIX PENDING`

Exact base: `eed7f7229699770bcda656e7a2947a5f043cbfcc` on `origin/main`.

This directive coordinates one bounded formalization of the already-landed
structured-spawn and task-context contracts. It does not make Rocq the owner of
compiler or runtime semantics and does not claim whole-language async safety.

## Shared objective card

- Objective: machine-check the current beta async invariants that (a) a live
  named Future can reach an admissible scope exit only through `await` or an
  explicit `own Future` transfer, while suspend and Cancel do not discharge the
  obligation, and (b) spawn capture, lane execution, and suspend/resume preserve
  the parent's exact capability masks, budget owner, and instance identity.
- Priority: preserve the separation between suspension, lifecycle, and
  authority; prove trace-level containment and fail-closed CFG merge behavior;
  prove exact context carriage and restoration; bind both models to their live
  owners; retain the kernel axiom budget; then minimize the patch.
- Fact owners: `src/semantic/type_checker_future_lifecycle.c` and the Future
  state in `src/semantic/symbol_table.h` own lifecycle;
  `src/runtime/pgy_runtime_context.h` owns task-context capture. The Rocq files
  are bounded models of those facts, not replacement authorities.
- Last legitimate consumers: semantic scope/function-exit admission and flow
  merge consume lifecycle; task execution boundaries consume captured runtime
  context. `ProofSpine.v` may connect the checked models but may not widen the
  implementation claim.
- Forbidden fallback: treating `async` as a lifetime owner, Cancel as join or
  cleanup, branch-OR closure, executor-default capability reads, fresh child
  budget state, AIR-to-backend lowering, a hidden finalizer, a new surface
  keyword, `Axiom`, `Admitted`, or a claim of termination, fairness, full C11
  memory safety, detached-capture safety, or whole-language verification.
- Verification gates: `make async-model-adequacy-test-smoke` binds theorem names
  and modeled transitions to the live owners; `bash tests/coq_kernel_check.sh`
  compiles and kernel-checks the full proof corpus while retaining exactly the
  existing two `SlotCalculus` interface assumptions. The official Rocq 9 CI job
  is required because no local prover is installed.

## Edit scope and falsifiers

- Allowed edits: two responsibility-named proof cores, one companion boundary
  document, one static adequacy gate, their Make/formal-smoke/proof-spine
  registration, canonical concurrency/proof indexes, and current
  progress/collaboration/handoff records.
- Falsifying cases: a live trace that closes without await/transfer; Cancel or
  suspend retiring a handle; an alternative-path mismatch admitted as closed;
  a second await/transfer from Retired; child capture changing either capability
  mask, budget owner, or instance identity; lane resume mutating the captured
  context; or task return failing to restore the surrounding context.
- Out of scope: compiler/runtime behavior changes, anonymous capture analysis,
  detach, scheduler fairness or termination, atomics/happens-before, checked
  slot revalidation after resume, ConcurrencyPlan lowering, and other SoT rows.
- The primary task owns all edits, integration, commit/push, and exact-head CI.
  No parallel implementation track is open.

## Local candidate

- `AsyncLifecycleCore.v` defines the exact four lifecycle states, five relevant
  events, a trace relation, alternative merge, and simultaneous-parallel merge.
  Twelve named theorems/examples cover the required positive and fail-closed
  cases without assumptions.
- `AsyncContextCore.v` defines exact parent capture, all six canonical lanes,
  yield/await resume, and task-boundary restoration. Eight named
  theorems/examples cover mask/budget/instance preservation and the forbidden
  executor-default counterexample without assumptions.
- `async_model_adequacy_smoke.sh`, proof-spine static contract,
  async-positioning, and documentation-quality gates pass. The local
  proof-spine run is an explicit `PGY_ALLOW_MISSING_COQ=1` skip because no
  prover is installed; it is not counted as a proof result.
- A read-only Rocq 9 Docker attempt could not start because the Docker daemon is
  absent. Therefore exact-head `formal-proofs-rocq9` compile, `rocqchk`, and
  axiom-budget evidence remain required before this lease can close.
- Commit `6feded443f5b5007eddee9fc60b984e518ed6252` reached
  `origin/main`. Exact run `33724537105` failed closed in Rocq 9 at
  `retirement_requires_await_or_transfer`: an automation-only proof left the
  `Suspend` equality case open. The repair replaces that tactic with five
  explicit transition cases.
- Repair `b5da5329` reached `origin/main`. Run `33724801136` then passed the
  Rocq 9 compile, `rocqchk`, and live axiom-budget negative self-test; its full
  matrix was cancelled only when the shared branch advanced.
- Concurrent follow-up `0a11b4b2` added four bounded direction cores for the
  scope tree, share/lend/move capability flow, suspension revalidation, and the
  deterministic subset. Run `33725481715` kernel-verified all 48 proof files
  with exactly the existing two `SlotCalculus` abstractions, no admits, and no
  unsafe kernel features. These four are direction models, not claims that the
  corresponding compiler/runtime rungs have landed. Their stale prose about
  current scope/context behavior is corrected and guarded by
  `async-direction-adequacy-test-smoke`; a final exact-head matrix remains.

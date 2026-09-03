# Async model cores

Companion to `AsyncLifecycleCore.v` and `AsyncContextCore.v`.

These files formalize two already-landed beta contracts without assigning both
responsibilities to `async`. `async` remains the suspension/coroutine marker.
The affine Future flow owns structured lifetime, and the runtime context owns
task authority carriage.

## Lifecycle core

`AsyncLifecycleCore.v` models the compiler's four Future states and five
relevant events. It proves:

- suspend preserves lifecycle and cannot discharge a live obligation;
- Cancel is request-only and cannot make scope exit admissible;
- await and explicit `own Future` transfer consume one live handle;
- a retired handle cannot be consumed a second time;
- every trace from `Live` to a scope-closed state contains await or transfer;
- disagreeing alternative CFG paths fail closed as `Diverged`;
- alternative-path and simultaneous-parallel merge are deliberately distinct.

The trace theorem is the bounded structured-task containment result. It covers
named `Future<T>` and `RemoteFuture<T>` bindings admitted by the current
semantic checker. It does not cover anonymous capture-bearing async blocks,
detach, or hidden runtime finalization.

## Context core

`AsyncContextCore.v` models parent task capture, the six execution lanes,
coroutine yield/await resume, and execution-boundary restoration. It proves:

- both capability masks are copied exactly, so capture cannot widen them;
- the child shares the exact parent budget owner and instance identity;
- lane selection and suspension/resume preserve the captured context;
- task return restores the surrounding context;
- reading an executor-default context instead is a concrete counterexample that
  can change capability and quantitative-authority identity.

## Evidence boundary

These are model theorems, not implementation verification. The live owners are
`src/semantic/type_checker_future_lifecycle.c`, the Future state carrier and
flow merge, and `src/runtime/pgy_runtime_context.h`. The static
`tests/async_model_adequacy_smoke.sh` gate binds every modeled decision back to
those owners. Existing structured-spawn and runtime-context execution gates
provide positive and fail-closed runtime evidence.

Neither core proves termination, scheduler fairness, C11 happens-before,
general data-race freedom, checked Slot revalidation after resume, detached
capture safety, or whole-language verification. Both add zero assumptions; the
proof corpus keeps only the two declared `SlotCalculus` interface abstractions.

Re-check with:

```sh
make async-model-adequacy-test-smoke
bash tests/coq_kernel_check.sh
```

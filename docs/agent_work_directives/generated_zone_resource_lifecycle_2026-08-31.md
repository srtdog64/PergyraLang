# Generated Zone Resource Lifecycle — 2026-08-31

Status: `IMPLEMENTATION COMPLETE — LOCAL GREEN; PUBLICATION PENDING`

Exact base: `d8debe30c169d98411d57d4e66af7f2506b7970e`, equal to
`origin/main` when this rung opened.

This directive coordinates one reached production resource-lifetime gap. It
does not add a source-level lifetime, make `zone` lexically predict a world's
lifetime, or promote the broader `selfhost.semantic_artifact_admission`
authority.

## Objective card

- Objective: make the installed source-to-C path initialize and destroy the
  hidden thread-safe lock owned by each freshly constructed local `zone`, while
  preserving the language's absence of a fixed world/domain lifetime model.
- Priority order: preserve admitted zone identity; bind the hidden C resource
  to the local lexical scope; run cleanup on normal exit and return/control-flow
  escape; fail closed for copies, reassignment, by-value parameter/return, or
  embedded carriage without an admitted transfer plan; ratchet manual harness
  lifecycle out of the production witness; then preserve single-threaded
  behavior.
- Production entrypoint: installed `pgy` source compilation through
  `pgy-self-driver --emit-c-artifact-verified SOURCE OUTPUT`, followed by the
  host C compiler with `PGY_ZONE_THREADSAFE`.
- Fact owners: `SemanticAstLocalBindingFacts` owns the local declaration and
  admitted type; `SemanticAstNominalConstructorFacts` owns nominal kind and
  constructor identity; the expression graph's resolved call-target identity
  proves a fresh direct constructor. `pgy_runtime_zone_sync_abi.h` remains the
  shared lock/generation ABI owner.
- Last legitimate consumers: `EmitStmtList` owns lexical statement scope and
  escape cleanup ordering; `EmitFunctionWithSpecialization` owns the function
  exit boundary. Neither consumer may reconstruct zone identity from a type
  suffix or source text.
- Direct bypass to delete: emitting a zone compound literal directly into a
  local and calling its generated `*_sync(self)` without any
  `PGY_ZONE_LOCK_INIT` or `PGY_ZONE_LOCK_DESTROY`.
- Forbidden fallback: relying on zero-filled `pthread_rwlock_t`; requiring a C
  harness to initialize generated values; treating single-threaded success as
  thread-safe evidence; silently copying or reinitializing a named zone lock;
  adding a user-visible fixed lifetime; or accepting a missing/stale constructor
  identity.
- Verification gate: extend
  `tests/self_hosted/parity/domain_runtime_zone_sync_execution_owner.sh` with a
  real source local-zone constructor and method call. Generated C must contain
  one init before first sync and one destroy on normal exit, compile and print
  the same result in single/thread-safe modes, and reject non-fresh local zone
  carriage until a separate transfer owner exists. The exact baseline
  falsifier prints `7` single-threaded and panics with
  `reason=zone write lock failed` when `PGY_ZONE_THREADSAFE` is enabled.

## Scope and budget

- Allowed edits: lexical statement/function emission, the existing domain
  runtime zone focused gate and fixtures, component ratchets, this directive,
  the ACTIVE registry note/witness, and current collaboration/handoff docs.
- Independent edit scopes: none. The primary task is the sole implementation
  and integration owner; no parallel task may edit this executable rung.
- Forbidden overlap: no native C/LLVM redesign, general ownership calculus,
  nonzero DIR admission, query/cache architecture, or unrelated SoT row may be
  folded into this lease.
- Out of scope: domain lifecycle syntax, nonzero DIR admission, native C/LLVM
  redesign, a general ownership calculus, world-embedded zone transfer,
  thread-pool policy, a cache, or another registry authority.
- Allowed commands and validation budget: static owner/line-cap checks first;
  focused parity within five minutes; one current-source full-driver emission
  as the bounded integration build; broader matrix work only in exact CI after
  publication. Do not repeat the full driver emission for fixture expansion.
- Integration owner and gate: the primary task owns the final diff and exact
  candidate. `domain_runtime_zone_sync_execution_owner.sh` is the focused
  falsifier; current-source driver construction plus exact CI is the
  integration boundary.

## Local implementation evidence

- `EmitStmtList` now returns body plus reverse lexical cleanup. A fresh direct
  zone constructor is proved from the admitted nominal kind and resolved direct
  call target, then receives generated `PGY_ZONE_LOCK_INIT` and
  `PGY_ZONE_LOCK_DESTROY` across normal, return, break, and continue exits.
- Named copy and reassignment preserve current single-threaded value behavior
  but emit exact thread-safe compile failures. By-value parameter/return and
  embedded zone carriage have the same fail-closed boundary until a separate
  transfer plan exists.
- The focused gate executes `7 / 11 / 13 / 17 / 19` identically in
  single-threaded and thread-safe C and observes the copy/reassignment
  negatives. A current-source production candidate repeats the lifecycle and
  both negative results through `--emit-c-artifact-verified`.
- Component inventory, hard substitution contract, build-source inventory,
  documentation quality, agent boundary sentinel, and SoT authority edge are
  green. The edge census remains 88/183 with
  `CLOSED=55 BRIDGE=32 ACTIVE=1`.
- The current-source full driver C is 164,246 lines with SHA-256
  `9e89012dac9491a5732f2e82a73f12740da4c82a6a134cae3641157b0d246e10`;
  its O0 candidate SHA-256 is
  `510ae50426c4f2e26214824d9cb5948bf03741971a34b341a80d3ba96dc3f46a`.
  O1 self-codegen emission took 492.01 seconds while host C compilation took
  16.46 seconds. This is bounded performance evidence and does not justify a
  cache or a new query architecture.

## Output classification

The output is an implementation candidate for one hidden C resource boundary.
It does not change 88 authorities / 183 carriers / `CLOSED=55 BRIDGE=32 ACTIVE=1`
or the 83% project forecast. A green manual harness is supporting ABI evidence,
not generated-lifecycle evidence. Publication and exact CI remain pending.

# 06. Backend Parity Proof Obligations

Last updated: 2026-05-29

Status: `IN PROGRESS / BLOCKER`

Surfaces: MIR, declaration inventory, C backend, LLVM backend, runtime ABI.

## Stable Surface

- MIR is the semantic contract for backend emission.
- C and LLVM must agree for the frozen subset.
- Unsupported backend cases must fail with structured backend errors, not partial output or silent fallback.
- Declaration inventory must not reintroduce AST/HIR truth drift for stable backend behavior.

Out of beta:

- Full backend optimization proof.
- Target-specific ABI proof for every OS/toolchain combination.
- Windows LLVM parity unless runner support is explicitly green and documented.

## Judgments

```text
P => HIR => DIR => RIR => MIR
MIRState |- declaration_inventory ok
MIRState |- emit C
MIRState |- emit LLVM
C ~= LLVM under observable beta behavior
```

## Theorem: MIR Source-of-Truth

For stable beta backend behavior, C and LLVM emission must be driven by MIR-level routine and declaration facts or fail with a structured backend error.

Assumptions:

- AST/HIR can exist as internal representation, but not as user-visible backend truth for frozen behavior.
- Missing MIR declarations do not produce partial C/LLVM output.

Current evidence:

- Routine body paths are mostly MIR-only.
- Domain method MIR-missing paths fail as explicit backend errors.
- Backend compare is green for the current frozen suite.

Remaining proof obligation:

- Finish declaration inventory bootstrap cleanup for zone/world/relation/effect metadata.

## Theorem: Backend Observational Equivalence

For every accepted beta program, C and LLVM emitted from the same MIR contract have equivalent observable behavior.

Observed behavior includes:

- stdout/stderr contract.
- exit state.
- runtime observability state.
- recoverable failure state.
- hard-fail class.
- runtime panic class.

Current evidence:

- Linux `llvm-test-backend-compare`, `llvm-test-smoke`, and ABI same-process tests are green for the current frozen suite.
- `backend-compare-llvm-coverage-test-smoke` prevents non-experimental
  `llvm_smoke.sh` cases from staying LLVM-only; the only current allowlisted
  LLVM-only surface is `qubit_slot`, which remains out-of-beta.
- `runtime-panic-codegen-test-smoke` executes C and LLVM panic boundaries for
  divide-by-zero, collection/slice out-of-bounds, direct unwrap failure, and
  `?` failure in a non-Result-returning function.
- `runtime-frontier-contract-test-smoke` keeps LLVM frontier overflow on the
  registered internal-invariant panic export instead of synthesizing an
  undeclared runtime function or falling back to raw abort.

Remaining proof obligation:

- Keep Windows support wording honest: official beta support is Linux C+LLVM and Windows C-only unless Windows LLVM runner parity is actually green.

## Theorem: Runtime Panic Parity

For every accepted beta program that reaches a runtime panic boundary, C and LLVM must fail in the same stable panic class without silently returning a value or falling back to a different behavior.

Stable panic classes:

- OOM
- divide-by-zero
- array/slice/list/map out-of-bounds
- released slot use
- double release
- invalid-secure-token
- authority-mismatch
- internal compiler/runtime invariant break

Policy:

- Recoverable runtime contract failures expose `Bool`, `Result<T>`, or queryable runtime state when the language surface says they are recoverable.
- Ownership/security boundary violations are hard-fail unless explicitly modeled as recoverable.
- Internal compiler/runtime invariant breaks are always hard-fail.
- Unsupported backend behavior must be a structured backend error before runtime, not a silent panic mismatch.

Current evidence:

- Runtime authority failure vocabulary is shared between generated C and LLVM runtime exports.
- ABI smoke and backend compare already cover authority failure snapshots and multiple propagation-frontier cases.
- `runtime-panic-contract-test-smoke` gates the shared panic class/reason vocabulary and prevents released-slot, secure-slot, device-slot, authority, OOM, out-of-bounds, and checked-arithmetic paths from drifting back to silent fallback.
- `runtime-panic-abi-test-smoke` executes inline-runtime and exported-runtime harnesses for released slot use, invalid secure-slot token, double release, device slot release violations, authority mismatch, OOM, out-of-bounds, and divide-by-zero.
- `runtime-panic-codegen-test-smoke` verifies generated C and LLVM programs lower integer divide/modulo by zero to the same `divide-by-zero` panic class and stable `Array<T>`/`Slice<T>` indexing, including temporary function-return access, plus `ArraySet`, `ListGet`, `QueuePop`, `MapGet`, `ListSet`, `ListRemove`, and `MapRemove` invalid access to the same `out-of-bounds` panic class.
- The same generated-code smoke verifies `Unwrap(Err)`, `Fail()?` in a `Void`
  function, and `UnwrapOption(None)` panic with the same
  `internal-invariant` class in C and LLVM.

Remaining proof obligation:

- Extend collection policy to any new beta-stable collection API before exposing it: absence must be either an explicit query/fallible result surface or a hard-fail boundary.

## Theorem: Structured Backend Failure

If stable syntax reaches an unsupported backend path, the compiler reports a structured backend error rather than silently emitting invalid code.

Current evidence:

- LLVM stmt/expr fallback is no longer treated as harmless warning-only behavior.
- AST dispatch partition smoke checks unsafe fallback categories.
- C and LLVM aggregate constructor lowering fail-close `Channel<T>` field
  initialization from inline `Channel(...)`, because channel initialization is
  statement-level runtime setup and cannot be embedded as a stable aggregate
  expression. The stable path is a named channel binding before aggregate
  construction.
- LLVM channel send/receive/select resolves local channels and current-host
  `Channel<T>` fields through the same `LLVMChannelTarget` pointer +
  inner-type fact. This keeps implicit field-channel operations aligned with
  the C backend's `self.ch` lvalue path instead of reopening local-only channel
  lookup in each consumer.

Remaining proof obligation:

- Continue reducing helper-heavy edge paths and declaration-side fallback inventory.

# 06. Backend Parity Proof Obligations

Last updated: 2026-04-25

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

Current evidence:

- Linux `llvm-test-backend-compare`, `llvm-test-smoke`, and ABI same-process tests are green for the current frozen suite.

Remaining proof obligation:

- Keep Windows support wording honest: official beta support is Linux C+LLVM and Windows C-only unless Windows LLVM runner parity is actually green.

## Theorem: Structured Backend Failure

If stable syntax reaches an unsupported backend path, the compiler reports a structured backend error rather than silently emitting invalid code.

Current evidence:

- LLVM stmt/expr fallback is no longer treated as harmless warning-only behavior.
- AST dispatch partition smoke checks unsafe fallback categories.

Remaining proof obligation:

- Continue reducing helper-heavy edge paths and declaration-side fallback inventory.

# Agent Entry Contract

This document is for future agents working on self-hosting.

## Non-Negotiable Rules

1. Do not start a full compiler rewrite before beta closure.
2. Do not claim Pergyra is self-hosted until a released compiler is built by Pergyra code.
3. Keep the C compiler as the oracle during soft and partial self-hosting.
4. Every self-hosted component must have an intent-verification pair: a named intent plus tests/contracts that verify it.
5. Prefer stable file/IR inputs over direct compiler internals for first-stage
   tools. Use JSON when the owner format is JSON; use diagnostic blocks for
   diagnostic verdicts.
6. Do not add unnecessary helper functions. A helper must name a real owner
   responsibility, isolate a repeated contract, or remove a source-of-truth
   seam; a one-off wrapper that only hides local logic is an anti-pattern.
7. Do not add empty `try` / `catch` blocks or silent catch-all handlers.
   Recover with an explicit `Result` / diagnostic path, propagate the failure,
   or document the invariant at the call site.

## Current Truth

The implementation is C with C and LLVM backends. Pergyra can dogfood through
small tools after beta, but the compiler core is not self-hosted.

## Agent Work Unit

The smallest acceptable unit is:

- one tool or pass,
- one explicit input contract,
- one explicit output contract,
- one smoke test,
- one parity check against the C implementation.

Do not migrate broad compiler subsystems as one task.

## Good First Agent Tasks

- Diagnostic catalog checker.
- AIR graph JSON validator.
- MIR dump diff tool.
- C/LLVM backend output comparator.
- Module/package manifest resolver helper.

## Forbidden First Tasks

- Rewriting the parser.
- Rewriting the type checker.
- Rewriting C or LLVM backend emission.
- Adding new syntax only to make self-hosting easier.
- Depending on native WASM, quantum, or Rust-style lifetime annotations.

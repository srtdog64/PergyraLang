# Backend Parity Fuzz Corpus Generator

## Intent

Generate deterministic Pergyra source corpora from Pergyra itself, so backend
fuzzing is no longer sourced only from an external Python script.

This is not a compiler-core replacement. It is a soft self-host test generator:
the generated programs are used to stress C/LLVM parity seams, while the oracle
compiler remains the C implementation during beta closure.

## Contract

- The same seed and count must produce byte-identical corpus files when the
  generator is compiled through the C backend and the LLVM backend.
- Generated programs must be deterministic and must not depend on runtime
  randomness.
- The generator may use `Random` only while constructing source text after an
  explicit `SeedRandom(seed)`.
- The generated subset intentionally covers local reassignment, `let mut`,
  `if`, `while`, `for`, helper calls, boolean logic, arrays, array
  parameter/return flow, structs, plain slots, substring windows, and string
  output.
- Failure is observable through a non-zero exit and a diagnostic line.

## Exclusions

This tool does not minimize failing cases and does not replace the existing
backend corpus. Minimization and long-running random campaigns remain
post-beta fuzzing work until a seed corpus, crash triage policy, and proof-pack
mapping are frozen.

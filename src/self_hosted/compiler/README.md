# Compiler Driver Substitution Track

Reserved for a Pergyra-written compiler driver — the top-level CLI that
wires lexer, parser, semantic, codegen, and runtime together. This
mirrors the C-side `src/compiler/` (entry point + flags + pipeline
orchestration), not the entire compiler.

Until the individual stages substitute the C ones, the driver here is
intentionally empty; substituting the driver before the stages it would
drive makes no sense.

# ArrayString owner-handle caller move retirement result — 2026-08-29

Status: `LOCAL GREEN — PUBLICATION PENDING`

Exact base: `ca2555e1e898f3ac2f0472e76d616dfba22e0410` on
`origin/main`.

This audit records evidence for one reached executable ownership prerequisite.
It does not own compiler semantics, registry status, or a successor rung.

## Result

`DirectMirScalarProgramOwnedArrayStringMoveFact` now owns one bounded transfer:
an entrypoint local is the last-use argument to exactly one owner-handle
`Array<String>` direct-call parameter. Its identity includes caller routine,
operation, expression, local, callable, parameter, and admitted ABI layout.

The program admission owner derives that fact once. C and LLVM cleanup consume
it through `DirectMirScalarProgramArrayStringLocalCleanupRequired`; neither
backend scans source or reconstructs move policy. The callee retains terminal
ownership and performs the owned drop.

The pre-change executable probe ended in Windows heap corruption
`-1073740940` because callee and caller both dropped the same storage. The
current positive C and LLVM artifacts print `released`, exit zero, contain the
callee drop, and omit caller-local cleanup.

## Falsifying evidence

- A use-after-move source reaches typed program admission but both C and LLVM
  projections reject it with extension readiness code 19 and publish no
  artifact.
- Carriage, pass-shape, ABI-layout, and call-target mutations also fail closed
  for both targets without artifacts.
- The component contract fixes the fact/admission/use owners, program-extension
  carriage, shared cleanup policy, backend consumers, exact fixtures, mutation
  calls, and aggregate Make target. Existing owner caps were preserved.
- Admission is deliberately bounded to one two-routine program, one-block
  entrypoint, one owner-handle candidate, a local argument, and no later local
  use. Multiple or conditional moves and fresh-result/literal moves remain
  rejected rather than guessed.

## Observed local gates

- final-source Pergyra-built DRV-2 generation: PASS
- native production `driver_bootstrap_main.pgy` C emission: PASS,
  32,902,238-byte artifact, zero errors and three pre-existing redundant-`who`
  warnings
- focused C/LLVM owner-handle execution and five negative cases: PASS
- full self-host component contract: PASS
- SoT authority edge: PASS, 88 authorities and 183 derived carriers,
  `CLOSED=55 BRIDGE=32 ACTIVE=1`
- SoT live adequacy: PASS; Coq/Rocq explicitly skipped because no prover is
  installed on this runner
- single-owner, hard-contract, likeness, documentation, Python syntax, and
  `git diff --check`: PASS

## Remaining boundary

The reached program-extension and ArrayString ABI rows remain `BRIDGE`.
Multiple/conditional moves, moves from parameters or members, fresh-result and
literal arguments, value-result transfer, owned return, and general aggregate
ownership are not closed by this result. No `56/31/1` census is claimed.

Publication commit and exact-head remote CI are pending. Until both are green,
the collaboration lease remains active.

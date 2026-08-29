# ArrayString owner-handle caller move retirement — 2026-08-29

Status: `LOCAL GREEN — PUBLICATION PENDING`

Exact base: `ca2555e1e898f3ac2f0472e76d616dfba22e0410` on `origin/main`.

This directive coordinates one bounded executable ownership prerequisite. It
does not close or reclassify `abi.mir_array_string_layout_projection`, and it
does not admit general move analysis.

## Shared objective card

- Objective: make one reached scalar GraphPlan owner-handle
  `Array<String>` direct-call argument carry a typed caller-side move fact,
  prove that the caller local has no later use, and prevent C/LLVM entrypoint
  cleanup from releasing the storage after ownership moved to the callee.
- Priority: exact local/callable/parameter identity, last-use proof, one
  target-neutral move fact, C/LLVM cleanup consumption, positive runtime
  parity, use-after-move rejection without artifacts, old-path ratchet, then
  patch size.
- Fact owner: `DirectMirScalarProgramOwnedArrayStringMoveFact` owns the one
  admitted caller-local/call-operation/callee-parameter move identity and the
  ArrayString ABI layout identity it crosses.
- Last legitimate consumers: the C and LLVM ArrayString cleanup emitters may
  suppress caller cleanup only through that fact. The callee remains the
  owner-handle storage consumer and performs the terminal owned drop.
- Forbidden fallback: unconditional caller cleanup after an owner-handle
  move, inferring move from type or `own` spelling inside either backend,
  rescanning source/MIR in cleanup emission, accepting later local use,
  treating value/value-result/readonly carriage as a move, silently admitting
  more than one move, or accepting a missing/drifted ABI identity.
- Verification gate:
  `direct_mir_scalar_owned_array_string_parameter_owner.sh` must execute the
  same source-produced program through C and LLVM, observe exactly one
  terminal owner drop per execution path, and reject use-after-move plus
  parameter-policy/layout mutations without artifacts. The component contract
  must require the move fact and reject restoration of unconditional cleanup.

## Bounded admission shape

- Exactly one owner-handle `Array<String>` argument moves one entrypoint local
  into one direct callee parameter.
- The entrypoint has one block and the moved local has no use after the call.
- Multiple moves, conditional moves, moves from parameters or members,
  fresh-return/literal arguments, ownership-return chains, and non-entrypoint
  caller cleanup remain outside this lease and must fail closed.

## Integration boundary

- The primary task is the sole implementation and publication owner. No
  parallel implementation track is open on this executable rung.
- Allowed scope is the typed move fact/admission, scalar program extension
  carriage/readiness, C/LLVM cleanup consumption, the focused fixtures/gate,
  structural/SoT ratchets, owner inventory, and exact audit/handoff notes.
- The census remains `CLOSED=55 BRIDGE=32 ACTIVE=1` until current registry and
  executable gates prove otherwise. No whole-row decrement is promised.

## Local result

- `DirectMirScalarProgramOwnedArrayStringMoveFact` is derived once from the
  admitted callable, graph-storage, expression, and ArrayString ABI owners.
  The C and LLVM cleanup owners consume it through one shared cleanup policy;
  neither backend infers ownership from syntax or type names.
- The positive fixture prints `released` in C and LLVM with no caller-local
  drop. Use-after-move and carriage, pass-shape, ABI-layout, and call-target
  mutations fail before either target artifact is published.
- Current-source DRV-2 generation, production bootstrap C emission, the
  focused gate, full component contract, SoT edge/live adequacy, single-owner,
  hard-contract, likeness, documentation, and diff checks are locally green.
  Coq/Rocq is an explicit local skip because no prover is installed.
- The registry remains `CLOSED=55 BRIDGE=32 ACTIVE=1`; multiple or conditional
  moves and fresh-result/literal moves remain outside this bounded lease.

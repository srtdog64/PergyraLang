# 2026-10-21: retire an iteration-local Array, preserve the birth check

Decided by: GPT integration lane under the assigned MIR directive.

The lowering owns the retirement; a new empty birth never overwrites a live
owner merely because control returned through a loop. Keep the existing
UNBORN/RETIRED admission rule.

## Bounded implementation contract

- Start at an admitted empty `Array<String>` definition in a source loop.
  The original HIR loop header/exit and binding identity delimit one
  incarnation. Derive its region from the checked CFG and dominance.
- Synthesize retirement only when every storage use is an exact synchronous
  non-retaining builtin operand. Consume the existing retention registry;
  unknown calls, alias formation, indexed views, transfers, deferred uses
  and explicit releases do not receive an automatic-free permission from
  this pass.
- Emit one typed MIR cleanup edge for each exit from that incarnation,
  including continue, break and return. A borrowed-element array releases
  only its backing; an owned-element array releases payloads and backing.
  Mixed/unknown mutation evidence refuses rather than guessing glue.
- Both backends consume that same edge, after condition/return evaluation.
  MIR validates edge coverage, identity and the current inputs again.
  Missing, duplicated or redirected edges must fail before publication.

## Closure plan and boundaries

Owner: existing MIR lowering/collection-transition chain. Last consumers:
native C/LLVM terminators. No semantic-file edits, source manual drops,
canonical rewrites, runtime incarnation counters or validator exemptions.

First run the unchanged no-drop and explicit-drop reproducers, then C/LLVM
execution, moves/aliases and edge mutations, `make test-mir`, and the existing
String-result control and function-flow gates. Use sanitizers for emitted
owned payload/backing cleanup. Integration owner: GPT root.

The semantic branch exposes Slot-style parameter masks to MIR, but not the
independent Array descriptor-retention facet. A zero Slot mask is not that
proof. Declared-call support needs the existing semantic owner to project its
storage result; this decision neither invents that fact nor resolves either
OPEN copy/move question. Canonical admission and V1 remain open.

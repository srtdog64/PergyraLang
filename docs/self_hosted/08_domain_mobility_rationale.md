# Why Hard Self-Host: Domain Mobility

This note records the decisive reason hard self-hosting is a real goal rather
than a vanity target, and it reframes the readiness work accordingly. Every
later sequencing decision in this directory follows from it.

## The reason

Hard self-hosting is needed to move the domain.

The asset of this project is the domain model: subject, zone, world, intent,
authority, role, slot. The C compiler is scaffolding that currently holds that
model up. When the domain is defined only inside a foreign host language, it is
frozen to that host's assumptions: C's platforms, C's ABI, C's toolchain, C's
era. Moving the domain to a new target, runtime, or generation means dragging
the entire C implementation along, or risking a semantic split during the move.
The domain cannot travel on its own.

Self-hosting makes the domain describe itself in its own terms. Once the
compiler that defines the domain is written in the language, the domain's
definition becomes self-describing and can be bootstrapped and retargeted
anywhere the language reaches. This is the slot philosophy, that resource
handles stay portable across backends, applied to the compiler itself: the
compiler should be as portable as the slots it compiles. Hard self-host is that
principle taken to its conclusion.

## What this settles

It settles the open question of whether the systems substrate work is worth the
investment. It is. Deterministic collections, language-level arena lanes, and
the single source of truth migration are not polish; they are the load-bearing
wall of domain mobility. Without them the domain stays tethered to C.

## Why single source of truth is the first wall

If the domain's truth is split between the AST and MIR, it cannot be moved
cleanly. A move would have to carry both representations, or the semantics would
diverge in transit. Single source of truth means the domain holds exactly one
canonical self-form. Before the domain can travel, it must tell one and only
one truth about itself. That is why task 74 (retire the source_ast readers and
make MIR the unconditional source of truth) is the first load-bearing step, not
a cleanup chore.

## What to self-host first

The ten validator tools already written in the language are warmup. They prove
the language can run over stable inputs while the C compiler stays the oracle.
The real prize is different: the passes that define the domain, which are the
declaration, slot, zone, and world lowering paths. Those passes are the domain.
In the mobility frame they are the cargo; the lexer and parser are the vehicle
that carries them. Self-hosting priority follows the domain, not the pipeline
order.

## What makes the move safe

A domain is only truly moved when it is moved bit-exact. The C versus LLVM
parity discipline, with the C compiler as oracle, is the safety mechanism for
the migration. When the self-describing compiler matches the C oracle byte for
byte, the domain has provably stepped off the C scaffolding and is standing on
its own. The parity harness is therefore not just a backend check; it is the
verification that the domain migration preserved meaning.

## Ordered priority

To move the domain:

1. Give the domain one canonical self-form. Close task 74: retire the
   source_ast readers, make MIR the unconditional source of truth.
2. Give the language a substrate that can carry its own definition.
   Deterministic collections with stable iteration order, and arena lanes
   exposed at the language level.
3. Self-host the domain-defining passes (declaration, slot, zone, world
   lowering), with the lexer and parser as the vehicle.
4. Remove the C scaffolding only where the self-describing compiler matches the
   C oracle bit-exact.

The current work on task 74 is the first wall. The direction holds; the next
steps are to finish the source_ast retirement, then build the deterministic and
arena substrate, then move the domain-defining passes under parity.

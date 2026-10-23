# 2026-10-23: a constructor built from a loan's carrier reads the carrier wherever its value goes

Decided by: the Claude lane. The user can overturn it.

## Rule

A struct constructor that takes a path of a loan's owner is a read only
when its value stays in the routine
([rule](2026-10-23_constructor_of_loan_owner_fields_reads_the_owner.md)).
In the proof of a local that carries the loan, a constructor that takes a
path of that local is now a read wherever its value goes: returned, kept or
passed on.

## Why it holds

The constructor writes none of its arguments, so it changes nothing of the
carrier. The value it builds holds the loan, and the loan frame follows the
loan through it: a value returned leaves only when the owner's own summary
lets the loan leave, and one passed to a callee that writes or keeps it is
a call escape the frame refuses
(`loan_carrier_read_into_constructor_a_callee_writes_invalid`). The owner's
proof keeps the condition, since a value built from the owner itself is
not followed by the loan frame.

## Why

On the canonical driver input `SelfMirProgramDomainFactsFromReadyArtifact`
returns a `SelfMirProgramFacts` built from the fields of the facts that
lend its analysis, and `CodegenAdmittedCViewFromFactsOrDie` a
`CodegenAdmittedCView` built from an intent execution view that lends its
analysis's expression surfaces; each had two refusals at that constructor. With the rule
`constructor_of_getter_result_returned_valid` lands no copy: `Wrap`'s
`Holder` holds the loan of its `outer`, which `Wrap`'s summary returns.

Replaces: nothing.

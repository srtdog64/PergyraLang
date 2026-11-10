# 2026-10-23: ArrayPop adds no call escape to a formal's summary

Decided by: the Claude lane. It extends
[ArrayPop keeps nothing of its receiver](2026-10-22_array_pop_keeps_nothing_of_its_receiver.md)
from the backing frame to the slot escape walk. The user can overturn it.

## Rule

The slot escape walk gives a formal handed to a builtin that the
publication owner does not read as local a call escape. `ArrayPop`'s
receiver is now local there too (`slot_analyzer_escape.c`), so popping an
`inout` Array formal adds no call escape to the formal's summary. The
descriptor walk still reads `ArrayPop` as a change of the descriptor, and
the collection ownership owner still decides whether an element may be
popped.

## Why it holds

`ArrayPop` shortens its receiver in place and keeps nothing of it; both
backends only lower the length (`pgy_array_pop_*`), so no element is freed
or kept. `ArrayPop` is reserved by the statement-form builtin family, so no
declared routine can take its place, and it takes one argument. A callee
that keeps the Array elsewhere before popping it still has that call
escape. `inout_string_chain_kept_before_pop_landed_valid` discharges the
escape only by landing one independent `Clone(chain)` at that store; the
pop itself never supplies the proof.

## Why

On the canonical driver input `ParseDeclsInto` pushes each import's path
onto its `inout` import chain, hands the chain to the nested call and pops
it after. The pop's call escape marked the recursive callee's chain formal
as escaping, so each call that takes the chain needed a proof that its
Strings are owned, which a formal's Strings lack
(`inout_string_chain_popped_by_recursive_parser_valid`).

Replaces: nothing.

# 2026-11-24: after V1 the native pipeline becomes a frozen seed and oracle

Decided by: the user, on 2026-11-24, accepting the review points
("전부 개선하고 SoT 닫자"); recorded by the Claude lane. It takes effect when
both V1 goals hold on one published commit
([V1 stays two goals](2026-11-22_v1_stays_two_goals.md)). Until then nothing
changes.

## Rule

1. Once V1 holds, the native C pipeline gains no new language rule, ownership
   rule or backend feature. Its jobs are to build the first self-host driver
   (seed) and to answer as the runtime oracle in the parity gates.
2. A divergence found after V1 is fixed in the self-host compiler. The native
   pipeline is changed only when it is the seed's own defect, or when the
   oracle answer is wrong (reads freed memory, prints a wrong value); then the
   fix is the smallest one that makes the oracle right.
3. A native-only gate that checks a rule the self-host compiler owns is
   replaced by the self-host gate for that rule, or deleted with the rule's
   native owner.

## Why

Both front ends implement ownership analysis. Every rule landed on one side
has to be landed on the other or recorded as a divergence; the 2026-11
records list several (`docs/truth/state/blocker.md`, item 5). Keeping two
analyses equal is a cost that buys nothing once the self-host compiler
compiles every program the native one admits.

## Not decided here

- When the native pipeline is deleted outright.
- Whether the oracle role moves to a recorded expected-output corpus.

Replaces: nothing.

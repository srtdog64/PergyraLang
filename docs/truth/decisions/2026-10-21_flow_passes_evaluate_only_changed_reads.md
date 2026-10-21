# 2026-10-21: a flow fixed-point pass evaluates only entries whose reads changed

Decided by: the Claude lane, under the user's request to take over the
main-line work and make required CI green. It changes how the recursive
parameter-flow fixed point spends its work, not what it admits. The user
can overturn it.

## Rule

The first pass of a recursive demand closure evaluates every entry it
reaches, as before. After that, a pass evaluates an entry again only when
an entry its routine read changed after the read:

- a read is recorded after the demanded entry answers, so a change made
  while answering it is already seen;
- the reader is the routine, not the one entry: its other entries check
  the frames this entry's evaluation built, and those frames name what it
  read;
- a change is any fact a reader can see: the mask, the two storage facets,
  unproved, pending, unsupported, the output relation and its producer;
- an entry this pass has not evaluated yet is not marked, since it reads
  the change when it is evaluated.

The closure is at its fixed point when no entry is marked. The pending
relations are then sealed as before. The 4096-unit cap is unchanged.

`PGY_VERIFY_FLOW_WORKLIST=1` runs one full uncharged pass at that point.
It fails closed if any entry changes, which would mean a pass skipped an
entry whose reads changed.

## Why

- One recursive closure of the C emitter's statement and expression
  routines (496 entries, root `EmitStmtList`) re-evaluated every entry on
  every pass. With `StringJoin` related, it needed 4182 units, so the first
  analysis of the canonical input failed the flow store
  ([record](../evidence/2026-10-21_native_census_string_join_hits_work_cap.md)).
  Only 51, 44 and 17 entries changed in the passes after the first
  ([census record](../evidence/2026-10-21_native_census_flow_worklist.md)).
- On the canonical census the refusals are unchanged at every analysis
  (2027), and the final analysis evaluates 15293 entries instead of 18352.
- Raising the cap was not an option: it is fixed and pinned
  ([cap rule](2026-10-18_flow_budget_counts_closure_entries.md)).

Gate: the flow summary gate's recursive fan-out control is a routine that
reads its own slot summary before its write is seen and lends its Array to
1400 read-only leaves. Full passes fail it closed at the cap (2698
evaluations). With this rule it is admitted with 1404 evaluations, and
1400 are skipped. The gate's second run sets `PGY_VERIFY_FLOW_WORKLIST=1`.
`tests/semantic/flow_closure_worklist_unit.c` forges store rows. Every
reader rule and every fact in the change test has a check there that fails
when the rule or fact is removed.

Replaces: nothing.

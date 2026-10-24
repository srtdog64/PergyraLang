# 2026-10-25: a real sharing site in the canonical source may take an explicit Clone

Decided by: the user, on 2026-10-25, after comparing the ownership
direction with Goose, a language with no heap that makes sharing explicit
in the source. It answers the first open question of the
[native checker handoff](../../agent_work_directives/native_checker_handoff_claude_to_gpt_2026-10-24.md)
(section 5).

It amends one finish-line item of [V1 is two goals](2026-10-14_v1_scope.md).
"The unchanged full canonical program admitted" now reads "the full
canonical program admitted, changed only by the explicit `Clone`s this
decision allows". The other items stand.

## Rule

A `Clone` may be written in the canonical source at a site only when all
of these hold:

- The sharing is real. After the site, the two values that share storage
  are both used, and one is changed or released while the other is still
  read, so the output would differ if they kept sharing. A site that could
  share safely, where the proof does not follow the program yet, does not
  qualify: that is checker work.
- No landing rule can put the copy there. The example is
  `CanonicalMirIdentityEpochArtifactFromTreeText`: `AppendLine(acc, line)`
  returns `line` itself when `acc` is empty, and the copy belongs where
  `output` is assigned, which a node-wrapping landing cannot place.
- `Clone` copies the value whole
  ([2026-10-22](2026-10-22_moved_argument_used_again_is_copied.md)). A
  Clone the checker counts only as a copy by value does not admit the site
  and is not written.
- The site is recorded where the P0 copies are recorded (the section
  "P0 takeover: copies added and manual drops removed" of the
  [V1 directive](../../agent_work_directives/v1_scope_ownership_and_selfhost_generality_2026-10-14.md)).
  Each record names the sharing in one sentence, the missing fact, the last
  consumer and the removal condition, as `AGENTS.md` asks.

Still not allowed: a broad or whole-analysis `Clone`, a broad `ref`, any
other source change made to reach zero, and a weakened refusal (the
forbidden list in [blocker.md](../state/blocker.md)).

## Why

- Goose pays for safety by making sharing explicit in the source. This
  direction pays for it by proof. Where the sharing is real, the program
  needs the copy whichever way it is admitted. Writing it costs at run time
  what a landed copy costs, and it stops proof work on a site no proof can
  admit.
- The copy is visible in the source and in MIR as any `Clone`. Nothing
  about it is hidden.

## Measured

No cap is set; the count is the check. The number of Clones written under
this decision, with their sites, is reported at the
[2026-11-08 re-evaluation](2026-10-25_ownership_direction_reevaluated_2026-11-08.md).

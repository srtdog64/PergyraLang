# The fixed point's native oracle emission runs in its own CI job (2026-11-11)

Decided by the user on 2026-11-11, after the measurements in
[the landing rounds record](../evidence/2026-11-11_canonical_driver_landing_rounds.md).

- The driver fixed point keeps its 60 minute job limit, its inputs and all
  of its comparisons.
- The native oracle emission of the canonical driver (28 minutes on CI at
  `51fbdca00`) and its host C compile move to their own job, which runs
  beside the seed side (seed, full seed MIR, gen2, gen3) instead of before
  it.
- A join job runs every step that needs both sides (the oracle driver's
  sample, bounded and full-source MIR runs and their comparisons with the
  seed driver's) and writes the profile receipt from both sides' phase
  rows.
- The join consumes the oracle job's artifacts only after checking them
  against the checkout and the downloaded compiler pair. A missing or
  different artifact fails the join; it never rebuilds.

Other options the user was shown: incremental native analysis (days of
work, the job stays red meanwhile) and a larger job limit (refused on
2026-11-10). Copy retention across store waves was rejected earlier
because it changes the landing plan. This narrows the "no larger
`timeout-minutes`" rule of
[2026-11-10](2026-11-10_root_lane_taken_over_all_work_lands_on_main.md)
only in that the work is spread over jobs; no limit is raised.

Hosted runners differed by about 1.5 times on the same commit range on
2026-11-11 (the codegen bootstrap took 20.5 minutes at `51fbdca00` and
passed 30 at `5c72599e2`), so each new job is sized to fit its limit at
that slowdown.

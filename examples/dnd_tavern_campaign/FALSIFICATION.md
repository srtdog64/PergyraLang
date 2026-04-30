# dnd_tavern_campaign — Falsification Log

Last updated: 2026-04-30 (declared *before* Sprint 1 work begins)

## Why this file exists

`docs/122_managing_intent_drift.md` §4 mandates: declare falsification
criteria *before* writing the program. After-the-fact rationalization
defeats drift recognition (docs/122 §2.5).

This file is the *evidence collection artifact* for Sprint 1
(Multi-class character + Party-Character cycle) and Sprint 2 (drift
audit). Each criterion below states (a) what counts as a falsification,
(b) what the response is. Future Sprint 1 evidence is logged at the
bottom under "Evidence Records."

## Falsification Criteria (declared 2026-04-30, *before* any code edit)

| ID | Criterion | If triggered, response |
|---|---|---|
| **F1** | The same `PGY_SEM_*` diagnostic recurs in 3+ unrelated sites in the campaign | The targeted abstraction is short. Redesign rather than patch each site. |
| **F2** | Ability bound declarations require 5+ generic params for normal characters | `multi-bound` expressiveness is insufficient. Lift decision needed (docs/118 §6.2 conservative classifier). |
| **F3** | `intent` nesting depth exceeds 5 in routine combat / spell flows | Intent primitive is too fine-grained. Either split intent saga or reconsider the primitive. |
| **F4** | `subject` is bypassed via `class` redefinition more than rarely | `subject` definition is unfit for some real shapes. Modeling failure, not a patch. |
| **F5** | Zone bypass via escape hatches appears more than rarely | Zone coordinate itself is wrong for this domain. Reconsider zone primitive. |
| **F6** | `ability` / `zone` / `intent` feels *ceremonial* — required by syntax but not load-bearing — across 30+ LOC × 5 routines | The coordinate is asymmetric / over-applied. Audit whether it should be optional metadata, not first-class primitive. |

## Baseline (state before Sprint 1 begins)

- `dnd_tavern_campaign/` total: ~2904 LOC (per pre-sprint count 2026-04-30)
- File breakdown: world.pgy 962, zones/journey.pgy 361, setup.pgy 239, subjects/units.pgy 181, tables.pgy 179, subjects/vessels.pgy 151, combat_cards.pgy 148, dialogue.pgy 117, story_cards.pgy 104, combat_text.pgy 92, events.pgy 89, subjects/roles.pgy 51, common.pgy 44, main.pgy 34, player_main.pgy 34, random_main.pgy 34, subjects/abilities.pgy 29, subjects/views.pgy 26, zones/layers.pgy 19, report.pgy 10
- Existing primitives in use: `subject`, `class`, `tobject`, `object`, `enum`, `struct`, `action`, `func`, `intent` (pending verification), `zone`, `ability` (pending verification), `world` (script in world.pgy)
- Sprint 1 features to add: A (multi-class character), C (party↔character cycle)

## Sprint 1 — Implementation log

(To be filled as Sprint 1 progresses.)

### Feature A — Multi-class character (Fighter + Wizard)

- **Stress test target**: ability bound composition expressiveness (docs/118 §6.2 conservatism)
- **Design**: TBD after baseline read
- **Implementation**: TBD
- **Falsification triggered**: TBD

### Feature C — Party ↔ Character cycle

- **Stress test target**: graph / cycle expressiveness via Slot handle (the Rust-critique territory; previous answer 2026-04-30)
- **Design**: TBD after baseline read
- **Implementation**: TBD
- **Falsification triggered**: TBD

## Sprint 2 — Evidence audit (to be filled)

After Sprint 1, classify drift evidence by docs/122 §2.5 signal table:

- Same drift in multiple places → abstraction redesign candidate
- Drift not aligned with zone boundaries → zone coordinate review
- User circumvents abstraction → modeling failure
- Drift at predicted location → patch
- Drift at unpredicted location → modeling gap, new coordinate candidate

(Records appear here after Sprint 1 produces evidence.)

## Evidence Records

### E0 (2026-04-30, pre-Sprint baseline) — `role` primitive ceremonial

**Observation**: `subjects/roles.pgy` defines 4 roles (VanguardRole,
RadiantRole, ShadowRole, EmberRole, 51 LOC). Grep across the rest of
the campaign (~2853 LOC) finds **zero uses** of these role names. The
working campaign uses `Adventurer.className: String` as the real class
label and defines actions directly on the subject, bypassing the role
system entirely.

**Falsification mapping**:
- F4 candidate: `role` is bypassed in favor of direct subject methods
  with `className: String` label. The author (BDFL) reached for the
  string label instead of the role primitive when actually writing
  game logic.
- F6 candidate: 51 LOC of `role`/`ability` machinery contributes 0 to
  the working campaign output. By definition this is *ceremonial*.

**Three possible explanations** (Sprint 1 is the test):
1. Time-pressure incomplete adoption (not a modeling failure)
2. Role primitive is awkward for game character classes; author tried
   and reverted (modeling failure for this domain)
3. Role primitive fits operator/supervisor business shapes but not
   game class shapes (modeling scope mismatch)

**How Feature A tests this**: Multi-class character (Fighter+Wizard) is
the natural occasion to *need* role composition. If multi-class via
roles unlocks naturally, explanation 1. If it requires fighting the
role system, explanation 2 or 3.

**Status**: Recorded as baseline. Sprint 1 Feature A will produce the
discriminating evidence.

### E0 Update (2026-04-30, after broader codebase scan)

**Refined diagnosis**: The `role` primitive *does work* in other examples:
- `examples/role_ability_demo.pgy` (235 LOC) — exercises the canonical
  binding pattern `<RoleName>_as_<AbilityName>(&entity)` and includes a
  "Multiple Roles Composition" test that puts two roles
  (`EntityDamageable` + `EntityAttackable`) on one `Entity`.
- `tests/cases/backend_compare/role_operator/main.pgy` — primitive role
  binding for `Int` with operator dispatch.

So the `role` primitive is *not* broken. The dnd campaign simply has
not had a *use case that requires* ability dispatch — its actions are
directly defined on `Adventurer` rather than routed through abilities.

**Revised explanation between the three options**: closest to **option 1
(time-pressure incomplete adoption)**. Multi-class character (Feature
A) is the natural use case that *needs* role composition. If Feature A
lands cleanly using the canonical `<RoleName>_as_<AbilityName>` cast,
the role primitive is vindicated. If it does not, the dnd-domain-fit
question (option 2 / 3) reopens.

**E0 status**: still recorded, but reframed from "primitive failure"
to "absent use case." Sprint 1 Feature A is the test.

### E1 (2026-04-30, Sprint 1 first compile) — F1 IS TRIGGERED

**Observation**: First compile attempt of `examples/dnd_tavern_campaign/main.pgy`
produces **8 errors** of the same shape:

```
[ERROR] 0:0 - Intent step '<NAME>' authorized participant '<ALIAS>'
of type 'Adventurer' is ambiguous in zone 'JourneyZone'.
```

Intents and aliases involved:
- Toast (rogue, mage)
- BondEvent (rogue, mage)
- Explore (rogue)
- HitTrap (rogue)
- FaceDragon (rogue, mage)

Five distinct intents × multiple participants = 8 occurrences of the
same diagnostic. **Pre-existing breakage**, not caused by Sprint 1
changes (WarmageRole addition is unrelated to these errors).

**Falsification mapping**:
- **F1: TRIGGERED.** "Same `PGY_SEM_*` diagnostic recurs in 3+
  unrelated sites" — 8 sites across 5 intents far exceeds the threshold.

**Root cause analysis** (per the diagnostic body):
- `JourneyZone` declares more than one `Adventurer` slot (fighter,
  cleric, rogue, mage)
- Intent step's `authorized by: <alias>` clause receives an alias
  (e.g., `rogue`)
- The compiler cannot resolve which `Adventurer` slot the alias
  corresponds to without an explicit slot-naming convention
- The compiler's suggested fixes:
  1. Rename the participant alias to match the authority slot name
  2. Authorize a participant whose alias directly names the slot
  3. Split the zone contract so authority is not ambiguous

**Why this is abstraction-short evidence (per docs/122 §2.5)**:

The user (BDFL) intuitively wrote: "in this journey, four adventurers
(fighter / cleric / rogue / mage) each perform their own actions
authorized by themselves." This is the natural modeling.

The current Pergyra type system rejects this because the *binding
between an alias name in `authorized by` and a specific slot in the
zone* is ambiguous when the zone has multiple slots of the same type.
The fixes the compiler suggests are **mechanical disambiguation**, not
natural domain expression. The mismatch is between how the user thinks
about authority ("each adventurer authorizes themselves") and how the
language resolves it (alias must directly identify a slot by name).

**Per docs/122 §2.5 signal table**:
- "Same kind of drift recurs in multiple places" → "Abstraction is
  short. Patching one site is theatrics. **Redesign the abstraction.**"

The targeted abstraction for redesign: the **`authorized by` alias
resolution rule** when the zone declares multiple subjects of the same
type. The current rule forces alias-to-slot-name lexical equivalence.
A natural alternative: allow `authorized by self` (which is already
used in `Adventurer` action declarations — see units.pgy:93,105,118,131)
to resolve at the *call site's bound subject*, not at the zone-slot
level.

**This is the most important evidence Sprint 1 has produced.**
Multi-class character (Feature A) and party-character cycle (Feature
C) are now *secondary*. The primary finding is that the **`authorized
by` alias resolution against multi-slot zones is short** — a real
modeling failure that affects 5 of the campaign's core intent steps.

**Status**: F1 trigger documented. Sprint 1 should now branch:
- **Branch 1 (continue)**: Continue Feature A + C as planned to gather
  additional drift evidence; the alias-resolution finding is recorded
  for Sprint 2 audit.
- **Branch 2 (pivot)**: Pause Feature A + C and investigate the
  alias-resolution rule directly — what would a natural redesign look
  like? Is `authorized by self` already the intended canonical form?

**Recommendation**: Branch 1 (continue), because Sprint 1's purpose is
*evidence collection*, not abstraction redesign. Redesign decisions
belong in Sprint 2 evidence-audit phase. Continue per plan.

### E1 REVERSAL (2026-04-30, after instrumented investigation)

**E1 is INVALIDATED.** The F1 trigger was caused by a stale binary,
not abstraction shortness. Detail of investigation:

1. Added `fprintf(stderr, ...)` instrumentation to
   `resolve_zone_subject_slot_for_participant` in
   `src/semantic/type_checker_decls_domain_helpers.c`.
2. Rebuilt with `make pgy` — build target wrote the new ELF to
   `bin/pgy` (Linux), but PowerShell-launched workflow was still
   invoking `bin/pgy.exe` (Windows binary, last built 2026-04-24).
3. The two binaries had divergent semantic logic. The old `.exe`
   produced the "ambiguous" diagnostic that triggered F1; the new
   `bin/pgy` produced **0 errors** for the same source.
4. Debug output from the new binary confirmed `direct_match=1` for
   every alias (`fighter`, `cleric`, `rogue`, `mage`) against the
   correctly-named JourneyZone slots. Resolver works correctly.
5. After syncing `cp bin/pgy bin/pgy.exe`, the workflow consistently
   shows the campaign passing semantic with 0 errors.

**True diagnosis**: This is **dev pain point #1** (per memory:
`project_dev_pain_points.md` — "stale .o, CI/로컬 차이"). The Makefile
build target writes `bin/pgy` but the Windows-side `bin/pgy.exe` is
not auto-updated. PowerShell-launched commands silently use the stale
binary.

**Falsification mapping update**:
- F1 (same diagnostic in 3+ sites): **NO LONGER TRIGGERED.** With
  current binary, all aliases resolve cleanly. The diagnostic does
  not occur for the working campaign.

**Methodology note**: The falsification protocol from `docs/122` §4
worked exactly as intended. Declared criteria → observed apparent
trigger → instrumented investigation → root cause flipped from
"abstraction failure" to "tooling failure." Without the upfront
declaration, this would have looked like an abstraction crisis. With
the declaration, it looked like one for an hour, then turned into a
tooling fix instead. **The protocol prevented an unwarranted
abstraction-redesign panic.**

### E2 (2026-04-30, post-E1 reversal) — Codegen segfault

**Observation**: With the corrected binary, the campaign passes
semantic (0 errors / 0 warnings) but produces a **segfault** during a
later phase (codegen / AIR / RIR / MIR — not yet localized). The
diagnostic output before the segfault includes the standard "0
error(s), 0 warning(s)" line, indicating the semantic phase completed.

**Falsification mapping**: This is **not** a Sprint 1 falsification
trigger directly — it does not match F1-F6. It is a **separate bug**
in the codegen pipeline that surfaces only when compiling the
dnd_tavern_campaign at full scope (with the WarmageRole addition or
without; need to verify which).

**Categorization** (per `docs/122` §1):
- Likely **runtime drift** in the compiler itself (the codegen pipeline
  diverges from intent on this input).
- Or **layer drift** at the codegen/runtime boundary if the campaign
  emits an instruction shape the backend doesn't handle.

**Status**: open. Investigation continues separately from Sprint 1
abstraction evaluation. Sprint 1's primary finding (E1 reversal) is
already complete and recorded.

### E2 Update (2026-04-30, after WarmageRole rollback verification)

**Confirmed pre-existing**: With my Sprint 1 change rolled back (`git
stash`), the campaign *still* shows the same `Segmentation fault
(core dumped)` after `0 error(s), 0 warning(s)`. The crash is **not
caused by Feature A**. It is a pre-existing issue in the codegen
pipeline that surfaces on the full dnd campaign source.

**Implication for the "examples/dnd_tavern_campaign is a working
2904-LOC program" baseline**: The semantic phase passes. The full
campaign does not compile through to a runnable binary on the
current toolchain. This is a separate codegen / AIR / runtime bug
that needs investigation independent of Sprint 1.

**Categorization**: This is dev pain point #2 — distinct from #1
(stale .exe). Both are tooling, not modeling. Both should be tracked
as beta-closure infrastructure debt rather than as drift evidence
against the language design.

**Out of Sprint 1 scope**: Sprint 1's purpose was modeling stress
test. Codegen segfault is a compiler-stability question. Investigation
of the segfault should happen in a separate session with proper tools
(gdb, coredumpctl, valgrind).

### E2 Localization (2026-04-30, post-bisection)

**Bisection result** (run with `bin/pgy` after dev pain point #1 fix):

| Bisect | Imports | Result |
|---|---|---|
| bisect1 | common + tables + vessels + abilities + views | OK |
| bisect2 | + combat_cards + units + roles | OK |
| bisect3 | + zones/layers + zones/journey | OK |
| bisect4 | + intents/campaign_intents | OK |
| bisect5 | + dialogue + combat_text + story_cards + events + world.pgy | **OK** |
| bisect6 | bisect5 + **setup.pgy** | **SEGFAULT** |
| bisect7 | bisect5 + **report.pgy** | **SEGFAULT** |

**Localized to**: `setup.pgy` (239 LOC) and `report.pgy` (10 LOC) each
independently trigger segfault when added to the otherwise-clean import
chain. Either has some construct that the codegen pipeline cannot
handle, or the *interaction* with `world.pgy` (which both extend or
delegate to) reveals a backend bug.

**Smaller suspect**: `report.pgy` is only 10 LOC — investigation
candidate first because the surface area is smallest.

**Cross-language note**: this is **dev pain point #2** — codegen
crash on legitimate semantic-passing code. Distinct from #1 (stale
.exe). Separate beta-readiness ticket.

**Bisect files cleanup**: `bisect1-7.pgy` are temporary investigation
artifacts; can be removed after this finding is recorded.

### E2 Localization Update — Trigger Is Broader Than First Bisection Suggested

**Re-verification finding**: After cleaning state and re-running, even
`bisect5` (which had appeared to pass) now segfaults. Earlier "OK"
output was likely a parallel-test escaping artifact, not a true
PASS — the test runner's escaping made several outputs collide.

**Refined trigger characterization**: The segfault is reproducible
when a program imports `world.pgy` (and its dependency chain through
`zones/journey.pgy`, `intents/campaign_intents.pgy`, etc.) and has any
non-trivial `Main` body — including just `let s: String = "..."; Log(s);`.

**Localized scope** (current best guess):
- Trigger: full dnd-campaign import chain + any code that emits past
  trivial parser-only output.
- Likely phase: codegen / runtime emission (after semantic + AIR
  reach 0 errors / 0 warnings).
- Out of scope for surface-level bisection. Requires gdb / valgrind
  / coredumpctl to localize precisely.

**Action items recorded for separate investigation**:
1. Install `gdb` in WSL, capture backtrace.
2. Optionally enable `-g` debug flags (already on per Makefile
   inspection: `-O2 -g`).
3. Use `coredumpctl list` and `coredumpctl gdb` to inspect the most
   recent core dump.
4. Bisect by *content* (not import) — try removing specific
   declarations from world.pgy until segfault stops.

**Out of Sprint 1 scope**: confirmed. This is dev pain point #2 —
beta-readiness compiler-stability ticket, separate from the modeling
evaluation Sprint 1 was set up to do.

**Bisect artifact cleanup**: All bisect[1-10].pgy files removed
(2026-04-30). Investigation is documented; raw artifacts are not
load-bearing.

## Out of scope for this log

- Web feasibility (verdict in plan: post-1.0; not addressed here)
- Beta closure tracking (docs/100)
- Production capability claims (docs/120 §1)
- AIR Phase 2 / Option C lift / distributed runtime (vision territory)

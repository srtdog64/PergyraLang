# Axis Fact-Ownership: Mechanization and Compiler Binding

This documents the formal track that turns the **docs/42 keyword-orthogonality**
design from prose assertions into machine-checked theorems, and then binds those
theorems to the *actual compiler* so the model cannot silently drift from the
implementation.

- Proof: [`AxisOwnership.v`](AxisOwnership.v) (Coq/Rocq, 18 theorems, `coqc`-checked)
- Differential test: [`../../../tests/axis_keyword_adequacy_smoke.sh`](../../../tests/axis_keyword_adequacy_smoke.sh)
- Design source of truth: [`../../42_keyword_orthogonality.md`](../../42_keyword_orthogonality.md)

## 1. Why

docs/42 fixes a discipline: every semantic question (fact) is owned by exactly
one of four axes (Resource / Execution / Domain / Type-Contract), and one axis
must never silently own another's question. That discipline is **design wisdom
expressed as a heuristic** -- a Domain-Driven vocabulary, not a calculus.

The objection this track answers: *if the foundation is heuristic, where is the
rigor?* The reply is that the heuristic governs **model choice** (all modeling
is heuristic), while the rigor lives one layer down, in **how the primitives
compose**. docs/42's orthogonality and fact-ownership are exactly such a
composition law, and they can be proved rather than asserted.

## 2. The model (`AxisOwnership.v` SS1-3)

- `Axis` -- the four top-level axes from docs/42 SS0.
- `Fact` -- the semantic questions: the intent clauses
  `who / where / requires / authorized-by / causes`, plus one carrier per other
  axis (`FResourceHeld`, `FExecutionPlan`, `FShape`).
- `Owns : Axis -> Fact -> Prop` -- the docs/42 SS2 ownership table, encoded as an
  **inductive relation** (deliberately, not a function) so that uniqueness and
  totality are theorems about the hand-written table.

## 3. The theorems

| Section | Theorem | What it states |
| --- | --- | --- |
| 4 | `ownership_unique` | No fact is owned by two axes (orthogonality). |
| 4 | `ownership_total` | Every fact is owned by some axis (no lost meaning). |
| 4 | `ownership_exists_unique` | Exactly one owner per fact. |
| 5 | `no_silent_override` | A transition by axis `a` leaves every fact owned by a different axis unchanged. The axis-level form of "no hidden control flow". |
| 5 | `no_silent_override_2step` | That preservation composes across a trace. |
| 7 | `axis_updates_commute` | Updates by distinct axes commute (verifier resolution is order-independent). |
| 9 | `reading_updates_commute` | Even **state-dependent** resolution commutes, provided each axis reads only the facts it owns (`ALocal`). |
| 8 | `keyword_axis_sound` | The docs/42 SS1 keyword table is consistent with `Owns` (surface adequacy, inside Coq). |
| 10 | `append_is_stepby` | A write attributed to the axis that **owns** the fact is exactly a `StepBy` step -- the attribution discipline *forces* single-writer. |
| 10 | `append_preserves_foreign` | Hence a well-attributed append preserves other axes' facts (runtime no-silent-override). |
| 11 | `projection_writes_nothing` | A projection (`object`/`tobject` view) writes no fact -- a view never owns one. |

All are checked with `Qed` (no `Admitted`).

## 4. Binding the model to the compiler

A Coq theorem constrains the *model*. It cannot, alone, know whether the model
still matches the *real compiler*. The differential test
`axis_keyword_adequacy_smoke.sh` closes that gap by pinning the layers against
each other; drift in any one of them fails the gate.

| Check | Binds | Catches |
| --- | --- | --- |
| A | docs/42 SS0 keywords subset of compiler-recognized keywords (lexer reserved + parser contextual) | a designed keyword the compiler does not implement |
| B | Coq `keyword_axis` = docs/42 axis | the model and the design disagreeing |
| C | intent clause -> owning semantic checker (`who`->participants, `requires`->ability, `authorized`->authority, `causes`->effect, `within`->zone) | a clause silently re-routed to a different checker |
| D | AIR runtime evidence kind -> Coq fact/axis, plus the provider+subject guard | anonymous facts (guard dropped) or an evidence kind mis-attributed |

Check D reaches the **runtime** write-attribution: the AIR evidence graph is the
compiler's runtime fact store, and `air_evidence_node.c` refuses to append a
fact without a non-empty provider+subject. That guard is the runtime form of the
single-writer discipline; `append_is_stepby` (SS10) is its Coq counterpart.

## 5. How to verify

```sh
# Proof (needs coqc / Rocq on PATH; CI installs it):
coqc docs/semantics/proofs/AxisOwnership.v        # exit 0 == all 18 proofs check

# Differential test (pure source consistency, no coqc):
bash tests/axis_keyword_adequacy_smoke.sh         # prints "... ok"

# Both, as the CI gate runs them:
make formal-semantics-test-smoke
```

`formal-semantics-test-smoke` is invoked by CI on linux and macos, so both the
proofs and the four binding checks are gated automatically. Negative cases are
exercised manually (a dropped guard, a mis-axied fact, a re-routed clause each
make the gate fail), so the gate is known to have teeth.

## 6. Remaining

- Connect the abstract `Append` model (SS10) to the concrete C append API:
  extract the provider/kind at each `air_evidence_node.c` call site and check it
  is `WellAttributed`, so the refinement constrains the real code path.
- A full operational semantics of the verifier graph (beyond single appends).
- Effect-propagation lifecycle as a derived, non-owning view (SS11 covers
  projections; `effect` propagation is the next case).

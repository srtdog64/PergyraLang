# Axis Fact-Ownership: Mechanization and Compiler Binding

This documents the formal track that turns a **subset** of the docs/42
keyword-orthogonality design from prose assertions into machine-checked
theorems, plus source-consistency tests that catch common drift between those
theorems and the compiler.

**Read [§7 "Scope and limitations"](#7-scope-and-limitations-what-this-is-not)
first.** This is a small, targeted proof track, not a whole-language soundness
result. Do not cite it as "the language is mechanically verified."

- Proof: [`AxisOwnership.v`](AxisOwnership.v) (Coq/Rocq, 18 theorems over a small
  model; `coqc`-checked). Proof-sketch scope, not language soundness.
- Differential test: [`../../../tests/axis_keyword_adequacy_smoke.sh`](../../../tests/axis_keyword_adequacy_smoke.sh)
  (grep-level source consistency, not a verified extraction)
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
| 7 | `axis_update_idempotent` | Re-running the same axis update over an already-updated state is stable; verifier reruns do not drift. |
| 9 | `reading_updates_commute` | Even **state-dependent** resolution commutes, provided each axis reads only the facts it owns (`ALocal`). |
| 8 | `keyword_axis_sound` | The docs/42 SS1 keyword table is consistent with `Owns` (surface adequacy, inside Coq). |
| 10 | `append_is_stepby` | A write attributed to the axis that **owns** the fact is exactly a `StepBy` step -- the attribution discipline *forces* single-writer. |
| 10 | `append_preserves_foreign` | Hence a well-attributed append preserves other axes' facts (runtime no-silent-override). |
| 11 | `projection_writes_nothing` | A projection (`object`/`tobject` view) writes no fact -- a view never owns one. |

All are checked with `Qed` (no `Admitted`).

## 4. Binding the model to the compiler

A Coq theorem constrains the *model*. It cannot, alone, know whether the model
still matches the *real compiler*. The differential test
`axis_keyword_adequacy_smoke.sh` **narrows** that gap -- it is grep-level
source consistency, not a verified extraction, so it catches the *named* drifts
below (renamed symbols, mis-attributed facts), not arbitrary divergence. Within
that scope, drift in any pinned layer fails the gate.

| Check | Binds | Catches |
| --- | --- | --- |
| A | docs/42 SS0 keywords subset of compiler-recognized keywords (lexer reserved + parser contextual) | a designed keyword the compiler does not implement |
| B | Coq `keyword_axis` = docs/42 axis | the model and the design disagreeing |
| C | intent clause -> owning semantic checker (`who`->participants, `requires`->ability, `authorized`->authority, `causes`->effect, `within`->zone) | a clause silently re-routed to a different checker |
| D | AIR runtime evidence kind -> Coq fact/axis, plus the provider+subject guard | anonymous facts (guard dropped) or an evidence kind mis-attributed |
| E | AIR append entry points (`air_append_evidence_node`/`_ex`) require `provider_name`+`subject_name` | the `Append` model's `ap_axis` losing its real counterpart -- an anonymous append becoming possible at the API level |

Check D reaches the **runtime** write-attribution: the AIR evidence graph is the
compiler's runtime fact store, and `air_evidence_node.c` refuses to append a
fact without a non-empty provider+subject. That guard is the runtime form of the
single-writer discipline; `append_is_stepby` (SS10) is its Coq counterpart.
Check E pins the other half: the C append signature itself names a provider, so
the un-attributed write the `Append` model rules out is structurally impossible.

### Sibling track: slot capability calculus

The same adequacy pattern covers the **slot** proofs. `SlotCalculus.v` mechanizes
stale-handle rejection, token-gated access, and pin non-eviction;
[`../../../tests/slot_calculus_adequacy_smoke.sh`](../../../tests/slot_calculus_adequacy_smoke.sh)
binds each modeled operation and each proven invariant to its live counterpart in
`src/runtime/slot_manager.h` (`HandleRead`->`SlotRead`, `pin_non_eviction`->
`PergyraSlotPin`, the stale-handle lemmas->`generation`, the token lemmas->
`TokenCapability`). Rename a runtime symbol the proof depends on and the gate
fails. All three suites run under `make formal-semantics-test-smoke`.

## 5. How to verify

```sh
# Proof (needs coqc / Rocq on PATH; CI installs it):
coqc docs/semantics/proofs/AxisOwnership.v        # exit 0 == all 18 proofs check

# Differential test (pure source consistency, no coqc):
bash tests/axis_keyword_adequacy_smoke.sh         # prints "... ok"

# Both, as the CI gate runs them:
make formal-semantics-test-smoke
```

`formal-semantics-test-smoke` is invoked by CI on linux and macos, so the
proofs, the binding checks (A-E), and the slot adequacy suite are gated
automatically. Negative cases are exercised manually (a dropped guard, a
mis-axied fact, a re-routed clause each make the gate fail), so the gate is
known to catch the drifts it names. It does not certify anything beyond them
(see §7).

## 6. Remaining

- Check E binds the append API *signature* (it forces a provider). The deeper
  step is per-call-site: extract the provider/kind actually passed at each of the
  ~dozen `air_evidence_*.c` call sites and check each is `WellAttributed`
  (provider = the owning axis), so the refinement constrains every real write,
  not just the entry-point shape.
- A full operational semantics of the verifier graph (beyond single appends).
- Effect-propagation lifecycle as a derived, non-owning view (SS11 covers
  projections; `effect` propagation is the next case).

## 7. Scope and limitations (what this is NOT)

State these plainly; do not let the proof artifacts be cited as more than they
are.

1. **Fragment proofs, not whole-language soundness.** `AxisOwnership.v`,
   `SlotCalculus.v`, and `IntentStepSoundness.v` are *proof sketches* over small
   models of targeted invariants (axis fact-ownership; stale-handle / token /
   pin safety; intent-step progress + preservation under authority).
   `IntentStepSoundness.v` does prove the canonical soundness shape (progress +
   preservation) -- but only for one fragment (authority-guarded linear
   intents), so it broadens the proof surface without reaching whole-language
   metatheory. There is still no type soundness for the language as a whole (no
   coverage of types, generics, world/zone nesting, effects, relations, slots,
   async, modules together). "Pergyra is mechanically verified" is **false** as
   stated; "Pergyra has machine-checked proofs of specific invariants, including
   progress + preservation for an intent fragment" is true.

2. **Judgment rules are not consolidated.** Formal judgments exist as prose in
   `docs/semantics/00_proof_contract.md`, `01_intent_world_zone.md`, etc., and
   are scattered across the verifier code. A compact rule table whose rules map
   1:1 to compiler diagnostic codes (e.g. `IntentStep |- Effect(e) requires
   Authority(a)` <-> a specific diagnostic) is **not finalized**. The
   differential test pins *symbols*, not derivations.

3. **Evidence is typed at the AIR record boundary, not yet a full proof
   calculus.** `AIREvidenceNode` now carries typed
   `provider_kind` / `subject_kind` fields in addition to
   `provider_name` / `subject_name` provenance strings and fact/fallback
   counters. AIR append constructs those typed fields from the evidence-kind
   owner table, and validation rejects nodes whose typed evidence class drifts
   from the declared evidence kind. This closes the older string-only runtime
   evidence shape. It still is **not** a formal-calculus-grade proof object that
   the type checker constructs and consumes directly; the next step is
   per-call-site well-attribution checking of the real AIR evidence writers.

4. **Loss contracts are indexed, but not fully enforced.**
   `09_abstraction_loss_contracts.md` defines per-pass semantic-loss rules, and
   `loss_contract_manifest.md` now gives the canonical boundaries a
   machine-readable stage/gate index. `loss_contract_adequacy_smoke.sh` verifies
   that each stage artifact exists and that every `enforced` row names a live
   gate. This closes the older "no manifest" gap, but it is still not a full
   executable loss calculus: the current manifest is 4/5 gate-enforced and 1/5
   documentation-only, and the gate checks stage/gate adequacy rather than every
   forbidden read in the prose contract.

5. **Relaxed mode is not a verified mode.** `PGY_AIR_STRICT_EVIDENCE=0` disables
   strict evidence checking so a backend build can be smoke-tested. A binary
   produced under relaxed mode is **not** domain-safety-checked and must never be
   treated as "verified". Only the strict, gated path carries the evidence
   guarantees; the relaxed path carries none. Do not conflate the two.

In short: this track is *targeted machine-checked invariants on small models,
drift-gated against named compiler symbols by source-consistency tests*. It is
not whole-language soundness, a typed evidence calculus, a fully enforced loss
calculus, or a guarantee about relaxed-mode builds.

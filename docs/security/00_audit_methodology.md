# AI Validator Audit Methodology

Last updated: 2026-04-26

This document defines the workflow for running adversarial
counterexample-search audits on Pergyra runtime safety contracts.

## 1. The Generator → Validator Shift

Most uses of AI in software are **Generator mode**: the AI writes new
code from a prompt. In this mode, AI hallucination is a liability — the
AI may invent APIs that do not exist, miss edge cases, or write
plausible-looking but incorrect logic.

This folder uses AI in **Validator mode** instead. The AI is given:

- An existing source file (it does not write code).
- An invariant statement the file's logic claims to maintain.
- A harness that allows enumerating concrete inputs.

The AI's task is to **construct a counterexample** that violates the
invariant, or to exhaust its enumeration without finding one. In this
inversion, the AI's enumeration tendency — its willingness to generate
many plausible attack vectors without fatigue — becomes a strength.
Human reviewers tire after the obvious cases; the AI does not.

This is the same pattern as fuzzing, but with semantic awareness of
what the contract claims rather than blind input mutation.

## 2. Audit Workflow

### Step 1 — Pick a target

Select a contract from `01_audit_targets.md`. Each target lists:

- The source file(s) governed.
- The invariant statement.
- Existing regression coverage (what is already known to be safe).
- The adversarial input shape (what kinds of operations to enumerate).

### Step 2 — State the invariant precisely

The invariant must be a property that can be checked from observable
state. Examples:

- *"After `PergyraSlotRelease`, no subsequent `PergyraSlotRead` of the
  same handle returns the slot's pre-release data."*
- *"For any `SecureSlotHandle` `h` with token `t`, no operation
  `PergyraSlotRead(h, t')` with `t' != t` returns the slot's data."*
- *"For any `Authority` transfer, exactly one zone has the authority
  at any wall-clock time (single-owner)."*

Avoid vague invariants like *"slots are safe"* or *"tokens cannot be
forged"*. The audit harness must be able to mechanically check whether
a candidate counterexample violates the invariant.

### Step 3 — Run the AI Validator harness

Provide the AI with:

1. The source file(s) and their dependencies (read-only).
2. The invariant statement.
3. The set of operations that can be sequenced (e.g., for slot audit:
   Claim, Read, Write, Release, Pin, Unpin, GenerationBump, TTLExpire).
4. Existing test fixture inputs (so the AI does not waste enumeration
   on already-covered cases).
5. The instruction: *"Construct a sequence of operations that violates
   the invariant. If you cannot, report which families of inputs you
   exhausted."*

The harness is conversational. The AI proposes a sequence; the human
or a checker tool evaluates whether the sequence actually violates the
invariant; iterate.

### Step 4 — Triage findings

Each candidate counterexample is one of:

- **Confirmed bug** — the sequence actually violates the invariant.
  File a finding (see `templates/finding_template.md`), add a
  regression test, fix the code, link the regression to the finding.
- **Spurious** — the sequence is invalid (uses an API incorrectly,
  assumes wrong semantics, etc.). Note in the audit log; do not file
  a finding.
- **Contract gap** — the sequence reveals the invariant statement
  itself is unclear or under-specified. Update the contract spec in
  `contracts/`, then re-audit.

### Step 5 — Log the audit run

Use `templates/audit_log_template.md`. Record:

- Audit date, target, AI tool used, harness configuration.
- Counterexamples proposed (real and spurious).
- Findings filed.
- Exhaustion claim (which input families AI reports as exhausted).
- Time spent.

Audit logs are append-only; do not delete past runs.

## 3. Tool Requirements

### What the AI needs

- Read access to the source file under audit and its direct
  dependencies.
- Knowledge of Pergyra type system, ownership classes, runtime ABI
  (point it at `docs/semantics/`, `pgy_abi_spec.h`,
  `docs/118_slot_model_rigor_audit.md`).
- Ability to reason about C and LLVM IR if backend-compare is being
  used as oracle.

### What the human needs

- Ability to run small isolated test programs against the runtime
  (Pergyra source or C harness binary).
- Ability to run `make test-security`, `make runtime-panic-abi-test-smoke`,
  and `make air-strict-backend-compare-test-smoke` to confirm whether
  a candidate counterexample is real on actual binaries.
- Ability to file regression tests in `tests/cases/` once a finding is
  confirmed.

## 4. When To Run

- Before declaring a Tier 1-pending invariant as Tier 1 (move from
  designed-only to active).
- Before declaring a contract beta-frozen.
- After any non-trivial change to runtime safety code (`slot_manager.c`,
  `slot_security.c`, `pgy_runtime_panic_contract.c`, authority/zone
  runtime, channel runtime).
- Periodically (recommended: once per audit target, once per quarter
  during the 1-year freeze).

## 5. Limits and Honest Scope

This methodology is **counterexample search**, not proof. It cannot
prove a contract holds. It can only:

- Find counterexamples that exist (and the AI can enumerate to).
- Report exhaustion of enumerated families.

Non-coverage:

- Inputs the AI did not enumerate to (combinatorial blowup).
- Concurrency interleavings deeper than the harness models.
- Hardware-level effects (CPU bug, weak memory ordering not in the
  Pergyra memory model — see `docs/113`).
- Side-channel attacks (timing, cache, speculation).

These limits are inherent to fuzzing-class methods. To upgrade from
counterexample search to proof, the path is mechanized verification
(Coq/Lean), which is post-1.0 academic work. See
`docs/118_slot_model_rigor_audit.md` §9.

The honest claim after a green audit:

> *"On audit date X, AI Validator search exhausted enumerated input
> families {F1, F2, ...} without finding a counterexample to invariant
> I. This raises but does not prove confidence in I."*

Marketing language must respect this limit. See
`feedback_marketing_language_drift.md`.

## 6. Reference — Swival Rust stdlib Audit

The pattern this methodology imitates:
`https://github.com/Swival/security-audits/tree/main/rust-stdlib`

Notable findings from that audit:

- Heap overflow in `CString::clone_into()`.
- Correctness bugs in `slice::join()`.
- Several other invariant violations in `unsafe` blocks that human
  reviewers had not caught.

The Rust core team initially dismissed the audit as AI slop; reversed
position after concrete bugs landed. The lesson is that adversarial
AI usage on existing code is qualitatively different from generative
AI usage and worth the workflow overhead.

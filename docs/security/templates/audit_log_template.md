# Audit Log Template

Copy this file when starting a new audit run. Naming convention:
`audits/YYYY-MM-DD_<contract-id>.md` (e.g.,
`audits/2026-05-01_secure_slot_token_unforgeability.md`).

---

# Audit Run: <contract name>

- **Date**: YYYY-MM-DD
- **Auditor**: <human running the harness>
- **AI tool**: <model + version, e.g., Claude Opus 4.7 (1M context)>
- **Contract**: link to `contracts/...`
- **Time spent**: HH:MM
- **Status**: Complete | Interrupted | Continued in <next-log>

## Configuration

- Source files supplied to AI: list
- Documentation supplied: list (e.g., `docs/118`, `pgy_abi_spec.h`)
- Harness mode: conversational / scripted / hybrid
- Backend-compare oracle used: yes / no

## Adversarial Techniques Attempted

For each named technique in `contracts/<name>.md` adversarial input
shape, mark exhaustion status.

| # | Technique | Status | Counterexamples found | Notes |
|---|---|---|---|---|
| 1 | (technique name) | Exhausted / Partial / Skipped | 0 / N | |
| 2 | | | | |

Status definitions:

- **Exhausted** — AI explicitly enumerated and reported exhaustion. No
  counterexample within the family.
- **Partial** — Some inputs tried; AI was not able to claim exhaustion.
- **Skipped** — Family not attempted in this run; defer to next.

## Counterexamples Found

For each candidate counterexample the AI proposed:

### CE-1

- **Operation sequence**: paste / link to Pergyra source
- **Predicted violation**: which invariant clause
- **Actual evaluation**: ran on binary? Result?
- **Verdict**: Confirmed bug → filed as finding `findings/...`
  | Spurious (reason) | Contract gap (note in §Contract Adjustments)

### CE-2

(repeat as needed)

## Findings Filed

| Finding ID | Severity | Path |
|---|---|---|
| (none) | | |

## Contract Adjustments

If audit revealed the contract spec itself was unclear:

- Adjustment 1: ...
- Adjustment 2: ...

Update `contracts/<name>.md` and reference this audit log in its
"Audit History" section.

## Exhaustion Honest Claim

Per `00_audit_methodology.md` §5, the honest result of a green audit:

> *"On 2026-XX-XX, AI Validator search exhausted enumerated input
> families {F1, F2, ...} for invariant <invariant short name>, without
> finding a counterexample. This raises but does not prove confidence
> in the invariant."*

Fill in the families exhausted (must match Status=Exhausted rows
above).

## Next Steps

- Re-audit triggers (when to run again):
  - On any change to source files governed
  - Quarterly during 1-year freeze
  - When contract spec changes
- Contract gap follow-ups: ...
- Findings to fix before next run: ...

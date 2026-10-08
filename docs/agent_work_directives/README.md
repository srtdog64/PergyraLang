# Agent Work Directives

This directory is the separate home for temporary multi-agent work directives.
It is intentionally outside the numbered architecture-document series.

## Naming

- Use a descriptive filename without a leading document number, such as
  `public_ir_bypass_readiness_audit_2026-08-26.md`.
- Do not allocate a project-document number or rename a directive into the
  numbered `docs/NNN_*.md` series.
- Put an agent's read-only result under `docs/audits/`, not beside architecture
  owners or semantic registries.

## Authority boundary

A directive coordinates work. It does not own language semantics, compiler
facts, progress percentages, SoT status, protocol or ABI status, completion, or
the next implementation rung. Current source, named owner documents,
registries, and executable gates remain authoritative.

Every directive must state:

- its status and exact base revision;
- one shared objective card;
- independent edit scopes and forbidden overlap;
- allowed commands and validation budgets;
- the integration owner and one integration gate;
- whether outputs are observations, proposals, or implementation candidates.

Completion of a directive or report never implies that its proposal is ready
to implement. The primary integration task must recheck current source and
produce an executable falsifier before opening a rung.

## Lifecycle

Keep completed directives as dated coordination evidence when their reports or
handoff notes cite them. Mark them `AUDIT COMPLETE`, `IMPLEMENTATION COMPLETE`,
or `CANCELLED`; do not rewrite their historical base revision into the current
HEAD. A new coordination scope gets a new descriptive file rather than a new
number in the architecture series.

Do not delete an old directive; history must stay traceable. When its date is
past and no active handoff card names it, mark it `OLD` with one banner line
under its title (`> **OLD** (date): ...`) that points at the current
collaboration document. The banner adds a marker only: the original status
line, base revision and content stay unchanged, and `OLD` does not mean the
work it describes was completed. The 2026-10-08 marking covered every
directive dated before that day except
`mir_builder_ownership_chain_2026-10-07.md`, which the active self-host card
still names. The current Claude/GPT split is
[`claude_gpt_ownership_collaboration_2026-10-08.md`](claude_gpt_ownership_collaboration_2026-10-08.md).

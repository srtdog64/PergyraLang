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

When a directive's date is past and no active handoff card names it, mark it
`OLD` with one banner line under its title (`> **OLD** (date): ...`) that
points at the current collaboration document, then move it into
[`archive/`](archive/). The banner adds a marker only: the original status
line, base revision and content stay unchanged, and `OLD` does not mean the
work it describes was completed. When moving a file, rewrite the relative
links in it and the paths that name it elsewhere; frozen snapshots under
`docs/audits/archive/` keep the paths of the tree they captured.

## Why old directives are archived, not deleted

Most work here is done by AI agents that rebuild their context from the
working tree at the start of every session. A directive records why a slice
was cut the way it was: the objective card, the rejected alternatives, the
falsifiers, and the base revision that the decision was made against. That
reasoning exists nowhere else. An agent reads files in the tree and does not
search git history by default, so a deleted directive is a lost design
decision, and the next agent tends to reopen an alternative that was already
rejected. The archive keeps the active directory short and the past readable.

- Never delete a directive. Move it to `archive/` instead.
- Agents should search `archive/` before reopening a design question that an
  old slice may have settled. An archived directive is evidence of a past
  decision, not authority: current source, owner documents, registries and
  gates still win when they disagree.
- A directive stays in this directory while an active handoff card or another
  tracked file still treats it as current. On 2026-10-12, 129 `OLD`
  directives moved to `archive/`. Three stayed here because the handoff still
  names them: `inout_array_release_2026-10-05.md`,
  `ownership_checkpoint_green_2026-10-05.md` and
  `proof_checker_assurance_2026-10-06.md`.

The current Claude/GPT split is
[`claude_gpt_ownership_collaboration_2026-10-08.md`](claude_gpt_ownership_collaboration_2026-10-08.md).

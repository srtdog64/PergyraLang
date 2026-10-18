# 2026-10-17: state and decisions live in `docs/truth/`

Decided by: the user.

- **The problem.** The handoff grew to 5,006 lines and the integration
  directive to 5,154. Both mix current state, decisions, measurements and
  evidence in one file. People can no longer read them, and two agents
  edit the same file.
- **The decision.** `docs/truth/` holds the state, decisions, evidence and
  lanes, one fact per place (see its README). `docs/current_work_handoff.md`
  becomes a facade: its top card links to `docs/truth/` and holds no facts
  of its own.
- **Old documents.** The large existing documents are frozen as evidence,
  not rewritten. This first step builds the folder and replaces only the
  entry point, so the GPT lane's work in progress is not disturbed. Moving
  older content out of the frozen documents is a later step.

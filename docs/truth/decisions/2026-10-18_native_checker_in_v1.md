# 2026-10-18: the native ownership checker is part of V1

Decided by: the user, answering the
[OPEN question](2026-10-17_open_native_checker_scope.md) (choice A).

- V1 waits until the native checker, with its current stricter rules,
  admits the unchanged canonical program. The rules are not rolled back to
  the `b8e66a387` set.
- Fixes go into the checker. Each fix that admits more is a precision fix
  with a positive and a negative behavioral fixture. No rule is weakened,
  and the canonical source is not rewritten to avoid a rule.
- Progress is the number of independent native refusals on the canonical
  input, measured with the cascade removed, not where the first error is.

Decided by the user on the same day: the Claude lane takes over the work
the GPT root lane held on main, with the goal of required CI green
([lanes/claude.md](../lanes/claude.md)).

Replaces: the OPEN file above.

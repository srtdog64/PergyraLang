# OPEN (2026-10-17): is the new native ownership checker part of V1?

Status: OPEN, waiting for the user.

## The question

The native compiler's ownership checks became stricter with the inherited
work merged on 2026-10-16. In `src/semantic`, the change since `571ae8c3a`
is 126 files, +14,866 lines. The native compiler builds gen0, so the
canonical program must pass it.

That program already passed the self-host checker and the native compiler
at `b8e66a387`. Now it fails the native compiler, at lines that did not
change (see [state/blocker.md](../state/blocker.md)). The same ownership
rules are implemented twice, once in C and once in Pergyra, and both must
admit the canonical program.

## Choices

- **A. Part of V1.** V1 waits until the native checker admits the canonical
  program. The rules the two checkers disagree on should come from one
  source, for example a rendered registry like the builtin retention
  `.def`. Otherwise fixing one checker keeps breaking the other.
- **B. Not part of V1.** The V1 finish line is reached with the rules both
  checkers held at `b8e66a387`. The stricter native rules land after it,
  without weakening any rule V1 already holds. This needs a plan for
  keeping those rules out of the V1 path without a silent fallback.

Whichever is chosen: measure progress by the number of independent native
refusals on the canonical input, not by where the first error is.

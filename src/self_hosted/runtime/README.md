# Runtime Track (stays C)

Mirrors C-side `src/runtime/`. Counted at 0% intentionally: the runtime
is what the target Pergyra program links against, so substituting it in
Pergyra would create a bootstrap cycle. This placeholder exists so the
self-host directory mirrors the C-side layout exactly; it is not a
substitution roadmap entry.

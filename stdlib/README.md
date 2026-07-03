# Pergyra Stdlib

The wiring doc is `docs/148_stdlib_architecture.md` — layers (L0 builtin /
L1 core / L2 domain), the seven module contracts (per-type, caps, namespace,
import, gating, promotion, stability ledger), and the inventory table that
`stdlib-inventory-test-smoke` locks against this directory.

Quick ledger meaning:

- **active** — gate green, contracts honored, safe to depend on.
- **sketch** — code exists, NO compatibility promise: ungated, no `with caps`,
  domain fail-closed not yet implemented. May change or vanish without notice.

Scope (what belongs at all) lives in `docs/138_standard_library_scope.md`.
Do not add a module here without a docs/148 inventory row — the smoke fails.

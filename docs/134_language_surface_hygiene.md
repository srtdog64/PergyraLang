# Language Surface Hygiene Closure

Status: beta-closure policy.

This note records the current language-surface cleanup decision. The target is
not to reduce Pergyra's domain vocabulary. `intent`, `zone`, `world`, `subject`,
`object`, `tobject`, `ability`, `role`, `party`, and `roster` remain distinct
because they answer distinct ownership questions. Keyword count is not the
debt; duplicated truth paths are the debt.

## Non-Goal: Flattening Orthogonal Terms

The goal is not to reduce the number of keywords mechanically. The accepted
orthogonality rule remains:

- resource facts stay with resource owners;
- execution facts stay with execution owners;
- domain authority and orchestration facts stay with domain owners;
- type and capability facts stay with type/contract owners.

`intent` is the orchestration spine, but it is not a universal owner. It may
derive execution order, compensation, rollback, and observability from declared
facts, but it must not invent authority, zone, effect, role, or resource facts
that belong to another axis.

## Cleanup Targets

These are the surface hygiene seams that must continue to close:

- `inout` is the only spelling for value-result mutable parameters. `&mut` is
  rejected because it implies a live borrow that Pergyra does not provide.
- AST/MIR compatibility fallbacks must be named and ratcheted down. Treating
  `source_ast` as semantic inventory truth is not allowed once MIR metadata owns
  the fact.
- Authority has one approval source of truth. Effect bits may say that
  authority is required, but `authorized_by`, zone authority, and runtime
  authority evidence own whether approval exists.
- TODO files track open work only. Completed implementation evidence belongs in
  execution logs or focused status documents.

## MPaC Placement

MPaC, Message-Passing and Contracted Concurrency, is a domain kit candidate, not
a core syntax family. Its planned home is `pgy.kit.mpac`.

The useful Pergyra integration point is that one `intent` can emit:

- the effect requirements of the coordination graph;
- the authority evidence required to execute it;
- deterministic coordination contracts for fanout, join, barrier, and result
  aggregation.

That makes MPaC a strong standard-library or domain-kit layer. It should reuse
existing `intent`, `world`, `zone`, `effect`, `authority`, `role`, `party`, and
projection machinery instead of adding new core keywords.

## Gate Rule

Allowed debt must be named. Unnamed fallback is not allowed. A compatibility
alias is acceptable only with an owner, an expiry path, and a smoke gate; the
language surface should otherwise prefer one spelling for one semantic flow.

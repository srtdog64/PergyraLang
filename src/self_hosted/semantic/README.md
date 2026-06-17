# Semantic Substitution Track

Pergyra-written semantic checks live here, mirroring C-side `src/semantic/`.
The first rung is intentionally tiny: function signatures, local typed `let`,
basic literal/identifier types, and return typing.

`main.pgy` emits a deterministic JSON diagnostic verdict for committed fixtures, and
`src/self_hosted/parity/semantic_parity.sh` keeps the C compiler as the
accept/reject oracle. Do not broaden this into declaration-heavy semantic
owners until expression-operator and diagnostic-code parity are gated.

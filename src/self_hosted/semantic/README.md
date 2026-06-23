# Semantic Substitution Track

Pergyra-written semantic checks live here, mirroring C-side `src/semantic/`.
The first rung is intentionally tiny: function signatures, local typed `let`,
basic literal/identifier types, and return typing.

`source_bundle_owner.pgy` owns the root-source/import graph bundle consumed by
`program_check_owner.pgy`. `expr_type_owner.pgy` owns expression type queries;
`expr_validation_owner.pgy` owns expression diagnostics that consume those type
facts, including undefined identifiers and operator operand checks.
`semantic_run_owner.pgy` owns CLI argument validation and deterministic
diagnostic verdict emission; `main.pgy` only wires `Args()` into that owner. The
run owner emits a deterministic diagnostic verdict for committed fixtures, and
`src/self_hosted/parity/semantic_parity.sh` keeps the C compiler as the
accept/reject oracle. Do not broaden this into declaration-heavy semantic
owners until expression-operator and diagnostic-code parity are gated.

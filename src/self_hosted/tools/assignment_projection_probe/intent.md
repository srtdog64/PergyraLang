# Assignment Projection Probe - Intent / Contract

**Status:** focused executable substitution proof.

## Intent

Prove that self-hosted C emission consumes semantic-owned assignment type and
expression-graph facts. The probe covers scalar Option assignments and indexed
array assignments without recovering types from source text or codegen state.

## Input Contract

- The probe has no source-file input.
- Normal execution uses the typed facts constructed in `main.pgy`.
- `--missing-expected-type`, `--missing-target-type`,
  `--missing-call-target`, `--missing-c-binding`, and
  `--collection-cref-only` deliberately remove or corrupt one required owner
  fact and must fail closed. A raw `cref` is not an interchangeable `cbind`.

## Output Contract

Normal execution emits the five C assignment rows pinned by `expected.txt`.
The five negative modes must exit nonzero with their owned missing-fact
diagnostic. C and LLVM output must remain byte-identical when LLVM is enabled.

## Oracle

`tests/self_hosted/parity/assignment_projection_probe_parity.sh` compiles and
runs the probe, compares normal output with `expected.txt`, checks all
fail-closed modes, and performs the C/LLVM parity leg. Reintroducing source
expression type recovery or codegen environment type guessing is forbidden.

# Self-Hosted Test Artifacts

This directory owns oracle-side artifacts for the self-hosted substitution
track.

`src/self_hosted/` is for Pergyra source owners. `tests/self_hosted/` is for
parity harnesses, committed fixtures, expected outputs, and migration probes.
Keeping those roles separate prevents the self-hosted compiler from copying the
C implementation's fragmented source tree while still keeping every rung
observable and reproducible.

Current layout:

```text
tests/self_hosted/
  parity/    C / LLVM / Pergyra comparison harnesses.
```

Migration rule:

- New parity harnesses go under `tests/self_hosted/parity/`.
- New long-lived self-host fixtures or expected outputs go under
  `tests/self_hosted/<stage-or-tool>/`.
- Existing collocated `src/self_hosted/**/fixture` and
  `src/self_hosted/**/expected` payloads are legacy test artifacts. Move them
  stage-by-stage together with the parity script that consumes them.
- Do not add shell harnesses, generated outputs, or committed build products
  under `src/self_hosted/`.

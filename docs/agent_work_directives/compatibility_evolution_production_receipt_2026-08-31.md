# Compatibility Evolution Production Receipt

Status: `ACTIVE` — implementation local-green; publication pending

Exact base revision: `551fef8836bf187a4931e9c7a8eafbcb0abe7081`

## Shared objective card

- Objective: make the installed self-host composition root obtain one typed
  compatibility-evolution receipt from the existing Pergyra fact owner and
  require that receipt before any admitted compiler request can execute.
- Priority order: exact surface/diagnostic/row identity, owner-issued receipt,
  production-root carriage, fail-closed mutations, retained native bootstrap
  oracle, then documentation and publication evidence.
- Fact owner: `src/self_hosted/compiler/compatibility_evolution_owner.pgy`.
  The receipt is a pure value fact; it is not a new lock-bearing world zone.
- Last legitimate consumer:
  `DriverRung2ExecuteInstalledRequest` reached from
  `src/self_hosted/compiler/driver_bootstrap_main.pgy::Main`.
- Forbidden fallback: a bool-only readiness call at the last consumer, a
  consumer-local rebuild or parse of the eleven-field rows, acceptance by
  prefix/count without exact surface-to-diagnostic binding, native retry, a
  second compatibility authority, or deleting the explicit native bootstrap
  manifest validator before its remaining consumers migrate.
- Verification gate and falsifying cases: the canonical owner receipt must
  pass C/LLVM manifest parity and a real installed-driver request; valid-shape
  `PGYCOMPAT003`/`PGYCOMPAT004` crosswire, missing row, and duplicate surface
  receipts must fail closed. The component contract must require the typed
  `Main -> DriverRung2ExecuteInstalledRequest` carriage and forbid a bool-only
  consumer check.

## Edit scope and overlap boundary

The primary task is the sole edit, integration, commit, push, and CI owner for:

- `src/self_hosted/compiler/compatibility_evolution_owner.pgy`
- `src/self_hosted/compiler/compatibility_evolution_manifest.pgy`
- `src/self_hosted/compiler/driver_bootstrap_main.pgy`
- `src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy`
- the bounded compatibility receipt fixture/gate under
  `tests/self_hosted/parity/`
- the matching component/topology ratchets
- this directive, the collaboration ledger, and the current handoff

Do not edit `src/compiler/driver_diag.c` or `src/compiler/driver_app.c` in this
prerequisite rung. Their explicit native manifest validation remains the
bootstrap oracle. Do not inspect, edit, stage, or delete the unrelated untracked
`docs/compiler_architectures/`, `pgy-80135c2c/`, or `pgy-91d769ec/` paths.

## Commands and validation budget

- Static owner, topology, component, syntax, and line-cap gates: 60 seconds
  each.
- Focused receipt C/LLVM parity, mutation rejection, and the last-consumer
  pre-dispatch structural ratchet: five minutes.
- One installed self-host CLI/package request and bounded build/integration
  shard: 30 minutes.
- Full push matrix is publication evidence and must not be multiplied locally.

The integration owner is the primary task. The integration gate is the
existing compatibility-evolution parity plus the bounded production receipt
gate, followed by the repository's installed-driver aggregate and exact push
CI.

## Local implementation evidence

- The owner now issues one `CompilerCompatibilityEvolutionReceipt` with exact
  schema, ordered surface, diagnostic, and canonical row identity. The
  installed request consumer checks it before request dispatch; `Main` carries
  the owner-issued value explicitly and no compiler-world zone was added.
- `make self-host-compatibility-evolution-parity-test-smoke` is green after one
  native compiler rebuild. Canonical C/LLVM output is equal, and executable
  crosswired-diagnostic, missing-row, and duplicate-surface receipts are all
  rejected by the same validator consumed at the installed boundary.
- A fresh `make self-host-compiler` installed a Pergyra-built DRV-2 from the
  modified source. Direct execution of
  `tests/self_hosted/parity/installed_driver_cli_mode_owner.sh` with that
  binary is green across the installed source-C, source-MIR, MIR-C, LLVM
  intent, diagnostic, REPL, format, and device-manifest requests.
- `tests/self_host_compiler_topology_smoke.sh` and
  `tests/self_hosted_component_contract_smoke.sh` are green. The component
  contract requires typed `Main -> DriverRung2ExecuteInstalledRequest`
  carriage and the exact validator call. The topology ratchet requires that
  call to precede request dispatch and rejects a bool-only readiness call.
  This is a composed negative proof; no production test-only receipt injection
  flag was added.
- Documentation, agent-boundary, object/action, post-selfhost manifest, SoT
  edge/single-owner/protocol, and hard self-host contract gates are green. The
  SoT adequacy live binding and negative mutations are green with an explicit
  local Coq/Rocq skip; this workstation has no prover, so the model itself was
  not checked locally and exact CI remains its required evidence.
- Publication and exact push-CI evidence remain pending. Until they are
  observed, the collaboration lease and this directive remain active.

## Evidence classification

Source and executable gate results are implementation evidence. This document
is only a coordination record. Opening or completing it does not by itself
close `compatibility.evolution`, change the `88/183` and `55/32/1` census, or
move the 83% forecast. The rung may establish production `REACHABLE` receipt
carriage; `CLOSED` still requires migration of diagnostic/package consumers,
missing-fact failure at those boundaries, deletion of the C text path, and a
negative ratchet against its return.

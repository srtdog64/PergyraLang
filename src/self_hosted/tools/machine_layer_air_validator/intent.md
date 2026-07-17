# Self-hosted AIR machine-layer validator

This tool is a bounded self-host consumer of the native `pgy --air-json`
`machine_layer_sites` projection. It checks the manifest, contact operation,
runtime operation, and hardware/authority/live-lease bits against
`CompilerRuntimeCallAbiMachineLayer*` from
`src/self_hosted/compiler/machine_layer_runtime_projection_owner.pgy` and the
native `pgy.machine-layer.declaration.v1` artifact supplied as the second
argument. The declaration consumer owns the physical grant view; this tool
does not define a second machine contract, read source/AST text, or provide a
default operation when a row is malformed.

The executable gate is
`make self-host-mir-machine-layer-test-smoke`; the gate feeds it a live AIR
dump and also mutates one manifest row to prove fail-closed behavior.

## Intent

Keep AIR machine contacts on the declaration-owned manifest path. A malformed
or mismatched contact is a compiler error, never an inferred backend default.

## Input Contract

One `pgy.air.graph.v1` JSON file and one native
`pgy.machine-layer.declaration.v1` JSON file.

## Output Contract

Emit one `pgy.selfhost.machine-layer-air.v1|sites=N` row on success and exit
non-zero with a diagnostic on any missing owner fact.

## Oracle

`tests/machine_layer_pipeline_smoke.sh` and
`tests/self_hosted/mir_machine_layer_smoke.sh` remain the native producer and
mutation oracles.

# Self-hosted RIR machine-layer validator

This tool consumes the native `pgy.rir.v1` JSON artifact produced by
`pgy --rir-json`. RIR owns `machine_contact` classification; the validator
checks that every serialized contact is one of the five rows exported by
`machine_layer_runtime_projection_owner.pgy` and fails closed on missing or
unknown values. It is an artifact consumer, not a second machine contract.

The live producer-to-consumer and mutation gate is
`tests/self_hosted/mir_machine_layer_smoke.sh`.

## Intent

Preserve the RIR machine-contact classification as a typed artifact fact.
Unknown, missing, or source-text-recovered contacts fail closed.

## Input Contract

One `pgy.rir.v1` JSON artifact.

## Output Contract

Emit one `pgy.selfhost.machine-layer-rir.v1|contacts=N` row on success and
exit non-zero for schema or contact drift.

## Oracle

`tests/machine_layer_pipeline_smoke.sh` is the native RIR producer oracle;
the self-host smoke supplies malformed-contact mutation coverage.

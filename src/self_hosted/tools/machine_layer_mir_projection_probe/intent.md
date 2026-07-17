# Self-hosted MIR machine-layer projection probe

This probe exercises the self-hosted MIR machine projection owner over a
declaration artifact. It is intentionally a graph-shaped fixture: no source
text or backend output is an authority for contact identity.

## Intent

Carry the five machine contacts from the semantic call-target graph into MIR
and reject a missing target fact before JSON materialization.

## Input Contract

One native `pgy.machine-layer.declaration.v1` JSON file, or the explicit
`--missing-call-target` negative fixture.

## Output Contract

Emit a `SelfMirProgramFacts` JSON projection on success and a non-zero
diagnostic for malformed declarations or missing call-target identity.

## Oracle

`tests/self_hosted/mir_machine_layer_smoke.sh` compares the live projection and
its negative mutation against the native MIR/AIR gates.

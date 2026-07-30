# Zone Authority Fact Probe

**Status:** focused executable semantic-to-DIR carriage contract.
**Parity owner:** `tests/self_hosted/parity/zone_authority_fact_probe_parity.sh`

## Intent

Prove that one semantic-owned zone authority row carries its exact zone,
subject-slot, and required-ability identities into DIR without an AST-text
rescan or name-only reconstruction.

## Input Contract

The probe owns one closed typed AST fixture containing `Runnable`, `Worker`, and
`Gate.worker`. It also constructs admitted fact rows with the same stable node
identities; no external source file is read.

## Output Contract

The positive semantic producer and admitted carriage emit
`zone-authority-carriage=PASS`. Missing authority rows, a missing ability
identity, duplicate required abilities, or duplicate zone-slot authority rows
must exit nonzero before DIR can consume a partial graph.

## Oracle

`tests/self_hosted/parity/zone_authority_fact_probe_parity.sh` delegates to the
zone-authority gate owner, compiles and runs the Pergyra probe through C, and
ratchets `TypedAstKindZoneAuthorityTag` rescanning out of the DIR consumer.

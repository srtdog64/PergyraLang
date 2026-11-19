# DRV-2 one-process source-to-C within the 14 GiB bound (2026-11-20)

Source: main `6ea9e53a8` plus this change. Tree: WSL `Ubuntu-C-WSL`
`/root/ts2`. Driver from `make self-host-compiler`: SHA-256
`84aa31bb2eadae41e297b8b5ed2549b039207111f493d6b5c9b6418569815bf8`.
The baseline driver is `6ea9e53a8`'s build, SHA-256
`da3180590480d203fbe8f0031cac8c6cd36252ffe1c13e5e6daa2ae70265d908`
(the [one-routine claim record](2026-11-20_general_route_one_routine_claim.md)).

## Before

The last complete one-process run of `--emit-c-artifact-verified` on
`driver_rung2_main.pgy` exited 134 with an explicit OOM at 14,077,584 KiB
under the 14 GiB address-space / 1,800 s boundary
([v5 record](2026-11-18_selfhost_array_transform_lifetime.md)). Split into
its two halves on the baseline driver, the producer
(`--emit-mir-json-verified`) peaked at 8,352,036 KiB and the consumer
(`--mir-json`) at 10,561,500 KiB.

The driver's C releases almost nothing, so the peak follows the bytes
requested. A byte-weighted allocation sampler (an `LD_PRELOAD` diagnostic,
not committed) attributed 9,573 MiB requested by the consumer. Two functions
built throwaway strings in loops whose size grew with the whole program:

- `SemanticArrayStorageReleaseVerdict`, 1,202 MiB: for every call node it
  concatenated `Enum.Variant` for every enum variant of the program to ask
  whether the callee is a variant constructor.
- `SemanticAstCollectionExclusiveSelectCopiesBeforeMutation`, 827 MiB: for
  every changing statement it built seven row keys for every copy site of
  the program, then skipped the sites of other functions.

## Change

- `array_storage_release_verdict_owner.pgy` builds the qualified variant
  names once into a set and asks the set per call.
- `ast_collection_exclusive_compiler_clone_owner.pgy` also records, per
  function, the list of its copy sites in recording order.
  `ast_collection_exclusive_statement_mutation_owner.pgy` walks only that
  list. Sites are visited in the same order as before, so the same Clone
  nodes are selected in the same order. A missing or out-of-range list row,
  or a listed site of another function, fails the selection closed.

No rule, budget, input or gate changed.

## Results

| Measurement | Baseline | This change |
|---|---:|---:|
| Producer peak RSS (KiB) | 8,352,036 | 5,184,116 |
| Consumer peak RSS on the baseline MIR (KiB) | 10,561,500 | 7,313,908 |
| Consumer bytes requested (sampler, MiB) | 9,573 | 7,569 |
| One-process source-to-C, 14 GiB / 1,800 s | exit 134, OOM | exit 0, 12,681,836 KiB, 9:15.69 |

- The consumer's C from the baseline MIR is byte-equal to the baseline
  driver's (30,474,291 bytes, SHA-256 `d464a53a...574e`).
- The one-process run wrote a 30,478,083-byte C artifact (SHA-256
  `c43d8b1c...abe2a2`). The same driver's producer MIR (SHA-256
  `943e2573...85c23`) through its consumer gives byte-equal C.
- It compiles with `gcc -O0`. The resulting DRV-2 binary is not an installed
  composition root and refuses `--emit-c-artifact-verified`, so it is not a
  fixed-point check; that stays with the CI join job.
- Peak RSS was sampled every 0.5 s from `/proc` for the split runs and taken
  from GNU time for the one-process run. No wall-time claim is made.

Gates on this build: shared storage copy landing (landed 2, refused 12),
public ArrayDrop and its owner caps, `Array<String>` cleanup, component
contract, the semantic owner probe build, collection focus recovery, field
read summary and inout effect: PASS.

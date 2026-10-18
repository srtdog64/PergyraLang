# V10 capture allocation bounds receipt

Observed 2026-10-18 KST on uncommitted source based on
`8404061d78f2d2bf9f2c325be1a67f83ab9904bf`.
Validation copy: `/root/pgy-nonowner-20261018.5t1W96`.

This record establishes only the reached native capture-storage repair and its
focused regressions. It does not establish unchanged-canonical admission,
bootstrap fixed point, installed-driver parity, published-SHA CI, or
whole-language memory safety.

## Candidate identity

| Artifact | SHA-256 |
|---|---|
| `bin/pgy` | `0779ca034ea5c091e995e9644b70f1cf57c7d7b8afaee4f253b30eb2a41afb2c` |
| `bin/test_air` | `6a8560de5c33ddc8db46fea84a8b60852d0f5cf4ef7aac223d953f5abdcd7a5a` |
| `bin/test_mir` | `2d57e8b8475dbc83c68724649616c53ddbb13731ee571758fbce0c7007431ef0` |

The candidate remains uncommitted. These hashes identify the Linux validation
artifacts; they are not a published or immutable review export.

## Implemented owner chain

- Semantic boundary and row allocations retain their real capacities. New
  allocation tails are initialized, the boundary capacity moves with
  `SemanticResult`, and teardown walks initialized capacity rather than a
  mutable active count.
- MIR import rejects semantic count/capacity inconsistencies before mutating
  its destination, allocates the exact admitted counts, records boundary and
  row capacities, and can release partial imports safely.
- MIR accessors and validation reject active counts above capacity and reject
  missing storage for active facts.
- AIR binding fingerprinting performs the same bounds preflight before any
  capture traversal. The existing C, LLVM and parallel planners check the
  binding before changing output or allocating a plan.
- Capacity remains private allocation metadata. This repair does not claim to
  defend against simultaneous fabrication of both a native pointer and its
  matching allocation metadata.

## Mutation falsifiers

The post-certificate matrix now contains 16 independent faults. It includes the
four required representative mutations below:

1. instruction identity changed after certification;
2. observability surface flag changed after certification;
3. capture boundary count raised above its allocation capacity;
4. one capture boundary's row count raised above its row capacity.

For each newly added size mismatch, MIR/AIR current-binding and all C, LLVM and
parallel plan paths refuse before output changes; source, AIR and output
sentinels remain unchanged; exact restoration re-admits the plans. The
sanitizer trace for each new case reported
`valid-capture=0 binding=0 ready=1 C=0 LLVM=0 parallel=0 outputs=1 unchanged=1 restore=1`.
The MIR lifecycle control also proves rejection before destination mutation,
then successful import and safe clear after exact restoration.

## Passing scoped gates

| Gate | Observed result |
|---|---|
| release AIR unit | 161 passed, 0 failed |
| release MIR unit | 682 passed, 0 failed |
| phase unit | 13,422 checks, 0 failures |
| semantic / transpile | 3,230/0 and 1,014/0 |
| RIR / DIR | 44/0 and 15/0 |
| ReadFile function-parameter positive/refusal matrix | PASS on C and LLVM; known rejected slot-wrapper secondary diagnostics retained |
| parallel capture C/LLVM projection snapshot | PASS |
| AIR MIR binding smoke | PASS |
| HIR/RIR/MIR/AIR function-parameter flow gates | PASS |
| ASan+UBSan `test_air`, `test_mir`, 40-source compile corpus | PASS with leak detection disabled |
| semantic core shape / semantic TU size | PASS |

## Broader observations that remain open

- The default strict backend comparison reached its same-process ABI precheck:
  186 passed and 4 failed. `handoff_world_state_frontier_abi` and
  `handoff_layer_state_frontier_abi` each failed compilation and its latency
  assertion with `LLVM expression type inference requires a concrete type:
  selected declared call has no result type`.
- With that precheck disabled, default discovery stopped because
  `typed_empty_map_factory`, `typed_literal_array_factory`, and
  `typed_retention_factory` exist on disk but are absent from the default case
  registry. Explicitly forcing all 975 discovered cases yielded 895 passes and
  80 failures. This is broader current-branch evidence and is not attributed
  wholesale to V10.
- The production-header gate passed its 31 source-size tests, 35 owner/host
  goldens, 2 cap branches and header caps, then failed while building the
  self-host parity manifest at source 161:29:
  `A String element cannot cross an opaque boundary without a current
  non-retention proof`.
- The unchanged canonical program was not rerun for V10. Its last observation
  remains 18,435 errors, 16 warnings and MIR0, first at
  `diagnostic_owner.pgy:573:39` on `ArrayPush(codes, found[j])`.
- A fresh portable replay, immutable V10 export, independent review, current
  bootstrap/gen2==gen3, installed C/LLVM driver and required CI at one
  published SHA remain unestablished.

# Collection program route deleted (2026-11-19)

Source: main `34ae17546` (parent `a9fbfd298`). Tree: WSL `Ubuntu-C-WSL`
`/root/ts2`, detached at that commit. Driver: `make self-host-compiler`
(1227 s), `bin/pgy-self-driver` SHA-256
`6b44116fc0d6dbe5e5b35d61551df4525b1e1cf7ddeed2f92c86320fcf42a304`.

## What was deleted

The direct MIR multi-routine dispatcher claimed a program with exactly three
routines: `Main()`, an `Array<Int>(Int)` producer and an `Int(Array<Int>)`
consumer of four blocks each. It sent that program to 15
`direct_mir_collection_program_*` owners (2,323 lines with their OWNERS,
cap and registry rows). A corpus scan of `tests/`, `examples/` and the
codegen fixtures found one program of that shape,
`src/self_hosted/codegen/fixture/array_param.pgy`.

## Before deleting (scratch driver at `a9fbfd298`, branch removed)

- The general route compiled the fixture and its changed argument on C and
  LLVM: `12 4` and `20 5`, the same as `--native-pipeline`.
- Forged MIR refused on both backends with no artifact: repaired parameter
  ABI (`three-routine structural shape is unsupported`), wrong call target
  (routine admission, stage definition), stale return use and cross-routine
  name collision (expression admission, leaf operand).
- The gate's MIR SHA-256 pin no longer matched the current producer, and its
  mutation script assumed `Main` had three rows (it now ends in a `return`
  row). The gate runs only in the manual `self_host_parity.yml` workflow.

## Gates on `34ae17546`

| Gate | Result |
|---|---|
| `one_mir_array_param_projection.sh` (rewritten) | PASS: C/LLVM under ASan, 4 forged MIR refused |
| `sot_authority_edge_smoke.sh` | PASS: 94 authorities, 210 carriers, CLOSED=70 BRIDGE=22 ACTIVE=2 |
| `self_hosted_component_contract_smoke.sh` | PASS |
| `direct_mir_general_emitter_coverage_owner.sh`, `..._builtin_rows_owner.sh` | PASS |
| `self-host-direct-mir-scalar-array-int-return-test-smoke` | PASS |
| `direct_mir_array_string_cleanup_fact_owner.sh` | PASS (refused 18+4+6+2) |
| `SoTAuthority.v` | not checked locally (WSL `coqc` has no stdlib); CI `formal-proofs-rocq9` checks it |

## Lost property, recorded

The deleted route printed integers directly and freed the returned array
once. The general emitter leaks the two `ToString` results on C and LLVM
and the returned `Array<Int>` (LSan reports it on LLVM; C keeps it reachable
on `main`'s frame). The gate runs with `detect_leaks=0`. This is S1/S2 of
the owned-value lifetime plan, not a new admission.

## Unrelated reds seen in the same tree

- `self-host-one-mir-array-return-projection-test-smoke`: owner cap
  `direct_mir_backend_projection_owner.pgy` 335/330; this commit removes
  nothing from that file.
- `self-host-one-mir-array-argument-projection-test-smoke`:
  `direct_mir_mutation_ownership_contract.sh` expects `--native-pipeline` to
  refuse `borrowed_ref_constructor_store_reject.pgy`; native admits it. A
  probe that calls it twice on a caller-owned record prints `2 5` on native
  C and LLVM under ASan with no memory error (leaks only); the self-host
  default route refuses the probe. Both gates run only in the manual
  workflow.

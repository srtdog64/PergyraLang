# ArrayString C DirWalk target projection result — 2026-08-29

Status: `LOCAL IMPLEMENTATION CANDIDATE`

Exact base: `568c7b07292ee4ddee4e573b4f1ec1db2d2e9f27` on
`origin/main`.

This audit records evidence for one reached executable consumer. It does not
own compiler semantics, registry status, or a successor rung.

## Result

The scalar C `DirWalk` adapter now receives the C-qualified
`DirectMirArrayStringAbiProjection` already derived by the program emission
root. It verifies that projection against the carried
`DirectMirScalarProgramArrayStringAbiFact` and materializes the private
`pgy_as` value with projected designated field names. The old positional
four-field initializer is deleted.

`DirectMirScalarProgramArrayStringAbiProjectionReadyForFact` is now the single
scalar-program fact/projection/target cross-seal. The former C storage and LLVM
collection readiness functions were removed and their consumers call the
shared owner.

## Falsifying evidence

- `direct_mir_scalar_dir_walk_direct_call_owner.sh` executes the nested-path
  program through C and LLVM and checks equal output.
- It rejects DirWalk target-name drift, DirWalk call-syntax drift, fixture
  call-syntax drift, and ArrayString allocator-offset drift. Each requested
  artifact remains absent on failure.
- Generated C contains a projected designated initializer for `data`,
  `length`, `capacity`, and `allocator`; the positional initializer is
  structurally forbidden.
- The component contract requires the shared readiness owner and projected C
  fields, rejects both deleted readiness functions, and rejects restoration of
  the positional adapter.

## Observed local gates

- current-source Pergyra-built DRV-2 installation: PASS
- native-oracle `driver_bootstrap_main.pgy` C emission: PASS, zero errors and
  three pre-existing redundant-`who` warnings
- focused C/LLVM DirWalk parity and four negative mutations: PASS
- full self-host component contract: PASS
- SoT authority edge: PASS, `CLOSED=55 BRIDGE=32 ACTIVE=1`
- SoT live adequacy: PASS; Coq/Rocq explicitly skipped because unavailable
- single-owner, hard-contract, likeness `4493/4493`, and documentation quality:
  PASS

## Registry boundary

The reached program-extension and ArrayString ABI rows now record the C
`DirWalk` consumer, focused gate, positional-layout fallback, and duplicate
readiness fallback. The ArrayString ABI row remains `BRIDGE`: Args, parameter
binding, value-result transfer, owned return, mutation, cleanup, and other
expression materializers still require consumer migration. No `56/31/1`
closure is claimed.

Commit, push, and exact-head CI remain the publication boundary.

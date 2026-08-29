# ArrayString named value-boundary semantic rejection result — 2026-08-29

Status: `PUBLISHED — REMOTE GREEN`

Exact base: `7fbe86c2c44b65c1330a34585ca5db0096595c1c` on
`origin/main`.

This audit records one reached executable source-semantic replacement. It does
not own compiler semantics, SoT registry status, project percentage, or a
successor rung.

## Result

`SemanticAstNamedValueBoundaryVerdict` now consumes the admitted function
signature type/mode and carried expression-place facts after body call-target
resolution. For the reproduced `Array<String>` boundary, `own`, `ref`, and
`inout` arguments whose place is a value fail before MIR lowering. MIR and the
direct C/LLVM program-extension backends do not reconstruct that source fact.

The previous installed self-host producer admitted
`ReleaseOwnedArray(BuildOwnedArray())`, published verified MIR, and depended on
backend extension error 19. The current producer reports stable diagnostic
`named_value_boundary_argument_required` and publishes no MIR. A populated
ArrayString literal follows the same fail-closed route. The native pipeline also
rejects both forms with its named-variable boundary diagnostic and publishes no
C artifact.

Named-local `Array<String>` owner transfer remains admitted, and a fresh
copy-only `String` call result remains admitted. This preserves the native
ownership-class distinction rather than banning all call-result arguments.

## Falsifying evidence

- `direct_mir_array_string_named_value_boundary_owner.sh` rejects both the
  unnamed call result and populated literal in native and installed self-host
  modes without C/MIR artifacts, then emits verified MIR for named ArrayString
  and copy-only String controls.
- `direct_mir_scalar_owned_array_string_parameter_owner.sh` retains C/LLVM
  owner-transfer execution and its negative mutations.
- `direct_mir_scalar_owned_string_parameter_owner.sh` retains C/LLVM String
  lifecycle execution and negatives. Its stale static lookup was corrected to
  inspect the actual split storage-materialization owner rather than an old
  importing file.
- The semantic owner fails closed when required signature, mode, or expression-
  place facts are absent. The SoT registry forbids treating backend rejection as
  source semantics and binds the new executable gate string.

## Observed local gates

- current-source Pergyra-built DRV-2 installation: PASS
- native production `driver_bootstrap_main.pgy` C emission: PASS,
  32,923,787-byte artifact, zero errors, and three pre-existing redundant-`who`
  warnings
- focused native/self-host rejection plus named/copy-only controls: PASS
- existing ArrayString and String C/LLVM ownership controls: PASS
- full self-host component contract: PASS
- SoT authority edge: PASS, 88 authorities and 183 derived carriers,
  `CLOSED=55 BRIDGE=32 ACTIVE=1`
- SoT live adequacy: PASS; Coq/Rocq explicitly skipped because no prover is
  installed on this runner
- single-owner, hard-contract, likeness `4509/4509`, documentation, progress,
  and `git diff --check`: PASS

## Remaining boundary

The new verdict is deliberately exact to the reproduced `Array<String>` type.
It does not claim a generic ownership-class query for every borrow-tracked type.
Multiple or conditional named owner moves, aggregate-result transfer, general
value returns, and the remaining direct-MIR program-extension consumers stay
open. No registry row closes: the census remains `55 CLOSED / 32 BRIDGE / 1
ACTIVE`, and the evidence-reconciled project forecast remains 83%.

## Publication evidence

Implementation `5f77be4fe3601487e5e5b7f6e7b67fb2df5a18b9` is on
`origin/main`. Its first remote run `33257018917` passed every completed
platform, proof, sanitizer, codegen, and backend-comparison job, but
`build-linux` correctly rejected stale generated language-word implementation
counts. The official registry generator changed only `func`, `let`, `own`, and
`return` counts; the 146-row registry smoke passed locally.

Generated inventory correction
`a6f530ff153f6cf9047bcc4d889418a08198f781` is on `origin/main`.
Exact-head full dispatch run `33258296309` completed GREEN 30/30 in 32m39s:
Linux 27m23s, full self-host 32m22s, codegen bootstrap 8m52s, sanitizers
12m27s, Windows 8m36s, Rocq 9 2m09s, backend toolchain 11m14s, and backend
comparison 20/20. The publication lease is released without opening a
successor rung.

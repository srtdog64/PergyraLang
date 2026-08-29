# Array type named value-boundary parity result — 2026-08-30

Status: `PUBLISHED — EXACT-HEAD CI GREEN`

Exact base: `965d4399e46795f583dcbb8b881cd78e626f13a2` on
`origin/main`.

This audit records one reached executable source-semantic replacement. It does
not own compiler semantics, registry status, project percentage, or a successor
rung.

## Result

`SemanticAstNamedValueBoundaryVerdict` now consumes canonical
`SemanticArrayElementType` shape evidence. Every well-formed `Array<T>` passed
as an unnamed `own`, `ref`, or `inout` value follows one semantic rejection;
the verdict no longer owns an `Array<String>` equality or an element-type
allowlist.

At the base, native rejected `ReleaseOwnedInts(BuildOwnedInts())` before C
publication. Installed self-host exited zero and published a 9,682-byte MIR,
then the direct C backend rejected only afterward with
`direct MIR Array argument entrypoint identity is invalid`. The current
self-host rejects at body semantic admission with stable diagnostic
`named_value_boundary_argument_required` and publishes no MIR.

## Falsifying evidence

- The consolidated gate rejects unnamed call results and populated literals
  for both `Array<String>` and `Array<Int>` in native and installed self-host
  modes without C/MIR artifacts.
- Named `own Array<String>` and named `own Array<Int>` remain admitted.
- Default-mode `Array<Int>`/`Array<Bool>` parameters and a copy-only fresh
  `String` call result remain admitted.
- The existing ArrayInt/ArrayBool by-value C/LLVM ABI gate and ArrayString
  owner-transfer C/LLVM gate remain green with their negative mutations.
- The semantic owner imports `array_type_shape_owner.pgy`, contains no
  element-type equality, and the shape contract rejects a missing closing
  delimiter instead of treating malformed text as an Array fact.

## Observed local gates

- current-source Pergyra-built DRV-2 installation: PASS
- native production `driver_bootstrap_main.pgy` C emission: PASS,
  32,923,853-byte artifact, zero errors, and three pre-existing redundant-`who`
  warnings
- consolidated native/self-host Array family rejection and four positive
  controls: PASS
- ArrayInt/ArrayBool by-value C/LLVM ABI and negatives: PASS
- ArrayString owner-transfer C/LLVM parity and negatives: PASS
- full self-host component contract: PASS
- SoT authority edge: PASS, 88 authorities and 183 derived carriers,
  `CLOSED=55 BRIDGE=32 ACTIVE=1`
- SoT live adequacy: PASS; Coq/Rocq explicitly skipped because no prover is
  installed on this runner
- single-owner, hard-contract, likeness `4509/4509`, official 146-row language
  inventory, documentation, progress, shell syntax, and `git diff --check`:
  PASS

## Publication evidence

- implementation: `0c07ccb3161be51268094ae78945ff5dbd20c1ac` on
  `origin/main`
- exact-head push run `33261198946`: GREEN 30/30 in 40m10s
- full self-host: 39m51s; Linux: 24m19s; sanitizers: 12m49s; codegen
  bootstrap: 8m39s; Windows: 8m57s; Rocq 9: 1m52s
- backend toolchain: 11m08s; comparison shards: 20/20

## Remaining boundary

This result closes canonical Array shape parity at the named source boundary.
It does not claim Slice/List/Queue/Set/Map, generic parameter, nominal value,
subject, or resource-handle ownership classification. Backend support for
named owner-handle ArrayInt, multiple or conditional moves, aggregate-result
transfer, and general value returns remains separate. No registry row closes:
the census remains `55 CLOSED / 32 BRIDGE / 1 ACTIVE`, and the project forecast
remains 83%.

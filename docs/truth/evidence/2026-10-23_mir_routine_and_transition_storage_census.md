# MIR routine and transition storage census

Status: RED integration receipt. Observed 2026-10-23 KST.

## Objective and boundary

Retained MIR routine String rows and DIR intent-transition facts must own the
storage they keep after semantic analysis continues. The fact owners are
`SelfMirAppendRoutine` and `SelfDirIntentFactsFromArtifact`; the last reached
consumer is `SelfMirProgramDomainFactsFromReadyArtifact`. Broad `ref`, a
whole-analysis copy, dual reads and weakened admission are forbidden.

Exact source commit: `674fb590302db1f7997c262e873c5fbf971692b2`.

| File | SHA-256 |
|---|---|
| `src/self_hosted/mir/artifact_lower_owner.pgy` | `3c3271f20d20033c19e9e061ea596ca7e07c72c501b0a99da57c64e3b1680ff0` |
| `src/self_hosted/dir/intent_fact_owner.pgy` | `74fc16e0156ef7b7c9c5f4002a751d6c5bb7f3a5a8ed5f6369080e7eb1c8f5b4` |

## Focused evidence

- Component contract: 14 unit checks, source-size caps and generator drift
  passed.
- Instrumented summary probe: `SelfDirIntentFactsFromArtifact` parameter 4
  changed from escape mask `0x9` to `0x1`; the retained program-domain path
  changed from `0x9` to `0x1`; zero hard ownership errors.
- Current native compiler built from the exact source tree. Binary SHA-256:
  `cb0363a4e9889e14fe20b8ff214fcaf8918a2f061fe80e356079ce648e110aba`.
- The String-copy release gate passed on C and LLVM. Removing emitted region
  cleanup made both mutants fail leak detection; a returned-copy control
  remained valid outside the producer region.

## Fixed canonical census

Input: unchanged
`src/self_hosted/compiler/driver_bootstrap_main.pgy`.

Instrumented native checker SHA-256:
`b1a0e7d3c83c50755d93b771dc9ca83d2e7b569598aecf36683edae8838ae31c`.

| Result | Before | After |
|---|---:|---:|
| Final semantic ownership refusals | 319 | 310 |
| Semantic additions | 0 | 0 |
| Semantic removals | 0 | 9 |
| Hard census errors | 0 | 0 |

One unchanged `ArrayDrop` refusal received different import-composed syntax
identifiers. Comparison therefore normalized source locations and the embedded
callable/parameter syntax identifiers; this yielded zero semantic additions
and nine removals. The run took 1028.91 s and peaked at 3,981,132 KiB.

## Remaining boundary

The first refusal is `AstTreeArtifactBindExpressionOwnerKinds` mutating escaped
`atom_table` storage. The full canonical program is not admitted. Bootstrap,
gen2 == gen3, the installed C/LLVM driver and exact-SHA CI were not reached.
This receipt does not claim V1 closure or whole-language memory safety.

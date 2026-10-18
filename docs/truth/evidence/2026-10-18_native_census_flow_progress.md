# 2026-10-18: flow engine progress on the canonical census

Recorded by the Claude lane on branch `claude/p0-native-checker`. Input:
unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`. Method as
in the [first census](2026-10-18_native_refusal_census.md): a scratch
tree with errors downgraded so the analysis runs to its first flow
failure, plus traces of incomplete frame sites. Times are on that scratch
build (WSL Ubuntu, `make -j16`), not a production measurement.

## First flow failure, by commit

| Commit | Flow entries reached | First failing frame | Cause fixed next |
|---|---|---|---|
| `9707a240f` | (stopped early) | `SelfMirIntentExecutionRoutineAppend#1` | scalar input frames, empty-origin stores, `own` rebinds |
| `0f0d53720` | — | `SemanticExpressionGraphFactsFromAstRows#0` | `Clone(...)` had no result relation |
| `05f2fd41d` | 6005 | `MirCodegenGenericSpecializationFacts#1` | loop recheck refused a field move from a re-declared struct |
| `1401c80bd` | 6005 (default cap, no closure above 4096) | same | — |
| `b336f4ce7` | 8390 | `DirectMirScalarProgramIndexedAssignmentRouteFromOwners#1` | open |

## Work cap and time

| Measurement | Value |
|---|---|
| Closures above 4096 before `1401c80bd` | 6; largest 28258 (`SemanticAstCollectionExclusiveClassify`) |
| Largest closure after `1401c80bd` | 2140 units |
| Largest frame node visits in one closure | 27871 |
| Census wall time at `05f2fd41d` / `b336f4ce7` | 206 s / 316 s (more of the program analyzed) |
| Census peak memory | 3.46 GB |

## Falsifiers found along the way

- A self-recursive function that only reads its `ref` input and returns
  fresh storage was refused at `7cd004fab` (fixed in `1401c80bd`).
- A struct passed by value to a callee that only reads it counted as
  moved under `9707a240f` (fixed in `05f2fd41d`).
- Moving a field out of a struct declared before a loop is refused only
  when something makes the checker revisit the body: a loop whose body
  only reads the moved Array is admitted at `7cd004fab` and on the
  branch. No drop is emitted for the moved local today, so nothing frees
  twice at runtime.

## Unchanged counts at `b336f4ce7`

Opaque-boundary String sites: 767. "Array argument call requires
preserved array storage": 1475. Both still come after the first flow
failure, so the cascade inflates the second count.

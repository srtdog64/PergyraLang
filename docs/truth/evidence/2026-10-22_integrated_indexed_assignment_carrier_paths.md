# Integrated indexed-assignment carrier paths

Date: 2026-10-22 KST. This is an immutable run record, not a semantic owner.

## Candidate and invariant

- Base: local `main` `5c8e0ac57`.
- Product diff hash: `0604d9ba6cd999805d7e77793b1c13728c51b400`.
- The final move owner keeps an indexed target's backing alive while its
  index and value are evaluated and until the store completes.
- When that backing is reached through a shallow-copy row, traversal uses
  both the source field path and the carrier field path. It must neither
  miss the copied field nor treat an unrelated carrier field as an alias.
- The traversal consumes the existing copy owner's rows. It does not create
  another alias or effect authority.

## Focused and integration gates

The fresh integrated compiler was built from the candidate at
`/tmp/pgy-root-loan-20261022-b/bin/pgy`, SHA-256
`a7d232ce4652ab797653bed9b33db50cce2725529d3bd1e141bb0f070d649975`.

| Gate | Observed result |
|---|---|
| Native C/LLVM compiler build | PASS; 157.99 s wall, 175980 KiB peak |
| Indexed assignment/member/receiver ABI gate | PASS; 8.84 s wall, 71052 KiB peak |
| Function parameter flow integration | PASS; 382.17 s wall, 254588 KiB peak; exceeds the 5-minute focused budget |
| Semantic units | PASS, 3240 passed and 0 failed |
| Transpiler units | PASS, 1014 passed and 0 failed |
| Documentation, manifest, truth caps and `git diff --check` | PASS |

The indexed-assignment negative programs are refused before C or LLVM
artifact publication. The same-backing read-only control runs on both
backends and prints `7`.

## Fixed canonical census

The existing scratch-only census instrumentation was applied to a private
copy, not the product tree. Input and conditions were unchanged:

- `src/self_hosted/compiler/driver_bootstrap_main.pgy`;
- native C pipeline, every final owner run, 4096 work budget;
- three landing reanalyses followed by the final analysis;
- no safety condition, input, timeout or memory limit was weakened.

Result:

- 350 final ownership refusals: 42 in `direct_mir_*`, 308 elsewhere;
- 102 move refusals, 57 borrowed-ref escapes, 3 opaque crossings,
  8 released-element uses and 23 escaped owned-string pushes outside
  `direct_mir_*`;
- 0 hard errors and 0 flow-store failures;
- 1801.23 s wall and 3979288 KiB peak. This exceeds the 30-minute
  integration-shard budget by 1.23 s;
- compiler metrics: 14642 flow entries, 15307 body evaluations,
  531726 cache hits, 8363 fixed-point passes and 551 work units;
- instrumented compiler SHA-256
  `b1a0e7d3c83c50755d93b771dc9ca83d2e7b569598aecf36683edae8838ae31c`.

The normalized final refusal set is byte-for-byte equivalent to the prior
completed 350-refusal run (`only-a=0`, `only-b=0`). The integration therefore
adds no canonical ownership regression, but it also does not advance the
fixed census toward zero.

## Boundary

The ordinary unchanged canonical compile still emits no artifact. Bootstrap,
gen2 == gen3, installed-driver C/LLVM execution and exact-SHA remote CI were
not run because the V1 ownership admission prerequisite remains red. This
record establishes only the reached indexed-assignment seam and its regression
boundary; it is not a whole-language memory-safety or V1 closure claim.

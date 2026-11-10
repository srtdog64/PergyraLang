# Installed Array retirement, r12 observation

Observed against shared HEAD `60fd1e26d8ea3987efbcbfc65f59b892b234701a`
plus preserved dirty work; isolated source and binaries are authoritative for
this receipt. This is not V1, fixed-point, CI or whole-language closure.

## Fixed identities

- Source: `/root/pgy-owner-production-20261107/candidate-r12`.
- Source manifest: `r12-dispatch/source.json` under that experiment base.
- Codegen seed: `954bd3a7905dbbd42bb7fd438d1b76ff101d55df8754dc6b6d10e2f6097a6f7e`.
- Official `self_host_compiler_build.sh` installation: 267.086 seconds, exit 0.
- Installed driver: `2bf31b405b6a276dd30748b17260d99f7be764e5851fce303cd7fd47bc366489`.
- Launcher: `b7074d708abaa5af09922ff8e78a858f8219fdcc4c8fd4c3dd6eec719b500adb`.
- Machine manifest: `44872a80153f54b29a207a5b96abd746ef289b0ebbfdc4220bdfd28c8789b4f2`.
- The launcher reused unchanged earlier native objects; some compiled runtime
  paths point to r7. Its exact binary and dependency trees were retained.
  This receipt does not call it a clean whole-r12 native rebuild.

## Observed behavior

The original eight inputs are fixed_owner, fixed_view, radix_sort, robin_hood,
dijkstra, astar, union_find and lru. Their original source/support/raw hashes
are recorded in `r12-dispatch/installed-observe/inputs.json`.

All 16 installed C/LLVM builds passed. The runner checked 251 seed results per
binary and performed a separate 503-iteration allocation observation. Across
the 16 Pergyra executables: 29,174 allocations, 29,174 frees, zero live blocks.
The raw-C controls are excluded from these totals. See `admission.json`,
`results.json` and `commands.jsonl` in the same observation directory.

The previous r11 installation admitted 15/16. Its C fixed_owner and union_find
artifacts retained 503 and 1,006 blocks, respectively; fixed_view C failed to
emit. The existing C dispatcher required spelling/loop identity pressure to
select the shared GraphPlan. Ordinary indexed Arrays entered MIR-to-AST and
lost retirement. R12 consumes the existing typed routine/expression-family
claim and keeps claimed failures terminal. It does not weaken ownership rules.

The installed array_index_type_semantic_admission_owner.sh gate passed in
`r12-dispatch/route-controls/`. Its preceding Windows-binary invocation was a
harness failure, retained separately; it was not a compiler safety verdict.

## Remaining obligations at this checkpoint

R12 regressed C's single external-function Main: its new general claim reached
an old declaration-empty guard. R13 repaired that guard through the existing
foreign ABI owner and re-executed the original C result (-17). R11 and R13
route censuses preserve Float/mixed-Array-Float C output. Slice-ref and mixed
Slice-runtime installed rejection existed in r11; they are not newly green.

Performance is not green here. R13's full unchanged timing matrix passes C
(0.873--1.137x) but fails LLVM (1.503--6.303x), using the fastest of three C
baselines. Runtime joint optimization and width-correct Int operations are
being validated in later isolated generations; their results cannot be inferred
from this retirement receipt. No commit or push was performed by this lane.

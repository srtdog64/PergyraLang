# String allocation CI consumer correction

Observed on published `1ff353e890021c33ca7fe1a513df8502c9e32974`,
CI run 44063602879. The Linux push shard completed 70 steps and failed
only step 22, `phi_dominance_reuse_owner.sh`: its emitted C includes
`pgy_runtime_panic_contract.h`, but the gate compiled without runtime
include paths. The new String allocation dependency exposed a missed
artifact compiler consumer. This was an integration regression.

The codegen bootstrap, new 22-case String fault/control matrix, native
oracle and driver bootstrap jobs passed. At this record the fixed-point
join was still running. The run is not green.

The phi gate now consumes the existing emitted-C runtime-header policy
and adds its include paths/thread flag when the artifact requires them.
No compiler policy, phi admission, negative, timeout or memory limit changed.

The validation checkout is WSL `Ubuntu-C-WSL`, `/root/mg-main`, base
`5b0d354fb` with the explicit source mirror. Its 2,842 production Pergyra
files and installed driver still match the
[String boundary source generation](2026-11-17_selfhost_string_allocation_boundary.md).
The in-progress role receiver sources in `D:\PergyraLang` were not
mirrored, staged or claimed as tested by this correction. Its future
fixture was preserved outside the frozen validation inputs.

Local results:

- Before: the unchanged gate reproduces missing-header compilation failure,
  `.tmp/self_hosted/phi-dominance-reuse.Vuqk8D/`.
- After: `.tmp/self_hosted/phi-dominance-reuse.OJxyEj/` passes SSA boundary
  checks, C/LLVM replay, corrupted dominance reuse refusal, uncovered
  predecessor refusal, and equal execution value 2.
- The frozen source manifest, generated keyword inventory and build-source
  inventory checks pass. Shell syntax and publication documentation checks
  run on the final staged slice.

Raw logs: `.tmp/selfhost-memory-20261117/string-ci-phi-{before,after}.log`
and `string-hotfix-*`. The correction contains no production Pergyra
change. The already recorded RSS and fixed-point results are reused only
for that identical source/executable generation.

Publication and the correction's exact-SHA CI result remain separate.
The full DRV-2 memory goal is still open; an OOM diagnostic is not completion.

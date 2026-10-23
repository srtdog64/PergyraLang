# Native String copy: physical release witness

2026-10-23. Local implementation evidence, not publication or V1 closure.
The user assigned this lane small commits and verification, leaving the full
push and exact-SHA remote CI to the main integration worker.

## Inputs and provenance

- Reference: native source snapshot `dca7fd0ce`, exported to the non-Git
  temporary directory `/tmp/pgy-cleanup-review.BMN5yu` in `Ubuntu-E-WSL`.
- Candidate: that snapshot plus the String region producer/consumer changes
  accompanying this record. The intervening `8fb4b7998` changes only the proof
  spine and its tests/docs. No installed binary or worktree was replaced.
- Reference executable SHA-256:
  `4997bc8cbd6377a1216a5b3911d7db3a83d53c3c4703d620a74250433c031217`.
- Candidate executable SHA-256:
  `4e74047e4f0366dce479c6c4204f8de9ef0c9ce418dfd3d3e3b658ce33526a52`.
- Native build: GCC, LLVM 18.1.3, `make -j2 bin/pgy`. Runtime tests use Clang
  18, C with ASan/UBSan and enabled leak detection, LLVM with LSan. LLVM is
  **not** claimed to have instrumented load/store ASan coverage.
- Fixed input: `tests/cases/backend_compare/region_string_clone/main.pgy`;
  100 calls, three String copies per call, alternating early and normal return,
  no source drop or lifetime annotation. Independent output: `copy-` repeated
  100 times followed by `150` and a newline.

## Observed results

| Check | Reference | Candidate |
|---|---|---|
| C physical leak check | 900 bytes / 300 allocations leaked | PASS, no reported leak |
| LLVM physical leak check | 900 bytes / 300 allocations leaked | PASS, no reported leak |
| Independent expected stdout | normal execution exercised | PASS on both backends |
| Remove emitted region destruction | reference has no such cleanup | both mutants detected by leak sanitizer |
| Return a copied String out of its function | existing heap lifetime | same output, no producer-region allocation/destruction |

Other candidate gates: semantic region unit, verified-plan unit, real C/LLVM
consumer fault injection, region backend wiring, runtime region arena, and
`test-transpile` (1,014 passed, zero failed). The arena budget witness
intentionally aborts on its negative case. Runtime header deprecation warnings
remain; these runs were not warning-free.

Formal checks performed in this lane before the runtime delta:
`proof_spine_smoke.sh` (six modules), full `coq_kernel_check.sh` (87 modules,
two existing approved axioms), and `coq_kernel_check_selftest.sh` all passed.
The new cleanup-spine consumer rejects missing/weak contracts. The core model
still has its existing nested-`SVal` warnings. These are model checks, not a
production C/LLVM refinement proof. Documentation quality also passed.

Logs are retained locally under `.tmp/ownership_cleanup_followup_20261023/`:
`string_clone_reference_final.log`, `string_clone_final.log`,
`clone-unit.log`, `clone-build-final.log`, `clone-regression-final.log`,
`proof_spine.log`, `kernel.log`, `kernel_selftest.log`, `docs_final.log`.

## Deliberate limits and broader failures

This is function-exit reclamation of certified synchronous String temporaries.
It does not reclaim ordinary local/returned Strings, aggregate fields, or all
copies inserted by ownership automation. An unbounded loop inside one function
can still grow its region until that function returns. The current native
producer and cleanup consumers are not self-host substitution evidence.

Broader checks were not hidden or weakened:

- `aggregate_storage_transfer_smoke.sh`: 12 mismatches on both reference and
  candidate. Ten are admissions where the old gate expects refusal; two are
  `PGY_SEM_BORROW_ESCAPE` at line 18 instead of the older expected location.
  This is not proof of 12 new unsafe behaviors. For example
  `post_field_push_negative` now emits an independent Array clone before the
  store, consistent with the October 21/22 copy decisions. `duplicate_array`
  still emits shared descriptors for a read-only pair. The main owner must
  distinguish legal deferred copying from missing ownership/retirement facts.
- `nested_array_copy_smoke.sh`: both backends print
  `7,7,8,alpha,alpha,beta,0,0` (one item per line) on reference and candidate;
  the old gate expects the source row's later mutation to remain visible in
  the outer value. This conflicts with the October 21 local-copy contract.
  Its expected-output contract needs a separate, behavior-preserving repair.

The broader logs are `aggregate_storage_reference.log`, `aggregate_storage.log`,
`clone-nested-reference.log` and `clone-nested-regression.log` in the same local
evidence directory. Required CI, canonical admission, bootstrap and installed
driver parity were **not** run here and are not reported green.

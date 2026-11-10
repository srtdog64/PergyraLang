# Raw View memory bridge: compiler facts, kernel proof and physical release

Observed 2026-11-05 KST. This is a bounded native bridge, not V1 or public
raw-pointer closure. No safety rule or existing refusal expectation changed.

## Fixed input, source and executables

- Main HEAD: `60fd1e26d8ea3987efbcbfc65f59b892b234701a`, main ahead of
  the locally recorded origin/main by 281 commits. The shared tree was already
  extensively dirty/staged; no staging, commit, push, reset or cleanup ran.
- Base source came from `/root/pgy-v1-live-20261105-current` into the separate
  `/root/pgy-raw-bridge-20261105/source` snapshot. The initial source-manifest
  SHA256 is `fe6f6d6dfbf3bbaaf0c2c179ff6b1a9452a8b804a75944e5813437df890f7b61`.
- Baseline native executable: `/root/pgy-raw-bridge-20261105/baseline-pgy`,
  SHA256 `c6c04a51a8f315dcfc4fbddb2efacb5e2ba5170eb5827a9fe6295bc4514a66f6`.
- Candidate native executable: `source/bin-bridge/pgy`, SHA256
  `7def2d6b7e06adde9cdf70238f76fccac52206605b369de813e7c971e7c090dd`.
- Candidate Pergyra MIR reader: `/root/pgy-raw-bridge-20261105/mir-reader`,
  SHA256 `bcde0b132f08b6a44292a9e3ff81e10e78e41a30ffc016c2b3b1686af030f76c`.
  It was compiled from that snapshot's `src/self_hosted/mir_lower/main.pgy` by
  the fixed native C pipeline: zero errors, 15 existing unreachable warnings,
  and a GCC large-source column-tracking note. It is a fresh component binary,
  not a replacement for the installed driver.
- Final gate evidence: `.tmp/raw-view-memory.b6kMiw/`. Its source-manifest
  SHA256 is `41396da218bd00e2e38a3dd011d3090134357357f7be0df85649cb84c1466970`;
  the fixture/proof/consumer manifest SHA256 is
  `16ed02388fbfa7aa8380e3f0f41a106e4a0c106faa57e3151ce5bd5efe26f714`.
  The gate rechecked source, executable and consumer hashes after execution.
  All 15 edited existing compiler/runtime files in the shared tree matched
  this validated snapshot at handoff. Their preserved baseline copies are
  `/root/pgy-raw-bridge-20261105/patch-base/`, checked against the initial ledger.

## Whole chain and repair

The fixed input is `tests/cases/raw_view_ownership/branch_exit.pgy`. A fresh
`Array<Int>` backs a `Slice<Int>` beginning at cell 2, with three cells. Writes
through the Slice change the Array; true/false paths return 75/100.
There is no manual drop.

The native graph materializer already issued `source_binding_syntax_id` for
identifier leaves, but JSON omitted it. JSON now projects that fact and each
instruction's existing binding ID. Formal-only `binding_syntax_id` and
`binding_kind` retain their previous meaning. The graph digest contract did
not change; the projection checks origin IDs independently of that digest.

The existing MIR cleanup owner previously considered String-array origins
only. It now also admits a fresh, typed scalar Array literal whose uses and
local Slice aliases remain private to the same dominance/iteration region.
It rejects the cleanup grant for escape, unknown use, reassignment, defer or
unsupported aliasing. This is conservative cleanup eligibility, not a new
source-level rejection or a guessed ownership annotation.

The existing exit synthesis/validator issues scalar retirement kind 3 with a
carried `MIRResource` `ReleaseStorage` row. C and LLVM consume that admitted
row; neither backend decides ownership or reconstructs a drop symbol from a
type name. LLVM's typed exports delegate to the existing raw backing release.
Both backends evaluate scalar return values before retirement.

The Pergyra strict graph reader validates the added native identity separately
from formal bindings. Reconstruction retires this generation's native ID.
Its cleanup reader requires the typed fresh DEF, binding identity, exact SSA,
carried ABI and existing exit contract. Duplicate retirement identity is
`(block, binding, target)`: alternative returns each have a duty; duplicate
rows on one exit refuse. String kind 1/2 validation remains separate.

## Observed gates

- `tests/raw_view_memory_bridge_smoke.sh`: PASS, two CFG paths. Seven corrupted
  provenance/range/cleanup certificates refused; seven corrupted Pergyra-reader
  rows refused, including laundering a String retirement into scalar kind 3.
  The reader reconstructed the valid input and all five scalar Array layouts.
- C output and LLVM output: `75\n100\n`. Actual allocator observations:

  | Backend | Initial allocations | Reallocations | Final frees | Live backing | Freed final capacity |
  | --- | ---: | ---: | ---: | ---: | ---: |
  | C | 2 | 0 | 2 | 0 | 88 bytes |
  | LLVM | 2 | 4 | 2 | 0 | 128 bytes |

  C allocates the literal's 11 cells directly. LLVM grows before creating the
  View. Both use the same retirement fact after the last access. Address and
  undefined-behavior sanitizers were enabled on emitted C and on the runtime
  linked to LLVM. No sanitizer diagnostics occurred in valid runs.
- Generated-artifact falsifiers: suppressing retirement produced a LeakSanitizer
  leak in both backends; moving retirement before a Slice read produced an
  AddressSanitizer heap-use-after-free in both. These controls never edit the
  compiler or weaken source admission.
- `RawViewMemoryBridge.v` SHA256:
  `2859bd72e3d3e701271efcd82f0060604f31de17f24ea9ce12ec1505063e121a`.
  The source-MIR witness plus four imported modules kernel-checked with zero
  axioms. Theorems cover bounds, write visibility through every alias to the
  same cell, preservation of other cells/extent/owner, erasure to checked raw
  traces, canonical ownership-heap retirement and refusal of a second exit.
- Existing Rocq was used at
  `OPAMROOT=/home/c/.local/share/pergyra-rocq/opam`: Rocq 9.3.0, Stdlib 9.2.0.
  No install ran. Full `formal_semantics_smoke.sh`: PASS, 78 source proofs,
  90 kernel-checked modules plus the approval consumer. Only the existing two
  approved Slot abstractions remain; no new axiom/admit/unsafe kernel feature.
  Log: `.tmp/raw-view-bridge-20261105/full-formal.log`.
- Twelve source fixtures on C and LLVM: 24/24 admission/refusal and execution
  checks passed. They include safe local borrowing, return-borrow refusal,
  explicit release while a View is live, helper retirement followed by use,
  conditional retirement and loop retirement. An outward View is an allowed
  lifetime control: this local-private pass must not prematurely free it.
  Log/rows: `.tmp/raw-view-bridge-20261105/regressions.log`, `regressions/results.json`.
- `scalar_arrays.pgy`: Int/Long/Float/Double/Bool writes and returns agree on
  C/LLVM, with no ASan/LSan/UBSan diagnostics. `raw_escape_contract_smoke.sh`
  still rejects `SlotRawPointer`. `collection_ownership_fact_projection_smoke.sh`
  passes its stable borrowed/owned/literal/empty-origin rows. The reached
  Pergyra file caps (679/680 and 300/300 lines) and scoped diff checks pass.

## Existing red gate and remaining boundary

`loop_local_collection_cleanup_smoke.sh` remains RED on
`loop_local_collection_cleanup_join_shadow_invalid.pgy` for both backends.
All preceding seven allowed and four refused cases passed per backend.
The fixed baseline also compiles this last fixture on C and LLVM. It resolves
to declared Join (ID 2), whose current summary publishes `mask=0` and
`array_preserved=3`; the pre-existing restored value-result permission admits
its inout owned-String push. This is an existing expectation/summary conflict,
not permission to rename it green or reject a safe call arbitrarily. The
fixture, expectation, summary and safety rules were left unchanged and the
main chat was notified. Whole regression/CI closure is therefore not claimed.

Public raw-pointer syntax/ABI, arbitrary foreign mutation/concurrency, owning
returned-Slice cleanup, allocator-pool lifetimes, all factory-produced Arrays,
installed C/LLVM drivers, gen2==gen3, canonical compiler-input closure and
exact-published-SHA CI are not established here. The Rocq importer is a checked
bounded projection, not a verified whole-compiler or C/LLVM refinement theorem.
Main integration remains open; the main chat was actively working on its
separate arena/provenance performance gate at the last compact status check.

To repeat the bridge, set the three fixed paths and existing OPAMROOT, then run
`bash tests/raw_view_memory_bridge_smoke.sh`. To build its reader, run the fixed
native compiler from the source snapshot with
`--native-pipeline --backend=c src/self_hosted/mir_lower/main.pgy -o ../mir-reader`.
The reader receives a relative MIR path from the gate's fixed working directory.

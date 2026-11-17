# Self-host String allocation failure boundary

Observed on 2026-11-17 after published main `3a0a385b7050ecedc3bfe9e1517bd84bd1f9bd4d`.
That parent's CI run 44045107368 completed all 33 jobs successfully, including
codegen bootstrap, driver bootstrap, native oracle and fixed-point join.
This record describes the subsequent local String repair; its exact-commit
publication and CI are not inferred from the parent's result.

## Source and artifacts

Edits are in `D:\PergyraLang`; validation uses the existing WSL
`Ubuntu-C-WSL`, `/root/mg-main`. That checkout has base `5b0d354fb` plus
the explicit source mirror. All 2,842 production Pergyra files matched
between the Windows and WSL trees. The sorted relative-path to normalized
UTF-8 SHA-256 map has digest `d33bb61b1c21a2ee5dc9a7892e1f4554c4bfddd32fffc81831c47ed735446619`.

| Artifact | SHA-256 |
|---|---|
| Native compiler | `f44310784eb710ac708abea794e9db517b94a1f08096b2088e55c62ad628878a` |
| Pergyra codegen gen2 executable | `d8d5b4834f9d592132ec922eaf193dba7ecfc58243c9a51d8105a2019adad0b9` |
| Codegen gen2.c == gen3.c (123,805 lines) | `f79dbf36a30a9efc227271fd106c3976f3468ba12f0b175a8d5cfa8290ed4885` |
| Fresh installed driver | `74e0dcb2ea2fe8c2ac5d44d357ec4eadbf067b2a24e6cad4b13b9a890f35bb79` |

The map reproducer and raw run logs are under the local run directory
`.tmp/selfhost-memory-20261117/`; artifacts are not semantic authorities.

## Reached cause and repair

After the A1 array guard, full DRV-2 still crashed. The retained
`a1-final-drv2-gdb.log` shows malloc returning NULL (`rax=0`) immediately
before a concat byte store in `SemanticAstCollectionExclusiveCopyLandingFor`.
This is a resource failure, not evidence of a freed JSON fragment.

The existing `string_runtime_owner.pgy` now owns checked extent addition,
extent multiplication and allocation, using the existing OOM panic contract.
Every allocating primitive carries its own guarded dependency; the ten
independently selected C blocks compose with one implementation per C unit.
The core's Bool conversion and Replace compose these blocks as before.
Public String signatures, empty results and malloc/free ownership are unchanged.
The runtime owner remains within its 320-line cap. No cap or process budget
was raised, and no user annotations, manual drops or new semantic owner were added.

The first integration mistakenly installed the dependency only at three
composition roots. Individually reused ToString/case/window/trim blocks then
failed C linking. The complete map includes AST program emission, scalar-CFG
concat, and scalar-program builtin, window, trim and collection materializers.
Dependency emission now belongs to the allocating producers themselves;
the three orchestration edits were removed.

## Executed falsifiers and integration

- `string_allocation_failure.sh` uses the actual Pergyra codegen gen2 seed.
  All 22 String branches preserve their normal values and report typed OOM
  when malloc is forced to fail. Removing only the emitted allocator's NULL
  guard makes all 22 controls terminate with SIGSEGV. Exact extent arithmetic
  passes, and three overflow cases refuse before reaching malloc.
  The matrix is Linux-specific, at `.tmp/self_hosted/string-allocation.6T2j27/`.
  Linux argv initialization is allocation-free and Args uses realloc, so
  the forced malloc failure reaches the selected String operation.
- Five existing direct-MIR families pass C/LLVM values and their forged-row
  negatives: case/math, String windows, trim, collections, concat/equality.
  Logs: `a1-string-consumer-*.log`, final collection/concat suffix `-v3`.
- Two stale source-MIR fingerprints in those harnesses were corrected only
  after the preserved baseline driver and candidate produced identical bytes:
  collection `f4bc0181f4c6f485a065e753153215b053bf1c79e0df66b5f903106a31e4e43d`,
  concat `8b64ad6af1bace171912ec061b9fe00b3b66bf66eba2b26b360d238b2c3b3bf9`.
  The collection LLVM leg now links the existing runtime using the owned
  platform profile; concat C uses the existing runtime-header include policy.
  Semantic mutations, runtime expectations and rejection checks were preserved.
- Codegen gen2 == gen3 and fresh installed-driver build/smoke pass.
  The native codegen build retained 17 existing unreachable-statement warnings.
  Clang target-triple warnings were visible in the passing parity legs.
- Component cap/generator gate, SoT registry, source inventory, keyword
  inventory and JSON-projection generator checks pass. Publication checks
  still have to run on the final staged files.

Two extra Array<String> probes, indexed access and mixed foreach, are refused
before artifact compilation by both the preserved baseline and candidate.
They are retained baseline failures, not green coverage or newly fixed routes.
No new implementation track was opened for them.

## Measured boundary

The current complete memory gate passes in
`.tmp/self_hosted/selfhost-memory.ZCxJ69/`:

| Phase | Peak KiB | Ceiling KiB | Result |
|---|---:|---:|---|
| Complete semantic MIR consumer | 797640 | 819,200 | PASS |
| Complete MIR-lower source-to-C, one process | 5557468 | 8,388,608 | PASS |
| Complete MIR-lower source-to-MIR-to-C | separate phases | existing boundary | C bytes equal |

Both MIR-lower C artifacts have SHA-256
`a8351f2ae908d3ba9f7cc459fd742cd375f3adf4ac45b15113ee4c7760c97b31`.

The full DRV-2 source-to-C run remains FAILED at the unchanged
14 GiB address-space / 30 minute boundary. It now reports `class=oom`,
SIGABRT / child exit 134, peak 14,098,004 KiB, rather than SIGSEGV.
No C artifact was published. GNU time reports 472.87 seconds; the owned
process receipt reports 435.36 seconds. No wall-time improvement is claimed.
The receipt records a drained process group; it correctly fails the success
validator. Logs and receipt: `a1-string-full-drv2.*`.

## Limits and next obligation

This repair prevents the reached String NULL stores. It does not reclaim
the accumulated unreachable values or establish compiler-wide leak freedom.
The generated private File/ReadFile runtime still has unchecked allocation
sites; those are outside the 22 String-primitive claim. The existing JSON
escaper temporaries and Clone carrier limitations also remain open.

Continue the [memory plan](../../agent_work_directives/selfhost_compile_memory_plan_2026-11-17.md):
role receiver ABI into the general emitter, then owned-value/retire production,
verification and String/array retirement. Preserve `alias_read_after`'s guard.
A scoped role inventory of the frozen DRV-2 MIR found 953 declarations and
15 roles, all targeting subjects; this is planning evidence, not admission.

Clarification of the previous A1 record: 21:27.71 was the elapsed time of the
whole C+LLVM release writer gate, not the LLVM component alone. That gate's
native LLVM release build failed its optimizer assertion. The separate LLVM
dev writer/golden leg passed with 23 preexisting warnings; neither result
establishes default-profile native LLVM release success.

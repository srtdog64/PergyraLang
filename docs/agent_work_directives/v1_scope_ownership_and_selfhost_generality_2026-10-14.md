# V1 scope: ownership automation and self-host generality

Status: `ACTIVE` (2026-10-14)
Base revision: `c08dfc6e9` (origin/main)
Decision owner: repository owner

## Decision

V1 is two things, and nothing else:

1. **Ownership automation.** Ownership-based automatic memory management
   compiles the compiler's own canonical program with no manual drops.
2. **Self-host generality.** The installed self-host driver compiles every
   program the native compiler admits, on both C and LLVM.

After V1, ownership errors are found by using the language, not by building
more rules ahead of use. New keywords are out of scope. The language surface is
complete: all 145 registry words are parsed by both the native and the
self-host parser (`language_word_implementation_inventory.generated.md`).

## Finish line

All of the following hold on the same published commit.

1. **Bootstrap.**
   - The installed self-host driver builds from main.
   - gen2 == gen3.
   - Required compiler CI is green, with the bootstrap and installed-driver
     jobs actually executed.
2. **Ownership.** The ownership finish line (`AGENTS.md`, V1 Scope) holds:
   - zero ownership refusals on the fixed DRV-2 input;
   - the unchanged full canonical program is admitted;
   - positive and negative gates are preserved.

   Weakening a safety rule to reach zero does not count.
3. **Generality.**
   - The installed driver compiles the native-admitted corpus
     (`tests/cases/**`, `examples/**`) on C and LLVM, with runtime-equal output
     against the native oracle.
   - No program gets a `CODEGEN ERROR` route refusal.
   - Each backend has one general MIR-consuming emitter, organised by MIR
     instruction kind, expression node kind and ABI layout row. There is no
     per-program-shape admission.
   - The `direct_mir_*` shape owners and the MIR → AST text round trip are
     deleted as the general emitter replaces them.

SoT rows close only as these three steps pass through them. The row count is
not a goal. A `projection.direct_mir_*` row retires with its route, not by
being promoted to `CLOSED`.

## Where the self-host compiler stands (measured 2026-10-14, origin/main)

- **The public launcher routes every plain compile to the self-host driver.**
  This covers C and LLVM, binaries, `--emit-c` and `--emit-llvm`
  (`src/pgy_driver.c`, `src/compiler/driver_self_host_selection_owner.c`).
  The native pipeline runs only with an explicit `--native-pipeline`.
- **The driver is large, and mostly program-shape code.** The driver entry is
  `src/self_hosted/compiler/driver_bootstrap_main.pgy`. Its import closure is
  1,912 files and about 270,000 lines.
  - 1,004 of those files (about 97,000 lines) are `direct_mir_*`
    program-shape owners.
  - The general pipeline, `mir_lower` + `mir` + `codegen`, is about 68,000
    lines.
  - `semantic` is about 62,000 lines.
  - The shape owners are named by program shape, for example
    `direct_mir_scalar_program_bool_two_array_string_two_array_int_value_result_policy_owner.pgy`.
    Every newly admitted shape adds files, so the route grows with the number
    of type and position combinations and does not converge.
- **C path.** The steps are:
  1. `CompileSourceToCVerified` runs source → self-host semantic → MIR JSON.
  2. `CompileMirJsonTextToCForTargetVerifiedObserved` first tries the claimed
     shape emitters (`DriverRung2ScalarCSubstitutionIfClaimed`, then
     `DriverRung2NestedIntentCSubstitutionIfClaimed`).
  3. For any other program it rebuilds an AST tree text from MIR, re-runs
     semantic analysis on it, and emits C through the AST codegen.

  That round trip is the text re-parse that
  `docs/self_hosted/14_target_compiler_world.md` rejects as a handoff.
- **LLVM path.** `DriverRung2PublishMirPayloadLlvmArtifactForIdentity` calls
  only the direct backend. A program no shape claims ends in a typed
  `CodegenRouteRefusal` (`CODEGEN ERROR: ...`). Self-host LLVM is therefore
  limited to the claimed shapes.
- **MIR is not yet self-sufficient.** Native MIR has 10 instruction kinds, and
  its instructions still hold AST pointers (`expr0`, `expr1`, `ast`, match
  patterns; `src/compiler/mir_types.h`). The native C backend is still mostly
  AST-driven by size:
  - `transpiler_*`: 1.8 MB, against 0.5 MB for `transpiler_mir_*`;
  - LLVM: 2.3 MB non-MIR, against 0.3 MB for `llvm_mir_*`.

  A general MIR emitter therefore needs MIR, and the MIR JSON the self-host
  reads, to carry everything emission needs. This is the
  `docs/193_mir_only_abi_first_backend_closure.md` track.
- **Bootstrap.** The bootstrap has been red since 2026-10-02 at the DRV-2
  frontier (`borrow_boundary_escape` / `unproved_exclusive_member_mutation`).
  No installed driver can be built from main, so no generality claim can be
  measured yet.

## Order

Steps 1 and 2 are P0 and P1 in
`compiler_closure_priorities_2026-10-14.md`, which owns their dependency order
and lanes. Generality comes after them, in the slot that document calls the
next production self-host substitution rung (P3). It is defined here as one
general emitter per backend, not as more admitted shapes.

1. **Bootstrap green.** Every later step needs an installed driver to measure
   against.
2. **Ownership finish line**, on the fixed DRV-2 input and the canonical
   program.
3. **Generality, C first.** The AST codegen stays as the reference while the
   general MIR → C emitter grows. Shape owners are deleted family by family
   once the general emitter covers their corpus, and the MIR → AST round trip
   is deleted last.
4. **Generality, LLVM.** Same method. Route refusals go to zero.

## Measurements to track

- `direct_mir_*` file and line count inside the driver import closure. It
  should only fall.
- Native-admitted corpus programs that the installed driver compiles on C and
  LLVM with runtime-equal output, against the corpus size.
- `CODEGEN ERROR` route refusals over that corpus. Target 0.
- Ownership refusals on the fixed DRV-2 input. Target 0, with safety gates
  preserved.

## Out of scope for V1

- New language words.
- Ownership precision beyond what the canonical program needs. Those cases are
  found in use after V1.
- Performance work, unless it blocks a step above (see `AGENTS.md`,
  Closure-Blocking Optimization Policy).
- SoT rows that none of the three steps passes through.

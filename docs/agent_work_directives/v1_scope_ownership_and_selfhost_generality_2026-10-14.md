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

  **The AST fallback is closed to programs that carry ownership facts.**
  `driver_rung2_scalar_c_substitution_owner.pgy` forces the direct route
  whenever a routine carries collection ownership facts. Rebuilding cleanup
  from the AST would throw the MIR fact away. If no shape claims such a
  program, the route admission ends in `Die`. Each ownership fact that
  ownership automation adds therefore ties C, too, to the shape list. This is
  where the two V1 goals meet.

  The rule for extending: when a program fails, add the missing MIR
  instruction kind, expression node kind or ABI row to the general emitter.
  Never admit the failing program's shape.
- **LLVM path.** `DriverRung2PublishMirPayloadLlvmArtifactForIdentity` calls
  only the direct backend. A program no shape claims ends in a typed
  `CodegenRouteRefusal` (`CODEGEN ERROR: ...`). Self-host LLVM is therefore
  limited to the claimed shapes.
- **The general emitter is smaller than the shape code it replaces.**
  - **The input exists.** The self-host direct route reads only the admitted
    MIR JSON (`MirMachineLayerAdmittedJsonInput`). For the shapes it claims,
    that JSON already carries everything emission needs: records, arrays,
    strings, options, enums and nested intents. The C fallback rebuilds an AST
    because the general codegen it reuses is AST-based. Nothing on that route
    names a fact that MIR lacks.
  - **Still to measure.** Whether the JSON carries enough for the whole
    native-admitted corpus is measured on the installed driver, not assumed.
  - **The work is bounded.** MIR has 10 instruction kinds, the expression
    graph has 41 expression node kinds, and types arrive as ABI layout rows.
  - **A reference point.** The native oracle's MIR-level emitters are about
    22,000 lines: `transpiler_mir_*` about 13,200 and `llvm_mir_*` about
    8,900.
  - **Pieces to reuse.** The per-operation C/LLVM text inside the `direct_mir_*`
    owners, such as the comparison and integer-wrap projections, can be
    consolidated into per-kind emitters instead of being rewritten.
  - Native in-memory MIR still holds AST pointers (`expr0`, `expr1`, `ast`;
    `src/compiler/mir_types.h`). That limits the native oracle, not the JSON
    contract the self-host reads.
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

## Work split (owner decision, 2026-10-14)

P0 is active. The lanes, in order:

- **GPT lane: P0 implementation.** This lane continues the `focus17`
  candidate under `compiler_closure_priorities_2026-10-14.md`, which owns the
  dependency order, the lanes and the integration gate. Its uncommitted files
  in the shared tree belong to this lane.
- **Claude lane: investigation and verification.** This lane does not edit
  the P0 candidate's files. Its outputs are observations and reviews, not a
  second implementation:
  1. **Full failure census of the fixed DRV-2 input on main.** Instrument a
     scratch copy of the reached verdict owner so the bootstrap reports every
     ownership refusal, not only the first. Record each refusal with its node,
     code, boundary and owner. That gives P0 its real remaining set, replacing
     the historical 97 and the current first error.
  2. **Review of the candidate** as each coherent unit lands. Check it against
     the ownership contract (`docs/semantics/27_ownership_clean.md` §5) and
     the census, and add executable falsifiers where a refusal has none.
  3. **C-path review.** List what in the self-host C path must be fixed or
     refactored for V1 generality:
     - the claimed-shape emitters and their per-operation pieces;
     - the MIR → AST round trip;
     - the route that ownership facts force;
     - the AST codegen.

     Record which parts become the general MIR → C emitter, which are
     deleted, and in what order. This is a plan for after P0; no C-path edit
     starts while P0 is open.

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

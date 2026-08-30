# Release Primary Debug-Section Hygiene — 2026-08-30

Status: `STRUCTURAL CAP REPAIR COMPLETE — REPUBLICATION CI PENDING`

Exact base revision: `5a9c34d3d946e4e5f103822253ae7da9c029a46f`

This directive coordinates one bounded release-artifact rung. It does not own
language semantics, SoT status, project progress, or the complete C++-class
reconstruction-resistance verdict.

## Objective card

- Objective: make `--opt=release` final links publish primary executables with
  debug sections and ordinary symbol tables stripped through one policy shared
  by native C, native LLVM, self-host C, and self-host LLVM artifact paths.
- Priority order: runtime and ABI parity; one release policy; release debug and
  symbol removal; fail-closed gate; patch size.
- Fact owner: `src/compiler/compiler_release_artifact_policy.c`, selected only
  from the typed `PgyOptProfile` already carried to the final link boundary.
- Last legitimate consumers:
  `compiler_build_native`, `compiler_build_native_llvm`,
  `compiler_compile_link_self_host_c_artifact`, and
  `compiler_compile_link_self_host_llvm_artifact`.
- Forbidden fallback: treating `-O3` as stripping, asking users to strip the
  primary executable, backend-specific release behavior, a fixture/type/symbol
  allowlist, post-link best effort whose failure is ignored, or applying the
  release policy to the developer profile.
- Verification gate: `release-primary-debug-section-hygiene-test-smoke`
  compiles and executes C- and LLVM-backed hello releases, rejects debug
  sections, compares them with a same-toolchain stripped C++ baseline when a
  C++ compiler is available, and proves the inspector rejects an injected
  debug-bearing artifact.

## Scope and integration

- Implementation scope: the named policy owner, the four final link consumers,
  Make/source inventory wiring, the focused gate, Linux push wiring, this
  directive, collaboration ledger, and current handoff.
- Forbidden overlap: semantic admission, MIR/AIR ownership, self-host
  substitution, generic contextual typing/query work, sandbox policy, packers,
  anti-debugging, and public ABI renaming.
- Integration owner: the primary task on this exact lease.
- Validation budget: static owner/gate checks under 60 seconds, focused two-
  backend release execution under five minutes, then the existing bounded CI
  publication boundary.
- Output class: implementation candidate until the focused gate and current
  build pass; publication evidence only after exact-head CI is observed.

## Fresh falsifier

The current `bin/pgy.exe` compiled `examples/hello.pgy` with
`--opt=release` through both C and LLVM. Both executables retained nine DWARF
section families (`.debug_aranges`, `.debug_info`, `.debug_abbrev`,
`.debug_line`, `.debug_frame`, `.debug_str`, `.debug_line_str`,
`.debug_loclists`, and `.debug_rnglists`). The C artifact was 126,881 bytes and
the LLVM artifact 453,335 bytes. Stripped temporary copies were 17,408 and
262,144 bytes, had no ordinary symbol table, and both still printed
`Hello, Pergyra!` with exit code zero.

This is a release-hygiene rung. It does not increment hard self-host
`SUBSTITUTING` progress or change the `CLOSED=55 / BRIDGE=32 / ACTIVE=1`
census. The complete reconstruction-resistance acceptance gate stays open.

## Implementation checkpoint

- Exact implementation revision:
  `477ada2202c019581e7d835d2f332d75a194991d`.
- The shared policy owner returns no flag for developer builds, `-s` for ELF/PE
  release links, and the Darwin driver equivalent `-Wl,-S,-x` for Mach-O.
- All four final link consumers use the owner; the focused gate rejects local
  strip flag reconstruction in those consumers.
- Local focused evidence is green for self-host C, self-host LLVM, explicit
  native C, and explicit native LLVM. All four stripped primaries execute with
  the expected output. The explicit native developer debug lane retains DWARF,
  and an injected `-g` artifact is detected.
- Local same-target C++ comparison sizes were 17,408 bytes for self-host C,
  262,144 for self-host LLVM, 110,080 for native C, 256,512 for native LLVM,
  and 17,408 for the optimized/stripped C++ baseline.
- First publication run `33300744973` exposed one structural-only failure:
  `compiler_self_host_artifact.c` was 183 lines against its shrink-only 180-line
  cap. The cap was not raised. Repair checkpoint
  `81f820595b7ba760eabb6770eb2df39b6578e7f0` names the policy result once per
  final-link function and leaves the consumer at 179 lines. The exact failing
  component contract and the focused four-path gate are green after repair.
- The next run showed that the gate had been listed only in the dormant/full
  `ci_linux_steps.sh`, while the workflow's `build-linux` job executes
  `ci_push_linux_steps.sh`. The gate is moved, not duplicated, into the actual
  fast-push list so exact-head CI must execute it without lengthening a separate
  full profile twice.

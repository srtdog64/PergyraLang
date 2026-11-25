# 2026-11-24: what closes `target.capability_profile`

Decided by: the user, on 2026-11-24 ("전부 개선하고 SoT 닫자"), after the
Claude lane reported that two of the row's open items have no consumer and
that closing the row honestly means recording that; recorded by the Claude
lane. The user can overturn it.

## Rule

The row closes when one owner gives the compilation target and every
consumer reads it there:

1. `src/common/target_profile.def` holds the target rows: LLVM triple, C
   driver target, object format, pointer width, pointer alignment and byte
   order. The native compiler selects the row of the host it was built for
   and refuses a row whose pointer width, alignment or byte order differs
   from the host, a row with an unknown object format or no target, and a
   host with no row. The self-host compiler reads the generated projection
   of the rows (`target_profile_row_projection_owner.pgy`).
2. The native LLVM backend takes its triple from the row; LLVM's default
   triple and the MinGW override are deleted. The C toolchain and the clang
   that compiles self-host LLVM IR take their `--target` from the row; a
   `PGY_CC`, `CC` or `PGY_LLVM_CLANG` command that names another target is
   refused instead of adopted.
3. The self-host runtime Array layout (word size, alignment, offsets, the
   LLVM aggregate type, the layout id) is computed from the pointer width
   and alignment all rows share; rows that disagree, or that are not 64-bit
   little-endian, make the layout contract invalid, and its consumers
   refuse.

Two items of the old open reason are not part of this row:

- AIR evidence references. AIR belongs to `air.evidence_graph`; the target
  facts above do not depend on AIR, and the self-host emission has no AIR
  input.
- A plan digest on `CompilerEmissionArtifact` and carrying the native
  full-width fingerprint into the self-host compiler. No consumer recomputes
  or compares either; carrying them would add fields nobody checks. If a
  consumer that verifies them appears, it reopens under
  `projection.verified_plan` or the artifact rows, not here.

## Not covered

- Cross compilation: the active row is always the host's.
- The native ABI layout rows (`src/compiler/mir/mir_abi_layout.c`) still use
  host `sizeof`; they belong to `abi.layout_rows`.

Replaces: nothing.

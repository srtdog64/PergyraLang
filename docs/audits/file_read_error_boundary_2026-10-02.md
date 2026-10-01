# File-read error boundary and bounded orphan review

Status: Windows and Linux runtime gates verified; full CI is separate evidence.
Initial base: `d36492602364ed96c55fe9584d3962eab5744e90`.
Main integration base: `69c7c859361fc0bf8e030bc8bd55598b9d1a1a48`.
Initial implementation used an independent clone. The latest user instruction
then authorized integration, selective commit and push directly on main. All
seven I/O paths were clean and unchanged between the two bases. Applying this
patch preserved all 93 other dirty file contents (SHA256 checked before/after).

## File-read boundary

Objective: preserve actual stream failure as `READ_FAILED`, while normal end
of input remains `EOF`. Priority: inspect the stream while its file-table
mutex is held, use the existing status/IoError vocabulary, retain ABI and
legacy FileRead behavior, then minimize the patch.

Owners: `pgy_try_file_read_result` in `pgy_runtime_io_qubit_inline.h` and its
LLVM-linked twin in `pgy_runtime_lib_io_string_exports.h`. Both consume the
FILE error indicator before unlocking. No new diagnostic, language builtin,
status value, retry, or ownership rule is introduced.

The new runtime fixture reads two normal lines (including one without a final
newline), repeated EOF, a closed handle, a real write-only stream, repeated
read failure, the legacy empty-string wrapper, and an empty reopened file.
It also checks failure name, stage, operation and recoverability. The same
fixture compiles against the inline runtime and the actual exported runtime
translation unit; the runner requires equal stdout and empty stderr.

Validation:

- Before the fix: the actual write-only-stream probe failed with
  `file-read did not return read-failed`.
- After the fix: GCC and Clang runtime fixtures passed for both implementations.
- Existing `runtime_panic_abi_smoke.sh` passed with the new runner integrated.
- The existing `io_result_builtin_owner.sh` also invokes the runner so the
  fast Linux CI I/O gate covers the boundary. Shell syntax was checked;
  its broader source/driver parity matrix was not replayed locally.
- `git diff --check` passed. Installed compiler binaries were not replaced.

The initial gates ran on Windows UCRT64. Follow-up validation used the existing
Ubuntu-E-WSL Linux environment, against the actual main working tree after the
patch was applied: GCC and Clang inline/exported runtime probes PASS; the entire
GCC runtime_panic_abi_smoke.sh PASS; shell syntax PASS; gate reachability PASS
(undeclared0, stale0, dual0). Each focused runtime gate was bounded at 300s.
This is actual Linux libc/runtime execution evidence, including the exported
runtime translation unit, not source-to-LLVM compiler or installed-driver
parity. The broader source/driver I/O matrix and macOS runtime probe were not
replayed locally. No installed-driver or whole-SoT closure is claimed.

## One bounded orphan review

The review covered the actual LLVM runtime translation unit and its included
runtime owners. Clang parsed `pgy_runtime_lib.c` with `PGY_LLVM_ENABLED`, and
the AST inventory compared static definitions with all FunctionDecl references,
including address references and previous-declaration identities. It was a
candidate inventory, not an assumption that one translation unit is the world.

Fifteen Pergyra-named static definitions had no reference in that translation
unit. Repository source, generated-code emitters, macro families, runtime
linkage modes, tests, documentation and the moving main tree were then checked:

- Panic emission is pinned by LLVM attribute policy and panic contract gates.
- Parallel run is runtime header surface, with runtime/ABI/concurrency contract
  consumers; absence from this translation unit does not authorize API removal.
- Region allocation is reached through a macro; strdup/reset have executable
  arena tests outside this translation unit.
- Slot status predicates and six generated result predicates are shared header
  surface, not private LLVM runtime helpers.
- String-view routines are shared/documented header surface, adjacent to the
  other worker's dirty string-window owner; they were retained.
- The Bool Option predicate uses `PGY_RT_DECL`, which also produces external
  declarations/definitions. It was retained as ABI surface.

No unreferenced private, non-inline LLVM runtime helper was found by this
bounded AST review. No file or function was deleted. This does not claim a
whole-repository dead-code proof, and historical tests or public functions are
not reclassified as orphan solely because the runtime TU does not call them.

## Pre-existing CI boundary

Baseline run `36931966062` for the exact base commit completed **failure**.
Its failed job was `self-host-codegen-bootstrap-linux`; the final diagnostic
was `[codegen-nominal-array-declaration] codegen rejected the MIR root control`.
Downstream Linux gates were skipped. Windows, macOS, TSan and Rocq jobs passed.
This failure predates the I/O patch. The I/O topic run36937942540 at exact
5ae7ab3bd9a8e1b57583952eb54fe9b324339220 failed identically. The newer main
run36939121651 at exact69c7c859 also failed the same self-host job.
The failing gate is codegen_nominal_array_declaration.sh: its second control
compiles fixture/mir_collection_receiver_root.pgy through the source codegen.
This is distinct from its first nominal-array fixture. The current aggregate
event boundary directive explicitly assigns the exact receiver-root relation
to its active read-only diagnosis; no overlapping semantic hunk was adopted.

Remote evidence must identify the final main commit and its run separately.
The local gates above do not imply the repository-wide CI is green.

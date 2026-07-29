# MIR JSON Instruction Writer Probe - Intent / Contract

**Status:** soft self-host parity candidate.

**Parity owner:** `tests/self_hosted/parity/mir_json_instruction_writer_byte_parity.sh`

## Intent

Prove that the self-hosted sequential MIR instruction writer projects one
validated fact graph to String and file artifacts with byte-identical ordering
and escaping. Invalid instruction rows must fail before the destination is
opened, so a rejected projection cannot truncate an existing artifact.

This tool is evidence for the writer boundary only. It is not
`SUBSTITUTING` until a production compiler entrypoint replaces the
corresponding C-owned emission path.

## Input Contract

The executable accepts exactly four paths:

1. a Pergyra source fixture;
2. the String-projection output;
3. the streaming file-projection output;
4. a pre-existing sentinel artifact used for the invalid-fact case.

The source is compiled through the typed AST artifact analysis and
`DriverRung2MirProjectionFromAnalysisObserved`. The probe may mutate only its
owned copy of the resulting instruction ID rows to construct the falsifying
case.

## Output Contract

For valid facts, `SelfMirProgramJson` and
`SelfMirProgramJsonWriteFile` must emit byte-identical JSON. On success the
tool logs `mir-json-instruction-writer-byte-probe-ok` and exits zero.

For the invalid negative case, file projection must return false without
opening or changing the destination. The destination must still contain
`writer-preopen-sentinel`. An accepted invalid row or changed sentinel is a
hard failure.

## Oracle

The C backend remains the bootstrap oracle. The declared parity owner compiles
this tool for C and LLVM when LLVM is available, runs the canonical fixture
set, compares String and file bytes, compares C and LLVM stream bytes, and
checks stable graph/ABI fields. LLVM unavailability may skip only the LLVM leg;
the C leg must always execute.

The byte-parity script is deliberately named after the observable contract,
rather than the tool directory. This explicit declaration is the sole mapping;
the scaffold gate rejects paths outside
`tests/self_hosted/parity/*_parity.sh`.

## Not In Scope

- General MIR semantic correctness beyond the selected instruction artifact.
- Parallel or chunked writer scheduling.
- Claiming hard self-host substitution from probe reachability alone.

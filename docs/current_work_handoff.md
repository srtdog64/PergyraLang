# Current Work Handoff

Updated: 2026-07-25 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. Verify it against the
current source, `git status --short --branch`, the SoT registries, the named
owner, and the named executable gate.

## Resume checkpoint

- Implementation checkpoint: `6329356f` (`bound-mir-json-string-leaf-lifetime`)
  on `main`, pushed to `origin/main`. Its production instruction-streaming
  predecessor is `e5587bee` (`stream-mir-instruction-json`).
- The verified driver now proves semantic readiness once and enters
  `SelfMirProgramFactsFromReadyArtifact`; the independently callable checked
  entrypoint still owns the complete validation contract.
- Direct local assignments still require local/target type equality. Member
  and indexed assignments validate the root local separately and no longer
  compare that root type with the final selected member/index type.
- Production `--emit-mir-json-verified` writes through
  `SelfMirProgramJsonWriteFile` instead of materializing one whole-program
  `String`. Program/routine/block and instruction-local unbounded graph/list
  rows are streamed. Escaped/quoted string leaves use a call-local allocator
  pool released immediately after synchronous `FileWrite`; numeric and fixed
  bounded projections remain unchanged.
- Initializer local visibility now advances through
  `SemanticAstInitializerEnvironmentCursor`. Function-base rows are seeded
  once, lexical locals are appended/popped in source order, destructure rows
  publish atomically, and the two per-row full-function local scans are absent
  from the production loop.
- C and LLVM remain peer native compiler backends. The Pergyra-built DRV-2 is
  still a bounded self-host replacement lane; this checkpoint does not claim a
  fully self-hosted driver or a Pergyra-owned LLVM emitter. It does establish
  the first complete current full-driver MIR artifact below 3072 MB.

## Exact dirty state

No task-owned implementation or documentation change is dirty at the
implementation checkpoint. These three unstaged files are concurrent user work
and must remain unmodified and excluded from task commits:

- `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`.

At the documentation checkpoint, no task-owned implementation or documentation
file should remain dirty.

## Active executable objective card

- Objective: consume the completed Pergyra-produced full-driver MIR artifact
  through the existing Pergyra MIR consumer, emit and compile gen2 C, then
  prove the generated driver on the bounded parity preflight.
- Priority: preserve the exact `pgy.mir.v1` artifact identity, keep the MIR
  consumer and semantic owners fail closed, stay below the fixed pressure cap,
  then establish gen2/gen1 bounded behavior before widening the fixture.
- Fact owner: the verified `SelfMirProgramFacts` producer and its completed
  `pgy.mir.v1` artifact. `MirJsonReadInput`/MIR lowering consume that artifact;
  they may not reconstruct a second producer authority.
- Last legitimate consumer: current `driver_oracle.exe --mir-json` emitting
  `driver_gen2.c`, followed by the native C compiler only as the bootstrap
  object-code boundary.
- Forbidden fallback: regenerating a native oracle MIR per generation,
  backend-specific JSON reads, source-text fact recovery, process-sharded fact
  stores, `new ? old` authority, or raising the 3072 MB cap.
- Focused falsifier: the completed artifact must emit gen2 C under observation,
  compile cleanly, and the generated driver must consume the existing bounded
  MIR fixture to the same C bytes as gen1. Any first explicit consumer
  diagnostic selects the next owner.
- Acceptance gate: pressure-owned full MIR consumption, gen2 compilation, and
  byte-exact generated bounded preflight all succeed; only then advance to the
  gen2/gen3 fixed-point comparison.

## Latest measured evidence

The original 20+ GiB observation was dominated by repeated graph/readiness
validation. Closing those repeated validations brought the current driver into
the fixed 3 GiB pressure window. Sequential instruction projection plus
call-local string-leaf lifetime now completes the full artifact in that same
window. The latest fixed-cap observations are:

| Slice | Peak private | Peak working set | Last observed state |
| --- | ---: | ---: | --- |
| `mir-fact-ready` | 2865.8 MB | 2359.0 MB | Reached MIR lowering; exposed the composite-assignment invariant at syntax node 5290. |
| `assignment-composite-ready` | 3233.9 MB | 2716.4 MB | MIR facts completed; crossed the cap after `json:start`. |
| `json-builder-ready` | 3195.6 MB | 2680.9 MB | MIR facts completed; whole-program JSON still crossed the cap. |
| `json-file-ready` | 3290.1 MB | 2775.6 MB | Wrote 20,013,056 bytes before routine-string materialization crossed the cap. |
| `json-block-file-ready` | 3197.3 MB | 2678.8 MB | Wrote 20,901,888 bytes; per-instruction/field strings still accumulated. |
| `initializer-cursor-ready` | 3117.9 MB | 2601.7 MB | All 8,229 initializer rows and MIR facts completed; crossed after `json-write:start` with 13,709,312 bytes. |
| `instruction-stream-ready` | 3092.7 MB | 2574.5 MB | Unbounded instruction/graph rows streamed; crossed with a 40,263,680-byte partial artifact because leaf strings remained result-lived. |
| `instruction-string-pool-ready` | 3064.3 MB | 2544.9 MB | Exit 0; complete 51,807,108-byte artifact and `json-write:done`. |

The cursor run completed in 869,913 ms before the pressure owner stopped it
inside routine `SemanticExpressionGraphNodeKind`. `e5587bee` then removed the
complete production instruction and graph Strings. Its first fixed-cap run
completed all current 8,266 initializer rows and MIR facts, started JSON near
2,956 MB, and advanced to 40,263,680 bytes before escaped/quoted leaf results
crossed the cap at 810,472 ms.

`6329356f` moves only those file-boundary leaves into a call-local pool and
destroys it after synchronous `FileWrite`. The successor run exited 0 in
675,355 ms. Peak private was 3,064.3 MB, with `driver_oracle.exe` at
3,063.1 MB; two processes and no compiler/link subprocess were observed. The
artifact is valid `pgy.mir.v1` with 2,345 routines, 142 declarations, and
SHA-256
`1621adf4070bc778dd90493e29db857c22f13722d951bea8a94d1241e9ee884e`.
The full JSON parse and closing `]}` were observed. The production gate is
green, but its 7.7 MB sampled margin is narrow and does not close the broader
semantic/MIR live-state debt.

The focused instruction-writer gate now compares raw, unnormalized
String/file bytes for five small, graph-heavy, match, destructure, and
ABI/optional fixtures through both C and LLVM, then compares C/LLVM file bytes.
It also corrupts instruction row count and proves the sentinel output is not
opened or truncated. The earlier 11,262-byte small fixture SHA remains
`007d5dacdd8157a0d5dd0f87975f82c7abe2fa4987983afb3945bd61b29efc09`.
`FileOpen` failure is observable and fails closed; the current runtime does not
return a `FileWrite` status, so the writer must not claim write-error detection
that the runtime cannot provide.

The attempted broad machine-MIR parity is not green evidence: its native
expected substring predates the current `param_types` field. Update that
fixture only when its owner is the active executable slice.

## Last observed gates

Green on `6329356f`:

- `tests/self_hosted_component_contract_smoke.sh`;
- `tests/self_hosted/parity/mir_json_instruction_writer_byte_parity.sh`
  (C/LLVM, five raw String/file and cross-backend fixtures, plus invalid
  pre-open sentinel rejection);
- `instruction-string-pool-ready` pressure shard: exit 0, complete JSON below
  3072 MB;
- `tests/self_hosted/parity/semantic_initializer_environment_cursor_owner_smoke.sh`;
- `tests/self_hosted/parity/semantic_expression_environment_owned_lifetime_smoke.sh`;
- `tests/self_hosted/parity/initializer_projection_probe_parity.sh` (C/LLVM,
  including shadow/exit/destructure positives and self/sibling negatives);
- `tests/self_hosted/parity/driver_rung2_iteration_graph_use_owner.sh`;
- `python scripts/protocol_registry_gate.py`:
  `7 protocol rows valid; no authority duplicated`;
- `python scripts/sot_registry_gate.py`:
  `49 authorities, 41 derived fact carriers; CLOSED=29 BRIDGE=20 ACTIVE=0`;
- `git diff --check` and `git diff --cached --check`.

The shell gates must use `C:\Program Files\Git\bin\bash.exe` in the current
Windows environment. `C:\Windows\System32\bash.exe` resolves to WSL and fails
because `/bin/bash` is unavailable; that is an execution-environment failure,
not a project gate result.

## Temporary artifacts

The current full artifact and driver oracle remain under
`.tmp/instruction_writer_pressure/` because the next executable rung consumes
that exact artifact. The relevant file is
`driver_source_pool.mir.json` (51,807,108 bytes, SHA above). The 40,263,680-byte
`driver_source.mir.json` is the preceding RED partial and must never be used as
input. Pressure evidence remains under
`.tmp/build-pressure/instruction-stream-ready.*` and
`.tmp/build-pressure/instruction-string-pool-ready.*`; these files are
diagnostic evidence only, not semantic authority or commit content.

## Next executable work

1. Verify the complete artifact SHA and run the current Pergyra driver
   `--mir-json` consumer under the pressure owner to emit `driver_gen2.c`.
2. Compile that C as the bootstrap object-code boundary; do not regenerate a
   second oracle MIR artifact.
3. Run the generated driver on the existing bounded MIR fixture and compare
   emitted C bytes with the current driver output.
4. If bounded gen2 is green, run gen2/gen3 fixed-point comparison. If it fails,
   the first explicit consumer diagnostic or byte mismatch selects the next
   Pergyra owner.
5. Schedule broader protocol/SoT registries and MIR parity after the focused
   consumer slice is stable; do not spend the 7.7 MB production margin by
   raising the cap or adding backend-local serialization.

## Resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, `src/self_hosted/OWNERS.md`,
   `docs/180_compiler_logical_spine_handles_gates.md`, and
   `docs/semantics/sot_owner_spine_registry.md`.
2. Verify HEAD/origin, `git status --short --branch`, and the three protected
   dirty files above.
3. Re-run the component and raw instruction-writer byte gates through Git Bash
   before a broad build.
4. Confirm no unrelated `pgy`, `genN`, `driver_oracle`, `gcc`, `cc1`, or
   `clang` process is active before the pressure gate; concurrent broad builds
   invalidate attribution.
5. Treat current source, registries, and executable gates as authoritative if
   this snapshot disagrees with them.

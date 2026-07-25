# Current Work Handoff

Updated: 2026-07-25 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. Verify it against the
current source, `git status --short --branch`, the SoT registries, the named
owner, and the named executable gate.

## Resume checkpoint

- Implementation checkpoint: `ffe31ce89f99f0891f6921ed013fe567534a8b9a`
  (`advance-initializer-environment-cursor`) on `main`, pushed to
  `origin/main`. It follows the bounded MIR JSON checkpoint `a6114336`.
- The verified driver now proves semantic readiness once and enters
  `SelfMirProgramFactsFromReadyArtifact`; the independently callable checked
  entrypoint still owns the complete validation contract.
- Direct local assignments still require local/target type equality. Member
  and indexed assignments validate the root local separately and no longer
  compare that root type with the final selected member/index type.
- Production `--emit-mir-json-verified` writes through
  `SelfMirProgramJsonWriteFile` instead of materializing one whole-program
  `String`. Routine and block aggregation are streamed; individual instruction
  and several field rows still materialize temporary strings.
- Initializer local visibility now advances through
  `SemanticAstInitializerEnvironmentCursor`. Function-base rows are seeded
  once, lexical locals are appended/popped in source order, destructure rows
  publish atomically, and the two per-row full-function local scans are absent
  from the production loop.
- C and LLVM remain peer native compiler backends. The Pergyra-built DRV-2 is
  still a bounded self-host replacement lane; this checkpoint does not claim a
  fully self-hosted driver or a Pergyra-owned LLVM emitter.

## Exact dirty state

No task-owned implementation or documentation change is dirty at the
implementation checkpoint. These three unstaged files are concurrent user work
and must remain unmodified and excluded from task commits:

- `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`.

This handoff, progress entry, and troubleshooting result are the only expected
task-owned dirty documentation until their separate checkpoint commit lands.

## Active executable objective card

- Objective: remove the complete instruction `String` materialization from
  the production MIR JSON file writer.
- Priority: preserve exact `pgy.mir.v1` bytes and field order, keep
  `SelfMirProgramFacts` as the sole fact owner, shorten instruction fragment
  lifetime, fail closed on malformed facts, then reduce the 3072 MB high-water
  mark.
- Fact owner: `SelfMirProgramFacts`; the new/extended writer owns only ordered
  file projection. It must call the same schema-aware graph, ABI, match,
  destructure, and use-row render policies rather than recreating facts.
- Last legitimate consumer: `SelfMirJsonBlockWriteFile`, currently calling
  `SelfMirJsonInstruction(facts, instruction_index) -> String` immediately
  before `FileWrite`.
- Forbidden fallback: a second MIR store, C/LLVM-specific serializers, a
  whole-block/routine/program string, `new ? old` reads, source-text fact
  recovery, or raising the pressure limit.
- Focused falsifier: small and graph-heavy instructions must be byte-identical
  between the existing String bridge and the production file writer through C
  and LLVM; a malformed instruction fact must fail before a success artifact
  is claimed.
- Acceptance gate: the production writer contains no
  `SelfMirJsonInstruction(...)` aggregate call, byte parity stays green, and
  the exclusive full-driver run produces a complete artifact below 3072 MB.

## Latest measured evidence

The original 20+ GiB observation was dominated by repeated graph/readiness
validation. Closing those repeated validations brought the current driver into
the fixed 3 GiB pressure window, but the full artifact still does not complete.
The latest fixed-cap observations are:

| Slice | Peak private | Peak working set | Last observed state |
| --- | ---: | ---: | --- |
| `mir-fact-ready` | 2865.8 MB | 2359.0 MB | Reached MIR lowering; exposed the composite-assignment invariant at syntax node 5290. |
| `assignment-composite-ready` | 3233.9 MB | 2716.4 MB | MIR facts completed; crossed the cap after `json:start`. |
| `json-builder-ready` | 3195.6 MB | 2680.9 MB | MIR facts completed; whole-program JSON still crossed the cap. |
| `json-file-ready` | 3290.1 MB | 2775.6 MB | Wrote 20,013,056 bytes before routine-string materialization crossed the cap. |
| `json-block-file-ready` | 3197.3 MB | 2678.8 MB | Wrote 20,901,888 bytes; per-instruction/field strings still accumulated. |
| `initializer-cursor-ready` | 3117.9 MB | 2601.7 MB | All 8,229 initializer rows and MIR facts completed; crossed after `json-write:start` with 13,709,312 bytes. |

The committed cursor run completed in 869,913 ms before the pressure owner
stopped it inside routine `SemanticExpressionGraphNodeKind`. It reached the cap
129,685 ms sooner than `json-block-file-ready`, but its pre-JSON baseline was
still about 2,937 MB. Because both measurements stop on the limit, the 79.4 MB
peak difference is kill/sampling evidence, not a proved live-state reduction.
The cursor substitution is real and its semantic gates are green; the full
artifact remains red. The next retained owner is the per-instruction JSON
String produced inside the block file writer.

Small-fixture `pgy.mir.v1` String/file projection was byte-identical at 11,262
bytes with SHA-256
`007d5dacdd8157a0d5dd0f87975f82c7abe2fa4987983afb3945bd61b29efc09`.
`FileOpen` failure is observable and fails closed; the current runtime does not
return a `FileWrite` status, so the writer must not claim write-error detection
that the runtime cannot provide.

The attempted broad machine-MIR parity is not green evidence: its native
expected substring predates the current `param_types` field. Update that
fixture only when its owner is the active executable slice.

## Last observed gates

Green on `ffe31ce8`:

- `tests/self_hosted_component_contract_smoke.sh`;
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

The task-owned builder/probe directories, the 122 MB temporary native MIR JSON,
and the accidental root `--emit-ast` file were removed after their evidence was
captured. The `initializer_cursor_pressure` oracle/partial artifact and focused
cursor probe directories were also removed after the latest measurement.
Pressure summary, sample, stdout, and stderr logs under
`.tmp/build-pressure/initializer-cursor-ready.*` remain diagnostic evidence
only; they are not semantic authority or commit content.

## Next executable work

1. Split production instruction file emission behind the existing instruction
   schema policy without adding a second MIR fact carrier.
2. Keep `SelfMirJsonInstruction` only as the bounded fixture/String bridge;
   reject it in `SelfMirJsonBlockWriteFile` with a negative gate.
3. Prove byte-exact output for small, graph-heavy, match, destructure, ABI, and
   optional-field instruction shapes through C and LLVM.
4. Run component, MIR JSON parity, protocol/SoT registry, and then the same
   exclusive 3072 MB full-driver shard.
5. If the artifact completes, run the fixed-point consumer and let its first
   parity failure select the next self-host substitution rung.

## Resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, `src/self_hosted/OWNERS.md`,
   `docs/180_compiler_logical_spine_handles_gates.md`, and
   `docs/semantics/sot_owner_spine_registry.md`.
2. Verify HEAD/origin, `git status --short --branch`, and the three protected
   dirty files above.
3. Re-run the focused lifetime/initializer gates through Git Bash before a
   broad build.
4. Confirm no unrelated `pgy`, `genN`, `driver_oracle`, `gcc`, `cc1`, or
   `clang` process is active before the pressure gate; concurrent broad builds
   invalidate attribution.
5. Treat current source, registries, and executable gates as authoritative if
   this snapshot disagrees with them.

# Current Work Handoff

Updated: 2026-07-25 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. Verify it against the
current source, `git status --short --branch`, the SoT registries, the named
owner, and the named executable gate.

## Resume checkpoint

- Implementation checkpoint: `a61143369e9f0f76517c697f88f81ab5039a0cb4`
  (`route-bounded-mir-json-artifact`) on `main`, pushed to `origin/main`.
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

This handoff refresh is expected to be the only additional task-owned dirty
file until its separate documentation commit lands.

## Active executable objective card

- Objective: replace per-initializer reconstruction of the complete visible
  local environment with one function-ordered environment cursor.
- Priority: preserve semantic identity and source-order visibility, publish
  each inferred local exactly once, make scope exit explicit, fail closed on a
  malformed schedule, then reduce peak memory and elapsed time.
- Existing fact owners: `SemanticAstLocalBindingFacts` owns local identity,
  function, scope, declaration type, and source order; the typed AST arena owns
  scope ancestry; initializer type facts own inferred types and diagnostics.
- Owner to land: a semantic initializer environment schedule/cursor owner that
  derives immutable scope events once and advances sequentially. It must not
  become a second local-binding or scope authority.
- Last legitimate consumer: the initializer expression verdict for the current
  local row. The row is committed to the cursor only after its type verdict is
  complete, so an initializer cannot observe itself.
- Forbidden fallback: `SemanticAstExpressionSeedVisibleLocals` plus
  `SemanticAstExpressionSeedVisibleLocalModes` scanning the full function range
  for every initializer; a `new ? old` dual read; AST-root rescans; backend-local
  visibility recovery; or raising the 3072 MB gate.
- Focused falsifiers: self-reference, sibling-scope leakage, nested-scope exit,
  destructure bindings becoming visible before their shared initializer
  verdict, and match/iteration binding leakage across a function boundary.
- Acceptance gate: initializer C/LLVM projection parity plus a negative owner
  smoke that rejects the two full-range seed calls inside the migrated
  initializer loop. The full-driver pressure gate remains the integration
  falsifier.

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

The last run stabilised around 2933.4 MB before JSON. This makes the next owner
the initializer environment reconstruction, not another file-writer split.
The streamed writer is a real bounded-lifetime improvement, but no full MIR
artifact or fixed-point result exists yet.

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

Green on `a6114336` or its exact staged source before commit:

- `tests/self_hosted_component_contract_smoke.sh`;
- `tests/self_hosted/parity/semantic_expression_environment_owned_lifetime_smoke.sh`;
- `tests/self_hosted/parity/initializer_projection_probe_parity.sh` (C/LLVM);
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
captured. Pressure summaries and samples under `.tmp/build-pressure/` remain
diagnostic evidence only; they are not semantic authority or commit content.

## Next executable work

1. Inspect the local-binding function ranges and typed scope facts, then land
   the smallest initializer-only sequential environment cursor.
2. Preserve the existing expression-verdict, diagnostic code, inferred type,
   binding mode, and C/LLVM projection contracts exactly.
3. Add the negative visibility fixtures before deleting the two repeated
   visible-local scans from the active initializer loop.
4. Run the focused owner smoke, initializer C/LLVM parity, component contract,
   protocol/SoT registry gates, and only then the exclusive 3072 MB pressure
   shard.
5. If the full artifact completes, continue with the first fixed-point parity
   failure. If it remains red, the first retained owner at the cap selects the
   next executable substitution rung.

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

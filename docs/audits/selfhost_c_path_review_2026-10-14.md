# Self-host C path review for V1 generality (2026-10-14)

Status: `AUDIT COMPLETE`. These are read-only observations. The plan at the
end is a proposal for after P0, not an open rung.
Base revision: origin/main `c08dfc6e9` (src/self_hosted unchanged through
`60b9d62bc`).
Lane: Claude, investigation and verification
(`agent_work_directives/v1_scope_ownership_and_selfhost_generality_2026-10-14.md`).

## How it was measured

- **Import closures.** Computed by walking `import "..."` lines.
  - `compiler/driver_bootstrap_main.pgy`: 1,912 files, 269,580 lines.
  - `codegen/main.pgy`: 655 files, 129,749 lines.
- **Function classes.** The 1,004 `direct_mir_*` files hold 3,232 top-level
  items, covering 88,763 of their 97,058 lines. Each item was classified by
  return type and name tokens:
  - *emit*: builds text and returns `String` or `Option<String>`;
  - *gate*: returns `Bool`, or its name contains
    Ready/Claimed/Admission/Policy/Supported/Refusal;
  - *plan*: everything else;
  - *schema*: struct and enum declarations.

  This is a heuristic. About 95 random rows were spot-checked and looked
  right.

## 1. Where the C route is decided

`CompileMachineAdmittedMirJsonToCForTargetVerifiedObserved`
(`compiler/driver_rung2_owner.pgy:263`) picks the route. It runs on the MIR
JSON that `CompileSourceToMirJsonVerified` writes, after
`MirMachineLayerAdmitJsonInput` has re-admitted it.

1. **Scalar substitution** (`driver_rung2_scalar_c_substitution_owner.pgy`).
   - It returns `None` if the target is not C, or if MIR `decls` is non-empty
     (:17).
   - If any routine has collection ownership facts, it is forced (:37).
     Otherwise it runs only when identity is required and every routine is
     claimed (:43).
   - Forced but unclaimed ends in `Die` (:53).
2. **Nested intent substitution.** Only programs with exactly 4 routines and
   2 domain-runtime declarations.
3. **Everything else takes the AST fallback.**
   - It rebuilds AST tree text from MIR (`DriverRung2IntentTreeEmissionOrDie`).
   - It runs a second full semantic pass
     (`SemanticAstArtifactAnalyzeWithExpressionGraph`, :311).
   - It emits through `GenerateCFromVerifiedSemanticArtifact` (:401).

Other entries:

- `--mir-json` uses the same dispatcher, so the gen2/gen3 driver fixpoint runs
  through the round trip.
- `--mir-json-backend=c` and source→LLVM use a separate cascade,
  `CompileAdmittedDirectMirForTargetObserved`
  (`direct_mir_backend_projection_owner.pgy:72`). It tries 8 shape routes,
  then refuses. It has no AST fallback. 238 files (30,351 lines) are reachable
  only through it.

## 2. What the 97k lines of direct_mir are

| Group | Lines |
|---|---:|
| C emission | 7,004 |
| LLVM emission | 8,539 |
| Gates (admit or decline a shape) | 27,426 |
| Plan / fact extraction | 41,943 |
| Schema | 3,518 |

- **C emission** splits into operation 3,743, instruction 1,129, ABI/type 853
  and program skeleton 1,279.
- **The split that matters:** reusable C emission is about 7k lines (7%).
  Pure shape admission is about 43k lines (44%): the gates plus the plan
  functions that return Invalid/None/false.
- **Expression ids.** The emitters dispatch over 147 type-specialized
  `DirectMirScalarProgramExpr*` ids. The graph has 41 `AstExpressionNode*`
  kinds. Six node kinds are never named: Await, Coalesce, FloatLiteral,
  SetElement, SetLiteral, Spawn.
- **Readiness/admission files:** 156 files, 16,944 lines.
- **Signature-shape files:** 59 files, 4,524 lines. One example accepts exactly
  2+2 parameters.
- **Seed pieces for a general emitter:**
  - `DirectMirScalarProgramCExpressionValues` (post-order expression emission)
    and its slice, hashmap, direct-call and builtin companions;
  - `direct_mir_scalar_cfg_graph_admission_owner.pgy` (per-instruction
    lowering);
  - `direct_mir_scalar_cfg_program_c_emission_owner.pgy` (labels, terminators,
    copy-outs, cleanup);
  - `scalar_cfg_program_c_operation_owner` and the signature/ABI owners.

## 3. What the MIR JSON does not carry

The AST fallback reads only `admitted`, but its second semantic pass
re-derives inputs that have no MIR field:

- **Zone carriage.** This covers fresh resource locals, resource field paths
  and mutable borrow parameters. It drives `PGY_ZONE_LOCK_INIT/DESTROY`
  (`codegen/emission/stmt_emit.pgy:205–255`). The self-host writer hardcodes
  `"resource_flow_symbol_count":0` and `[]`
  (`mir/program_json_artifact_writer_owner.pgy:131–132`). Native
  `mir_json_dump_flow.c` emits real rows.
- **DIR intent source facts** for intents the MIR intent plan does not own
  (`codegen_callable_receiver_bridge_owner.pgy:87`).
- **Declaration rows.** `SelfMirDeclarationsFromAnalysis` (:54) rebuilds them
  instead of reading MIR `decls`. That is a duplicate authority.
- **Ownership facts are dropped.** No file under `codegen/` references
  `collection_ownership`. Every program that takes the fallback loses the MIR
  ownership facts, and cleanup comes from the AST codegen's own reasoning.

## 4. Ownership facts in the direct route

- **Coverage.** The transition admission handles `Array<String>` only. Any
  other type returns Invalid.
- **Where drops go.** Drops are emitted only at the entrypoint's exits
  (program C emission :87, :161).
- **A second authority.** HashMap drops come from the emitter's own lifetime
  proof (`c_hashmap_cleanup_owner`), which makes a second ownership authority
  inside the backend.
- **What a general emitter should do.** It should read per-edge cleanup facts
  from the MIR producer: at every exit terminator, at owned-local reassignment,
  at loop back-edges and at `retired` dispositions. When those facts are
  missing it should fail closed, never recompute them.

## 5. The AST codegen

- **What must stay for the bootstrap.** gen0 is `codegen/main.pgy` built with
  `--native-pipeline`. gen1 to gen3 use `--source`, and the installed driver is
  `gen2 --source driver_bootstrap_main.pgy`. All of them go through
  `GenerateCUnitFromAdmittedSemanticArtifact`, which needs `codegen/` (21k
  lines) and semantic (about 61k lines).
- **What exists only for the post-MIR fallback:**
  - `GenerateCFromVerifiedSemanticArtifact`;
  - about 560 lines of MIR bridge functions;
  - 42 files (7,143 lines) reached only by the fallback, measured at file
    level.
- **Mixed bridge owners.** gen0 carries 52 `mir_lower` files (12.4k lines)
  only because mixed bridge owners import them.

## 6. Proposed order (after P0)

Steps marked [P0] need the installed driver, so they wait until the bootstrap
is green.

1. **Falsifier harness.** Build a fixed corpus: `codegen/fixture` (85),
   `mir_lower/fixture` (67) and `examples`. Build each program with
   `--native-pipeline --backend=c` and with the driver's C route. Run both and
   compare stdout, stderr and exit code. Record the route each program took.
   Add one negative case: an unsupported (instruction, node kind, ABI row)
   triple must produce a structured refusal that names the triple. Never Die
   mid-emission, and never fall back to the AST route.
2. **Seed the general emitter** from the pieces in §2. Re-key dispatch from the
   147 type-specialized ids to (MIR instruction kind × node kind × ABI row).
3. **Ownership.**
   - Remove the `decls != 0` early return.
   - Drive cleanup from producer facts for every type and every exit.
   - Delete the emitter's private HashMap lifetime proof.
   - Falsifier: struct plus `Array<String>` programs under ASan/LSan, with
     runtime output equal to native.
4. **Producer gaps.** The MIR producer emits zone-carriage and resource-flow
   rows, DIR intent facts and declaration use.
5. **First deletions.**
   - Remove the nested-intent C substitution (27 files, 5,137 lines).
   - Remove the policy and signature-shape gates that ABI rows make redundant.
   - Point `--mir-json-backend=c` at the general emitter. [P0]
6. **Remove the MIR → AST round trip** once the general emitter compiles the
   full driver MIR. Delete the fallback-only code and split the mixed bridge
   owners out of gen0. Gates:
   - `driver_bootstrap.sh` with the full fixpoint (gen2 == gen3);
   - `codegen_bootstrap.sh`;
   - the installed CLI;
   - CI green on one SHA. [P0]
7. **The AST codegen and semantic analysis stay** as the bootstrap compiler.
   Replacing them is outside V1.

## Unknowns

- How many corpus programs take each route today. Step 1 measures it.
- Whether collection ownership facts are meant to cover types other than
  `Array<String>`.
- Whether the native and self-host MIR schemas differ anywhere beyond
  `resource_flow_symbols`.
- Fallback-only counts are at file granularity, not function granularity.

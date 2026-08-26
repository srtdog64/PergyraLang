# Public AIR bypass readiness audit — 2026-08-26

## Scope

- Directive: `docs/agent_work_directives/public_ir_bypass_readiness_audit_2026-08-26.md`
- Audited revision: `8b8c78f0d6f5efd0eecaeaec7ee2b1796b6723dd`
- Modes: public `pgy --air` and `pgy --air-json`
- This is a readiness audit, not substitution evidence. No build or test suite was run.

## Verdict: NOT READY

The repository does not yet have a Pergyra-owned general AIR producer that can
replace the native public AIR path. The existing Pergyra AIR assets are bounded
MIR CFG certificates, validators, vocabulary, and inspection tools. None owns
the complete `AIRProgram` observable for arbitrary source input.

## Native public bypass evidence

1. `src/pgy_driver.c:44-45` registers `--air` and `--air-json` as native driver
   flags.
2. `src/compiler/driver_self_host_selection_owner.c:68-82` excludes both AIR
   flags from the Pergyra source-stdout selection predicate. There is no sibling
   Pergyra AIR request/selection predicate.
3. After the recognized self-host delegations, `src/pgy_driver.c:339` falls
   through to `driver_run_pipeline(&flags)`. Therefore public AIR currently
   reaches the native pipeline.
4. In that pipeline, `src/compiler/driver_app.c:419-435` calls native
   `air_synthesize(hir, dir, rir)`, collects DAG evidence, and verifies AIR.
   `src/compiler/driver_app.c:441-486` then lowers/validates MIR, collects MIR
   evidence, and verifies AIR again. `src/compiler/driver_app.c:487-495` renders
   the result through native `air_dump` or `air_dump_json`.

This is not only a native serializer bypass. The native path still owns general
AIR construction and augmentation before serialization.

## Observable owned by the native path

Native JSON uses schema `pgy.air.graph.v1` and exposes, at minimum:

- capabilities, effects, slots, machine-layer sites, and effects by operation;
- lifecycle state spaces and function-parameter flow summaries;
- intent and boundary identities, ownership, coordination, failure,
  compression, source/authority provenance, and locations;
- evidence nodes and drift rows;
- summary counters and HIR/RIR/MIR binding facts;
- observability and runtime-frontier-policy facts.

`src/compiler/air.c` constructs the general intent/boundary inventory from
HIR/DIR/RIR. `src/compiler/air_evidence_dag.c` and
`src/compiler/air_evidence_mir.c` add distinct DAG and MIR evidence families.
`src/compiler/air_dump.c` and `src/compiler/air_dump_json.c:573-671` render the
text and JSON projections. A renderer or validator alone therefore cannot own
the public result.

Read-only native oracle probes on the audited revision confirmed that these
modes produce live payloads:

- `tests/cases/backend_compare/intent_zone_binding/main.pgy --air-json` emitted
  one intent, one boundary, twelve evidence rows, and clean diagnostics;
- the same source with `--air` emitted the corresponding text graph;
- `tests/capability/cap_env_demo.pgy --air-json` emitted live effect and
  effect-by-operation rows.

These observations characterize the native oracle only; they are not evidence
of a Pergyra substitution.

## Why current Pergyra AIR assets are not a general producer

- `src/self_hosted/air/README.md:3-5` scopes this directory to AIR verification
  and graph-shape checkers.
- `src/self_hosted/air/mir_cfg_certificate_owner.pgy:1-5` declares a bounded,
  fixed-size MIR CFG certificate. Its general constructor admits only three- to
  six-block shapes (`:35-36`); Option match has a separate bounded seven-block
  fact. This certificate cannot issue the arbitrary `pgy.air.graph.v1` intent,
  boundary, capability, lifecycle, evidence, drift, and policy inventory.
- `src/self_hosted/compiler/air_evidence_owner.pgy` owns an eight-name evidence
  vocabulary, not evidence instances, graph construction, or serialization.
- `src/self_hosted/tools/air_graph_json_validator/` and the related graph and
  machine-layer tools consume native or committed AIR JSON. Their validation
  reports cannot become compiler authority for the input graph they parse.
- `src/self_hosted/compiler/backend_air_access_contract_owner.pgy` constrains
  AIR access; it does not produce AIR.

Promoting any of these bounded consumers or certificates would create a false
owner and leave the native semantic producer intact.

## First missing fact

The first missing fact is a **Pergyra-owned general AIR graph issuance fact**:
for arbitrary typed HIR/DIR/RIR input, it must issue the ordered intent and
boundary identities together with their ownership, source/authority
provenance, coordination, failure, and compression facts.

This precedes JSON/text serialization and later DAG/MIR augmentation. The
bounded MIR CFG certificate cannot reconstruct it. After that owner exists, it
must also admit the remaining capability/effect/slot/machine/lifecycle/flow,
evidence/drift, observability, and runtime-policy families before public AIR can
be considered complete.

## Smallest future falsifier

Add one focused sourced sibling gate, proposed as
`tests/self_hosted/parity/public_air_installed_self_host_owner.sh`, under the
existing installed-driver CLI-mode owner so it reuses the already-built native
oracle and installed self-host driver. Do not add a Make target, CI job, or
second self-host build.

The gate should prove all of the following in one run:

1. For `tests/cases/backend_compare/intent_zone_binding/main.pgy`, public
   `--air` and `--air-json` are byte-identical to explicit
   `--native-pipeline --air` and `--native-pipeline --air-json` oracle output.
2. For `tests/capability/cap_env_demo.pgy`, JSON parity covers the live effect
   and effect-by-operation rows absent from the intent fixture.
3. With the installed Pergyra driver unavailable, public AIR fails nonzero,
   emits no AIR payload, and does not retry through `driver_run_pipeline`.
4. A malformed or incomplete admitted general AIR fact fails closed with no
   payload; in particular, dropping a required boundary authority/provenance
   fact must not be repaired from parsed JSON, native AIR, or a bounded CFG
   certificate.
5. A static ratchet requires AIR in the Pergyra request/selection owner and
   rejects any default AIR branch that reaches native `driver_run_pipeline`.

Byte parity without the missing-driver and static negatives is insufficient:
the current all-native route can already equal its explicit native oracle and
would yield a false green result.

# Compiler Navigation Projection — 2026-08-26

Status: `AUDIT ONLY`; this report is a human-navigation projection, not a
semantic owner, move authorization, readiness claim, or substitution
checkpoint.

## Scope and method

This is Track C from
`docs/agent_work_directives/semantic_hop_parallel_audit_2026-08-26.md`. The named base is
`9ca4a69517142a4c87eb47862afcd55a9a9f2011`, and `HEAD` still equals that
revision. The working tree also contains the primary task's uncommitted Lease F
implementation. The inventory below records that overlay explicitly because it
adds one compiler file, but no Lease F file was edited by this audit. No build,
test, stage, commit, or push was run.

The measurements used read-only file, import, and literal-reference censuses:

- a file belongs to a responsibility group by its current filename and declared
  role; the groups intentionally overlap and are not proposed modules;
- an importer edge is a resolved Pergyra `import` from any file under
  `src/self_hosted/` to a member of the measured group;
- an internal edge has both endpoints in the same projected group;
- a path-literal occurrence is an exact current
  `src/self_hosted/compiler/<file>` spelling, not a bare-name text hit;
- cycle evidence comes from a topological reduction of the complete current
  `src/self_hosted/**/*.pgy` import graph.

Folders in every proposal below are navigation projections only. Typed owner
names, protocol identities, registry identities, and executable behavior must
remain unchanged.

## Physical inventory

### Observed facts

- The base tracks 1,012 entries under `src/self_hosted/compiler/`, including
  1,002 Pergyra files. The dirty Lease F overlay adds
  `driver_source_mir_stdout_execution_owner.pgy`, so the current filesystem
  census is 1,013 entries and 1,003 Pergyra files.
- 1,006 files are in the directory root. `expected/` is the only child
  directory and contains seven files.
- 889 Pergyra files begin with `direct_mir_`; 345 begin with
  `direct_mir_scalar_program_`. The flat-namespace cost is therefore
  concentrated rather than evenly distributed: `direct_mir_` occupies 88.6%
  of the current Pergyra files in this directory.
- Top-level filenames average 55.3 characters, reach 99 characters, and 15 are
  longer than 80 characters. Long names preserve responsibility clues, but
  completion and directory scans still present more than one thousand peer
  choices.
- The complete current self-host import graph has 2,163 Pergyra nodes and 5,966
  unique import edges. Topological reduction removed every node; there is no
  observed import cycle.

### Responsibility projections and import pressure

These tags are search/navigation views, not disjoint ownership buckets. For
example, a target projection may also be a fact owner, so row counts must not be
summed.

| Projection | Current Pergyra members | Selection | Incoming edges / unique importers | Internal / external edges | Highest observed fan-in | Navigation reading |
|---|---:|---|---:|---:|---|---|
| world | 5 | `world.pgy`, `stage_intents.pgy`, the two compiler-world/root execution owners, and `artifact_zone_owner.pgy` | 34 / 33 | 4 / 30 | `artifact_zone_owner.pgy`: 27 | The composition root is small, but one resource owner is shared broadly. A `world/` folder must not imply that every Zone owner belongs beneath it. |
| facts | 294 | names ending in fact, identity, admission, readiness, shape, or representation owner | 1,056 / 624 | 465 / 591 | `direct_mir_scalar_cfg_graph_fact_owner.pgy`: 82 | This is the largest cross-consumer surface. Moving it by suffix would mix semantic dimensions and create path churn without reducing decisions. |
| plans | 67 | Pergyra filenames containing `plan` | 153 / 114 | 51 / 102 | `direct_mir_scalar_cfg_string_array_plan_lookup_owner.pgy`: 24 | Plan files are not one layer: sealed plans, plan facts, mutation owners, and lookup projections have different evidence lifetimes. |
| targets | 214 | LLVM, C-emission, emission-owner, projection-owner, and `target_` names | 514 / 257 | 424 / 90 | `direct_mir_scalar_program_runtime_abi_projection_owner.pgy`: 33 | Target-tagged files are internally dense. A bulk `targets/c` or `targets/llvm` move would risk hiding shared plan/fact imports behind target folders. |
| driver | 34 | `driver_*` | 54 / 31 | 44 / 10 | `driver_pipeline_owner.pgy`: 9 | The group is relatively cohesive, but it contains the active public executable rung. Path churn here has direct installed-driver risk. |
| compatibility-like | 9 | compatibility-evolution, `_legacy_`, and compatibility harness Pergyra files | 16 / 16 | 5 / 11 | `compatibility_evolution_owner.pgy`: 4 | This is the smallest coherent navigation surface. Its legacy routes and compatibility policy must still remain separate semantic owners. The adjacent expected text artifact is not counted as Pergyra code. |

### Cycle-risk inference

The current graph is acyclic, so a literal move with every relative path updated
does not inherently introduce a semantic dependency cycle. The risk begins if a
folder migration adds an aggregate import, re-export facade, or duplicate path
owner to make path changes easier. That would change the graph rather than
project it and is forbidden for all three clusters below.

The import counts also contradict a simple `facts/`, `plans/`, `targets/`
mass-move. Facts have 591 incoming edges from outside their filename group;
plans have 102; target projections have 90. Those are semantic cross-seams, not
evidence that a folder should become their authority. Track B, not this audit,
owns any claim that two of those files reconstruct the same decision.

## Bounded move cluster 1 — compatibility evolution pair

Proposed navigation path, if a later objective authorizes it:
`src/self_hosted/compiler/compatibility/`.

Exact files:

- `src/self_hosted/compiler/compatibility_evolution_owner.pgy`
- `src/self_hosted/compiler/compatibility_evolution_manifest.pgy`

Observed importer and blast-radius evidence:

- Importer count: four distinct importers and four incoming edges. The manifest
  imports the owner internally. The three external importers are `world.pgy`
  and the compatibility checker's `main.pgy` and `report_owner.pgy`.
- Import-path updates: three. Keeping the two files together preserves the
  manifest's same-directory import; only the three external imports change.
- Exact full-path literals: 84 occurrences across 10 files. They occur in two
  compiler path owners, three registry/document files, `OWNERS.md`, and four
  structural or parity tests. This count is much larger than the importer count
  because structural gates intentionally pin owner paths and terms.
- Generated/path references: `path_manifest_owner.pgy` owns the compiler owner
  path, while `test_harness_compatibility_paths_owner.pgy` owns the runnable
  manifest path. The expected artifact remains
  `expected/compatibility_evolution.txt`; it does not need to move.
- Registry/document references: both
  `docs/semantics/sot_owner_spine_registry.md` and
  `docs/192_protocol_abi_api_registry.md` name the owner path;
  `docs/166_production_bar_review_2026_07.md` and `src/self_hosted/OWNERS.md`
  also require path correction. Historical prose that uses only the basename
  does not require a mechanical rewrite.
- Cycle risk: low. The owner imports no Pergyra file, the manifest points only
  to that owner, and no reverse path was observed. No facade or compatibility
  aggregate is justified.

Gate if later authorized:

- executable: `tests/self_hosted/parity/compatibility_evolution_manifest_parity.sh`;
- path projection: `tests/self_hosted/compiler_world_manifest.sh` through the
  existing compiler-world contract;
- structural negative: reject both old root paths after all importers,
  registries, path owners, and gates have migrated.

Conclusion: **bounded and navigation-only, but defer**. It is the lowest-risk
physical move found, yet it removes no production bypass, repeated decision, or
compiler-scale operation.

## Bounded move cluster 2 — compiler-world composition anchors

Potential navigation path considered:
`src/self_hosted/compiler/world/`.

Exact files:

- `src/self_hosted/compiler/world.pgy`
- `src/self_hosted/compiler/stage_intents.pgy`
- `src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy`
- `src/self_hosted/compiler/compiler_root_intent_execution_owner.pgy`

`artifact_zone_owner.pgy` is deliberately excluded. Its 27 importers prove that
it is a shared resource owner, while putting it under `world/` would encourage
the false reading `folder == Zone authority`.

Observed importer and blast-radius evidence:

- Importer count: seven distinct importers and seven incoming edges. Three are
  internal (`composition -> world`, `root execution -> composition`, and
  `world -> stage intents`). Four external importers are the MIR-C stdout,
  source-C stdout, source-MIR stdout, and rung-2 artifact execution owners.
- Import-path updates: 20. Four external incoming imports change, and
  `world.pgy` has 16 outgoing imports to owners left at the compiler root. The
  three internal imports remain same-directory spellings.
- Exact full-path literals: 140 occurrences across 28 files: one benchmark
  evidence JSON file, two compiler path/source files, 12 documents,
  `OWNERS.md`, and 12 tests. Historical handoff and completion-log commands
  require individual disposition rather than blind replacement.
- Generated/path references: `path_manifest_owner.pgy` and
  `tests/self_hosted/compiler_world_manifest.sh` both publish current world and
  stage-intent paths. The benchmark evidence names the composition target and
  would become stale evidence after a move.
- Registry/document references: the architecture and dogfood documents name
  the exact root paths extensively. No new registry identity is needed; every
  existing owner identity must remain stable.
- Cycle risk: no current cycle. The internal chain is one-way, but the 16
  outward relative imports make accidental path mis-resolution materially more
  likely than in cluster 1. An aggregate `world` module would be a semantic
  change and is forbidden.

Gate if later authorized:

- structural/topology: `tests/self_host_compiler_topology_smoke.sh` and
  `tests/self_host_compiler_world_contract_smoke.sh`;
- executable authority: `tests/self_hosted/parity/compiler_root_intent_takeover_gate.sh`;
- old-path negative: reject all four old root paths only after path manifests,
  benchmark evidence, executable gates, and live documents are migrated.

Conclusion: **reject for the current work sequence**. The cluster is visually
coherent, but 20 import changes and 140 pinned path occurrences would obscure
the active executable composition root without changing one compiler decision.

## Bounded move cluster 3 — active source-MIR driver slice

Potential navigation path considered only to measure it:
`src/self_hosted/compiler/driver/source_mir/`.

Exact current files:

- `src/self_hosted/compiler/driver_source_mir_protocol_owner.pgy`
- `src/self_hosted/compiler/driver_source_mir_execution_owner.pgy`
- `src/self_hosted/compiler/driver_source_mir_stdout_execution_owner.pgy`

The stdout owner is the uncommitted Lease F addition, so this cluster is not a
base-revision move candidate.

Observed importer and blast-radius evidence:

- Importer count: five distinct importers and five incoming edges. Execution
  imports the protocol internally. The four external importers are the source-
  LLVM protocol and execution owners, `world.pgy`, and the rung-2 CLI read
  execution owner.
- Import-path updates: nine. Four external incoming imports change. Five
  outgoing imports cross to rung 2, MIR artifact transaction, compiler-world
  composition, and MIR diagnostic projection owners.
- Exact full-path literals: 38 occurrences across 12 current files: the SoT and
  protocol registries, the Track A audit, `OWNERS.md`, and eight executable or
  structural tests.
- Generated/registry/document references: no generated artifact owns these
  paths. `docs/semantics/sot_owner_spine_registry.md` and
  `docs/192_protocol_abi_api_registry.md` do, and their identity rows must not
  change while the physical paths move.
- Cycle risk: no current cycle, but the slice crosses compiler world, MIR,
  MIR-lowering, source-LLVM, and CLI consumers. A folder-level driver facade
  would conceal those typed cross-seams rather than remove them.

Gate if later authorized:

- executable: `tests/self_hosted/parity/public_mir_diagnostic_installed_self_host_owner.sh`
  and the installed-driver CLI mode gate;
- structural/action: `tests/self_hosted/parity/driver_source_mir_execution_action_gate.sh`;
- old-path negative: reject all three root paths only after Lease F is committed
  and every registry, importer, and executable gate is updated.

Conclusion: **reject while Lease F is active**. Moving these files now would
combine semantic migration with path churn and violate the active-rung lease.

## Final decision — defer navigation movement

A navigation-only move is **not currently lower risk or higher value than the
next executable substitution**.

Cluster 1 is the smallest defensible later move, but even it changes three
imports and 84 exact path literals while removing no bypass or repeated owned
operation. Clusters 2 and 3 touch the executable composition root directly.
The dominant 889-file `direct_mir_` surface cannot be safely rearranged by
filename family until Track B demonstrates a shared semantic dimension; moving
it first would merely preserve the distributed monolith under more folders.

Therefore the Track C integration recommendation is `DEFER`:

1. checkpoint Lease F and continue the one active executable substitution;
2. do not open a mass `world/facts/plans/targets/driver` migration;
3. if a later navigation-only objective is explicitly chosen, start with only
   cluster 1, preserve typed owner identities, update path owners and
   registries atomically, run the existing parity gate, and ratchet both old
   root paths.

No cluster in this report is `READY`, `CLOSED`, or `SUBSTITUTING`.

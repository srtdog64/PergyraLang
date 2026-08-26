# Public HIR Bypass Readiness — 2026-08-26

Status: `READ-ONLY AUDIT`; this report is not a semantic owner, implementation
authorization, readiness promotion, or substitution checkpoint.

## Scope and method

This is the HIR track from
`docs/agent_work_directives/public_ir_bypass_readiness_audit_2026-08-26.md`.
The named base and observed `HEAD` are both
`8b8c78f0d6f5efd0eecaeaec7ee2b1796b6723dd`.

The audit separated `--hir`, `--hir-cfg`, `--hir-dom`, and `--hir-ssa`. It used
read-only source/import searches and existing `bin/pgy.exe` probes. No source,
test, Make/workflow, registry, handoff, progress, or dogfood file was edited;
no build, suite, stage, commit, or push was run.

The common executable fixture was
`src/self_hosted/mir_lower/fixture/if_else_assign.pgy`. It has one `Main`
routine and an if/else assignment, so it distinguishes a one-block summary
from real CFG, dominator, and phi behavior without using a fixture dispatcher.
Captured stdout was kept in memory. Hashes below are observation aids, not
schema or compatibility owners.

## Public bypass trace

### Observed route

1. `src/pgy_driver.c:57-60` maps the four public options to one
   `DRIVER_OPTION_HIR_DUMP` with distinct `HIRDumpMode` values.
   `apply_driver_option` sets `dump_hir` and the selected mode.
2. Explicit `--native-pipeline` is handled first, but there is no installed HIR
   selector after it. Existing installed-selection predicates in
   `src/compiler/driver_self_host_selection_owner.c` explicitly reject
   `dump_hir`.
3. The final `return driver_run_pipeline(&flags)` in `src/pgy_driver.c` therefore
   owns all four default public routes today.
4. The native pipeline loads modules, runs semantic analysis, and calls
   `hir_lower_with_semantic_facts` at `src/compiler/driver_app.c:302-306`.
   `hir_finish_cfg_routine` then finalizes predecessors, dominance, dominance
   frontier, dominator tree, loops, local definitions, phi candidates, phi
   nodes, and routine CFG summaries in
   `src/compiler/hir_routine_cfg.c:109-131`.
5. The pipeline continues through DIR, RIR, AIR, and MIR before the late HIR
   dispatch at `src/compiler/driver_app.c:537-540`. That dispatch calls
   `hir_dump_mode` in `src/compiler/hir_public.c`; the late position does not
   transfer ownership of the HIR payload to any later IR.

### Executable bypass evidence

Public and explicit-native output were byte-equal for all four modes on the
fixture:

| Mode | Exit | Captured stdout bytes | SHA-256 | Public vs explicit native |
|---|---:|---:|---|---|
| `--hir` | 0 | 1,140 | `db0729629654e3d376246e2b52e58b3ff8f7c1da771734cda055085a3c4ca93f` | equal |
| `--hir-cfg` | 0 | 323 | `6243014d240c5bf2f7c6f8880bee302f8e461b4d52df55b89cf4f066c76c7d58` | equal |
| `--hir-dom` | 0 | 471 | `5200231392f299d3464cef765c13074dc60697166273357ee14ab810aad4368c` | equal |
| `--hir-ssa` | 0 | 655 | `f63a555d78d90c092793260b353dabbb47e8547b11eed8c511d4853b403ba257` | equal |

The stronger negative also proves that no installed Pergyra producer is
currently reached:

```text
PGY_SELF_DRIVER_BIN=.tmp/definitely-missing-hir-driver
PGY_DEBUG_PIPELINE_TIMING=1
bin/pgy.exe --hir-cfg src/self_hosted/mir_lower/fixture/if_else_assign.pgy
```

Observed result: exit 0, 323 stdout bytes, and a `[pipeline timing]` record on
stderr. A missing installed driver has no effect because the final native
pipeline remains the route owner.

## Native observable fact families

The four modes are not aliases. Each payload below is emitted from typed native
`HIRProgram`, `HIRRoutine`, and `HIRBasicBlock` storage in
`src/compiler/hir.h`.

### `--hir` — full human summary

`hir_dump` emits:

- program counts: items, declarations, routines, externs, types, abilities,
  roles, parties, rosters, worlds, subjects, events, functions, and
  executables;
- program booleans: resource-flow facts present, function-parameter-flow facts
  present, and `Main` present;
- ordered top-level item ordinal, kind, and name;
- ordered routine ordinal, stable `RoutineId`, source syntax identity, kind,
  name, HIR phase, direct-call count, resolved-callee count, hosted/action-like/
  exported/entry-reachable/control-flow booleans, and signature type rows;
- routine CFG count and entry block;
- per-block predecessor and dominance-frontier counts, successor shape, loop
  header/depth, reachability, RPO index, immediate dominator, statement count,
  and pin-region state;
- for pin blocks, source/view name and read/write mode;
- per-statement AST node type and source line.

The successful payload is also environment-sensitive. With
`PGY_DEBUG_RESOURCE_FLOW_FACTS` it emits each routine's stable symbol index,
declaration syntax identity, parameter position, line/column, symbol kind, and
name. With `PGY_DEBUG_FUNCTION_PARAM_FLOW` it emits parameter index/mask rows.
An observed resource fixture produced
`resource-flow[00] stable=0 decl=5 ... kind=4 name=s`.

This is not a small summary-only replacement surface: it includes CFG,
dominance, statement provenance, pin facts, and optional flow rows.

### `--hir-cfg` — HIR CFG summary and topology

The payload emits:

- HIR declaration and routine counts;
- routine kind/name, entry reachability, direct-call count, block count,
  reachable/dead block counts, return/normal-exit counts, total phi candidates,
  and blocks containing phi candidates;
- per-block reachability, predecessor count, and true/false/no-successor shape.

The fixture's meaningful row is one routine with four live blocks, one normal
exit, one phi candidate, a `TF` entry split, and a two-predecessor join.

### `--hir-dom` — CFG plus dominator/loop facts

This mode includes every `--hir-cfg` field and adds, per block:

- reverse-postorder index;
- present/absent immediate dominator identity;
- dominance-frontier cardinality;
- loop-header flag and loop depth.

These values are computed by the native HIR algorithms in
`src/compiler/hir_cfg.c`, not copied from MIR or AIR.

### `--hir-ssa` — dominators plus HIR SSA-preparation facts

This mode includes every `--hir-dom` field and adds:

- local-definition count and ordered local names per block;
- materialized HIR phi-node count and ordered phi names per block;
- dominator-tree child count per block.

For the fixture, three blocks define `value` and the join block contains one
materialized HIR phi named `value`.

## Existing typed Pergyra ownership

### What exists

- `src/self_hosted/hir/typed_ast_arena_owner.pgy` and
  `ast_node_kind_owner.pgy` own a typed AST arena and compact syntax/HIR-shared
  node-kind vocabulary.
- `ast_expression_graph_owner.pgy`, `ast_destructure_graph_owner.pgy`, and
  `ast_match_pattern_fact_owner.pgy` own selected parser graph facts.
- `ast_text_arena_projection_owner.pgy` constructs the parser-owned
  `AstTreeArtifact`; the remaining text inventory files are explicitly
  transitional inputs.
- Self-host semantic owners add typed semantic facts. The production source-MIR
  path in `driver_rung2_owner.pgy` then goes from `AstTreeArtifact` and
  `SemanticAstArtifactAnalysis` directly to `SelfMirProgramFacts`.
- The admitted/self-produced MIR graph owns MIR routine, block reachability,
  successor, definition-dominance, and phi facts. Those are valid MIR owners,
  not HIR owners.

### What does not exist

A complete search found no Pergyra `SelfHirProgram`, `HirProgram`,
`HIRProgram`, `HirRoutine`, or `HIRRoutine` object/struct and no HIR lowering
entrypoint under `src/self_hosted/`. The only `Hir`-named Pergyra function found
outside comments is compiler-path metadata for native region evidence.

This matches the repository's own contracts:

- `src/self_hosted/hir/README.md` says HIR lowering or validation must consume
  parser-owned facts and compare against the C compiler **before** it counts as
  substitution.
- `docs/semantics/sot_owner_spine_registry.md` keeps
  `hir.typed_control_flow` at native owner `src/compiler/hir.c#hir_lower` with
  status `BRIDGE`; it says complete HIR-owned control-flow input remains open.
- The self-host HIR `OWNERS.md` section names typed AST and selected graph fact
  owners only. It names no Pergyra HIR program/routine/CFG producer.

### Why MIR is not a replacement HIR owner

The Pergyra `pgy.mir.v1` artifact has useful nearby fields: routine kind/name,
source syntax identity, blocks, reachability, successors, instructions, and
MIR phi rows. It does not carry the native HIR declaration/routine inventory,
entry reachability, direct-call count, HIR live/dead/return/normal-exit
summaries, HIR phi-candidate counts, predecessor rows, RPO, immediate
dominators, dominance frontiers, loop depths, HIR local-definition rows,
dominator-tree children, HIR phi-node names, pin rows, or statement AST
type/line rows.

Some missing cardinalities could be recomputed from MIR successors or
instructions. Doing so would be both a reconstruction and an IR substitution:
MIR blocks and phi instructions are not the native HIR blocks and
SSA-preparation nodes promised by the public mode names. The directive forbids
presenting that partial projection as the legacy HIR payload.

## Per-mode readiness decision

| Mode | Closest typed Pergyra evidence | First unowned observable fact | Decision |
|---|---|---|---|
| `--hir` | typed AST artifact, semantic facts, and later MIR facts | a post-lowering typed HIR program/routine inventory bound to stable HIR `RoutineId`; without it even `decls`, `routines`, ordered items, and routine rows are not HIR-owned | `NOT READY` |
| `--hir-cfg` | MIR routine/block graph has nearby successor and reachability facts | a typed HIR routine/CFG owner keyed by HIR `RoutineId` and block identity, beginning with exact HIR declaration/routine inventory and carrying predecessor, terminator, exit-summary, and phi-candidate facts | `NOT READY` |
| `--hir-dom` | MIR definition-dominance predicates over admitted MIR blocks | HIR RPO, immediate-dominator, dominance-frontier, loop-header, and loop-depth rows bound to the HIR CFG owner | `NOT READY` |
| `--hir-ssa` | self-produced MIR SSA definitions and phi instructions | HIR block-local definition inventory, materialized HIR phi nodes, and dominator-tree child rows derived from the HIR dominator owner | `NOT READY` |

`--hir-cfg` is the smallest individual surface: it omits statement/pin/debug
rows from `--hir` and omits the dominance and SSA extensions. It is still not
ready because the repository has no typed Pergyra HIR routine/CFG owner. That
missing identity-bearing inventory is earlier than every per-block projection
and is the first blocker for this track.

## Smallest future falsifier

No implementation rung should open from the current tree. Once a typed Pergyra
HIR routine/CFG producer exists, the smallest focused gate is one
`public_hir_cfg_installed_self_host_owner` case over
`if_else_assign.pgy`:

1. compare a direct installed internal HIR-CFG mode, public `pgy --hir-cfg`,
   and explicit `pgy --native-pipeline --hir-cfg` byte for byte;
2. require the four-block `TF` split, two-predecessor join, four-live/zero-dead,
   one-normal-exit, and one-phi-candidate rows;
3. remove the typed predecessor fact for the join block and require the direct
   Pergyra producer to exit nonzero with zero stdout rather than reconstructing
   predecessors from source, AST, or MIR text;
4. set `PGY_SELF_DRIVER_BIN` to a missing path and require public `--hir-cfg`
   to exit nonzero with zero stdout and no `[pipeline timing]` marker;
5. reject mixed/unsupported requests such as `--hir-cfg --hir-dom` without
   falling through to native execution.

Step 4 is already an executable negative and currently fails exactly as
expected for an open bypass: it succeeds natively. Step 3 cannot be implemented
honestly until the missing HIR owner and fact carrier exist; a parser fixture,
MIR mutation, or native dump parser would not be an equivalent negative.

## Final result

Overall HIR track: **`NOT READY`**.

The first missing fact is an identity-bearing, typed Pergyra HIR program/routine
inventory produced after semantic admission and before CFG projection. The
smallest mode, `--hir-cfg`, additionally needs its own HIR block/predecessor/
exit-summary/phi-candidate carrier. Until that owner exists and fails closed on
a missing join predecessor, none of the four public native dispatches can be
deleted without guessing facts or substituting MIR semantics for HIR semantics.

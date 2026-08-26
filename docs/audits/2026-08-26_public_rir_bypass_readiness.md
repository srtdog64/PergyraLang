# Public `--rir` / `--rir-json` bypass readiness audit

- Audited revision: `8b8c78f0d6f5efd0eecaeaec7ee2b1796b6723dd`
- Scope: public RIR stdout modes only
- Result: **NOT READY**

## Decision

The public launcher still reaches the native C-owned RIR producer and dumpers
for both modes. There is no complete Pergyra-owned general RIR producer behind
the installed production root. The only self-hosted component named for RIR is
a consumer of native `pgy.rir.v1` JSON, and the owner registry explicitly says
that the initial RIR shape/resource collection and self-hosted RIR producer
remain open. Therefore routing either public mode to the installed self-host
driver now would require reconstructing or guessing RIR semantics and would
violate the audit's forbidden-fallback boundary.

The first missing owned fact is an ordered, admitted Pergyra RIR program/scope
inventory: scope identity (`kind`, `source_syntax_id`, owner/name and resource
identity status) together with its typed resource facts, operations, normalized
state summaries, parameter/resource-flow identities and CFG flow-block facts.
No existing Pergyra artifact owns that complete family.

## Observations

### 1. Public route and native rejection/production boundary

1. `src/pgy_driver.c:69-70` admits `--rir` and `--rir-json` into
   `DriverFlags.dump_rir` / `dump_rir_json`.
2. The one declared native opt-out is handled first at
   `src/pgy_driver.c:252-261`. The installed source-stdout selector below it
   covers tokens, AST, capability manifest and DIR only
   (`src/pgy_driver.c:271-283`); its owner explicitly rejects either RIR flag
   (`src/compiler/driver_self_host_selection_owner.c:61-82`). There is no RIR
   branch before the final `return driver_run_pipeline(&flags)` at
   `src/pgy_driver.c:339`. Thus default public RIR and explicit
   `--native-pipeline` RIR enter the same native function today.
3. Native production is
   `driver_run_pipeline -> driver_run_pipeline_timed`
   (`src/compiler/driver_app.c:90-117`). It creates RIR from the annotated AST
   with `rir_lower`, enriches it from HIR, validates it, and validates it
   against DIR at `src/compiler/driver_app.c:373-417`. The C owner scans the
   `AST_PROGRAM` and constructs function/method/zone/relation/effect/world/
   intent scopes at `src/compiler/rir_builder.c:294-409`.
4. The native path then synthesizes AIR and lowers/validates MIR
   (`src/compiler/driver_app.c:419-486`) before allowing either RIR stdout
   projection. Human output calls `rir_dump` at
   `src/compiler/driver_app.c:513-516`; JSON calls `rir_dump_json` at
   `src/compiler/driver_app.c:519-522`. A replacement must preserve this
   fail-before-output admission behavior; a partial RIR printer is not parity.
5. JSON mode suppresses the human semantic summary so stdout stays one machine
   artifact (`src/compiler/driver_app.c:254-259`).

### 2. Complete observable stdout fact inventory

The static inventory comes from the two actual dump owners. A bounded probe used
`examples/slots_simple.pgy:3-29`, which has three routines, two explicit slot
facts and non-empty resource operations.

Human `--rir` (`src/compiler/rir_public_surface.c:369-526`) emits:

- program scope count;
- per-scope ordinal, scope kind, qualified owner/name, source syntax ID,
  resource-identity status, fact count and operation count;
- normalized state-summary count, state-error flag and conservative semantic
  flags. The flag vocabulary is authority, projection, world handoff,
  invalidation, authority loss and projection invalidation
  (`src/compiler/rir_public_surface.c:8-48`);
- function-parameter-flow summary count and each parameter index/mask;
- resource-flow-symbol count and rows containing name, stable index,
  declaration syntax ID, parameter status and parameter index;
- fact rows containing fact kind, name, slot anchor, two arguments, resource
  kind/state, flow-identity status and stable index;
- operation rows containing operation kind, subject, slot anchor, two
  arguments and machine-contact classification;
- normalized state rows containing origin fact kind, name/slot, resource kind,
  initial/final state, last operation, transition-error flag, flow-identity
  status and stable index;
- flow-block rows containing block ID, reachable/join flags, fact count and
  entry/exit semantic flags, plus each flow fact's name/slot, entry/exit state,
  join, widening and entry/exit-conflict flags.

The probe observed `scopes=3`; `DemoParallel` had two resource-flow symbols,
two facts, nine operations and two state summaries. Its stdout, captured and
LF-normalized in memory, was 2,820 UTF-8 bytes with SHA-256
`5DB65E1ADA7470695308F2ECFACD8A1D3785F48F0B1ED0D64B316ADBC1966507`.
Default public and explicit native captures were equal and both exited zero.

JSON `--rir-json` (`src/compiler/rir_dump_json.c:27-215`) emits:

- top level: `rir_version`, `scope_count`, `scopes`;
- scope: `index`, `kind`, `source_syntax_id`,
  `resource_identity_verified`, `resource_flow_symbol_count`,
  `function_param_flow_summary_count`, `name`, `owner`, `fact_count`,
  `op_count`, `resource_flow_symbols`, `facts`, `ops`, `summaries`,
  `has_state_errors` (`src/compiler/rir_dump_json.c:127-165`);
- resource-flow symbol: `stable_index`, `declaration_syntax_id`, `line`,
  `column`, `symbol_kind`, `is_parameter`, `parameter_index`, `name`
  (`src/compiler/rir_dump_json.c:53-68`);
- fact: `kind`, `resource`, `name`, `slot_anchor`, `arg0`, `arg1`, `state`,
  `flow_identity`, with `stable_index` and `declaration_syntax_id` when the
  identity is present (`src/compiler/rir_dump_json.c:27-49`);
- operation: `kind`, `subject`, `slot_anchor`, `arg0`, `arg1`,
  `machine_contact` (`src/compiler/rir_dump_json.c:72-85`);
- summary: `name`, `slot_anchor`, `kind`, `resource`, `initial_state`,
  `final_state`, `last_op`, `has_error`, `flow_identity`, with stable and
  declaration IDs when present (`src/compiler/rir_dump_json.c:89-112`).

Unlike human mode, JSON publishes only the function-parameter-flow count and
does not publish parameter-flow rows, conservative semantic flags, flow blocks
or flow facts. On the same source the JSON document had three scopes and the
same per-scope 0/2/0 symbol and fact cardinalities; the captured normalized
stdout was 3,994 UTF-8 bytes with SHA-256
`F851A35A61903F9EADF4A965F36A3FAD93B0A13ED97E7A77EA8463714BF80648`.
Default public and explicit native captures were equal and both exited zero.

### 3. Producer ownership and reachability

- Native RIR has a concrete complete data model: scope/fact/op/state/flow types
  are declared at `src/compiler/rir.h:15-265`, and `RIRProgram` owns its scope
  array and AST root at `src/compiler/rir.h:267-273`. HIR enrichment copies and
  validates routine-local resource-flow and parameter-flow identities at
  `src/compiler/rir_flow.c:611-668`. These are C-owned production facts, not a
  Pergyra producer.
- The installed Pergyra composition root admits argv once and dispatches its
  typed request at `src/self_hosted/compiler/driver_bootstrap_main.pgy:7-10`.
  Its complete request enum has tokens, AST, capability, DIR, machine, C, LLVM
  and MIR variants but no RIR variant
  (`src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy:8-37`). This is
  direct evidence that public RIR is not production-root reachable there.
- `src/self_hosted/tools/machine_layer_rir_validator/main.pgy:1-5` explicitly
  declares itself a consumer that does not recover source meaning. It reads a
  native JSON document, checks `rir_version` and non-empty `scopes`, then scans
  only each scope's `ops[].machine_contact`
  (`src/self_hosted/tools/machine_layer_rir_validator/main.pgy:42-84`). Its
  contract likewise calls native `pgy --rir-json` the producer and the tool an
  artifact consumer (`src/self_hosted/tools/machine_layer_rir_validator/intent.md:3-10`).
  This is bounded `REACHABLE` tool evidence, not `SUBSTITUTING` production
  evidence.
- `src/self_hosted/sea/execution_lane.pgy:51-59` carries selected boolean
  `has_rir_*_evidence` inputs. Those booleans neither own the ordered RIR scope
  graph nor the dump payload and cannot be promoted into a producer.
- The authoritative registry row for `rir.resource_transition_graph` still
  names native `src/compiler/rir_builder.c | rir_lower`, marks the family
  `BRIDGE`, and says initial AST-owned shape/resource collection and the
  self-hosted RIR producer remain open
  (`docs/semantics/sot_owner_spine_registry.md:67`). This agrees with the
  production-root and source inventory above.

## Inference

The equal public/native output proves the bypass, not Pergyra ownership. The
validator proves that one native JSON subfield can be checked against an
existing machine-contact vocabulary; it does not produce scope order, resource
lifecycle, state normalization, flow identity, CFG flow facts or either stdout
projection. MIR, DIR and SEA facts overlap parts of the information, but
reconstructing RIR from them would create a second authority and would still
guess fields whose lifetime is currently owned by native `rir_lower` and
`rir_enrich_with_hir_flow`.

Accordingly neither `--rir` nor `--rir-json` is ready for a hard substitution
rung at this revision.

## Smallest future falsifier (proposal, not completed work)

Do not change public routing until one responsibility-named Pergyra owner
produces and fail-closed validates the complete ordered RIR fact family above.
Once that exists, the smallest focused gate should use
`examples/slots_simple.pgy` and:

1. invoke the installed driver's exact internal RIR text and JSON requests;
2. invoke default public `pgy --rir` / `pgy --rir-json` with native opt-outs
   unset and require byte equality with those installed outputs;
3. invoke explicit `pgy --native-pipeline --rir` / `--rir-json` as the
   independent oracle and require complete field/cardinality/value parity (no
   omitted flow rows and no guessed source/stable identities);
4. set `PGY_SELF_DRIVER_BIN` to a missing sibling and
   `PGY_DEBUG_PIPELINE_TIMING=1`; each default public mode must exit non-zero,
   leave stdout empty, report the unavailable installed driver, and contain no
   `[pipeline timing]` marker. This falsifies native retry;
5. add one unsupported-option case such as `--rir-json <source> --verbose` and
   require non-zero/empty stdout with the selector's owned diagnostic. This
   falsifies a broad or implicit dispatch.

No implementation rung is justified before item 1 has a real typed producer;
adding only CLI variants, a serializer, fixtures, or another native-JSON reader
would not change this **NOT READY** result.

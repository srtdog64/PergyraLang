# DRV-2 checked cast and consumed-edge admission

Status: IMPLEMENTED / FROZEN; focused gates GREEN, canonical RED. Base: shared main
`f0edb866c9e914d9f1bdb5e24982319f9c4081ea`, preserved implementation
`1e39d7f6f085bcd3c66fee166bdf1627b6de4177` and navigation checkpoint
`06dcf1ae17118bb80b62d96dff5008d55184b86c`. This coordinates one execution
unit of the active DRV-2 recovery, not a new semantic owner or successor rung.

## Shared objective and whole-chain boundary

- Objective: connect admitted numeric cast semantics and every consumed syntax
  edge to the existing parameter-flow owner without losing operand effects or
  accepting stale origins/output relationships.
- Priority: semantic identity/one owner, current typed evidence, retaining-effect
  preservation, stale-fact refusal, then patch size. Add no owners or ceremony.
- Entry: the unchanged import-composed
  `tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy` through
  the native driver; its C and LLVM source probes must ultimately compile.
- Fact owner: typed values and original operand/target spelling snapshots in
  `function_param_publication_owner.c`; original consumed-edge claims in
  `function_param_flow_store_destroy.c`; existing all-formal routine/index and
  fixed-point transfer in `function_param_flow_summary.c` / its index owner.
- Last consumers: cached index/no-store queries, returned backing/output
  admission, finalized raw parameter masks and pending ref-call diagnostics.
- Lifetime: all rows belong to the existing admitted program generation;
  compilation-owned index/store retirement frees owned names and edge rows.
- Forbidden fallback: node spelling/type guess instead of current published
  input, unproved empty relation, dropped child effects, stale cache reuse,
  a constructor-freshness grant, larger budget or source annotations/copies.

The fixed-input census is complete at the diagnostic level: 4872 callsite,
2022 routine CALL and 34 RETURN refusals (6928/9). Private `Y25UJI` reproduces
that baseline with guards. Its final-row issuer map exposes incomplete facts,
not proof of safety: 2461 synthetic-CALL backing rows have no retained effect.
CAST absence is source verified in the selector/backing/claim paths. The exact
unsupported-shape distribution is not yet measured; do not claim CAST closes
all rows. Independent 30InZc review also identified missing child claims for
channel sends, array literals and tuple literals actually consumed by backing.

## Intended change set and dependency order

1. The existing AST_CAST type-check branch, not a second numeric type table,
   records its actual successful numeric check when publication is pending.
   Publication owns a checked numeric-cast query: original cast/operand IDs and
   pointer, exact owned target spelling, current result and operand scalar
   facts. Missing/stale facts fail closed. String/aggregate casts get no grant.
   Reuse the existing node-specific publication carrier and retirement path;
   this is not another scalar taxonomy or a new source mode.
   Consolidate duplicated current-value/call-child validation and physical
   classification inside that same owner to preserve its existing 599-code-line
   cap; no new owner, cross-owner transfer or compressed statements for the cap.
   Place private snapshot declarations beside the existing PublishedFormal/
   PendingRoutine schemas in this owner's existing header; no raw consumers.
2. Claims project every child consumed by the admitted walker, including send
   value/channel and array/tuple elements. Capture count, pointer and ID;
   revalidate numeric-cast eligibility/target on cached use. Unsupported rows
   remain conservative rather than becoming a local permission.
3. Selector and backing consumers admit only the checked numeric cast. Its
   result is a physical value, but its operand is still traversed: a retaining
   call returning an Int cannot become harmless because of an outer cast.
   Ordinary RETURN/CALL/CHANNEL masks, output ordinals and recursion stay owned.
4. Freeze the joined source and replay original plus new falsifiers on one
   identified executable. Rerun full canonical and record every remaining
   obligation; no SoT/cleanup/bootstrap closure from this bounded delta.

No source ABI, public syntax, result-loan contract or copy/drop obligation is
changed. Some/Unwrap return-loan production, separation of actual retention
from incomplete output facts, the direct-ref-String last consumer, and genuine
RETURN refusals remain required parts of the full DRV-2 plan, not exemptions.
String channel transport owns a copy; shallow Slot/opaque retention is separate.

## Exactly three implementation scopes

- A: existing summary and program-point index C owners; no publication/store
  edits. Consume the B-owned checked-cast seam, preserve all operand effects.
- B: existing publication C/header, store claim C and internal claim header,
  plus only the AST_CAST branch in `type_checker_expr.c` if needed to publish
  its actual admitted operation (no global error-count/dispatcher change);
  no transfer walker or tests. Own target/name snapshot lifecycle and complete
  consumed-edge validation. Current-node validation consolidation stays inside
  this owner. Coordinate the query signature before edits.
- C: existing native phase unit and focused query fixtures/gate; no production
  edits. Preserve original expectations and use real parsed nodes/definitions.
- Root: integration, receipts, independent reruns, scoped checkpoint/push and
  delivery to `Red-team review DRV-2 candidate` (thread
  `01a13074-17a4-7c23-aaad-1509e785e664`). Review is external, not a fourth lane.

## One shared integration gate and falsifiers

Root builds once from a frozen manifest and runs the original native/query
matrix, work-cap controls, full canonical and release witnesses. Keep 4096
work units, original input sizes, 30-second pressure controls and existing
60-second static / 5-minute focused / 30-minute integration budgets.

Falsifiers: valid numeric cast loop/read and scalar return; retaining operand
under a numeric cast; nonphysical cast receives no permission; target spelling,
operand/binding/generation drift; completed send/array/tuple parent retains its
ID while a consumed child/count changes. All stale queries must refuse and
restored controls remain valid. Preserve the original 17/46, 68-formal and
setter/recursive output negatives; unsafe programs are analysis-only.

Outputs are implementation candidates, not closure receipts. No shared builds
by agents, broad process termination, new owner files, main RED landing or
claim that a local native pass substitutes a Pergyra compiler path.

## Root-observed joined receipt

`xGbOB4` / outer `ATi2jr`: native phase 1881/0; semantic 3222/0; HIR 58/0;
MIR 327/0; transpile 1008/0; transition 15/0; LLVM receiver 98/0; query
`l0Gnl7` 18 C/LLVM positives and 47 exact analyze-only negatives. Original
assertions remain unchanged. Summary `9c3bwB` / `9q59jjgx` passes original
4097 independent queries and 4096-work single-closure exhaustion; no budget or
input was weakened. Witness `pHLNeO` / `sMgozz` passes original controls and
four validation-only mutations. Source/input/binary/recipe guards pass.

Full unchanged canonical `rjVLUu` / `abLmfq` is still RED: 6898/9, down 30
diagnostics from 6928/9 (4845 callsite CALL, 2019 routine CALL, 34 RETURN).
No work-budget error. Documentation gate passes; component inventory rc1
under the original 60-second wrapper has no usable diagnostic and stays OPEN.
No installed-driver/bootstrap/CI or new-candidate independent review is claimed.

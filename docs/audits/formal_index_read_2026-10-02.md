# Indexed String formal dependency in the held aggregate lifetime rung

Date: 2026-10-02, 08:04 KST. Base: `d36492602364ed96c55fe9584d3962eab5744e90`.
Status: focused dependency VERIFIED; aggregate-formal lifetime remains HELD.
Root alone edited/built/integrated. Three read-only agents checked primitive
operator authority, caller liveness, actual formal identity and gate wiring.
Their findings were independently compiled and exercised by root.

## Observed baseline and change

Frozen d364 native C/LLVM source analyzers, from
`inout-final-staged.EVczHC/.tmp/self_hosted/collection-inout-effect.600OZP`,
over-refused four intended indexed-read positives on both backends: owned
equality, inequality with an Option-return RHS, copied formal forwarding, and
read before own consumption. Evidence: `index-read-baseline.0xNfpC`.
Borrowed read and read before a future opaque local call were accepted; the
nine original indexed negative inputs were refused. Supplied inputs were only
analyzed, not emitted or executed.

The same frozen analyzers accepted the two unproved formal-forwarding inputs
(`inout_index_formal_unknown_negative` and its deferred counterpart).
Evidence: `index-formal-baseline.A6yP7C`. This is an admission/proof-contract
observation, not an executed memory-safety exploit or measured runtime fault.

The existing formal-effect owner now distinguishes metadata-only effect1,
conditional Live {Empty, Owned} effect2, and live indexed-read effect3. An
Index carries borrowed-element identity, not Owned permission. Each physical
parent edge and each lane root is checked. Only primitive Equality/Inequality
with carried target kind None consumes an element without escape. Custom role
operators, indexed writes, aliases, returns, array stores and shallow append
leave the formal unproved. A forwarding cycle without a proved sink grants
nothing. Two new scratch arrays are explicitly retired; this does not retire
the returned inventory or effect maps.

Caller entry consumes the existing typed possible-retirement bounds, including
same-call own evaluation, branch/loop consumption and defer. Unknown source
effects are possible retirement bounds, not actual release receipts. A normal
local read before a future opaque call remains accepted; after it, or deferred
past it, entry is unproved. Conditional copy entry still requires clean state.
Non-local forwarding additionally requires the exact actual-formal ID's proved
whole-use effect2/3. Unknown wrapper formals are refused.

Supported indexed actuals are scoped locals and proved formals. Direct literal,
fresh expression and aggregate-field actuals are conservatively refused in
this slice; these limitations follow the guard but were not separately executed
as new literal/fresh-expression controls. No general non-local liveness claim,
public proof syntax, arbitrary scalar sink or type/name-based grant was added.
Pure facts stay func/struct, as required by pergyra-authoring.

## Actual compiler formal observation

One existing identity analyzer also observes the imported real producer after
identity sealing. Module suffix, source signature, formal syntax ID, ordinal
and raw parameter mode are checked; names select test observations only.

| Actual source function | Formal ordinal | Raw mode | Proved effect |
| --- | --- | --- | --- |
| SemanticAstExpressionFunctionRowIndex | 0 | 0 default | 3 live indexed read |
| SeedSemanticOwnedBuiltinSignaturesUnclaimed | 1,2,3 | 1 inout | 2 clean-preserving |
| SemanticAstExpressionFunctionTables | 3,4,5 | 1 inout | 2 clean-preserving |

This reaches real indexed `==`, `!=`, Bool composition, default-mode forwarding
and three-array copied seeding, not a same-named replacement fixture.
The observer accepts a successful body verdict or the collection owner's
borrow_boundary_escape refusal with a real syntax ID. In the current pipeline
that collection call is reached only after successful identity sealing; the
initial Ok/admission-error carriers cannot satisfy this witness. It additionally
checks borrowed surface and carried call-target readiness. It does not re-seal
or claim aggregate Release/full body acceptance. Future stage-order changes
must preserve or revise this test-only witness explicitly.

## Observed gates

Frozen index source snapshot: `.tmp/self_hosted/index-read-staged.dstKiW`.
Earlier snapshot `index-read-staged.rfZvea` stopped at native compilation on a
new local-name shadow; that name was corrected before the final snapshot.

- `collection_inout_effect_owner.sh`: PASS, evidence
  `collection-inout-effect.LabNlQ`;46 fixed source admissions +16 identity/
  boundary checks +7 actual formal observations per C/LLVM backend =138.
- Mutation13 proves indexed3 and forwarded copied2;14 supplies a structurally
  ready nonempty role-operator target and refuses the same formals;15 gives one
  Index a second escaping physical parent, requires graph readiness, and
  refuses all three summaries. No mutation is re-sealed.
- `index-read-regressions.sh`: PASS, evidence `index-read-regressions.lWeEbL`;
 31 constructor identity +13 field-write owner checks +8 constructor source
  controls per backend =104. Final frozen source analyzers are reused.
- Native binary, owners, fixed inputs and all transitive self-host Pergyra source
  SHA checks PASS before/after. Supplied programs remain analyze-only.
- Seven lexical cap requests PASS:95/120,176/180,160/160,35/80,51/60,175/200,
  collection verdict587/600. Comment-only records excluded; blanks retained;
  no cap raised. Component-checker mechanics PASS14 tests.

Native/installed driver/manifest SHA256 stayed unchanged respectively:
`A5DFE2CB3258216AB546EA39EEF65839D45100950D7DF7295D756DA9F72B3936`,
`707DCD40049A1697A5827B2A7C8D3CF509573AA3C0031F2EEE338C9FA0D78EC7`,
`0A83B0DB5EFE3C00C6D9413C63045C4B17AFF079781213B280442C588E5A9C19`.
Native compile probes still report9 unreachable warnings. Full structural
inventory, installed replacement, whole lifetime matrix, fixed point and
compiler-scale performance were not run or promoted by these focused gates.

## Unresolved executable boundary

Exact base-head CI36931966062 completed failure. The failed job was
self-host-codegen-bootstrap-linux, step Run self-host codegen fixed point and
breadth: codegen_nominal_array_declaration rejected the MIR root control
`tests/self_hosted/parity/fixture/mir_collection_receiver_root.pgy`. Six dependent
jobs were skipped. CI logs did not provide the underlying control diagnostic;
no cancellation, rerun, fix or full-CI success was inferred.

Registry remains CLOSED70 / BRIDGE23 / ACTIVE2; whole-row delta0 and C-owned
substitution delta0. No C bypass was identified for this dependency. Event-time
constructor/result field state, conditional exact field release/retired writeback
and last-owner retirement of returned temporary facts remain BLOCKED facts in
the one HELD rung. Preserve FromArtifact and all three Release consumers.
Do not delete cleanup, revive native UNKNOWN-formal exemptions, grant private
retirement by spelling, or invent a public MapDrop to hide missing ownership.

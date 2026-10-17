# Indexed String field lineage at canonical466

Status: BOUNDED IMPLEMENTATION APPROVED; CANONICAL/CI OPEN. Base: main
`7349d73559057ce2c1e88ea10c581a2bed151c7d` (independent native MIR review approved).
This coordination plan is not semantic authority or a replacement acceptance rung.

## Objective and finish boundary

- Admit the unchanged canonical String field handoff only after its actual
  Array-element lifetime source is produced by an existing owner and remains
  current through the final consumer. Do not replace a missing lifetime proof
  with a callee read-only mask.
- Priority: stable source/field identity, sound backing lifetime, one owner,
  pure current replay, preserved negatives, then fixed/current canonical and
  actual bootstrap/installed-driver CI. No manual Clone/drop/annotation edits.
- Existing fact owners: indexed String sources and publication's typed
  projection/nominal-constructor mappings; flow claims and returned-result
  dependencies own the executed expression/control relationships.
- Production chain: expression/argument type checking -> indexed String source
  publication -> explicit one-time preparation -> program-success replay ->
  SemanticResult -> HIR/MIR admission and final C/LLVM emission.
- Last consumers: the actual source use and immutable admitted evidence at
  final emission. Wire/import remains separately OPEN; no new serialization
  assertion or independent AIR attestation is implied.
- Forbidden: UNKNOWN=0, zero formal-origin bits as local-storage independence,
  an EMPTY origin as no-owned-history, a source-name whitelist, regenerated
  facts on a failed current query, new cleanup/owned-element permission,
  smaller canonical input, raised budgets, or weakening original negatives.

## Observed blocker and rejected shortcuts

- cae7f20b native: QVTkpu full fixed/current inputs both fail MIR0/errors18435/
  warnings16, first BORROW_ESCAPE466:22 in
  `AstTreeArtifactPayloadContractReady`.
- The source is `artifact.tree_text` in the four-actual call to
  `AstTreeArtifactIdentityDigest`. Other actuals are a physical count and two
  REF aggregates. An unrelated local `no_texts:Array<String>=[]` triggers the
  conservative UNKNOWN-source scan.
- `AstTreeArtifactFromText` returns a nominal constructor with its tree_text
  formal in the selected String field. The current caller supplies a literal.
  This is a source observation, NOT an admitted field-source fact.
- UNKNOWN fields have no current backing identity. The existing scan skips a
  consumed candidate when it has no deferred reservation; String IDENTIFIER
  invalidation does not prove MEMBER liveness. A read-only callee cannot make
  an already-dead field live. Later actual evaluation and worker/defer timing
  are independent obligations if a borrowed source is supported.
- Declared-result frames accept aggregate types, but their zero formal-origin
  bits do not track local Array generations. They cannot alone certify every
  String payload in an aggregate as independent.

## Proposed fact path; representation not yet selected

- Prefer producing the selected field's exact lineage rather than adding an
  opaque-read exemption. Candidate path: current local definition -> selected
  result call -> every reached return -> typed nominal constructor field ->
  exact formal/actual String source. Constructor field ordinal/name/Type and
  declaration identity must come from the existing nominal mapping owner.
- A genuinely independent source may yield the existing source0 classification
  only from an explicit, current recipe. It grants no deep-drop or owned-element
  authority. A local Array origin, ambiguous return, changed definition/field,
  unsupported effect, missing row or stale generation remains unproved.
- Whole-caller/callee current claims and mutation/transfer obligations must
  prevent reuse after a field or its root changes. Do not rely on a frozen LET
  syntax ID alone. Formal assignment, inout/own use, alias mutation, branches,
  loops, defer and nested result calls participate where reached.
- Materialize only at an explicit existing preparation boundary under the
  existing4096 budget. Current queries cannot create a frame/index/recipe.
- Before implementation, choose how the admitted field/source identity and
  all required claims survive semantic teardown and join actual HIR/MIR uses.
  Reusing the existing opaque snapshot requires a correctly typed value-use
  carrier, not pretending a read is a SHALLOW mutation effect. The actual use
  may be nested in a return expression; do not assume a Void root call.

## Selected bounded implementation

- The new recipe certifies only independence from Array<String> element
  storage. It does not support a borrowed field, create a cleanup grant or
  infer independence from an empty formal-origin matrix. Local Array-derived,
  ambiguous, mutated and recursive-unproved fields remain fail closed.
  The first leaf is an exact typed String literal (including a constructor
  field linked to a producer formal whose actual is that literal), not a
  generic allocating-expression or aggregate zero-origin classification.
- Existing flow claims/control cover the local definition and every reached
  return. The selected return is a current nominal constructor; the original
  field mapping identifies its exact String formal/actual. Rebinding, alias
  creation, field writes and unproved mutating/retaining calls invalidate the
  root/formal proof. Default/ref use is not presumed read-only from its mode.
- One cohesive indexed-String-family producer owns the joined recipe, not a
  new top-level fact family. It consumes existing typed projection, constructor
  mapping and current flow summaries. Preparation is explicit; current replay
  cannot rebuild withdrawn indexes, summaries or recipes.
- At the last semantic boundary, the existing opaque snapshot takes the
  relevant original typed mapping payloads and the union of already-captured
  caller/callee claims. No retained SemanticContext or second AST snapshot.
- Pure source facts are reusable: deleting or reusing a pure field read is not
  an affine-effect violation. Every actual protected sink/argument and field
  identity must still match its issued recipe/caller. Existing private
  mutation effects keep their exact-once join. The same existing HIR expression
  visitor serves actual MIR executable roots; type metadata and RIR summaries
  are not additional executions. Unknown protected roots fail closed.
- Parallel edit scopes on this one chain: A, member/constructor typed mapping
  getters and frozen syntax replay; B, shared HIR/MIR actual-use consumption;
  C, source fixtures and getter negative units. Root owns recipe issuance,
  immutable handoff, integration, all process/build/Git operations and gates.
  These are slices of one repair, not independent executable rungs.

## Read-only investigation scopes and blocking unknowns

- A: existing projection/nominal mapping and definition/version owner; exact
  selected-field recipe and local mutation/alias validity. No source edits yet.
- B: current returned-result/claim dependencies and proof lifetime through
  HIR/MIR; determine the minimum typed carrier without a second authority.
- C: minimal combined positive and necessary negative axes using existing
  cases where possible. Root owns scratch execution/build/process/Git.
- Unknowns that can change the ownership decision: field mapping consumption,
  root-definition currency, all-return recipe coverage, local backing lifetime,
  preparation budget, immutable evidence lifetime, actual nested-use joining.
  Resolve these before assigning independent implementation edit scopes.

## Shared acceptance and falsifiers

- Minimal positive: local struct String field through a named result, two REF
  aggregate actuals, physical result, and an unrelated empty String array.
- Preserve the four original named_string_field invalid cases and current
  mixing/retention/inout/return/worker/defer gates. Add only missing axes:
  same-call later-actual retirement/growth; a local Array-derived field;
  changed constructor field/formal/definition; withdrawn lineage/claims;
  semantic-to-HIR/MIR deletion/duplication/forgery of the admitted actual use.
- Narrow phase and MIR gates first, then unchanged strict/broad/SAN/parity,
  exact D/Linux equality and independent review of an immutable checkpoint.
- Return immediately to full fixed/current canonical admission after this
  fact chain is sound. V1 remains open until unchanged canonical admission,
  actual bootstrap/gen2==gen3 and installed C/LLVM jobs are green on one
  published commit. This plan or additional fact/fixture counts are not progress.

## Frozen candidate observations (2026-10-18 KST)

- Implementation checkpoint is main
  `7d08b9db575a1a06c46b64b77a8cae6c61802a51`; the historical base at the
  top remains7349. A navigation-only child records these receipts.
- Root alone runs builds, WSL, process control and Git. Static gates retain
  their 60-second budget, focused gates 300 seconds and integration shards 30
  minutes. No timeout, safety condition or canonical input was relaxed.
- Native3a8beae6d16828e49392518992c47564c4d633ec43814fb5524d01c1401e98ff
  in `/root/pgy-exact-main-20261017.LlEIOb` passes size/build, phase11994/0,
  MIR462/0 and strict paired admission/refusal plus C/LLVM runtime controls
  (deliverylgUC21). PairGG60Nb verifies81 changed/new source/test paths and
  Makefile byte-equal D/Linux. Broader acceptanceLMTDFX passes old-six,
  strict C/LLVM, MIR462/0, semantic3230/0, transpile1012/0, AIR156/0,
  Map and four IR-flow gates. Separate ASan/UBSan1cDKRi passes phase11994/0
  and MIR462/0 with live-UAF witness; leak detection stays OFF.
- A real issuer mismatch was measured: equal nominal Types can have different
  object pointers. The two cross-owner joins use type equality while keeping
  original declaration/field/argument identities and each issuer's own checks.
  All temporary traces were removed. No user annotations or copies were added.
- Existing snapshot lifetime now includes the moved original mapping payloads
  and unique claims. The shared HIR expression visitor consumes actual MIR
  roots too; write/return role checks prevent replacing an original producer
  by another same-caller nominal call. Pure reads are not exact-once effects.
- Unrelated match/with fixtures first reproduced a global shape refusal, then
  passed strict C/LLVM after exact evaluated-child coverage was added to that
  same visitor. Unknown shapes still refuse; no unknown-as-absence fallback.
- Independent frozen review APPROVED the bounded field recipe/native
  contract. QktA0r reruns the exact frozen binaries and confirms the actual
  slot/value/sink/ordinal, foreign-caller and active-cycle refusal labels.
  VG45XI separately rejects erased-family evidence/full-MIR/emission and
  accepts restoration. Pure-use deletion is not an equivalence claim.
- Full fixed/current inputs were run unchanged on final3a8bea: both
  MIR0/errors18435/warnings16 (KK4sKy), 30.01s/1689884KiB and
  30.66s/1688460KiB. This does not prove the deferred field recipe in the
  whole canonical program, because final success was never reached.
  Bootstrap/fixed point, installed-driver substitution and published-SHA CI
  remain required; this supporting native repair is not substitution progress.

## Next reached blocker: investigation only, not a selected implementation

- Diagnostic-only7QTzYm uses the final native's objects plus an isolated
  diagnostic observer. Production binary/source remain unchanged. The sole
  `prior_error=0` is `SemanticAstEnumFactsFromArtifact` at
  `src/self_hosted/semantic/ast_enum_fact_owner.pgy:80:61`.
- Exact path: `TypedAstArenaAuxValueText(artifact.arena,node_id)` returns
  `Some(arena.atom_table[value_atom])`; the caller unwraps its named Option
  and passes the String to `SemanticNestedCommaRangeFactsFromSource`.
  The scanner reads synchronously and returns a length and two Int arrays.
  That is non-retention evidence, not evidence that backing is still live.
- Existing owners: stdlib variant typed result relation, publication current
  relation, flow backing/input dependencies and indexed-String storage source.
  `Some`/`UnwrapOption` preserve dependency. Concrete indexed-String sources
  currently cover bare Array bindings, not nested aggregate fields, Option
  payload storage or a default aggregate parameter's entry lifetime.
- Missing authoritative premise: the selected field's actual backing root,
  alias/generation and entry loan, transported through the named return and
  unwrap to the last String use, with retirement rejected through other
  actuals/formals/aliases too. A formal-origin bit is not this premise.
  No implementation contract is selected. Do not create a guessed source ID,
  source0 conversion, a name whitelist, extra user annotations or manual copies.
- The active substitution rung is BLOCKED at this exact native admission
  boundary, not closed by another SoT-only commit. Existing owner:
  indexed String/array storage publication; last current consumer: the
  actual String call/retention and immutable HIR/MIR use. Falsifier:
  unchanged full canonical at80:61 plus the scratch Option cases below.
  No new implementation contract is selected until constructor field
  storage responsibility, payload lifetime and the entry-loan producer are
  mapped together. The root's next diagnostic census is never acceptance.
- Read-only scopes: A existing result/parameter-call relation seams, B storage
  lifetime/retirement and last consumers, C scratch representative falsifiers.
  Root combines the complete map before reopening source edits.
- Baselinern5q6P runs five scratch cases on3a8bea analyze-only: direct,
  local-Some and nested candidates refuse at opaque String handoff;
  deepdrop refuses earlier escaped storage, and later-actual retirement also
  refuses at opaque handoff. These are observations, not new passing gates
  or reached-after-admission lifetime falsifiers. No unsafe case executes.

# Canonical MIR Constructor-Fact Carriage — 2026-08-31

Status: `LOCAL IMPLEMENTATION GREEN — EXACT CI PENDING`

Exact base: `d69ee85195eb6f300bcc421fdfa795e359ced9e2` on
`origin/main`.

This directive coordinates one reached semantic-artifact consumer migration.
It does not introduce another constructor-fact owner, change MIR identity, or
claim closure of the broader `selfhost.semantic_artifact_admission` authority.

## Objective card

- Objective: make public canonical-MIR execution consume the nominal
  constructor facts already carried by `DriverRung2VerifiedFacts.analysis`
  instead of rebuilding them from the canonical AST artifact immediately
  before identity-epoch rebinding.
- Priority order: preserve the admitted semantic artifact identity; carry the
  existing constructor rows; fail closed through the existing row-readiness and
  exact field-identity join; delete the reconstruction read; ratchet the old
  path; then preserve executable canonicalization parity and fixpoint behavior.
- Production entrypoint: installed
  `pgy-self-driver --canonicalize-mir-json INPUT`. The CLI read execution owner
  reaches `CanonicalizeMirJsonVerified`, which seals one
  `DriverRung2VerifiedFacts` value before calling the canonical projection.
- Fact owner: `SemanticAstArtifactAnalyzeWithExpressionGraph` produces
  `SemanticAstArtifactAnalysis.constructors`; semantic artifact admission and
  `DriverRung2VerifiedFacts` bind those rows to the exact artifact and body
  analysis epoch.
- Last legitimate consumer:
  `CanonicalMirIdentityEpochRebindProgramFacts` uses the carried constructor
  rows to bind canonical declaration-field identities.
- Direct bypass to delete:
  `CanonicalizeMirArtifactWithAdmittedTopology` calling
  `SemanticAstNominalConstructorFactsFromArtifact(artifact)` after it already
  received verified semantic facts.
- Forbidden fallback: reopening AST constructor rows in the canonical core;
  name-only or numeric-ID joins; an empty/default constructor table on missing
  facts; retrying the old reconstruction when carried facts fail readiness; or
  weakening raw-entry deep verification for external callers.
- Verification gate: strengthen
  `tests/self_hosted/parity/canonical_mir_verified_projection_owner.sh` to
  require the exact carried constructor expression and reject the reconstruction
  call. Preserve its public launcher/installed-driver byte equality and repeated
  canonicalization fixpoint. The existing canonical identity-epoch gate remains
  the non-empty declaration-field and wrong-identity executable falsifier.

## Scope and budget

- Allowed edits: the canonical MIR execution owner, its existing focused gate,
  the SoT registry consumer/negative/witness row, and current coordination and
  handoff documents.
- Out of scope: changing constructor semantics, canonical identity algorithms,
  MIR JSON protocol, parser or AST structure, zone lifecycle, a new cache, a new
  registry authority, or another self-host build lane.
- Run the static focused ratchet first. Use the already installed driver for the
  executable canonical projection and identity-epoch gates; do not trigger a
  redundant full compiler rebuild merely to validate this source-local consumer
  move. Broader CI follows only after the focused slice is green.

## Output classification

Success removes one production whole-artifact reconstruction after semantic
admission and leaves one carried owner path. It does not by itself change 88
authorities / 183 carriers / `CLOSED=55 BRIDGE=32 ACTIVE=1` or the 83% project
forecast.

## Local implementation and evidence

- `CanonicalizeMirArtifactWithAdmittedTopology` now snapshots
  `verified.analysis.constructors` before building the MIR projection and passes
  those admitted rows to the unchanged canonical identity-epoch rebind. It no
  longer calls `SemanticAstNominalConstructorFactsFromArtifact`.
- Both canonical focused gates reject restoration of that AST read. The
  component structural inventory has the same carried-owner requirement, and
  the SoT registry now names the canonical consumer, forbidden reconstruction,
  and exact negative witness while retaining `ACTIVE` status.
- One isolated current-source emission produced a complete 164,145-line,
  10,932,091-byte driver C artifact, SHA-256
  `b0965a89259a513a2ca9c3e419c45631b340210dc75934c55644d9a4cf67c719`.
  The bounded installer command was stopped after 5m30s while its subsequent
  host `gcc -O3` compile was still running; the complete admitted C artifact was
  then compiled once with the same release flags instead of being re-emitted.
- The resulting isolated candidate SHA-256 is
  `e78be3776fd448e61403c696439b36e233fc59d6907b943555d0808f751693b3`.
  Its source smoke produced the canonical
  `f36551de450917d19f93a8ad4cbe48ec827c1954b905f3664706a3dd21796a33`
  artifact, and its machine-manifest projection was hash-equal to the native
  owner input.
- With that candidate, public canonical byte equality plus repeated fixpoint
  and the non-empty hosted-method/apply/link epoch remap with stale/wrong-kind
  field-ID negatives are GREEN. Hard self-host contract and SoT edge are GREEN;
  the latter reports 88 authorities, 183 carriers, `CLOSED=55 BRIDGE=32
  ACTIVE=1`.
- An additional non-acceptance gate,
  `driver_rung2_analysis_admission_owner.sh`, exits before this consumer because
  it still requires a pre-existing direct
  `SemanticAstBodyTypeBundleFromAdmittedAnalysisObserved` call while current
  source delegates through the identity-policy wrapper. The last exact CI was
  green with this existing non-wired drift; it is not repaired or treated as
  evidence for this scoped rung.

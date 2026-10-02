# Collection syntax-event admission correction

Status: focused VERIFIED; aggregate lifetime, installed driver and substitution HELD.
Base: `f16f57c03a5a2c5f4eeea2debc3f92586af94129`.
Directive: `docs/agent_work_directives/collection_event_time_2026-10-02.md`.
Root alone edited, built and integrated; three agents reviewed read-only.
The held formal-root/resolved-type hunks and six unrelated component-counter
hunks are not part of this frozen candidate.

## Ownership claim and change

The call-effect owner produces typed per-local Unknown, all-retirement and
non-deep-drop retirement bounds without changing caller state. Exact direct
moves inherit bounds; Clone results do not. Every reached argument edge applies
Unknown at its current syntax event, including opaque generic callees. Terminal
permission removal occurs only after use checks and preserves Retired tags and
disposition. Frame readiness checks producer status and aligned vector lengths;
it is not a claim that independently supplied scalar bounds were validated.

Statement and call events now fold together. The existing statement index owns
push/set/pop rows; the existing assignment index owns direct indexed writes.
Both queries borrow facts by ref. Exact typed Index bases, not target spelling
or a new AST scan, identify direct local String-array writes. A conditional
effect2 widens an exact live Empty-origin local to scratch Clean{Empty,Owned};
Owned is retained. Shallow mutation rejects Clean; true owned push and deep-drop
consume it. No ownership wire identity or fabricated push receipt is emitted.

Retirement is not base Call graph order. Ordinary mutation can precede known
future deep cleanup, but a deferred/repeated mutation retains the full bound.
Deep-drop excludes only deep-drop events from its additional bound: prior own,
opaque and storage-transfer events still deny entry. Prior actual deep-drop
already carries Retired. Generic required-owned transfer cannot read the stale
Owned state after an opaque event. Old global Unknown-mask and whole-statement
prepass APIs are deleted and structurally ratcheted, without compatibility reads.

## Observed falsifiers and evidence

Frozen source: `.tmp/self_hosted/collection-event-candidate.FOynoE`.
All32 initial scoped index files matched before compilation.

- Gate `collection-inout-effect.iHefHy`: per C/LLVM backend67 source admissions,
  five diagnostic locations, three invalid observer-mode/arity checks,
  sixteen identity/boundary checks and seven actual formal observations.
  Total196 checks. The same source/identity analyzer is built once per backend
  and reused. Supplied programs are analyzed only, never emitted/executed.
- Gate `collection-event-regressions.gwJtlV`: per backend31 constructor identity,
  thirteen field-write and eight reused source controls; total104 checks.
- Native, selected owners, every input and transitive self-host source hashes
  pass. Old API residue, bash syntax and focused lexical caps pass:
  verdict594/600, call-effect146/160, preserving60/80, statement128/140,
  state39/80, identity178/200, retirement46/80. Comments excluded, blanks retained;
  no cap raised.
- The existing source-size checker mechanics pass31 tests in the final frozen
  checkout. All34 final scoped files, including this audit and the held repro,
  match the frozen files before commit.
- Baseline f16 C observer rejects push/copy before a future opaque call but
  accepts direct indexed borrowed write after Copy and own transfer followed
  by child-block drop. The candidate reverses those outcomes. Copy followed
  by borrowed push refuses at the later push, not the earlier valid Copy.
  Normal owned push followed by deferred cleanup still passes; deferred push
  after drop and repeated push/drop refuse.
- Identity11/12 check the exact Main.values row, producer-state invariance,
  before/at-event state, aligned site bounds, independent Retired tags and
  disposition-only stickiness, terminal removal and damaged frame shape.

Failed candidates are preserved, not green evidence. `collection-event-final.rhjJQb`
refused default-mode forwarding of borrowed assignment facts and exclusive
ArrayDrop of shallow-copied returned bound fields. The query API was corrected
to ref; the unproved retirement attempt was removed, not replaced with a private
bypass. `collection-event-verified.jl32zB` stopped on an inline Clone literal
whose initializer type was unresolved; controls now use a named input. A
String ArrayDrop control was omitted because baseline already rejects its plain-
element contract; it was not a valid temporal-permission falsifier.

## Actual consumers and limits

Current C/LLVM diagnostics agree byte-for-byte:

- Actual FromArtifact program first refuses owned_string_drop in
  JsonOwnedFragmentWriteFile, `src/self_hosted/lib/json_emit.pgy`, syntax8026,
  atom ArrayDropOwnedStrings(owned_fragment). The String parameter is wrapped
  in a literal array. Its element transfer/lifetime remains unproved.
- Actual empty/owned/borrowed table programs all first refuse owned_string_drop
  in SemanticAstExpressionFunctionTableFactsRelease, syntax30, atom
  ArrayDropOwnedStrings(names). Empty/owned table cleanup is not green.
- The changed earliest diagnostic does not prove that the later real
  SignatureFactsFromArtifact push was reached and accepted; the earlier Json
  refusal now masks it. Typed atom text is observation, not lifetime identity.
- The retained repro `event_time_retired_metadata_own_2026-10-02.pgy` is wrongly
  accepted by both current analyzers: deep-drop followed by a metadata-only own
  target has no required-element entry check. Storage-live admission after
  retirement is the next named falsifier, not a fixed or ratcheted acceptance.

Three returned bound arrays still have no admitted exclusive field-release
carrier. Existing five inventory arrays, two maps, two caller arrays and
aggregate field/result/retired-writeback obligations remain unproved.
Automatic cleanup and accumulation are unmeasured, not verified leak claims.
No real cleanup was deleted. Alrescha and TypeSafe/Jev are not compiler owners.

Registry remains CLOSED70/BRIDGE23/ACTIVE2, whole-row closure delta0 and C-owned
substitution delta0. No installed binary/manifest changed. Full inventory,
bootstrap/fixed point and installed-driver gates were not run in this slice.
Exact-base CI36943927329 failed self-host-codegen-bootstrap-linux at the
codegen_nominal_array_declaration MIR root control; underlying cause is Unknown,
not proved by this diagnostic shift. Native unreachable warnings remain.

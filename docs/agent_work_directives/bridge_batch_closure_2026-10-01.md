# Installed compiler bridge closure batch

> **OLD** (2026-10-08 표시): 연혁 기록이다. 지금의 작업 대기열이 아니며, 아래 원래 상태 줄과 내용은 바꾸지 않았다. 현재 협업 상태는 [claude_gpt_ownership_collaboration_2026-10-08.md](claude_gpt_ownership_collaboration_2026-10-08.md)와 인계 문서의 활성 카드를 본다.

Status: ABI IMPLEMENTATION IN PROGRESS; no CLOSED promotion yet.
Base HEAD: `62a83a8256bf4fa34878cca8f1f14641736fdeba`.
Baseline: 70 unrelated dirty status entries; `CLOSED=69 BRIDGE=24 ACTIVE=2`.

## Shared objective card

- Goal: close as many genuine BRIDGE seams as one coherent executable
  installed-compiler replacement can support, not relabel bounded feature gaps.
- Priority: stable semantic identity, owner-carried facts, old-path deletion,
  explicit missing-fact refusal, negative ratchet, then batch size/cost.
- Production boundary: the installed source -> verified MIR -> direct C/LLVM
  artifact routes. The registry's live last consumers fix each fact owner.
- Fact owner: the declared source owner for each selected existing registry ID;
  reports and this directive do not acquire semantic authority.
- Last legitimate consumer: the selected route's admitted projection/emission
  consumer; root must name its exact path before implementation authorization.
- Allowed bridge: the bounded existing bridge until its exact consumer is
  migrated. Unsupported features are not evidence that an old read is gone.
- Forbidden fallback: `new ? old`, native retry, backend AST/MIR rediscovery,
  narrowed row identity disguised as closure, or CLOSED without source deletion
  and executable negative evidence.
- Integration owner: root. Phase-one gate is
  `tests/sot_authority_edge_smoke.sh`, which verifies the baseline inventory,
  not compiler behavior. Root fixes one shared executable integration gate and
  independent edit scopes for the selected rung before implementation begins.
- Budget: 60 seconds for static gates, 5 minutes for a focused executable
  falsifier, 30 minutes for an integration shard. No independent driver rebuilds.
- Non-goals: Alrescha integration, dirty-work cleanup, new fact families,
  aggregate-formal ownership promotion without proof, general cache/query work.
- Reject condition: the candidate needs unowned facts, unchanged reachable
  fallback, or a broader unproved semantic contract; keep it BRIDGE and record
  the exact next falsifier instead of counting it as completed.

## Phase-one independent scopes

All three subagents are read-only source reviewers. They may write only their
own descriptive report under `docs/audits/` with `apply_patch`; source, registry,
handoff, binaries, shared build/test artifacts and Git index are forbidden.
They do not spawn further agents. Reports classify observation vs proposal and
name actual owner, last consumer, deletion delta, positive and negative gate.

- Semantic reviewer: semantic domain/receiver/nominal facts, syntax provenance,
  symbol/type graph, resource/loop flow and allocation bridges. Report:
  `docs/audits/bridge_batch_semantic_candidates_2026-10-01.md`.
- Projection reviewer: the six direct-MIR collection/CFG and ArrayString ABI
  bridges, one coherent producer/GraphPlan consumer batch. Report:
  `docs/audits/bridge_batch_projection_candidates_2026-10-01.md`.
- Boundary reviewer: remaining HIR/DIR/RIR/MIR/AIR, generic specialization,
  machine/ABI layout, target capability and diagnostic bridges. Report:
  `docs/audits/bridge_batch_boundary_candidates_2026-10-01.md`.

Root checks the active collection ownership seam and candidate dependency map.
The user's current request explicitly reopens bridge inventory; it does not
mark either ACTIVE row closed. There is still only one implementation/integration
track after candidate selection, with any parallel edits independently bounded.
Commands allowed in this phase: source/config/docs reads, `rg`, read-only Git,
and existing installed-binary inspection. No unsafe rejected artifact execution,
installation, staging, commit, push, or full matrices by a subagent.

## Selected executable rung: complete ArrayString layout consumer migration

The installed verified-MIR -> GraphPlan -> C/LLVM routes already reach
`abi.mir_array_string_layout_projection`. The physical layout owner remains
`DirectMirArrayStorageLayoutContract`; the captured row owner remains
`DirectMirArrayStringCapturedAbiReady`, with the stable registry ID unchanged.
Remaining source paths reconstruct C descriptor layout, use positional empty
initialization, or guess LLVM storage/element alignment after discarding the
admitted target projection. Those are the actual old reads being removed.

Priority: preserve exact captured ABI identity, consume its admitted target
projection at every supported materializer, fail on missing/drift facts, delete
layout/alignment reconstruction, then reject repaired-digest mutations before
publication. The last consumers are scalar-program and scalar-CFG storage,
copy-in/out, mutation, read, return and foreach materializers. Unsupported
lifetime/conditional-move facts remain owned by the existing semantic/projection
rows; closing this layout row will not claim those facts or a new target profile.

One integration gate, owned by root:
`tests/self_hosted/parity/array_string_layout_consumer_closure_owner.sh`.
It must execute each unique producer-issued MIR once per C/LLVM projection,
cover both general program and legacy CFG layout consumers, refuse ABI
missing/offset/size/alignment/cross-family mutations with prior outputs intact,
and ratchet every deleted layout/alignment read. Existing Int/Bool shared
materializer consumers and public ArrayDrop require focused non-regression.

Current fixture names are not route evidence: issued `str_array` and
`str_array_push` use the general program route. One mixed String push/set/index
program followed by Int foreach therefore exercises the legacy materializers
through the actual dispatcher. Reuse its issued MIR for coherent ABI negatives;
do not claim historical indexed/push labels cover that route.

Independent implementation scopes (one common ABI rung, not separate tracks):

- C editor: shared `direct_mir_scalar_cfg_array_c_materialization_owner.pgy`,
  foreach/string-array C storage, scalar-program C ArrayString literal/storage,
  array-Int C emission and collection-plan C storage callers. Carry admitted
  storage projection, layout assertions and named-field initialization. Do not
  edit LLVM, registry, shared tests, or the handoff.
- LLVM editor: owned ArrayString parameter binding, DirWalk adapter, CFG
  ArrayString value/storage/mutation/pop/read materializers, typed foreach LLVM,
  collection-plan LLVM storage, and shared Int/reverse LLVM storage callers.
  Carry element/storage alignment and descriptor projection. Do not edit C,
  registry, root integration callers, shared tests, or the handoff.
- Root: program LLVM/root materialization callers, ownership-safe integration,
  new focused gate/mutations/ratchet, complete consumer re-audit, one frozen
  driver build, installed evidence, registry promotion only after all gates.
- Orphan reviewer: read-only; report genuinely unreachable old functions and
  duplicated materializers, including entrypoint/build/generated/test anchors.
  Root may delete only proven in-scope residues with a negative ratchet.

Subagents use `apply_patch`, source checks and bounded developer probes only.
No shared compiler/driver build, installation, Git mutation or status promotion.
Root preserves the 70 pre-existing dirty entries. Additional nominal/receiver/
machine/IR bridges are not being silently relabelled or implemented in parallel.

## Reached integration dependency card

The frozen private candidate built successfully, but current producer-issued
MIR exposes existing admission mismatches in both that candidate and the old
installed driver. No registry row is promoted on the strength of that build.

- Objective: restore exact admission of the already-owned facts needed to
  execute the migrated ArrayString consumers, without inventing lifetime proof.
- Priority: canonical builtin signature, stable binding identity, shared exact
  Void-return contract, preserved unsupported-fact refusals, then execution.
- Fact owners: `SemanticBuiltinSignatureRows` for `ToInt(String) -> Int`;
  `BuildMirCollectionOwnershipFacts` / `BuildMirRoutineFactIndex` for existing
  borrowed-literal/live rows; the current scalar-program terminal Void predicate
  is extracted into one responsibility-named admission owner.
- Last consumers: collection builtin signature projection, legacy scalar-CFG
  routine claimant/graph admission, and scalar-program routine admission.
  Root also restricts the existing seven-block Option dispatcher to an actual
  carried match claim; block count alone must not rename a CFG rejection.
- Forbidden fallback: delete MIR ownership rows, infer borrowed from an array
  type or field spelling, admit owned/unknown/retired rows as borrowed, loosen
  the strict Option envelope, or accept a return with operands/successors/uses.
- Gate and falsifiers: the same root layout-consumer closure gate, plus borrowed
  identity/origin/state and Void-return operand/topology mutations; the original
  producer JSON must stay immutable and rejected output sentinels must survive.
- Scope: root owns shared Void-return extraction, caller integration and tests;
  the semantic editor may change only the exact legacy borrowed-fact admission
  boundary; the projection editor may align only the `ToInt` expected signature.
  Review/probe work remains read-only outside those explicitly assigned files.
- Native resource/loop/provenance migration, the existing accepted negative
  indexed while initializer, and aggregate-formal deep lifetime are not silently
  repaired or counted by this ABI rung. Keep their exact refusals/limits visible.

One new frozen driver build follows the bounded dependency corrections. A
bootstrap-compiled development probe is only an edit-loop check, not installed
driver, fixed-point, or self-host substitution evidence.

## Reached prefix-pop descriptor composition card

The next private build executes thirteen unique bases through C and LLVM and
passes the public ArrayDrop gate. The legacy `array_pop` C artifact instead
declares `pgy_ai` twice: its admitted prefix-pop mode already uses foreach-owned
storage, while the standalone ArrayInt preamble still emits a second descriptor.

- Objective: one descriptor declaration from the admitted storage owner.
- Priority: preserve the sealed foreach identity and ABI assertions, remove the
  duplicate declaration, then execute both legacy pop backends and reverse.
- Fact owner: `DirectMirScalarCfgArrayIntForEachPrefixPopMode` is issued only by
  `DirectMirScalarCfgArrayIntForEachPopFromOwners`; readiness joins it to the
  existing foreach receipt. That owner already supplies the storage declaration.
- Last consumer: `DirectMirScalarCfgArrayIntCPreamble`; it must honor the same
  explicit mode exclusion as `DirectMirScalarCfgArrayIntCDeclarations`.
- Forbidden fallback: textual preamble deduplication, preprocessor typedef
  guards, guessed element types, or dropping the producer-issued pop fixture.
- Gate and falsifier: the same layout closure gate; generated pop C must carry
  exactly one Int descriptor receipt, execute `30/2/2/a`, and retain the legacy
  String-pop LLVM operation marker. Public ArrayDrop must remain green.

Root reopens only that reached preamble boundary, checks it with a native-built
development probe, and then freezes another private candidate. The successful
ArrayDrop result from the preceding candidate is not proof for the new source.

## Final bounded integration checkpoint

The final Pergyra-built candidate and installed pair passed the same integration
gate: sixteen unique bases through C/LLVM, 102 coherent ABI refusals and 28
borrowed/Void-route refusals, with immutable producer inputs and prior outputs
preserved. Public ArrayDrop passed on both pairs. All thirteen existing registry
enforcement gates passed, including the two documented stale-test corrections.
An independent review checked 2,441 source hashes without mismatch and matched
private/installed artifact identity. The lexical size counter's 31 tests and all
361 scalar-owner caps passed. These are observed, bounded results, not full CI,
fixed-point or whole-compiler evidence.

The stable ABI row is CLOSED with all original consumer, fallback and enforcement
obligations retained: 50 consumers and 33 forbidden paths. Counts are now
`CLOSED=70 BRIDGE=23 ACTIVE=2`. Two definitions-only orphan functions were removed;
no general orphan sweep, new C-path substitution or lifetime closure is claimed.
Root owns the scoped commit/push. Existing unrelated dirty work stays local,
except the explicitly reached lexical-counter integration dependency. Exact
final driver and source identities, failures, corrections and remaining bounds/
aggregate-lifetime/native-size blockers are in the audit linked above.

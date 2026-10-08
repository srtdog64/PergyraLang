# Pergyra Agents workload-to-vision admission

> **OLD** (2026-10-08 표시): 연혁 기록이다. 지금의 작업 대기열이 아니며, 아래 원래 상태 줄과 내용은 바꾸지 않았다. 현재 협업 상태는 [claude_gpt_ownership_collaboration_2026-10-08.md](claude_gpt_ownership_collaboration_2026-10-08.md)와 인계 문서의 활성 카드를 본다.

Status: SOURCE REVIEW / INTAKE. This card records current source observations
and unverified work, not a new implementation rung or a closure certificate.
Compiler base: main `3658548d24bca3d721e4f1974ac7a10da99f7aa8` plus preserved
dirty source graph `34358cba05b0ad2d1548524e9431cb197abd456e62daa3c71a58736238774529`.
Agents source: `F:/pergyraAgents`, HEAD
`fa136620f94b73bd1e330b108546d4efba892ae6`; its compiler pin is
`8c3f074aae035d1b6c069daf3cf00f14e89cba6f`. Those are different evidence epochs.

## Shared objective card

- Objective: turn real authoring friction into source-owned compiler contracts
  and falsifiers while preserving Pergyra's purpose, authority and lifetime axes.
- Priority: semantic/diagnostic identity and ownership/FFI safety; then diagnostic
  quality, API discovery and DX; performance only under the closure-blocking policy.
- Fact owners: existing SoT registry families and their production source owners,
  not VISION, this card, Agents' pain-point notes or old compiler executables.
- Last consumer: the applicable installed source/MIR route and C/LLVM runtime or
  the user-facing API/diagnostic projection, named individually below.
- Forbidden fallback: native retry, prefix/name-based type merging, private shim
  semantics, guessed purity/ownership, weakened fixtures, or documentation-as-proof.
- Integration owner/edit scopes: root alone integrates compiler/docs. Agents and
  GUI repositories are read-only evidence. Do not open parallel implementation
  tracks or overlap the active MIR builder ownership chain.
- Allowed work/budgets: relevant source/fixture inspection and non-mutating
  diagnostics; static 60 seconds, focused 300 seconds, integration shard 1800.
  Ownership-unsafe negatives are analyzed only, never executed.
- Integration gate: finish the active source-bound seed -> actual DRV-2 ->
  installed/default ownership gate first. Then admit a next falsifier on the same
  revision; required CI belongs to the exact published compiler SHA.

## Evidence and classifications

Read evidence: Agents' `docs/language_pain_points.md`, `docs/WORK_STATUS.md`,
`docs/architecture.md`, old `docs/pergyra_idiom_audit.md`, and current
`src/agent/task_intent.pgy`, `src/agent/agent_run.pgy`,
`src/provider/model_backend.pgy`. The old idiom audit uses compiler df097da5
and predates today's HarnessWorld and typed intents; it is not current topology.

`FIXED_DOC` below means source/document inspection only. `UNVERIFIED_CURRENT`
means a past reproduction has not been rerun on the new installed pair.
`DESIGN_LIMIT` is a documented semantic/operational boundary, not a defect
silently accepted as fixed. No entire axis is CLOSED.

| Axis / priority | Current classification and exact boundary | Fact owner -> last consumer | Gate / falsifier and done condition |
|---|---|---|---|
| 1. Meaning/diagnostic parity, P0 | PP-071/072 and temporary/import/constructor differences: UNVERIFIED_CURRENT on the new driver. Old enum/concat repros exist under Agents spike/provider. | Existing declaration/type/call identities and ABI owner families -> source/MIR admission, C/LLVM emitters and diagnostic catalog consumer. | Run the identical safe positive or source-only negative across native/default and applicable C/LLVM. Keep value, code and span oracles, module-distinct same-name controls, and malformed/unsupported admission. Done only after actual installed-path evidence plus exact-SHA CI; parser stack failure must not become a smaller-input pass. |
| 2. Caps/authority/effect, P0 | DESIGN_LIMIT: omitted with-caps does not set an empty bound; default runtime grant is all unless restricted by its host. Current native/Pergyra invocation graph exists, so PP-024's old claim of no propagation is not assumed current. Extern enforcement is UNVERIFIED_CURRENT/OPEN boundary. | Callable capability equations/program seal and builtin capability/effect registries -> inferred manifest, explicit bound checks, runtime granted-mask gate. | Existing capability/intent/callable admission and runtime manifest gates plus extern/callback/unknown-target controls. Distinguish declared facts, static attribution, runtime denial and external OS confinement. No compiler gate proves general FFI or same-user OS sandboxing. |
| 3. Ownership/FFI, P0 | OPEN: semantic.hashmap_collection_ownership remains ACTIVE; PP-067 native field-extraction/alias behavior and full FFI return/handle lifetime are not globally closed by the current source matrix. | Collection binding/lineage/generation/retention and ABI/layout owners -> release finalization, MIR ownership receipt, actual runtime last consumer. | Active 30-positive/66-falsifier C/LLVM source matrix, actual MIR copy/retire and installed ArrayDrop/inout gates. Revisit PP-067 by analysis only; falsify stale field aliases, shallow Clone, retained/returned storage, missing generation and non-owner free. String/handle FFI requires its own explicit allocation/borrow/release contract, not a guessed C shim. |
| 4. API discovery/docs, P1 | FIXED_DOC for the reported absence in docs/108: its current String windows section already lists SubIndexOf and WithLen, relative index meaning, length precondition and route limits. Runtime/signature implementations also exist. This is not proof every overload is accepted by the new driver. | src/common/pgy_builtin_type_table.c and Pergyra builtin signature/runtime ABI projections -> docs/108, examples and tooling API consumers. | Signature/implementation owner checks plus actual portable window examples on applicable installed routes. Reject false new APIs and copied documentation authority. Complete discovery means signatures, examples, failures and ownership/length requirements agree, including unsupported overload diagnostics. JSON/HTTP/process/env/console integrations retain stdlib/runtime/adapter ownership. |
| 5. Diagnostics, P1 | OPEN/UNVERIFIED_CURRENT for stable cross-route span, temporary/match/constructor guidance and unsupported projections. | diagnostic.catalog and source-node/call identities -> public structured diagnostics. | Existing diagnostic registry/language golden gates plus each actual workload repro. Separate misuse, design restriction, unsupported projection and internal failure; missing or foreign identity refuses instead of returning a guessed location/code. Done requires executed code/span oracles, not only message text. |
| 6. Modeling/SRP, P1 | Current source has HarnessWorld and CompleteTask/Delegate/PursueGoal; old no-intent/world audit is superseded. AgentRun's residual SRP stays Agents-owned. No compiler-wide SRP enforcement claim. | Existing intent/authority/effect contracts and pergyra-authoring responsibility guidance -> small canonical examples and actual workload composition. | Review purpose/participant/typed terminal, real resource/authority/effect boundaries, and one decision owner; run reached examples. Keyword counts, class-like flattening or file-size reshuffling do not meet completion. |
| 7. Claims/performance, validation | UNMEASURED comparative language performance and general safety; workload CI is scoped acceptance. No independent optimization rung. | Fixed workload/toolchain/provenance and validation receipt owners -> reproducible public claims. | Equivalent workload/target/toolchain/grants; separate latency, throughput, memory, compile time and tail regressions. Synthetic/loopback and live-provider results stay separate. Optimize only the measured operation blocking the active closure gate and re-run the same input plus semantic negatives. |

## Current implementation link and remaining work

The ownership work is already linked to
`mir_builder_ownership_chain_2026-10-07.md`: a production metadata query now
uses an independently owned type cell with a named last-consumer retirement,
without a Bool/type/name grant. Current source matrix and the production
actual-free probe passed on 46dd4299; its source-bound seed also passed, but
actual DRV-2 refused a combined serialization/analysis-retirement boundary
before installation. A facts-only serializer and explicit last-consumer release
passed supporting gates on bc7e3ca1, but actual DRV-2 then refused a mixed raw
analysis admission/own-transfer boundary before emission. The active owner card
records its separate deep value-input check and canonical result generation.
Fatal diagnostic transport then passed the 32/67 source matrix on 8a873313,
but the whole-driver diagnostic refused the exact body receipt metadata query
at node 219167. Current 34358cba proves physical text/metadata no-retention
behind the existing formal-effect owner; it does not omit receipt validation.
The first C/LLVM metadata matrix passed 33/72 on 4ad3eb4c. Current 33/73,
fresh source-bound seed/DRV-2, installation and required CI remain pending.
This does not discharge PP-067, arbitrary field extraction or FFI.

Next workload verification must identify the actual installed launcher/driver
hashes and source receipt first. Do not reuse the Agents pin or GUI's older
F655/707D pair as evidence for this candidate. Confirmed compiler-owned defects
then get a scoped implementation/negative gate in the existing registry family.
The seven-axis intake is not a request to stop the active executable rung, add
new semantic authority, modify Agents, deploy services or change credentials.

# Body-bundle typed last-consumer retirement

> **OLD** (2026-10-08 표시): 연혁 기록이다. 지금의 작업 대기열이 아니며, 아래 원래 상태 줄과 내용은 바꾸지 않았다. 현재 협업 상태는 [claude_gpt_ownership_collaboration_2026-10-08.md](claude_gpt_ownership_collaboration_2026-10-08.md)와 인계 문서의 활성 카드를 본다.

Date: 2026-09-30 (Asia/Seoul)
Status: COMPLETE in the focused scope — P1 authority transfer remains ACTIVE
Base revision: `7a9fe09d8293170c4ce676de6464486cd25ca348`, dirty tree.
The reached owned-result and routine-build repairs are accepted in their
focused scopes; this is the next boundary exposed by their lifetime gate.

## Objective card

- Objective: retire the20 omitted body-bundle backings at the existing MIR
  ProgramFacts last-consumer boundary, including two concretely typed Bool
  arrays, without destroying shared String payloads or codegen views.
- Priority: exact evidence lifetime and typed ABI; all leaves exactly once;
  no alias/double-free; refusal authority; inspectability; data caps.
- Fact owner: existing SelfMirBodyTypeBundleStorageRetireAfterProgramFacts.
  Nested semantic struct owners declare the72 leaves. Existing canonical
  internal-caller authority is unchanged. CollectionRuntimeCDropStorageFn and
  CollectionRuntimeCArrayBlock own self-host source-C drop symbols/layout.
- Last consumer: driver_rung2_owner MIR ProgramFacts projection before its
  canonical-ID and validation steps. Semantic receipts copy scalar values into
  separate MIR backings. Zone/capability backings are fresh semantic arrays.
  Separate source-C codegen views alias zone backings and do not use this
  retirement call; do not move or add retirement to common admission/codegen.
- Forbidden fallback: Bool-as-Int casts, skipped leaves, deep String drop,
  early/shared-view retirement, new generic cleanup bucket/authority, widened
  caller access, implicit retry, or inferred whole-driver publication.
- Gate/falsifier:72-leaf exact census (Int47/String23/Bool2), once-only calls,
  native concrete Bool shallow drop, actual owner/free observation, emitted
  self-host Bool runtime C compiled/executed for populated/empty/null cases,
  unchanged approved-caller refusals and combined lifetime gate.

## Structure decision

The lifetime function is a declarative inventory with one responsibility and
no branch, search, semantic policy or runtime dispatch. Preserve that cohesion
instead of creating several pass-through files just for a number. Its existing
98-line inventory can gain20 binding and20 retirement rows: cap100 ->140 is
bounded field-inventory growth, not an algorithm complexity allowance. The
routine owner cap180 and reached-proof cap180 do not change. The source-C
runtime owner adds only the concrete Bool symbol and matching layout operation.

## Independent scopes and integration

- Root is the sole compiler editor: existing body lifetime owner, self-host C
  collection runtime owner, its structural data cap, existing lifetime gate,
  Makefile registration and handoff. No registry row or native C edits expected.
- proof_cost_tests: only new typed body-retirement/runtime probe fixtures and
  focused runner under tests/self_hosted, plus unique scratch. Use actual
  owners, not a duplicated copy/drop implementation. Native launcher F06E9EB7;
  no installed binaries, compiler edits, Makefile or shared scratch edits.
- proof_contract_review: read-only semantic backing alias/last-consumer and
  current candidate review; findings under docs/audits only.
- Root owns integration. One acceptance is the focused new probe together with
  the existing combined lifetime gate. Full source/public/fixed-point gates
  remain explicitly unrun if pressure prevents them; P1 is not CLOSED.

Allowed: scoped apply_patch, native MSYS2 UCRT64 diagnostics/build/run, hashes,
unique scratch. Static60s, focused5min; no whole-driver rebuild, commit, push,
installed publication, new stage rung or unrelated concurrent-script repair.
Outputs are implementation candidate and scoped observed evidence, not semantic
authority or whole-world completion.

## Observed acceptance

Root and reviewer observed the existing combined lifetime gate exit0:
97 routine leaves plus72 body leaves, with internal-caller refusal checks.
Actual typed probe exit0 observes the20 added backings freed once, shared
zone String untouched, and real self-host Bool rewrite/runtime storage reset
for populated/null/empty/allocated-empty/repeated-drop inputs. Root replayed
both body executables and the watched Bool runtime independently. The new
runner is registered immediately before the existing combined lifetime gate.
Sources: body9B1A4070, runtime1F9ADE54, runner5A3C625E. Evidence:
`.tmp/self_hosted/body_bundle_typed_retirement.9IvwqA/` and
`.tmp/owned_result_reachable_20260930/lifetime-final.{out,err}`.
The two initial stale gate failures are retained; fixing their return-pair
count and native definition selector does not prove general CFG cleanup.
Pre-early-AST returns, whole-source/public routes and fixed-point remain open.

# Plain Result return after an Array ref observation

> **OLD** (2026-10-08 표시): 연혁 기록이다. 지금의 작업 대기열이 아니며, 아래 원래 상태 줄과 내용은 바꾸지 않았다. 현재 협업 상태는 [claude_gpt_ownership_collaboration_2026-10-08.md](claude_gpt_ownership_collaboration_2026-10-08.md)와 인계 문서의 활성 카드를 본다.

Status: IMPLEMENTED, but not a registry closure. Native C/LLVM execution and
the Pergyra owner component are green. Installed-driver publication remains
OPEN until the pre-existing `c8c7afe4` codegen-bootstrap
`owned_string_drop` failure is repaired and the all-stage gate passes.

- Objective: preserve a caller's exclusive `Array<T>` storage after a `ref`
  observation whose return type cannot carry that storage.
- Priority: one recursive plain-value owner, native/self-host parity, retained
  alias refusal, executable C/LLVM evidence, then patch size.
- Fact owner: `array_storage_plain_element` and
  `SemanticArrayStoragePlainElement`; wrapper spelling alone grants nothing.
- Last consumer: the call-argument escape decision before caller `ArrayDrop`.
- Forbidden fallback: treating every `Option`/`Result` as plain, ignoring an
  array/resource payload, or special-casing the Alrescha function name.
- Verification: `Result<Int, payload-free enum>` after `ref Array<Int>` must
  execute and release on native/self-host C/LLVM; `Result<Array<Int>, ...>`
  must still refuse without replacing a pre-existing output artifact.

Observed on 2026-10-03:

- `PGY_ARRAY_DROP_STAGE=native` passed all 11 positive C/LLVM cases and all
  28 preserved-artifact refusals, including both Result fixtures.
- The Pergyra `SemanticArrayStoragePlainElement` owner executed through native
  C/LLVM and installed self-host C: the plain Result was accepted while an
  Array payload, an Array-bearing error enum, and a one-argument Result were
  rejected.
- Exact baseline `c8c7afe4` and this patch both fail the full self-host seed at
  the same pre-existing `mir_collection_receiver_root.pgy` node 5251
  `owned_string_drop` diagnostic. GitHub Actions run `37028898324` records the
  same baseline failure. Do not promote `semantic.hashmap_collection_ownership`
  or call the installed self-host seam closed until that blocker and the
  all-stage gate are green.

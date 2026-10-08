# Proof checker assurance hardening

> **OLD** (2026-10-08 표시): 연혁 기록이다. 지금의 작업 대기열이 아니며, 아래 원래 상태 줄과 내용은 바꾸지 않았다. 현재 협업 상태는 [claude_gpt_ownership_collaboration_2026-10-08.md](claude_gpt_ownership_collaboration_2026-10-08.md)와 인계 문서의 활성 카드를 본다.

Status: IMPLEMENTATION COMPLETE (local verification); Rocq 9 CI unobserved.
Base: bbc9a713ce0ebbf4ab805037a207e967ee5b0a1a. Changes remain uncommitted.
The user reopened the three Rocq/checker review findings on 2026-10-06.

## Objective card

- Objective: make checker adequacy executable, delete AIRBinding's private
  machine owner, and pin the types of the two approved abstract assumptions.
- Priority: semantic identity, one owner, fail-closed admission, executable
  negative evidence, then patch size. No compiler safety or self-host progress
  claim follows from this work.
- Fact owners: WholeProgramCore for the coordination-expanded machine;
  ProofCarryingIR for its finite certificate predicate; the shared envelope
  admission module for JSON/input admission; AssumptionBudget for approved
  abstract signatures. SlotCalculus still owns its abstract slot API.
- Last consumers: AIRBinding locality and BinaryAdequacy verdict proofs, the
  Stage 1 envelope smoke and Stage 2 differential gate, and the corpus
  kernel-check gate's approval export/binding consumer.
- Forbidden fallback: a private machine guard, keyword presence as adequacy,
  an unchecked certificate fact, name-only assumption approval, or a missing
  prover reported as proof success.
- Falsifiers: required-fact deletion/policy refusal; disagreement between the
  freshly extracted checker and the executable finite predicate; retained
  private machine definitions; same-name abstract-parameter type drift.

## Ownership and integration

Root performs this slice without parallel edit tracks. Edit scope is the
named proofs, their admission/test owners, build/CI registration and scoped
documentation. Do not edit native/self-host ownership code, mark SoT rows
CLOSED, or alter the active inout cleanup rung. Preserve the pre-existing
untracked bridge audit and gmon.out. Do not commit/push unrelated checkpoints
or report an old remote CI run as verification of these changes.

Allowed validation: local static checks (60 seconds), focused executable
checks (300 seconds), corpus/formal integration (1800 seconds). Use the
already configured WSL Coq 8.18/OCaml toolchain; Rocq 9.0.1 CI remains its own
version-specific evidence. Root owns integration. The integration gates are
the adequacy smoke, corpus kernel check and its negative self-test.

Outputs were implementation candidates until the named checks below ran.
The finite checker does not prove payload generation, native AIR/MIR checking,
cryptography, or backend correctness. Abstract verify_token remains an API,
not a proved implementation.

## Observed verification

- WSL Coq 8.18.0 / OCaml 4.14.1: all 57 corpus proofs compiled and
  kernel-checked, plus the approval export/binding consumer; exactly the two
  approved assumptions, no unsafe kernel features. The isolated proof copy was
  byte-compared with every current corpus source before the final run.
- The real kernel gate accepts clean/approved controls and refuses the existing
  planted Admitted, three same-name API type/domain changes and empty/missing
  approval modules, with the expected failure identities.
- Fresh kernel-checked extraction (zero assumptions): every 262144 finite
  valuation and four invalid lengths agrees with the executable finite core;
  41 envelope projection/typed-policy/owner controls pass.
- Current-source native C producer, built in isolated BUILD_DIR/BIN_DIR with
  LLVM_ENABLED=0: Stage 1 live AIR/MIR envelope and the existing input-binding
  negatives pass. Native executable SHA-256:
  f2c676a25d124be77a66f70c3268b4975f29ac39d7824fa6bcc4f36cb36ebb77.
  Build passed with 12 compiler warnings; no warning-free claim. Native source
  has no diff from base HEAD. This is not installed self-host/LLVM evidence.
- Formal semantics (57 proofs actually checked), proof spine, language golden
  spine, beta freeze, shell syntax, CI YAML parsing and git diff --check pass.
- Logs: .tmp/proof-checker-assurance/{native-build,pipeline,corpus-kernel,
  kernel-selftest,adequacy}.log. Earlier seed-only and declared-skip runs are not
  the final evidence. No Git/remote writes or GUI handoff occurred in this slice.

The three reviewed assurance seams are locally improved. Production compiler
refinement, cryptography, the active inout/ArrayDrop installed-driver contract,
and Rocq 9.0.1 exact-source CI are not closed by this evidence.

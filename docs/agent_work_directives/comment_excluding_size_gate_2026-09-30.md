# Comment-excluding source-size gate

> **OLD** (2026-10-08 표시): 연혁 기록이다. 지금의 작업 대기열이 아니며, 아래 원래 상태 줄과 내용은 바꾸지 않았다. 현재 협업 상태는 [claude_gpt_ownership_collaboration_2026-10-08.md](claude_gpt_ownership_collaboration_2026-10-08.md)와 인계 문서의 활성 카드를 본다.

Date: 2026-09-30 (Asia/Seoul)
Status: COMPLETE in the focused metric scope — broader repository gates remain OPEN
Base: `7a9fe09d8293170c4ce676de6464486cd25ca348`, dirty tree.

- Objective: source-size ceilings count physical source records minus lexical
  comment-only records, not documentation volume. Keep numeric caps unchanged.
- Priority: preserve code/string payloads; one host counter; fail-closed I/O;
  duplicate tighter ceilings; consistent unterminated final records; speed.
- Owner: source-size counter under scripts; existing manifests/call sites still
  own limits. This is developer tooling, not a compiler semantic fact family.
- Consumers: component batched caps and production/owner size policy first;
  direct focused cap consumers migrate to the same counter. Pergyra tool size
  counters are a separate compatibility boundary, not a global TextScan change.
- Forbidden fallback: regex deleting comment markers from strings, loosest
  duplicate cap, newline-only final-record omission, silent unreadable inputs,
  changed numeric caps to make this task green, or unrelated occurrence counts.
- Metric: inline code+comment counts1; blank records still count1, including
  physically blank records inside block comments. Strings,
  Python docstrings and shell heredoc payloads count. A shebang counts as an
  interpreter directive. Explicit plain-data suffixes (.txt/.tsv/.def/.json/
  .md/.csv) keep physical records; unknown dialects are refused, not guessed.
- Gate: comments beyond600 do not fail600; code601 still fails; code600 plus
  inline comments passes; markers/escaped quotes/multiline literal/heredoc
  payloads stay counted; CRLF/NUL/no-final-newline and duplicate caps retain
  explicit behavior. Record lexing limitations rather than inventing syntax.

Root is the only counter/integration editor. The existing read-only reviewer
may inventory consumers and inspect metric/negative tests; it does not edit
source. After its accepted typed lifetime probe, proof_cost_tests may add one
independent counter test under tests/ only, covering this metric's lexical and
600/601 negative cases. Once that runner is accepted, it may add a separate
tests-only fixture/runner importing the actual SourceSize Pergyra owner to
compare C/header lexical records with the host counter. Root owns that owner,
both tool migrations and integration; the agent must not edit them or old gates.

Shell support refuses case/esac grammar inside command substitution rather
than guessing a ')' boundary. Other quoted nested expansion payloads are
conservatively counted. C phase-2 split comment delimiters are explicitly
refused rather than given a guessed count. Full preprocessing grammar is not
owned by this bounded metric; no ordinary code is stripped to hide a limit.
Use apply_patch and unique scratch; static60s, focused5min. No commit/push,
installation, compiler-stage change or overwrite of concurrent bootstrap work.

## Observed acceptance and remaining scope

Host7232D0B9:30 methods/99 CLI subprocesses PASS. Actual Pergyra ownerD68A596F:
24 positive goldens/11 lexical refusals/2 cap branches PASS, native emission
0 errors/0 warnings. Root replayed600/601 and unsupported-splice refusal.
Component mechanics checker with13 fast lexical tests PASS within60s; the full
99-CLI hook is in the focused production-size Makefile targets, not the static
checker. Actual1880-source self-host size-policy scan exit0. Both current Pergyra
production tools emitted/compiled/executed with matching host findings: four
existing headers above600 and five C owners above699, so repository-wide size
acceptance is not green. Full component bounded rerun exit1 after mechanics
PASS, with no final inventory verdict; no full acceptance is inferred.
The previous whole owner-size-policy mechanics checker exceeded static60s;
its partial result is not promoted into a full pass. Full production target
oracles/C+LLVM/fixed-point/CI were not run. Audit and exact repro artifacts:
`docs/audits/comment_excluding_size_gate_2026-09-30.md`.

Latest moving-tree refresh: concurrent host4FBA796B/test158E5032 edits of Unknown
provenance were preserved, not attributed to this team. Current31 methods/99
CLI subprocesses and native24 positives/11 refusals/2 caps PASS with identical
before/after hashes; current component mechanics14 lexical tests PASS exit0.
Evidence `source_size_count.wwrvba3m` and `source_size_c_owner.vUAQSO`. The older
7232 packet above remains historical, not the current-file acceptance hash.

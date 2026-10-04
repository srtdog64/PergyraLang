# SoT Closure: Optimization Only at a Blocked Execution Step

Status: IMPLEMENTATION COMPLETE (documentation only; no compiler closure claim).
Base revision: `165c66b28721867716631056d464eeb3bdc23652` in `D:/PergyraLang`.
Date: 2026-10-04 KST. The inherited dirty compiler tree is not sealed by this note.

## User instruction and authority

> 최적화는 'SoT 전부 닫힌 뒤 한꺼번에'가 아니라, '다음 폐쇄 단계가 실행 속도에 막힐 때만, 그 막힌 지점을' 하는 것이다.

The standing work-policy owner is [AGENTS.md](../../AGENTS.md), section
`Closure-Blocking Optimization Policy`. This note distributes that policy;
it does not own compiler facts, registry status, or the next executable rung.
It supersedes the earlier blanket instruction to stop all further optimization.

## Shared objective card

- Objective: close the active SoT seam; remove only execution-cost blockers
  that prevent its next closure step from being verified.
- Priority: semantic identity and one owner, consumer migration, fail-closed
  behavior, old-path deletion and negative evidence; bounded cost work only
  when that next step is demonstrably blocked.
- Fact owner: unchanged active compiler owner and SoT registry; work policy
  belongs to `AGENTS.md`, not this coordination note.
- Last legitimate consumer: the main implementation chat and its read-only
  integration reviews, following the active executable rung.
- Forbidden fallback: independent performance tracks, weaker admission,
  reduced correctness inputs, extra resource allowances, dual authority, or
  treating timeout/exit code/elapsed time as a closure verdict.
- Verification gate: targeted documentation diff/whitespace review and
  delivery of this instruction to the main chat and existing review agents.
  This documentation gate is not compiler execution or SoT closure evidence.

## Scopes, commands and integration

- Root edits only `AGENTS.md` and this directive, then shares their paths.
  Compiler sources, fixtures, execution, install and Git delivery stay with
  `pergyraLang 메인`; no overlapping or parallel implementation is opened.
- Root may read source/status/diffs, use `apply_patch` for these documents,
  and run `git diff --check` plus a targeted policy-content check. It does
  not run compiler/probe commands as part of this documentation request.
- Existing validation budgets remain: static owner checks 60 seconds,
  focused parity 5 minutes, integration shard 30 minutes. No budget extension
  follows from this policy.
- Root integrates the documentation. Main owns any resulting compiler change
  and must re-run the next blocked gate on the same semantic input, with the
  exact executable identified and relevant parity/negative checks preserved.
- Outputs here are work instructions only, not measured speedups, semantic
  observations, implementation candidates, or a successor rung.

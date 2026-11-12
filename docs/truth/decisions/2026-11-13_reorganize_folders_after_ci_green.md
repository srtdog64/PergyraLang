# 2026-11-13: reorganize the folder structure right after CI is green

Decided by: the user, choosing "CI 그린 직후 전체 정리" over a staged
cleanup and over waiting for V1.

## Rule

Once required CI is green on one published commit, the next work is one
reorganization of `tests/`, `docs/`, `tests/self_hosted/parity/` and `src/`:
files move out of the directories that hold hundreds of loose entries, and
`editor/` and `editors/` become one. Every reference moves with them in the
same change (`.pgy` imports, Makefile paths, line-cap pins, OWNERS.md, gate
script paths), and the full gate set runs on the result before it lands.

## Context

At `b462c72ce`: 13,068 tracked files in 1,457 directories; 1,506 files carry
`direct_mir` in their path; `tests/self_hosted/parity/` holds 769 loose
scripts, `tests/` 314 and `docs/` 233. Deleting the `direct_mir` routes stays
with the general MIR emitter work (V1 self-host generality); the
reorganization does not move them first.

Replaces: nothing.

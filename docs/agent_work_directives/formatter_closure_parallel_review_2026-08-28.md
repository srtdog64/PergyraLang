# Public formatter closure parallel review — 2026-08-28

Status: `REVIEW COMPLETE`
Base revision: `97d54a649cfaa2d38022d5d4e866292efb013e03` plus the primary
task's dirty public-formatter substitution delta.

This directive coordinates read-only pre-publication review. It owns no
compiler fact, progress number, registry status, or implementation authority.

## Shared objective card

- Objective: falsify the claim that public `pgy fmt` now has one typed Pergyra
  layout owner while C retains only one installed handoff and a safe host
  transaction.
- Priority: semantic/output parity, fail-closed missing facts, exactly-once
  execution, no partial source/stdout publication, old native-path deletion,
  then documentation/count consistency.
- Fact owners: `lexer/scan_owner.pgy` publishes typed facts;
  `fmt/layout_owner.pgy` owns layout; `fmt/session_owner.pgy` owns stable and
  parser-admitted artifacts; `fmt.c` owns the final host transaction.
- Forbidden fallback: C lex/parse/layout, token-debug reparsing, missing-driver
  native retry, unstable/unparsed output success, remove-then-rename, or a
  registry CLOSED claim unsupported by the named gate.
- Integration owner: the primary task. Integration gate is
  `tests/self_hosted/parity/public_fmt_installed_self_host_owner.sh`, followed
  by installed-driver CLI integration and exact-head remote CI.

## Independent review scopes

- Pergyra semantic review: lexer typed-fact integrity, layout parity and
  termination, stability/parser admission ordering. No C/docs review.
- Host transaction review: C launcher/child argv, path identity, cleanup,
  stdout/check/write/atomic publication, error and concurrency boundaries. No
  Pergyra policy/docs review.
- Closure-evidence review: registry/Coq projection, forbidden-fallback receipt,
  OWNERS/component/build inventory, progress/handoff arithmetic and claims.
  No implementation edits.

## Boundaries

- Review only. Do not edit, build, test, stage, commit, or push.
- Do not inspect `docs/compiler_architectures/` or `pgy-80135c2c/`.
- Use only bounded `git diff`, `rg`, and source/document reads; five-minute
  budget per review.
- Report concrete defects with path/line and a falsifying case. Absence of a
  finding is not a publication receipt.

## Review result and integrated repair

- Semantic review found lossy doc-comment and interpolated-string lexemes,
  silent invalid-character or unterminated-comment deletion, incomplete token
  stream admission, and output publication before typed parser-artifact
  admission. Token facts now carry exact source lexemes and one complete EOF
  stream; invalid or unterminated input dies; formatter session admission uses
  `AstTreeArtifactReady` before `WriteFile`.
- Host review found fixed suffix temporary-file deletion, child-time stale
  source overwrite, raw symlink replacement, permission drift, and ambiguous
  argv admission. The host now uses a private workspace, one canonical source
  identity, pre-commit source comparison, exact argv admission, and the atomic
  path owner. Child diagnostic stdout is captured and translated to stderr so
  a rejected format request cannot leak a partial public payload.
- Closure review found that remove-then-rename and centralized read-boundary
  invariants were not fully ratcheted. Focused and component gates now reject
  `remove(dst_path)` in the atomic path owner and require bounded-size,
  short-read, and embedded-NUL checks in the centralized reader.
- Final semantic/host re-review found valid `use`/`lifecycle` input rejected by
  general parser admission, token kinds not sealed to exact lexemes, Windows
  final-target proof falling back to a lexical path, and rollback-failure
  cleanup after exchange. Format-only admission is now isolated, forged token
  kinds fail, installed source identity uses the strict existing-file owner,
  and a deterministic negative proves displaced concurrent bytes remain in a
  reported recovery workspace.
- The repaired installed driver and public launcher were rebuilt. Focused
  public formatter, legacy formatter smoke, public token, installed-driver
  integration, build-source inventory, component structural contract, hard
  contract, SoT edge, authority negative mutations, documentation/progress,
  shell syntax, and changed-C warning-as-error checks pass locally. Local Coq
  execution is a declared skip because neither Rocq nor Coq is installed; the
  static owner/consumer and negative mutation checks did run and pass.

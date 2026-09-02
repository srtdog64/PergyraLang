# Native Malformed-Enum Parser Progress

Status: ACTIVE — LOCALLY GREEN, PUBLICATION/EXACT CI PENDING

Exact base revision: `9090a5fa7c9c9b63aa2e774b25aaff5aa8873724`

This directive coordinates one bounded native parser-progress repair. It is
not a semantic owner, SoT registry row, self-host substitution increment, or
general parser-recovery campaign.

## Shared objective card

- Objective: make explicit native AST loading terminate with a structured
  parse rejection for every known malformed-enum deletion minimum while
  retaining valid enum parsing.
- Priority order: preserve native parser progress; preserve the existing
  diagnostic contract; reject before partial AST publication; ratchet all
  known minima; keep the patch local to the enum grammar owner.
- Fact owner: `src/parser/parser_enum.c` owns the required enum name, opening
  brace, variant-name admission, and body-loop progress. `parser_consume`
  retains its repository-wide report-without-consume contract.
- Production entrypoint: `pgy SOURCE --native-pipeline --ast`, reached through
  native module loading. Native RIR/AIR/HIR/CFG/DOM/SSA are downstream
  inheritors, not repair owners.
- Direct bypass to delete: the enum parser calls `parser_consume` at required
  header/body positions and then enters or repeats its loop even when the
  current token was not consumed.
- Last legitimate consumer: the enum declaration parser must either return a
  valid declaration or return control with `parser->has_error`; the program
  parser then owns synchronization.
- Forbidden fallback: spelling-specific checks for the five inputs, a driver
  timeout as recovery, global `parser_consume` behavior changes, HIR/backend
  repair, token skipping after successful grammar matches, silent acceptance,
  or broad parser refactoring.
- Focused gate: `tests/native_pipeline_malformed_enum_progress_owner.sh`.
- Falsifying cases: exact no-newline inputs `enum({`, `enum)`, `enum[`,
  `enum]`, and `enum{{` plus a malformed in-body variant token must reject
  without timeout or crash in at most one second. Public token/AST/MIR neighbor
  modes must terminate, and `enum Ready { A, B }` must succeed natively.

## Coordination bounds

- Independent edit scope: `src/parser/parser_enum.c`, one focused gate, its
  Make/CI wiring, and current coordination/handoff snapshots.
- Forbidden overlap: no other task may edit the enum parser-progress rung.
  Self-host parser/semantic owners, HIR, module-loader policy, diagnostics,
  backend code, and protected unrelated untracked paths are read-only.
- Allowed budgets: focused source/static checks within 60 seconds, focused
  runtime gate within 5 minutes, and one normal push CI after local closure.
- Integration owner and gate: the primary task owns integration; the focused
  gate above is the single executable falsifier. Existing parser tests are the
  regression boundary for accepted enum syntax.
- Outputs remain implementation candidates until both the focused gate and
  parser regression gate are observed green. No status/count changes are
  inferred from source edits or documentation.

## Local implementation evidence

- The enum parser now checks the required identifier and opening brace before
  body entry. It checks every variant name before the loop can repeat; a
  mismatch destroys the partial declaration and returns control to the
  existing program-parser synchronization owner.
- `parser_consume`, the module loader, HIR/backends, diagnostics, and valid enum
  grammar are unchanged. There is no input-spelling table or driver timeout
  recovery path.
- The focused gate first failed on `enum({` by exceeding one second. With the
  repair, all five exact deletion minima and the in-body `]` control reject
  through native AST within the one-second hard budget. Public token/AST/MIR
  neighbors terminate, and the valid enum native AST control succeeds.
- `make test-parser`, the Make-wired focused target, Bash syntax, build-source
  inventory, self-host preparation, and CI-profile static gates are green.
  Exact publication and push CI remain pending.

# Parser Statement Typed-AST Source-Location Admission

Status: COMPLETE — PUBLISHED, EXACT-CI GREEN

Exact base revision: `59cdf9002d5b190a911c1eca2ae26b835e95951e`

This directive coordinates one bounded executable parser-parity repair. It is
not a new parser, a general statement-kind expansion, a debugger redesign, a
SoT status change, or a progress increment.

## Shared objective card

- Objective: admit the parser-produced statement AST rows missing from the
  compact typed-AST registry under stable kinds so their already reserved
  source-location observations commit and the existing parser parity path
  reaches the admitted AST artifact.
- Priority order: preserve all published compact kind identities; classify the
  existing AST row at the canonical text-to-kind boundary; retain fail-closed
  incomplete-observation checks; preserve AST bytes and channel semantics;
  avoid any source or emitted-tree rescan outside the existing owner.
- Fact owner: `ast_node_kind_owner.pgy` owns compact kind identity and
  `TypedAstTextKindOf` in `ast_text_inventory_owner.pgy` owns the canonical
  transitional AST-text classification. `stmt_owner.pgy` already owns the
  `ChannelSend:` syntax row and must not gain a second kind registry.
- Production entrypoint: the installed compiler building and executing
  `src/self_hosted/parser/main.pgy` through parser parity.
- Direct bypass to delete: `TypedAstTextKindOf` returns
  `TypedAstKindUnknownTag()` for seven statement rows already emitted by the
  parser cluster: channel send, parallel, with-slot, transaction, fail, event
  subscribe, and event unsubscribe. `ParserObservedStatementFinish` therefore
  cannot commit the corresponding reserved observations and the program owner
  fails closed before AST publication.
- Last legitimate consumer: `ParserObservedStatementFinish` commits the
  canonical kind; `DebugSourceLocationFactsFromParserBuild` later joins the
  same kind against the admitted typed AST without reparsing text.
- Forbidden fallback: special-casing `ChannelSend:` in the source-location
  observation or debugger owner; guessing from raw source; mapping it to a
  generic existing statement kind; dropping an unknown observation; weakening
  `ParserSourceLocationObservationsReady`; renumbering existing kind IDs; or
  changing parser/channel semantics or AST bytes.
- Focused gate: execute the current parser tool on
  `src/self_hosted/parser/fixture/channel_ops.pgy`, then run
  `tests/self_hosted/parity/parser_parity.sh` across its owned 189-source
  inventory.
- Falsifying case: the unchanged opening parser tool exits 1 for
  `channel_ops.pgy` with `PARSER GRAPH ERROR: source-location observation rows
  are incomplete`. Admitting only channel send makes that fixture pass but
  moves the same failure to `async_demo.pgy` at its `Parallel:` row, proving a
  bounded statement-family inventory gap. The repaired tool must emit all 189
  committed parser AST fixtures byte-for-byte, while deleting or cross-wiring
  any canonical kind admission must restore fail-closed rejection.

## Coordination and validation bounds

- The primary task is the sole code, gate, integration, commit/push, and exact-
  CI owner for this rung.
- Edit only the compact AST kind owner, the canonical AST-text classifier,
  required old-path ratchets, this directive, and current coordination
  snapshots. Do not edit the parser statement cluster, observation owner, debugger
  owner, fixtures, or expected AST output unless new evidence falsifies this
  boundary.
- Protected unrelated untracked paths remain outside inspection, edit, and
  staging: `docs/compiler_architectures/`, `pgy-80135c2c/`, and
  `pgy-91d769ec/`.
- SoT remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9 blockers;
  project forecast remains 83%. This repair restores an existing executable
  parity invariant and does not by itself close a top-level owner family.

## Local implementation evidence

- Local implementation revision is
  `196ed689508e40956b8656537e617009e245007d`. Existing compact identities 1
  through 93 remain unchanged. Seven C-AST
  statement counterparts are appended as IDs 94 through 100 and admitted only
  by `TypedAstTextKindOf`; parser, observation, and debugger owners are
  unchanged.
- Fresh C and LLVM parser backends each emit all 189 committed AST fixtures
  byte-for-byte with live native drift checking enabled. The opening
  `channel_ops.pgy` and next `async_demo.pgy` failures are both closed without
  weakening source-location readiness.
- Fresh installed Pergyra-built DRV-2 SHA-256
  `9C6B01FA11C3AB6276FBD34EBEB42271F0A1B76249669F22FEC1F29E3791A9FB`
  passes the complete installed-driver CLI aggregate: semantic/parser/lexer/
  LLVM receipts, AST/tokens, machine/device manifests, optimization profiles,
  REPL, formatter, MIR-C world/action parity, pressure observation, and
  transaction rejection.
- Preparation, hard-owner, documentation, progress, SoT edge, single-owner,
  protocol, and broad component inventory gates are green. The size gate still
  reports only the pre-existing unrelated
  `src/parser/ast_expr_control_accessors.c` 725/699 violation.
- The installed CLI aggregate's phony prerequisite rebuilt the codegen seed and
  missed the DRV-2 prebuild receipt, causing a second compiler-scale emission
  for unchanged semantic input. This is bounded performance evidence, not part
  of this correctness repair.

## Publication evidence

- Implementation `196ed689508e40956b8656537e617009e245007d` and checkpoint
  `d47d4e5f97783f0066da314931404e88c5483caf` are on `origin/main`.
- Exact-head CI run `33681672307` completed `30/30` success in 36 minutes 46
  seconds. `build-linux` passed in 25 minutes 46 seconds and confirmed the
  broad component inventory plus hard owner ratchet. Full self-host passed in
  36 minutes 26 seconds with `gen2 == gen3 (173546 lines)`, installed a
  Pergyra-built DRV-2, passed the installed AST/CLI aggregates, and completed
  the three-source policy-corpus census.
- Windows, macOS, sanitizer, TSan, Rocq 9, codegen bootstrap, and all 20 backend
  comparison shards are green. The publication falsifier is closed.

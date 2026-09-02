# Semantic MIR/C Receipt Coherence Fuzz Audit

Status: READ-ONLY FINDING — REPRODUCED, NOT REPAIRED

Observed Pergyra revision:
`ff8aa5d09985634d03861fdb0bcb8f6f1df6c386`

Observed `F:\tex_bug` revision:
`fce6df20012fd764c97a4756cb33a5f624ec23de`

Observed executable SHA-256 values:

- `D:\PergyraLang\bin\pgy.exe`:
  `464C55FAF5F2489F9293E678BF42377A640A6321A14C6A67BE157810FC7972F1`
- `D:\PergyraLang\bin\pgy-self-driver.exe`:
  `5B09C6FC287BFFD75877F514564DFC7942ABA53E40B409B6F9A5A7C4D7912F06`

The Pergyra revision and both executable hashes were unchanged from the start
to the end of the admitted campaign. This document is delegated, read-only
fuzz evidence. It is not a semantic owner, source-of-truth registry entry,
active self-host rung, implementation lease, repair priority, progress
increment, or completion claim. No compiler source, owner registry, active
handoff, CI wiring, corpus file, or generated case was changed or retained.

## Non-overlapping objective

The earlier audits exercised malformed parser receipts and native parser
progress. This campaign instead admitted only source files whose unmutated
form already succeeded through both the public and explicit-native AST, MIR,
and C paths. It then introduced parse-preserving semantic mutations and
compared:

- public versus explicit-native semantic diagnostic identities;
- public MIR versus public C diagnostic coherence;
- native MIR versus native C diagnostic coherence;
- crashes, timeouts, internal-error markers, and exact repeated-run
  determinism.

This baseline condition prevents a pre-existing corpus failure from being
attributed to a mutation. LLVM was deliberately excluded from the admitted
campaign: none of the 92 no-import corpus inputs succeeded through all of the
public and native AST, MIR, C, and LLVM paths, so there was no all-mode green
seed from which a mutation effect could be isolated.

## Commands and admission

For each source, the campaign used these command shapes:

```text
D:\PergyraLang\bin\pgy.exe --ast SOURCE --error-format=json
D:\PergyraLang\bin\pgy.exe SOURCE --native-pipeline --ast --error-format=json

D:\PergyraLang\bin\pgy.exe --mir SOURCE --error-format=json
D:\PergyraLang\bin\pgy.exe SOURCE --native-pipeline --mir --error-format=json

D:\PergyraLang\bin\pgy.exe SOURCE --emit-c --error-format=json -o OUTPUT
D:\PergyraLang\bin\pgy.exe SOURCE --native-pipeline --emit-c --error-format=json -o OUTPUT
```

The input owner was
`F:\tex_bug\corpus\pergyra\imported-local`. Inputs with source-level imports
were excluded rather than flattened. Of the 92 remaining files, 25 were green
through all six baseline commands above and 67 were excluded. The campaign
selected only from those 25 admitted seeds.

Each mutated command was executed twice. C repetitions reused the exact same
output path after deleting the preceding artifact, so output-path text could
not masquerade as nondeterminism. The per-command classification timeout was
0.9 seconds.

## Mutation and execution budget

The deterministic RNG seeds were `20260902`, `424242`, and `7331`. Forty
mutations were requested per seed, for 120 requested cases. The operator
distribution was:

- identifier use replaced with a missing name: 27;
- first call argument removed: 31;
- last call argument duplicated: 26;
- integer literal replaced with a string: 21;
- string literal replaced with an integer: 10;
- boolean literal flipped: 5.

Of the 120 mutations, 117 remained accepted by both AST paths. The admitted
campaign made exactly 1,968 compiler executions:

- 552 baseline executions: 92 files x 3 modes x 2 paths;
- 480 mutated AST executions: 120 cases x 2 paths x 2 repetitions;
- 936 mutated MIR/C executions: 117 cases x 2 modes x 2 paths x 2
  repetitions.

Measured campaign wall time was 103.87 seconds. Temporary sources and output
artifacts were held under a unique OS temporary directory and removed when the
campaign ended.

## Minimized promoted finding

The campaign observed three cases with the same public cross-mode signature:
public MIR succeeded while public C failed with an untyped malformed-receipt
message. In each corresponding native execution, MIR and C agreed on a typed
builtin signature error. The behavior class was reduced to this 21-byte
source, with no trailing newline required:

```pergyra
let x=SetSize(0,0)-0;
```

The reduction is a compact class-preserving reproducer, not a proof that no
shorter Pergyra spelling exists. Both AST paths accept it. Its observed
boundary results are:

- public MIR exits 0 and emits one `def` instruction whose expression is
  `(SetSize(0, 0) - 0)`;
- public C exits 1 with
  `pgy: self-host JSON diagnostic receipt is malformed`;
- explicit-native MIR and C both exit 1 with exactly
  `stage=semantic`, `layer=type`,
  `code=PGY_SEM_BUILTIN_ARGS_INVALID`,
  `cause_ir=semantic:builtin:signature_mismatch`, and
  `fix_source=match-builtin-signature`;
- the native diagnostic location is line 1, column 7 and states that
  `SetSize` expected one argument but received two.

The six commands were replayed twice with a one-second limit and twice with a
three-second limit, for 24 independent confirmation executions. All return
codes and normalized outputs repeated exactly. Observed public MIR time was
56.76-70.13ms, public C time was 51.28-58.77ms, and native MIR/C time was
20.71-24.21ms. No timeout participated in the result.

The finding proves a public MIR/C semantic-admission disagreement for this
shape and a public/native diagnostic-relay disagreement. It does not by itself
identify whether the missing check belongs to expression admission, MIR
lowering, or orchestration, and this audit does not authorize a repair or
reorder the active executable rung.

## Campaign distribution

Across the 117 dual-AST-admitted mutations, the campaign observed 128
public/native differential instances spanning 26 signatures. The largest
classes were:

- 42 public MIR malformed receipts versus native success;
- 42 public C malformed receipts versus native success;
- 7 public C malformed receipts versus native typed builtin-arity errors;
- 6 public MIR results involving native typed builtin-arity errors, including
  four public successes and two malformed receipts;
- four public MIR and four public C malformed receipts versus native
  `PGY_SEM_CLASS_CONTRACT_INVALID`;
- smaller differences in diagnostic multiplicity or identity for undefined
  symbols, binary/unary operands, assignability, conditions, unknown types,
  and array element types.

These counts are observable signatures, not independent root-cause counts.
Only the public MIR/C cross-mode class above was minimized and promoted. The
remaining signatures need an independently leased falsifier before they can
be treated as owner defects or work priority.

## Negative evidence and limits

For the admitted campaign:

- 0 crashes;
- 0 hangs or timeouts;
- 0 internal-error marker findings;
- 0 determinism mismatches;
- 0 native MIR/C cross-mode diagnostic mismatches;
- 3 public MIR/C cross-mode mismatches, all in the one promoted signature.

This negative evidence is bounded to the 25 baseline-green F corpus seeds,
six mutation operators, three RNG seeds, MIR/C modes, executable hashes, and
timeouts above. The campaign compares external process behavior and diagnostic
identity; it does not decide which pipeline's complete semantic model is
correct. Successful public/native C artifacts were not required to be
byte-identical. The discarded exploratory run that used per-repetition output
filenames was not counted because those filenames produced false determinism
signals; the admitted run corrected that harness error before any claim was
recorded.

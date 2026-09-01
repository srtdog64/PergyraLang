# Public/Native Parser Receipt Differential Fuzz Audit

Status: READ-ONLY EVIDENCE — REPRODUCED, NOT REPAIRED

Observed Pergyra revision: `f7800aed4b710890aca3df13e2b38cc280badddd`

Observed `F:\tex_bug` revision:
`fce6df20012fd764c97a4756cb33a5f624ec23de`

Observed `D:\PergyraLang\bin\pgy.exe` SHA-256:
`464C55FAF5F2489F9293E678BF42377A640A6321A14C6A67BE157810FC7972F1`

This audit records delegated, read-only differential fuzz evidence. It is not
a semantic owner, source-of-truth registry entry, active self-host rung,
implementation lease, repair priority, progress increment, or completion
claim. Current owners and executable gates remain authoritative. The campaign
did not edit either repository or preserve generated cases outside an OS
temporary directory.

## Objective and comparison boundary

The campaign compared the default public self-host path with the explicit
`--native-pipeline` path. It classified process failure, timeout, diagnostic
identity, and per-path determinism. The previously reported native
module-load/parser progress-hang minima `enum({`, `enum)`, `enum[`, `enum]`,
and `enum{{` were excluded from both seed admission and generated inputs; they
are not counted again here.

The exact command pairs were:

```text
D:\PergyraLang\bin\pgy.exe --ast SOURCE --error-format=json
D:\PergyraLang\bin\pgy.exe SOURCE --native-pipeline --ast --error-format=json

D:\PergyraLang\bin\pgy.exe --mir SOURCE --error-format=json
D:\PergyraLang\bin\pgy.exe SOURCE --native-pipeline --mir --error-format=json

D:\PergyraLang\bin\pgy.exe SOURCE --emit-c --error-format=json -o PUBLIC_C
D:\PergyraLang\bin\pgy.exe SOURCE --native-pipeline --emit-c --error-format=json -o NATIVE_C
```

Every AST command ran twice. MIR and C command pairs ran twice only when both
AST paths accepted the mutated input, so a parser rejection could not be
misreported as a later-stage differential.

## Corpus admission and execution budget

The campaign used the mutation operators in
`F:\tex_bug\tools\run_pergyra_profile.py` over the 100-file imported Pergyra
corpus. A seed was admitted only when its copied, self-contained form passed
both public and explicit-native AST baselines. Admission was:

- 69 admitted seeds;
- 2 enum-bearing seeds excluded to keep the known progress-hang family out of
  this campaign;
- 8 flattened import-context seeds excluded;
- 21 seeds excluded because the current public AST baseline rejected them;
- 0 native-AST baseline rejects, timeouts, crashes, or read failures among the
  remaining candidates.

The deterministic RNG seeds were `20260902`, `424242`, and `7331`, with 50
mutations requested for each seed, for 150 executed cases. The campaign made
832 compiler executions:

- 600 AST executions: 150 cases x 2 paths x 2 repetitions;
- 232 MIR/C executions: 29 dual-AST-admitted cases x 2 later axes x 2 paths x
  2 repetitions.

The classification timeout was 0.9 seconds. Minimized evidence was replayed
with independent 1-second and 3-second bounds. Total measured campaign time
was 127.7 seconds. Outputs and C artifacts were compared for determinism
within each path; C artifact hashes participated in that comparison.

## Promoted differential evidence

The campaign promoted three minimized public/native differentials. These are
observable boundary failures, not proof of three independent implementation
root causes.

### Empty source: first divergence after AST

The minimum is the exact zero-byte source file.

Both public and explicit-native AST requests exit successfully. The first
observed divergence is therefore after AST admission, at the public self-host
JSON receipt/MIR boundary:

- public MIR exits 1 with
  `pgy: self-host JSON diagnostic receipt is malformed`;
- explicit-native MIR exits 0 and reports a zero-routine MIR program;
- public C exits 1 with the same malformed-receipt failure;
- explicit-native C exits 0 and emits a 1,034-byte artifact with SHA-256
  `acc2c4cc83acc07bdd3ea501804562367d5497f54fbd5c7abc4adfad07442fe8`.

The one- and three-second replays produced the same outcomes. This evidence
does not decide whether an empty program should be accepted; it proves that
the two installed paths currently disagree after both accepted the AST input,
and that the public failure is not a typed JSON diagnostic receipt.

### `c C`: malformed public AST receipt

The deletion-minimum source is the exact three ASCII bytes `c C`, with no
trailing newline.

- public AST exits 1 with
  `pgy: self-host JSON diagnostic receipt is malformed`;
- explicit-native AST exits 1 with the structured identity
  `stage=parse`, `layer=syntax`, `code=PGY_PARSE_SYNTAX`,
  `cause_ir=parse:unexpected_token`, `fix_source=check-syntax`;
- the native location is line 1, column 3 and its message says that `;` was
  expected after the expression.

The same identity split reproduced under both the one- and three-second
checks. The first divergence is the public AST diagnostic-receipt relay after
the parser rejects the source, not a process crash or parser hang.

### `=`: generic public AST failure

The deletion-minimum source is the exact one ASCII byte `=`, with no trailing
newline.

- public AST exits 1 with the generic text
  `pgy: self-host driver failed (exit 1) emitting AST diagnostic`;
- explicit-native AST exits 1 with the structured identity
  `stage=parse`, `layer=syntax`, `code=PGY_PARSE_SYNTAX`,
  `cause_ir=parse:unexpected_token`, `fix_source=check-syntax`;
- the native location is line 1, column 1 and its message says the token is
  unexpected in an expression.

The outcome reproduced under both one- and three-second checks. The first
divergence is again the public AST diagnostic-receipt relay, but its observed
public failure mode is generic driver text rather than the malformed-receipt
text seen for `c C`.

## Campaign distribution and negative evidence

The campaign observed 161 path-differential instances across AST, MIR, and C.
The most frequent signatures were:

- 53 public nonzero responses without a usable identity versus native
  structured `PGY_PARSE_SYNTAX`;
- 28 public generic AST-driver failures versus native structured
  `PGY_PARSE_SYNTAX`;
- 24 public nonzero responses without a usable identity versus native text
  `PGY_PARSE_SYNTAX`;
- 16 public MIR failures versus native MIR success for the empty-program
  family;
- 16 public C failures versus native C success for the same family.

Negative evidence for this bounded input set is:

- 0 new crashes;
- 0 new hangs or timeout findings;
- 0 internal-error marker findings;
- 0 determinism mismatches across the repeated executions;
- 0 re-admitted instances of the excluded enum progress-hang minima.

This negative evidence is bounded to the admitted F corpus, mutation
operators, three RNG seeds, modes, executable hash, and timeouts above. It is
not a general parser or compiler correctness proof.

## Observed but not promoted

The following singleton or low-count differences were observed but were not
fully minimized and root-separated, so they are not promoted as findings or
work priority by this audit:

- two AST cases accepted publicly but rejected natively with
  `PGY_PARSE_SYNTAX`;
- two AST cases rejected publicly without an identity but accepted natively;
- one MIR/C case where native reported `PGY_SEM_UNDEFINED_SYMBOL` while the
  public response lacked a usable identity;
- one MIR/C case where native reported `PGY_SEM_MISSING_RETURN` while the
  public path either succeeded at MIR or failed without that identity at C;
- one MIR/C case where public reported `PGY_SEM_TYPE_MISMATCH` while native
  succeeded;
- one MIR/C case where public reported `PGY_SEM_UNDEFINED_SYMBOL` while native
  reported both `PGY_SEM_REDECLARATION` and `PGY_SEM_UNDEFINED_SYMBOL`;
- isolated mixed lexer/parser text-versus-JSON receipt shapes.

These observations require a separately leased falsifier before they can be
treated as owner defects. They must not be used to reorder the active
self-host rung or infer a semantic owner from this document.

## Evidence limits

The public/native comparison adjudicated process behavior and externally
visible diagnostic identities. It did not decide which path's language
semantics are correct, and byte-different successful AST/MIR/C artifacts were
not automatically classified as semantic drift. Flattened import consumers
were excluded rather than repaired in the harness. The temporary harness and
generated cases were removed after the campaign; no regression fixture or
gate was installed.

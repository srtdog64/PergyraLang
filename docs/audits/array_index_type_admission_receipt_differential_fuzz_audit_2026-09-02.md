# Array Index Type Admission/Receipt Differential Fuzz Audit

Status: READ-ONLY FINDING — REPRODUCED, NOT REPAIRED

Observed published Pergyra revision at admitted-campaign start and end:
`6d849b168e616eab63640ac4ca5eefbe97b1e929`

Observed `F:\tex_bug` revision at admitted-campaign start and end:
`fce6df20012fd764c97a4756cb33a5f624ec23de`

Observed executable SHA-256 values at admitted-campaign start and end:

- `D:\PergyraLang\bin\pgy.exe`:
  `464C55FAF5F2489F9293E678BF42377A640A6321A14C6A67BE157810FC7972F1`
- `D:\PergyraLang\bin\pgy-self-driver.exe`:
  `CB38B9AED81B841DB58A976239BC5AECA084680CA3B46BA0C066D9443FF15BA9`

This delegated audit is bounded, read-only fuzz evidence. It is not a compiler
or semantic owner, source-of-truth registry entry, active self-host rung,
repair lease, priority change, progress increment, or completion claim. The
root integration owner was working on the already active vessel method
argument-type rung concurrently; this campaign neither inspected that dirty
work as an instruction nor changed it. No compiler source, owner registry,
current handoff/collaboration state, CI wiring, active directive, corpus input,
generated case, or output artifact was retained. This audit is the only
retained output.

## Non-overlapping boundary

The campaign explicitly excluded the active vessel method argument-type axis
and the already closed or recorded nested `SetSize`, binary-operator,
malformed-enum, and parser axes:

- any source containing a `vessel` declaration was excluded before baseline;
- any source-level import or enum declaration was excluded before baseline;
- any source containing `SetSize` was excluded before baseline;
- mutation operators changed only an array index value from `Int` to `String`,
  or one element of an already multi-element homogeneous array literal;
- no call argument was added, removed, duplicated, or retargeted;
- no arithmetic, comparison, logical, delimiter, brace, comment, truncation,
  enum, import, or token-progress mutation participated;
- a generated case reached MIR/C comparison only after both public and
  explicit-native AST routes accepted it twice.

The exclusion reason counts were 8 import, 12 vessel, 2 enum, and 1 `SetSize`.
Reasons can overlap. Their union left 78 of the 100
`F:\tex_bug\corpus\pergyra\imported-local` candidates eligible for baseline
execution.

## Commands and artifact discipline

The exact command shapes were:

```text
D:\PergyraLang\bin\pgy.exe SOURCE --ast --error-format=json
D:\PergyraLang\bin\pgy.exe SOURCE --native-pipeline --ast --error-format=json

D:\PergyraLang\bin\pgy.exe SOURCE --mir --error-format=json
D:\PergyraLang\bin\pgy.exe SOURCE --native-pipeline --mir --error-format=json

D:\PergyraLang\bin\pgy.exe SOURCE --emit-c -o SAME_OUTPUT --error-format=json
D:\PergyraLang\bin\pgy.exe SOURCE --native-pipeline --emit-c -o SAME_OUTPUT --error-format=json
```

Every mutated command ran twice. C repetitions and public/native paths reused
one exact output pathname after deleting the preceding artifact. Return code,
stdout plus stderr bytes, timeout/crash/internal classification, and any C
artifact SHA-256 participated in repeated-run determinism. The admitted
campaign used a 0.9-second per-process bound and a unique OS temporary
directory, removed at campaign end.

## Admitted campaign

Baseline and mutation accounting was:

- 78 eligible corpus inputs x 3 modes x 2 routes = 468 baseline executions;
- 19 inputs green through all six baseline commands and 59 rejected;
- 7 unique deterministic mutations requested and executed;
- 3 array-index `Int`-to-`String` mutations and 4 homogeneous-array-element
  type mutations;
- all 7 mutations accepted twice by both AST routes: 28 AST executions;
- 7 admitted mutations x 2 modes x 2 routes x 2 repetitions = 56 MIR/C
  executions;
- exactly 552 admitted-campaign executions in 23.70 seconds;
- maximum observed admitted-process latency 77.23 milliseconds.

The campaign found three external signature classes:

1. All 3 wrong-index-type cases had public MIR success, native MIR rejection
   with `PGY_SEM_TYPE_MISMATCH`, public C malformed-receipt rejection, and
   native C rejection with the same typed identity.
2. Three typed heterogeneous array literals had malformed public MIR/C
   receipts while native MIR/C rejected with `PGY_SEM_TYPE_MISMATCH`.
3. One inferred heterogeneous array literal had malformed public MIR/C
   receipts while native MIR/C emitted the element mismatch plus downstream
   collection-inference/indexability diagnostics.

Only the first, stronger public-admission/public-cross-mode signature was
minimized and promoted below. The two array-literal signatures are observable
rows, not independently leased owner defects or repair priorities.

The promoted mutation originated from
`pergyra-0005-examples_array_literal.pgy`, whose input SHA-256 was
`91A636A3FD5464BE1A3555115F389E59B47CDB529FFF503AB65CF62EAA51833B`.
The campaign replaced the third `primes[4]` index with `primes[""]`; the full
mutated source SHA-256 was
`305D7C0E6CFC7AAF1CDCF1CAC827299D7E14F07F0630746F484D1DB7BBFA1D10`.

## Minimized production differential

Class-preserving reduction produced this 12-byte UTF-8 source, including its
final LF:

```pergyra
Log([][""])
```

Its SHA-256 is
`6E847F7068B9B07010BA92F96C5294E9EF4FB1B74A04BDB054DCDFBCD2F79840`.
An exhaustive one-byte deletion pass tried all 12 deletions through all six
command modes. None preserved the exact predicate, so this is a deletion
minimum for the observed signature, not a proof that no independently written
Pergyra program can be shorter.

The exact predicate and observed behavior were:

- public and explicit-native AST both exit 0 with byte-identical output;
- public MIR exits 0 and publishes one statement for `Log([][""])`;
- explicit-native MIR exits 1 with exactly one JSON diagnostic:
  `stage=semantic`, `layer=type`, `code=PGY_SEM_TYPE_MISMATCH`,
  `cause_ir=semantic:array_access:index_non_int`, and
  `fix_source=use-int-index`;
- public C exits 1 with
  `pgy: self-host JSON diagnostic receipt is malformed` and creates no C
  artifact;
- explicit-native C exits 1 with the same single typed diagnostic as native
  MIR and creates no C artifact.

The native message was `Array index must be Int, got 'String'`. The finding is
therefore both a public/native semantic-admission differential and a public
MIR/C cross-mode receipt differential. It is not merely different diagnostic
wording: the public MIR route accepts and publishes MIR for a non-`Int` index,
while the public C route reaches a private rejection that has no admitted
public receipt.

## Confirmation, hashes, and negative evidence

The minimized source was replayed twice with a one-second bound and twice with
a three-second bound. Each replay executed all six modes, for 24 confirmation
executions. All return codes and output bytes repeated exactly. Latencies were
20.40-72.22 milliseconds. Stable output SHA-256 values were:

- public/native AST output:
  `3509953245C67779DAB877D76B88926181C6E6335D81C6CF109CBDF87493B262`;
- public MIR output:
  `765EC1EF234887667A3FC3E146A4F0B5815B306313181A69884235EBC874C31C`;
- native MIR/C diagnostic output:
  `3BBD26484B2C289B339C3570966B4A43B3ACB491B7D4CDEBCBEEBA34A64992FD`;
- public C malformed-receipt output:
  `C5B4A44C92732F564C053939B72FB1F3445C532F282808027B6C0D9F1D2BF709`.

Across the 552 admitted-campaign executions there were:

- 0 crashes;
- 0 hangs or timeouts;
- 0 internal-error markers;
- 0 repeated-run determinism mismatches;
- 0 vessel method argument mutations;
- 0 nested `SetSize`, binary-operator, parser, or enum findings.

The complete delegated effort made 1,498 production compiler executions,
below both the 2,500-execution and ten-minute bounds. Besides the 552 admitted
executions, this includes 552 executions from an otherwise identical first
run whose final report serialization was discarded after the Windows console
could not encode one corpus em dash, plus 394 calibration, hand-reduction,
exact-predicate reduction, and confirmation executions. Only the admitted
campaign and final exact-predicate confirmation support the distribution and
negative-evidence claims above.

## Evidence limit and bounded falsifier

This external evidence does not decide whether the missing owner is index
expression admission, statement-call traversal, public receipt projection, or
the last orchestration consumer. It does not authorize interrupting or
reordering the active vessel rung. A separately leased production falsifier
can test the claim after that rung closes: a valid `Int` array index must still
reach MIR/C, the minimized `String` index must fail before either artifact with
the existing exact native identity, and public/native MIR/C must agree without
native retry, C-side semantic mapping, message parsing, or a second diagnostic
serializer. Current owner documents and executable gates remain authoritative
for whether that becomes the next rung.

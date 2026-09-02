# Vessel Method Argument-Type Admission Differential Fuzz Audit

Status: READ-ONLY FINDING — REPRODUCED, NOT REPAIRED

Observed Pergyra revision at campaign start and end:
`6b720aa7d372696764719ec2d25c6333f4debf92`

Observed `F:\tex_bug` revision at campaign start and end:
`fce6df20012fd764c97a4756cb33a5f624ec23de`

Observed executable SHA-256 values at campaign start and end:

- `D:\PergyraLang\bin\pgy.exe`:
  `464C55FAF5F2489F9293E678BF42377A640A6321A14C6A67BE157810FC7972F1`
- `D:\PergyraLang\bin\pgy-self-driver.exe`:
  `DDB7C126CBC5A483C4E45DE33CF91D08CCFCC8125AAD64D8411FCC8A9C3FF189`

After the admitted campaign and its 24-run confirmation had ended, a
concurrent integration-owner rebuild changed only the observed self-driver
hash to
`CB38B9AED81B841DB58A976239BC5AECA084680CA3B46BA0C066D9443FF15BA9`.
HEAD, the F revision, and `pgy.exe` stayed unchanged. A six-command post-build
recheck reproduced the same six return codes, exact native identity, and public
C artifact hash. That recheck is reported separately below and is not merged
into the admitted campaign distribution.

This delegated audit is read-only fuzz evidence. It is not a semantic owner,
source-of-truth registry entry, active self-host rung, repair authorization,
priority claim, progress increment, or completion claim. The delegated agent
changed no compiler source, SoT registry, active handoff/collaboration state,
or CI wiring. Concurrent integration-owner edits already present in the shared
worktree were neither inspected as work instructions nor modified. Generated
cases and artifacts were removed; this audit is the only retained output.

## Non-overlapping boundary

The campaign excluded the already recorded native malformed-enum/parser
progress family and the closed nested `SetSize` arity-signature family:

- all eight source-import consumers and both enum-bearing corpus files were
  excluded before baseline execution;
- mutation operators did not add, remove, or duplicate call arguments;
- a generated source containing a comma-bearing `SetSize(...)` spelling was
  rejected before execution;
- a mutation was allowed into MIR/C comparison only after both public and
  explicit-native AST paths succeeded twice.

The remaining operators changed integer/string literals, flipped booleans,
replaced an identifier use or initializer with a missing symbol, or changed a
return expression's type. They preserve the parser shape relevant to the
promoted finding. No brace, delimiter, truncation, line, token-duplication, or
comment-operator mutation participated.

Each corpus seed was baseline-admitted only when its original copied source
succeeded through all six public/native AST, MIR, and C commands. This makes
the promoted result a mutation-induced production differential rather than a
pre-existing corpus failure.

## Commands and artifact discipline

The command pairs were:

```text
D:\PergyraLang\bin\pgy.exe --ast SOURCE --error-format=json
D:\PergyraLang\bin\pgy.exe SOURCE --native-pipeline --ast --error-format=json

D:\PergyraLang\bin\pgy.exe --mir SOURCE --error-format=json
D:\PergyraLang\bin\pgy.exe SOURCE --native-pipeline --mir --error-format=json

D:\PergyraLang\bin\pgy.exe SOURCE --emit-c --error-format=json -o SAME_OUTPUT
D:\PergyraLang\bin\pgy.exe SOURCE --native-pipeline --emit-c --error-format=json -o SAME_OUTPUT
```

Every mutated command ran twice. Both C repetitions and both paths reused the
exact same output pathname after deleting the previous artifact. Output bytes,
return code, timeout/crash/internal classification, and C artifact SHA-256 all
participated in the determinism check. The classification timeout was 0.9
seconds. Temporary sources and the shared output artifact lived under a unique
temporary directory and were removed after the campaign.

## Admitted campaign budget

The input owner was
`F:\tex_bug\corpus\pergyra\imported-local`. Admission and execution were:

- 100 corpus candidates;
- 8 import-context and 2 enum-axis files excluded before execution;
- 90 self-contained, non-enum seeds executed through the six baseline modes;
- 22 seeds green through all six baseline modes and 68 baseline rejects;
- deterministic RNG seeds `918273`, `271828`, and `161803`;
- 150 requested mutations, 106 unique executed cases, and 44 duplicates;
- 105 cases admitted by both AST paths and one clean AST rejection;
- 1,804 compiler executions exactly:
  - 540 baseline executions: 90 seeds x 3 modes x 2 paths;
  - 424 mutated AST executions: 106 cases x 2 paths x 2 repetitions;
  - 840 mutated MIR/C executions: 105 cases x 2 modes x 2 paths x
    2 repetitions;
- 77.32 seconds of measured admitted-campaign wall time.

The 106 unique operator counts were: 23 initializer-to-missing-symbol, 26
identifier-use-to-missing-symbol, 24 integer-to-string, 21 string-to-integer,
8 Boolean flips, and 4 wrong-type return expressions.

The campaign observed 34 public/native differential rows across ten external
return-code/diagnostic-code signatures. Most were already familiar untyped
public-receipt versus typed-native families and were not promoted. One row had
the stronger new production signature below: both public MIR and public C
succeeded while both native modes rejected the same source with one exact
typed identity.

## Promoted production differential

The finding originated from baseline-green seed
`pergyra-0014-examples_battle_test.pgy` after one `Int` call argument was
replaced with a `String`. It was reduced to this exact 58-byte source, with no
trailing newline:

```pergyra
vessel V{func F(self,x:V)->Void{return;}}let v=V();v.F(0);
```

The exact class-preserving predicate was: public/native AST both succeed;
public MIR and C both succeed; native MIR and C both fail with only
`PGY_SEM_TYPE_MISMATCH`. Delta reduction and an exhaustive one-character
deletion pass required 438 compiler executions. No single-character deletion
from the 58-byte source preserves that predicate. This is a deletion minimum
for the exact observed signature, not a proof that no independently written
shorter Pergyra program can express the defect.

The result localizes to vessel method argument-type admission after parsing:

- public and explicit-native AST both exit 0 and emit byte-identical output;
- public MIR exits 0 and publishes a call statement for `v.F(0)`;
- public C exits 0 and emits a 2,575-byte C artifact with SHA-256
  `98717DBC2C7600BE96B8D19D5525B0AAE8E5E64E2E53E677AA47600DFB9BED63`;
- explicit-native MIR and C both exit 1 with exactly
  `stage=semantic`, `layer=type`, `code=PGY_SEM_TYPE_MISMATCH`,
  `cause_ir=semantic:call:arg_type_mismatch`, and
  `fix_source=align-arg-type`;
- the native message is exactly
  `Argument 1 for 'F' expects 'V', got 'Int'`.

The public route therefore admits and materializes a vessel method call whose
argument violates the native-owned method signature. This is not the closed
arity class: the call supplies exactly one argument, and the disagreement is
the argument's `Int` versus expected vessel type `V`.

## Confirmation and negative evidence

The minimized source was confirmed with independent one- and three-second
bounds. Each of the six mode/path commands ran twice at each bound, for 24
confirmation executions. All return codes, output bytes, diagnostic identities,
and C artifact bytes repeated exactly. Representative stable SHA-256 values
were:

- public/native AST output:
  `F8133C9A60590B33EE06BC4B3C8DA8E8040B7252097F020DC28E238B74705D57`;
- public MIR output:
  `7AB5D7DAC81C96992519029197AEC12B5B046340C628A0E072081A27EAB8945A`;
- native MIR/C diagnostic output:
  `9023D168E6A4BD22AD1EE8EED8044209E9BAACBAD7D139F5AF54B1239DDDA4A6`.

Confirmation latencies ranged from 22.19 to 77.71 milliseconds and produced
no timeout. Across the admitted 1,804-execution campaign there were:

- 0 crashes;
- 0 hangs or timeouts;
- 0 internal-error marker findings;
- 0 repeated-run determinism mismatches;
- 0 parser/enum progress findings;
- 0 nested `SetSize` arity findings.

Beyond the final admitted campaign, harness calibration/replay, reduction, and
confirmation made 4,938 additional compiler executions: 552 from a rejected
output-path calibration, 1,924 from a UTF-8 serialization-lost deterministic
run, 1,924 from a superseded run that was discarded to exclude enum seeds and
comment-operator mutations explicitly, 70 interactive reduction probes, 438
delta-reducer/deletion probes, 24 confirmation executions, and 6 post-rebuild
recheck executions. Together with the admitted campaign, the delegated effort
made 6,742 compiler executions and remained below the ten-minute wall-time
budget.
Only the 1,804 fully adjudicated executions support the distribution and
negative-evidence counts above.

## Evidence limit and bounded falsifier

This evidence does not determine the repair owner or authorize changing the
active executable rung. It proves only the installed public/native behavior at
the recorded revisions and hashes. A separately leased repair can falsify the
ownership claim with a focused vessel-method argument gate: the valid
same-type call must still reach MIR/C, the one-argument wrong-type call above
must fail before either artifact with the existing `call_arg_type_mismatch`
identity, and public/native MIR/C must agree without C mapping, native retry,
or message parsing. Current owner documents and executable gates, not this
audit, decide whether and when that rung is opened.

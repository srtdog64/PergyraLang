# Oracle MIR repeated-copy repair and process boundary

Recorded 2026-11-06 KST at shared HEAD
`60fd1e26d8ea3987efbcbfc65f59b892b234701a` plus preserved dirty work.
This is a frozen run record, not a V.001 or published-CI closure claim.

## Exact measured boundary

Original run: `/root/pgy-v001-overlay.ml3fkD/.tmp/v001-driver-carrier47/`.
Frozen reproduction: `/root/pgy-oracle-rss-20261106/`.
Reviewable manifests/patch/results:
`D:\PergyraLang\.tmp\oracle-rss-20261106\delivery\`.

The original native-produced oracle is
`f2296a5a5e1d0c29ac0251c763f29eb28f1eaf9ab0f48157ddc522874a9d71db`.
The frozen native source compiler is
`0ba9ac10dfb2b37ce61587bdd819478579b4107d161f71f711b7d8aaa4150aa3`.
It generated the candidate from the entire changed driver source with
`--native-pipeline --emit-c`; GCC used the original `-O3 -fwrapv
-fno-strict-aliasing` profile and unchanged frozen runtime headers.

Candidate C:
`84da6e303270d189177fe14e279fb2a4db9eab9c70e1e54738b9924e11c34f32`.
Candidate executable:
`3d1f9244be44e8e648b103fea24c28f4f14a1defa23ccb88b1f5881b994eace0`.
It executed against the **original frozen full input**, not the modified
producer source or a smaller program:

```
--emit-mir-json-verified src/self_hosted/compiler/driver_bootstrap_main.pgy
--pressure-owned-full-fixpoint -o .tmp/oracle-rss-final-guarded/driver.mir.json
```

| Full-input observation | Original | Candidate |
|---|---:|---:|
| Maximum RSS, GNU time KiB | 21,250,796 | 11,274,212 |
| Maximum RSS, GiB | 20.266 | 10.752 |
| Wall time, seconds | 225.98 | 224.44 |
| MIR bytes | 389,800,975 | 389,800,975 |
| Exit | 0 | 0 |

RSS fell 46.95%. Both MIR files have SHA-256
`5e59c2e1f521f2a21b7bca2e7aa2e9b19f1fed49d5475258949bdd75d174ca8c`.
The final candidate completed inside a **14 GiB RLIMIT_AS** boundary. This is
a virtual-address-space limit; actual RSS is measured separately above.
The guard's group was drained, and its source hash stayed
`7a0eaf514ff4f2d41d0450f73120668cb92a05090e659259dea83e9198a2d8de`.

## Whole-chain cause and repair

The full measured chain is native copy planning -> native-produced oracle ->
semantic statement receipts -> routine/CFG/local-ref/ABI row accumulation ->
whole program facts -> verified MIR JSON publication.

The original instrumented run independently reproduced 21,250,760 KiB and
the same full MIR. Fixed-size allocator counters had no overflow or failed
allocation. The later stack sampler had no dropped entries. Its byte counts
are **sampled allocation traffic estimates**, not retained-RSS measurements.
The heap was predominantly live; allocator trimming was not the remedy.

Two repeated operations dominated array-copy traffic: seven complete receipt
columns passed through each statement decision (~3.49 GB sampled traffic),
and reconstruction of all preceding String rows during CFG/graph/routine
updates (~2.05 GB sampled clone traffic for the two largest CFG operations).
The census also covered local-ref, local-scope, termination and runtime-value
ABI paths. Immutable artifact/graph snapshots and final receipt publication
have independent lifetimes and remain separate from these per-row copies.

The repair keeps each existing row owner, mutates its exact field and returns
that same owner. The scan keeps one receipt table; its transition result keeps
the existing diagnostic fields and no longer transports that table.
No ownership refusal/admission rule was relaxed. A direct-mutation trial
without independent incoming Strings correctly failed 23 native borrow
checks and was rejected. The final stores copy each retained String once.

Actual emitted array-clone calls in the changed repeated operations fell to
zero: CFG instruction 9->0, primary graph 16->0, runtime-value ABI wrapper
76->0, routine instruction 3->0, local add 6->0, local-count boundary 3->0,
termination 3->0 and the two reached local-ref operations 1->0 each. The
semantic scan changed 15->8: seven repeated columns were removed; its final
independent publication copies remain. These artifact observations support,
but do not replace, the runtime measurements.

## Behavioral evidence

- The same imported-owner row fixture at 256/512 instructions requested
  21,456,351/81,283,823 allocation bytes before the patch and
  313,355/629,275 after it. Doubling growth changed 3.788->2.008. Row IDs,
  names, use offsets/counts, graph roots and block counts matched. The old
  emitted program was actually rejected by the same new allocation gate.
- The changed semantic receipt path passed the existing registered 80-case
  source admission/refusal matrix on both C and LLVM, with byte-equal outputs.
  That run used the v2 snapshot; the three semantic files are byte-identical
  in final v3. Later changes were confined to MIR String retention. This is
  not a rerun of the separate constructor-query budget gate.
- The borrowed-row fixture frees its source String before reading retained
  local/instruction fields. C and LLVM AddressSanitizer executions passed.
  An allocator observer confirmed one actual input-String free on each
  backend; suppressing that free was rejected with exit 89 on both. Leak
  detection was disabled for this bounded UAF/physical-source-release test;
  it is not evidence that every compiler allocation is automatically freed.
- The standalone process boundary passed kernel-limit, allocation failure,
  launch-time cancellation, timeout with a grandchild, root exit with a
  child, SIGTERM, controller SIGKILL/parent-death, unaffected-sibling,
  receipt-collision and malformed-boundary checks. The existing profile
  gate passed with and without isolation, and rejected missing/undrained
  boundary evidence and invalid mode configuration.
- The earlier guard version also ran the original oracle on the original
  full input: explicit `class=oom reason=allocation failed`, exit 134,
  no published MIR, drained group, and the sibling native rebuild survived.
  Its exact guard hash is retained in `baseline-bulkhead-result.json`.

## Budgets, source publication and limits

Native source-to-C admission completed in 1,688.02 s, 0 errors/23 warnings.
The first combined 30-minute shard stopped during host compilation after
108.36 s of that stage. It was not reported green or given a larger timeout.
The completed C artifact was pinned; a separate artifact stage compiled it
in 175.76 s and ran the guarded full input. No source reanalysis or generated
C edit was used to resume an ambiguous intermediate state.

Eight shared Pergyra files were updated only after full byte equality passed.
Every dirty preimage was checked and backed up before any write; line endings
were preserved. `shared-published-source.json` binds shared bytes and their
tested LF forms. No staging, commit or push occurred.

The patch wires isolated full-MIR seed/oracle phases into Linux bootstrap CI,
with 14 GiB per producer, a 30-minute phase ceiling, owned process-group drain,
parent-death handling and no core dump. The profile consumer validates and
hash-binds both boundary receipts; CI retains their artifacts. There is no
uncapped retry. Windows keeps its existing pressure owner.

The new-source gen2/gen3 fixed point, installed-driver matrix and required CI
on a single published commit remain main's integration obligations. The
constructor-query budget is a separate still-active main lane. This run does
not close broader ownership precision, Raw/View or all SoT rows.

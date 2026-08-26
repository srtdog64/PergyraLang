# Self-Host Status (verified snapshot)

Branch main. This is a dated evidence snapshot, not a live CI assertion for
the current commit. It records what was verified by building pgy and running
`make self-host-preparation-test-smoke` on 2026-06-21. The lexer/parser scale
figures below were refreshed on 2026-06-23 with
`make self-host-lexer-parity-test-smoke self-host-parser-parity-test-smoke`
and `tests/self_hosted/parity/lexer_scale_probe.sh --failing` /
`tests/self_hosted/parity/parser_scale_probe.sh --failing`. The gate runs the
self-hosted tools on C and, when the current `pgy` build includes the LLVM
backend, LLVM. `LLVM_ENABLED=0` jobs still prove the C leg and require an
explicit LLVM-leg skip instead of treating the build configuration as a tool
failure. The C compiler remains the oracle.

For work after this snapshot, follow `docs/152_validation_isolation_policy.md`:
rerun only the owner gate for the touched self-host rung unless a broader
compiler-world owner changed or broad parity is explicitly requested.

Focused evidence on 2026-08-26 closes installed DeviceSlot source-C machine
declaration carriage at checkpoints `10055d0b` and `464a907a`. Public source-C
now derives the installed sibling companion path and carries its admitted
declaration through the existing typed request and `PgyCompilerWorld` action.
The C adapter owns no physical facts and has no native retry.

The reached DeviceSlot program emits the machine binding block and startup call,
compiles, and runs exact `0` equal to the explicit-native oracle. Non-machine
source and MIR programs emit neither block nor call because both now consume
the same `usage.uses_machine_layer` fact. Missing or corrupt companions publish
no artifact. A current typed-source DRV-2 install and the installed-driver
parent gate are locally green. Replacement run `32949495441` passed 29/29,
including full self-host fixed point and every platform/proof/backend axis.
This is bounded `SUBSTITUTING`; the top-level registry remains 49/36/1.

The subsequent all-nonclosed-row audit covers the registry exactly once:
37 expected, 37 observed, no missing, extra, or duplicate owner. Current
fixed-point evidence invalidated its first routine-1197 successor before any
implementation: the canonical 236,684,385-byte MIR emits byte-equal gen2/gen3
C and exact-revision remote full self-host is green. The corrected single
successor is native intent-observability ABI-ID consumption; the audit itself
changes no status or percentage.

Focused evidence on 2026-08-05 closes
`string_array_index_return.pgy` at checkpoint `52715894`. The 5,048,145-byte
current-source Pergyra-built driver
(`B698C2C4C86C6BACB96C3D7F3E6FABB030F8B2629DEA06800C574BF89822CD2A`)
consumes one 6,234-byte self-produced MIR
(`EE124A64CBFF373C365992E7EAC63084C8358A152F094AC13A2595C45BCF0DE6`)
through GraphPlan v23. It emits 1,819-byte C
(`FFD6E40F6100B917BA7E65B30E9EEF981238CD388C53E92BEC3675E2BD4B00DD`)
and 4,660-byte LLVM
(`B1B03043830710D151AC18D9FAD7C39B372DFC6D9F22CD801EC3E4826CB2A0F9`).
Both compile and execute exact `one`.

One target-neutral boundary fact seals the literal cardinality, canonical
`Array<String>` ABI, by-value formal parameter, bounded callee index,
caller-frame storage, borrowed static elements, and borrowed result. C and LLVM
materialize different target storage from that fact and suppress deep cleanup
only for the sealed borrowed local; owned Split arrays keep their existing
cleanup. The canonical typed literal parser is shared with the legacy indexed
String-array route, so no second graph or literal decoder was added.

The focused gate proves display and routine-order equality, semantic value and
index changes, exact execution, and no-artifact rejection for malformed
parameter, ABI, local, call, return, index, topology, and literal-spine facts.
Nine adjacent scalar/string/array regressions are green on the final driver;
owner caps remain green without raising an existing cap. The final build took
128.8 seconds. Full CI, fixed point, proofs, sanitizers, pressure sampling, and
release promotion did not run. This is bounded direct-C/LLVM `SUBSTITUTING`,
not arbitrary aggregate calls or whole-compiler replacement.

The next observed falsifier is `string_utils_core.pgy`: its producer emits
7,229-byte MIR
(`46DC2EC9AF786D4D072608B32F6C29F919B99994CFA9749E1319794EFBFBD6D9`),
while both targets fail closed without artifact at `direct MIR scalar local
type inventory is missing or invalid`. The native C oracle executes exact
`hello world pergyra` and `3.500000`. The next rung must admit the reached Float
local and carry canonical Join/ToFloat facts through this same GraphPlan;
source-text type recovery, a fixture route, a second graph/emitter, count
routing, backend reconstruction, and native retry remain forbidden.

Focused evidence on 2026-08-05 closes `str_trim.pgy` at checkpoint
`d88cab37`. The 5,030,274-byte current-source Pergyra-built driver
(`62FD572FAD63D9917A96E2154DF1AAF3AA277E4F3B878FAB769B4AF5DC6287BE`)
consumes one 11,463-byte self-produced MIR
(`1A10A12B315C2B48E715441966738724C0E1D8E5A120766DC87987E494D52BE8`)
through GraphPlan v23. It emits 1,577-byte C
(`6CD19BFA68AF738019BD720F6ACF6C33B8B6CCFDF92DC1E9E9EDB6F2DF96BA5F`)
and 6,244-byte LLVM
(`84196925990CDA66A47F5F981C8BE60975A82D5FB6EE151409BC3E2084C49430`).
Both compile and execute exact `hello world`, `11`, `0`, and `[x]`.

The same GraphPlan now carries canonical StringTrim signature, expression,
runtime behavior, and target materialization facts. The runtime contract fixes
space/tab/LF/CR boundaries, null-to-empty behavior, and owned-String result
identity. The first combined LLVM artifact rejected duplicate `memcpy`
declarations; one plan-derived foreign-declaration owner now emits each runtime
declaration once while the semantic runtime owners retain their bodies.

The focused gate proves display-only equality, semantic change,
already-trimmed and empty inputs, exact C/LLVM execution, six no-artifact
malformed families, and one declaration each for `strlen`, `malloc`, `memcpy`,
and `abort`. The final build took 135.9 seconds; the focused gate took 16.5
seconds and six adjacent String runtime regressions took 88 seconds. Existing
central caps stayed fixed and all new owners are capped. Full CI, fixed point,
proofs, sanitizers, pressure sampling, and release promotion did not run. This
is bounded direct-C/LLVM `SUBSTITUTING`, not arbitrary String transforms or
whole-compiler replacement.

The next observed falsifier is `string_array_index_return.pgy`: its producer
emits 6,234-byte MIR
(`EE124A64CBFF373C365992E7EAC63084C8358A152F094AC13A2595C45BCF0DE6`),
while both targets reject the terminal multi-routine graph without an artifact.
The native C oracle executes `one`. The next rung must reuse the canonical
Array<String> ABI for typed parameter carriage and indexed String return in the
same GraphPlan; source-name/routine-order inference, flattened arrays, raw
pointer carriage, a second emitter, count routing, and native retry remain
forbidden.

Focused evidence on 2026-08-05 closes `str_indexof.pgy` at checkpoint
`fb0561f6`. The 5,019,513-byte current-source Pergyra-built driver
(`699D7D7847AE07B1F6E6BB5AF22CACACAF23185EE4CD363939BB744342AC598E`)
consumes one 14,215-byte self-produced MIR
(`28F0C0C026E62F749AEF2150B5100444962B6260D242F4560E9A2262954F1C75`)
through GraphPlan v22. It emits 1,898-byte C
(`DC0BB8CDFAEA1CE29E62AE2C2ED294FAA1EAE767BDE8708BDF0D9AA653C0430A`)
and 5,632-byte LLVM
(`68E1A169E649979F92BCFC7749824BEF0B75885D0ABB3337EE9774064F5D3E7F`).
Both compile and execute exact `5`, `-1`, `hello`, and `world`.

The same typed GraphPlan now owns the canonical StringIndexOf signature,
expression kind, runtime ABI, and `-1`-or-byte-offset result range. The reached
`p + 1` and length-subtraction forms are admitted only from a same-source,
same-block StringIndexOf definition. Both targets materialize the search from
the sealed fact; no compile-time search result, text reconstruction, or backend
MIR read exists. This rung also corrected self-host Substring invalid-window
behavior to match the native empty-string contract.

The focused gate proves display-only equality, semantic mutation, absent and
empty-needle behavior, exact dual-target execution, and seven no-artifact
malformed families. The final composition build took 167.8 seconds and the
focused gate took 17.7 seconds; adjacent String and routine regressions are
green. New condition-bound, StringIndexOf range, and LLVM substring owners have
tight caps; no existing cap was raised. Full CI, fixed point, proofs,
sanitizers, pressure sampling, and release promotion did not run. This is
bounded direct-C/LLVM `SUBSTITUTING`, not arbitrary String search or whole-
compiler replacement.

The next observed falsifier is `str_trim.pgy`: its producer emits 11,463-byte
MIR (`1A10A12B315C2B48E715441966738724C0E1D8E5A120766DC87987E494D52BE8`),
while both targets reject expression row 1 for `StringTrim(raw)` without an
artifact. The next rung must join the canonical StringTrim contract to this
same GraphPlan and materialize it in both targets. Fixture routing,
compile-time trimming, a second emitter, count routing, backend MIR reads, and
native retry remain forbidden.

Focused evidence on 2026-08-05 closes `str_case_math.pgy` at checkpoint
`1b620f9b`. The 5,006,609-byte current-source Pergyra-built driver
(`FD3C0343318992F13A60FCBE8B4C7628AC3486466A236F317E4F2AFBC2B1FB42`)
consumes one 24,283-byte self-produced MIR
(`D0E8EDFAF1B91AED04D5ED99BBDDCD3BB7B250DB673810DD5CCB224E29CDA7AF`)
through GraphPlan v21. It emits 2,944-byte C
(`09EE9010DC668710BE8F6615BBF7418715269B0CF1A27577B26327B77C94DDB7`)
and 10,087-byte LLVM
(`723A4AE4154280513F4779771E01B2276D71779250251FE0D714308341A3BD9C`).
Both compile and execute exact `HELLO, WORLD!`, `hello, world!`,
`Hello, Pergyra!`, `a+b+a+b`, `42`, `3`, `7`, `50`, and `7`.

The same typed GraphPlan now owns ordered scalar parameter arrays, persisted
direct-call argument order, and registry-derived StringReplace/Abs/Min/Max
runtime identities. C and LLVM render different target syntax from one sealed
fact and materialize actual runtime bodies; no compile-time result evaluator or
backend MIR reader exists. Bounded constant-DAG magnitude evidence admits the
one reached addition without weakening the unbounded signed-add rejection.

The focused gate proves display-only and routine-order artifact equality, an
exact semantic-output mutation, and eight no-artifact parameter/call/type/
registry/magnitude negatives per target. The final composition build took
171.9 seconds and the final focused plus partition gates took 17.9 seconds.
Five adjacent scalar-program regressions also passed. No existing LoC cap was
raised; plan verification was split from the 87/85 seal owner, which is now 73
lines. Full CI, fixed point, proofs, sanitizers, pressure sampling, and release
promotion did not run. This is bounded direct-C/LLVM `SUBSTITUTING`, not
arbitrary call graphs or whole-compiler replacement.

The next observed falsifier is `str_indexof.pgy`: its 14,215-byte MIR
(`28F0C0C026E62F749AEF2150B5100444962B6260D242F4560E9A2262954F1C75`)
fails both targets without artifact at typed expression row 1 for
`StringIndexOf(s, ",")`. The native oracle executes `5`, `-1`, `hello`, and
`world`. The next rung must join the canonical StringIndexOf runtime fact and
its `-1`-or-byte-index contract to the same GraphPlan and reached arithmetic;
fixture routing, expression-text evaluation, unproved signed arithmetic, a
second emitter, or native retry remain forbidden.

Focused evidence on 2026-08-05 closes `str_builtins2.pgy` at checkpoint
`97db96d1`. The 4,972,723-byte current-source Pergyra-built driver
(`9A48625E5694780CC70E79B6CA35E70A9A58B4DC759CB59FE96DD427674D91EF`)
consumes one 20,672-byte self-produced MIR
(`CFE3D90203905D4F98BD5F3D72338DA95324388DB2F6B7CAAA88302E00B079D3`)
through GraphPlan v20. It emits 3,322-byte C
(`2B75104A03F8BA746C6FE129C3AC06E40EE0ABDF3DDA12EA23878530ED18D1C1`)
and 9,823-byte LLVM
(`C62334A0403D78EBE05E35B378B9DBCFEF19F53586205853795B666548B4D9EC`).
Both compile and execute exact `yes`, `3`, `bb`, `43`, `1`, `2`, and `left`.

The normalized expression arena and sealed runtime facts now own
StringContains, Split/StringSplit, String-to-Int, and Array<String> length/get.
The required four-field Array<String> layout is captured once from admitted
MIR, checked across every definition, and consumed by both targets. A named
program GraphInput entry prevents the legacy String-array literal/mutation
plan from claiming Split results; the general CFG collection path is
unchanged. The expression-kind owner supplies the last valid kind, shared kind
queries are no longer duplicated, and generated ABI readiness has an explicit
dependency owner.

The focused gate proves display-only artifact equality, semantic mutations
with exact changed `no`, `3`, `bbbb`, `8`, `1`, `2`, `left`, and seven
no-artifact type/topology/identity/layout negatives per target. The final build
took 144.0 seconds. Collection, window, nested-builtin, and legacy String-array
gates are green on the final driver. No existing LoC cap was raised. Full CI,
fixed point, proofs, sanitizers, pressure sampling, and release promotion did
not run. Documentation quality is green after the registry update; the SoT
authority-edge gate reaches the pre-existing duplicate-Coq-authority red. This
is bounded direct-C/LLVM `SUBSTITUTING`, not general collection or whole-
compiler replacement.

The next observed falsifier is `str_case_math.pgy`: its 24,283-byte MIR
(`D0E8EDFAF1B91AED04D5ED99BBDDCD3BB7B250DB673810DD5CCB224E29CDA7AF`)
is rejected by both targets without artifact at
`direct MIR terminal multi-routine graph is unsupported`. It requires ordered
three-parameter callable facts plus registry-owned StringReplace/Abs/Min/Max
inside the same GraphPlan. Fixture routing, expression-text evaluation, a
second graph/emitter, count routing, backend MIR reads, or native retry remain
forbidden.

Focused evidence on 2026-08-04 closes `str_builtins.pgy` at checkpoint
`585776f0`. The 4,942,644-byte current-source Pergyra-built driver
(`40FAA8C611A2F8219CC1F22BFAC25EB0085049DCA8C270EE782CDE2AD7667619`)
consumes one 11,879-byte self-produced MIR
(`0378770C6AF86E963E8C73B700B4F043250DDA397AE5D3B7E9290220520220C4`)
through GraphPlan v19. It emits 1,798-byte C
(`F5475B5CAA4865B63037805394C7D2048431201D00E6EF66412C930E5E3ABEC9`)
and 4,937-byte LLVM
(`253076BF5DCDD75308303D9DDAF231995F63AFF6CD25C7C3AE9FC693BFEDAC4C`).
Both compile and execute exact `7`, `perg`, and `perg-yra`.

Persisted argument-chain identity supplies n-ary topology while the semantic
builtin registry owns builtin name, result, arity, and argument types. The
normalized arena maps StringLength, Substring, SubstringWithLen, and Concat to
typed definition expressions. One sealed runtime-ABI subfact owns the concrete
requirements; C and LLVM materialize the same required symbols without backend
MIR reads or expression-text recovery.

The focused gate proves display-only artifact equality, a semantic String
mutation with exact changed `11`, `perg`, and `perg-yralang`, and five
no-artifact signature/edge/type/registry negatives per target. The final driver
build took 128.3 seconds. Window-builtin, nested-builtin, String concat/equality,
and Bool gates were green on the final driver in 45.9 seconds. No pressure
result is claimed. All existing LoC caps stayed fixed; responsibility-specific
route, n-ary topology, readiness, runtime ABI, and target projection owners were
added. Exact C runtime bodies are no longer duplicated.

The component contract still stops at the independent pre-existing
`ast_expression_graph_fact_owner.pgy` 616/600 cap. The SoT registry gate retains
its pre-existing duplicate Coq fact-authority failure. Full CI, fixed point,
prover, sanitizer, and released/default promotion did not run.

This is bounded direct-C/LLVM `SUBSTITUTING`, not arbitrary String-program or
whole-compiler replacement. The next executable falsifier is not inferred from
fixture order; it must be selected by probing current producer output after
this closure. Expression text, source-local spelling inference, a fixture
route, a second graph/emitter, or backend-local builtin identity remains
forbidden. The bounded layering inventory and its shrink-only priorities are in
`18_self_host_layering_duplication_audit.md`.

Focused evidence on 2026-07-27 advances the integrated driver beyond this
dated full-suite snapshot. The Pergyra-built gen2 driver emitted verified MIR
for the current complete driver source, byte-identical to separate C-oracle
evidence. The Pergyra seed/gen2 consume only that Pergyra artifact and emit
byte-identical gen2/gen3 C. This is an explicit producer/fixed point, not a
claim that the full preparation matrix or released/default replacement passed.

Focused evidence on 2026-08-01 promotes one released/default target. Public
`pgy source.pgy --emit-c -o output.c` now selects the sibling Pergyra-built
driver without an opt-in flag; missing and unsupported boundaries fail before
native semantic/codegen. The current graph produces a 90,429,326-byte MIR and
byte-identical 5,595,167-byte gen2/gen3 C. This promotes pure-C artifact emit
only. Plain compile/link, run, package, and LLVM remain C-owned/open, and the
dated full preparation matrix was not rerun.

After the AST-text compatibility bridge was confined to the public entry owner,
the current MIR SHA-256 is
`A151D69CD7B3BD8F81C5587C6E9FB4B75503CD3411D9D3CD1004DED794F9CA9B`.
Gen2/gen3 C are byte-identical with SHA-256
`275A66AC3203CDC3EE194952ED0CFA03A2E72A1D6E92A6F66F97EDBF0A33440F`.
The bounded two-process source-to-MIR/MIR-to-C path completed below 3 GiB, but
the in-process compiler-source-to-C convenience path crossed the hard stop at
3.187 GiB. That path is not product-ready compiler-scale evidence and must not
replace the bounded artifact composition.

The same dated focused lane now includes a direct backend-neutral consumer.
One Pergyra-produced hello MIR and one `let_log` MIR each drive both C and LLVM
without MIR-to-AST/semantic reconstruction. The `let_log` artifact is 2,341
bytes with SHA-256
`0ad63b8802e964f238807aabf3f2c73e59a1f795dc7fa73e078a59aff998ecee`;
both target artifacts compile and match native output `42`. This proves only
the bounded literal/local/add/direct-call shapes, not general backend
admission or released/default replacement.

The current-source direct lane now also admits `multilet.pgy` as one typed
scalar block. Its single 4,135-byte MIR artifact has SHA-256
`31fb7b7300674c1483a5c54370d90a66c1ab1d4cddc3998d2eafbc03931f4efd`;
the unchanged identity produces `35` then `12` through both compiling C and
LLVM projections. Admission consumes multiple typed local/result/use facts,
exact graph sequence, and add/multiply topology. Projection consumes the typed
formatted-print runtime ABI fact, line format, and C `Int` ABI owner. Machine
admission carries the document index forward, so the direct consumer does not
reindex the MIR or reopen raw `expr0`/AST/semantic facts. The focused
hello/let_log/multilet gate is green. The final r3 Pergyra-built bounded
bootstrap and the same direct C/LLVM positive/negative gate passed with the
final source.

Focused v74 evidence adds the distinct loop-break rung after the v73 phi-free
range rung. The one 7,054-byte `break_after_stmt.pgy` MIR has SHA-256
`cb2d4f9fad6411ae9ce54e2d072d038735c29d2499a960909a09fae8eb59efbf`.
One v6 certificate/plan binds six typed block roles, the actual empty
continuation predecessor, the forwarded header-phi definition, exact break
row, two Log uses, and distinct normal/break exit values. C and LLVM compile
and match native `3`, `3`; LLVM's backend-only exit phi is not claimed as a
second MIR fact. Late-break and zero-trip variants execute correctly, phi
storage permutation is byte-identical, and topology/SSA/graph/break/plan
mutations reject pre-artifact. The native-current r5 driver passed the full
scalar/CFG predecessor chain through this rung. A fresh Pergyra-built bounded
seed also matched the native oracle for sample C, MIR production, and bounded
MIR consumption, then passed the complete scalar/CFG chain through v74.
Released/default replacement remains 0%.

Focused evidence on 2026-08-02 advances the installed direct-MIR targets to a
passive vessel generic member. Checkpoint `ceb43938` preserves one distinct
`vessel/mutable-identity` host fact through the existing inferred-member plan,
then selects pointer receiver ABI for C and LLVM. The same 6,527-byte self MIR
executes exact `42` in both installed public paths; the existing class/value
path remains exact `41`. Host/carriage/ABI/SSA/no-retry negatives, hard and
component contracts are green. This is target-specific `SUBSTITUTING` evidence
for that bounded slice, not whole-compiler replacement. The next observed
falsifier is `nominal_tobject.pgy`, whose 2,857-byte self MIR is produced but
rejected by both direct targets before artifact creation.

Later focused evidence on 2026-08-02 closes that tobject falsifier at checkpoint
`f5eedd97`. One sealed route fact claims the nominal literal frontier before
scalar routing, and separate declaration, graph, program, instruction,
physical-ABI-absence, plan and target owners feed exclusive C/LLVM emitters.
The same 2,857-byte self MIR constructs and reads a real value and executes
exact `12` in both installed public paths. Four variants, 28 C negatives and
nine LLVM sentinels pin kind/nominal mismatch, non-object method tail,
instruction tail, ABI/graph/use drift, single-shot routing and no scalar retry.
The existing class/vessel exact `41`/`42` gate, final hard contract and installed
C/LLVM paths remain green. This is bounded target-specific `SUBSTITUTING`, not
whole-compiler replacement. The full component contract passed earlier in the
session but was not rerun after the final route/cardinality correction. The
next observed falsifier is the 2,791-byte `nominal_subject.pgy` MIR, rejected at
passive program identity before artifact creation; it requires a distinct
stable mutable-identity representation and exact output `7`.

The next focused checkpoint `e52e07da` closes that subject falsifier. The
former passive-only declaration/graph/program/instruction/ABI/admission owners
are promoted into one shared nominal-literal admission seam. The semantic
receiver-carriage owner then selects an aggregate-value plan or a stable
mutable-identity plan without retry. One 2,791-byte self MIR drives one C
storage plus `T *const` write/read and one LLVM `alloca`/GEP/store/load chain;
both installed paths execute exact `7`. Coherent rename, exact `73`, value-host
representation separation, 34 C negatives, 14 LLVM sentinels, final hard and
component contracts, and installed public C/LLVM compile/run are green. This
is bounded target-specific `SUBSTITUTING`, not whole-subject/action or whole-
compiler replacement. The next observed falsifier is the 2,785-byte
`nominal_vessel.pgy` MIR, rejected by both targets at subject identity
admission; it requires exact `13` while preserving vessel semantic identity.

Checkpoint `89951491` closes that vessel falsifier without a vessel-specific
family. Subject and vessel now share one target-neutral mutable nominal identity
plan while retaining exact declaration/type/field identity. C uses one storage
plus one stable pointer and LLVM uses one alloca/GEP/store/load field chain;
installed C/LLVM execute exact `7` and `13`. Six positive pairs, 33 C negatives,
14 LLVM sentinels, passive tobject regression and final hard/component contracts
are green. The bounded kind allow-list lives only in the semantic carriage
policy, and shared routing cannot recreate subject/vessel spelling dispatch.
This remains a bounded target-specific `SUBSTITUTING` slice, not whole compiler
or compiler-root intent replacement. The next observed falsifier is the
1,563-byte `ability_decl.pgy` MIR
(`679EA54CDA224A2832603B48A8FD7A747B5944328AEECBAEF31C01401A0D81EF`),
rejected by both targets before artifact creation at the current declaration-
free string-hello admission. It requires explicit compile-time declaration
erasure and general integer literal execution, not silent declaration loss.

Focused evidence on 2026-08-03 closes the bounded continue-plus-fallthrough
range frontier at checkpoint `aefebe13`. One 7,796-byte producer MIR carries
header `sum.3 = phi(sum.1, sum.3, sum.9)` over its preheader, continue, and
fallthrough predecessors; the same immutable MIR drives C and LLVM to exact
`42`. A general predecessor snapshot owner captures local versions at control
transfer, and the header binding owner verifies the actual backedge before
publishing the phi. The compiler does not rescan CFG roots or reconstruct a
continue value. Missing, stale, and retargeted continue inputs reject before
artifact publication; focused, cumulative CFG/range, public LLVM, and full
component removed-path gates are green. This is bounded target-specific
`SUBSTITUTING`, not arbitrary foreach, nested/multiple loop, scoped iteration-
binding, or whole-compiler replacement. The next producer falsifier is an outer
local shadowed by a same-name range binding and then read after loop exit. Full
CI, current fixed point, pressure and prover suites were not rerun.

Focused evidence at checkpoint `6122051f` closes the bounded local-literal
`Array<Int>` foreach frontier. One 6,761-byte producer MIR carries the
collection ValueId, exact ABI layout, typed iteration and loop-flow rows,
literal expression graph, binder identity, scalar phi, and four-block CFG.
One target-neutral collection receipt joins those owners before the existing
scalar-CFG plan; C and LLVM compile and execute exact `6`. Reordered phi inputs
are byte-equal, and a graph-only `[4,5]` mutation executes exact `9` while the
display expression remains unchanged. Twelve negative families reject without
artifact or legacy range retry. Current-source build, focused, existing
scalar/range, public LLVM, cumulative CFG/AIR, and structural component bodies
are green without raising a hard LoC cap. This is `SUBSTITUTING` only for the
covered identifier-backed local literal collection. At that checkpoint,
returned-array composition and nested/sequential collection foreach were
still open and `for_each_call.pgy` was the next exact falsifier.

Focused evidence at checkpoint `7069f852` closes that returned-array
composition rung. The 17,155-byte `for_each_call.pgy` MIR identifies one pure
`MakeValues() -> Array<Int>` producer and three synthetic call-result ValueIds
feeding nested and sequential loops. One target-neutral producer receipt and
the existing foreach receipt now drive 1,821-byte C and 6,083-byte LLVM
artifacts; both execute exact `30`. Routine permutation is byte-equal, a
producer graph-only `[4,5]` mutation executes exact `36`, and seven ABI,
call-identity, hoist, result-name, and LocalRef mutations fail before artifact
publication. `storage_identity` causes the pure returned collection to be
materialized once while preserving one cursor per loop. This is bounded
`SUBSTITUTING`, not a claim for effectful producers, identity-observable
mutable returns, arbitrary element ABI, mixed string operations, or the whole
compiler. The next falsifier is the 14,425-byte `for_each.pgy` mixed
`Array<Int>`/`Array<String>` graph, currently rejected by both targets at the
legacy Option-match claimant before artifact creation.

Focused evidence at checkpoint `9e33ec37` closes reallocating `Array<Int>`
return/parameter carriage. One 18,849-byte `array_param.pgy` MIR binds
`Build:r.1 -> Main:xs.1 -> SumAll:param0` through routine-qualified return and
argument edges, one canonical ABI, and one storage identity that may reallocate
before a single Main cleanup. The same sealed `CollectionProgramPlan` drives C
and LLVM to exact `12,4`; `Build(5)` drives exact `20,5`. Routine permutation,
display-only changes and a cross-routine raw-ValueId collision remain valid,
while repaired parameter ABI, wrong target, stale return use and cross-routine
endpoint mutations publish no artifact. The old three-routine topology rule is
now limited to its exact one-block/no-loop ArrayArgument slice, and claimed
collection-program failures cannot retry it. This is bounded target-specific
`SUBSTITUTING`, not general alias, ownership, arbitrary call-graph or whole-
compiler replacement. The next observed falsifier is the 17,188-byte
`bool_logic.pgy` MIR, incorrectly claimed by the returned-Array foreach route
and rejected before both target artifacts.

## Verified

Front-end self-hosts on both backends in LLVM-enabled builds.

- Lexer (src/self_hosted/lexer/): compiles on C and LLVM. `main.pgy` is only
  the entrypoint; character/codepoint handling, token classification/output
  formatting, and scan-loop state are owned by separate modules. Token output
  is byte-identical to `pgy --tokens` across the 8 committed source fixtures,
  and the live drift guard confirmed those fixtures matched the then-current
  oracle. The broader lexer scale probe now measures 993 of 993 examples +
  backend_compare sources byte-equal.
- Parser (src/self_hosted/parser/): compiles on C and LLVM and compares
  byte-identical against `pgy --ast` on 188 committed source/fixture rows. It parses
  the domain grammar, not just generic constructs: it dispatches on zone, world,
  party, role, and intent keywords, plus bind, if, within-zone, and intent
  steps, intent retry declaration metadata, with full expression precedence.
  The parity set includes a deep nested generic type fixture so LLVM
  depth/type-name handling is covered. Parse failure rendering, source
  cursor/token reads, written type-name parsing, expression parsing,
  statement/block parsing, function declaration/signature rendering,
  recursive declaration dispatch, type/ability/event/enum/zone/effect/relation/
  role/intent/nominal-domain declaration parsing, and compact AST text formatting
  are owned by separate parser modules; `main.pgy` is entrypoint-only and
  delegates parser CLI mode selection to `run_owner.pgy`. Parser tool input is
  single-sourced through `Args()[0]`; the previous
  probe-only source override is retired. The last examples scale probe was
  120 of 121 byte-equal against live `pgy --ast`, with zero byte-drift, zero
  self-host parser exits, and 1 C-oracle skip (`secure_slots`).
- Backend parity: the parser compiled by the C backend and by the LLVM backend
  produce byte-identical output. This is the core self-host correctness signal,
  the language compiles its own pass to the same result on both backends.
- Integrated complete-source producer/fixed point: the Pergyra-built gen2
  driver emits verified MIR byte-identical to C-oracle evidence. The
  Pergyra-built seed consumes it to emit `driver_gen2.c`; that artifact builds
  and runs as gen2; gen2 consumes the same immutable Pergyra MIR and emits
  byte-identical `driver_gen3.c`. No regenerated second MIR participates. A
  fresh isolated composed runner completed all these stages in 3,770,822 ms at
  2,658.0/2,667.1 MB peak private/working set, below the 3,072 MB cap.

Single source of truth (capability 5) is closed for the measured
source_ast/source_decl frontier and the supported self-hosted MIR-lowering
subset.

- Codegen source_ast frontier is at 0, all 127 original reads retired.
- Compiler-side source_ast is at 0. `MIRDeclHeader.source_ast` and
  `mir_decl_header_source_decl` are removed, and the ratchet is locked at 0.
- Residual `MIR_INST_STMT` source-payload emission is retired in C and LLVM:
  side-effect statements now flow through explicit `MIR_STMT.expr0`
  executable facts and backend emitters fail closed instead of redispatching
  the source payload as a statement.
- Source-local declaration and assignment paths no longer call raw
  source-statement emitters. View-like pin aliases are preserved through MIR
  pin facts and SSA map ownership instead of materializing a second slot.
- LLVM source-local resource constructors for `Channel<T>`, `Slot<T>`,
  `SecureSlot<T>`, and `DeviceSlot<T>` now consume MIR expected type-name facts
  plus initializer facts at the DEF owner. Standalone resource constructor
  expressions still fail closed. Assignment DEF emission preserves the source
  assignment side effect before storing the resulting value into the SSA local,
  so host-field writes do not regress to local-only updates.
- LLVM await DEF emission, C pending SSA-use materialization, and LLVM source
  DEF copy consume MIR `expr0` / `expr1` plus local-decl/source-statement flags;
  those paths no longer open `mir_instruction_source_payload`.
- LLVM MIR for-in and with-slot resource-claim diagnostics use MIR expression
  anchors instead of opening source payload statements.
- C resource mirroring compares MIR source-statement indexes instead of source
  payload pointer identity, and the C resource hook consumes DEF `expr1` type
  annotations instead of recovering local-decl payloads.
- C SSA local type/view registration consumes DEF `expr0` / `expr1` and routine
  source-local type facts. Destructured locals use MIR destructure
  binding-name/index facts, so this path no longer opens source payloads for
  binding-name recovery.
- C MIR destructure emission consumes the MIR initializer expression and
  destructure binding facts; it no longer reads `ast_let_destructure_*` from
  the source statement payload.
- C source-local LET DEF emission, generic DEF expression emission, and
  receive-payload type inference consume instruction `arg0` / `expr0` /
  `expr1` facts directly. C codegen is ratcheted against reopening
  `mir_instruction_source_payload`.
- LLVM MIR destructure emission consumes the same MIR initializer and binding
  facts through `llvm_emit_mir_destructure_inst`; the non-MIR statement emitter
  remains AST-backed for the legacy statement path.
- C and LLVM assignment emission consume MIR assignment facts. `MIR_INST_ASSIGN`
  is validated to carry target/value `expr0` / `expr1`, assignment DEFs carry
  their target in `expr1`, and C/LLVM assignment-parts emitters preserve slot,
  array, field, and projection semantics without reopening the source statement
  payload. Residual assignment MIR JSON carries target/value graphs in
  `expr0_graph` / `expr1_graph`; self SSA definitions carry value/target, and
  hard canonicalization consumes both forms in target-before-value order.
- Typed array-literal initializer, assignment, return, and codegen paths consume
  one semantic-owned array spine view; bracket trimming and element splitting
  are no longer live type authority. Collection statement arguments also use
  parser-owned roots: `ArrayPush` carries its value, while `ArraySet` carries
  index/value through the MIR secondary/primary graph lanes.
- Assignment typing derives the writable binding root, member field type,
  indexed collection base, index type, and RHS scalar type from the parser-owned
  target/value graphs. The assignment owner is ratcheted against reopening
  `SemanticProjectionExpressionType`; changing only an internal member-name
  graph node fails closed while source spelling remains unchanged.
- Initializer typing derives member, index, and call result types from the same
  parser graph. After initializer, assignment, and statement consumers moved,
  the unreferenced `projection_type_owner.pgy` was deleted and is forbidden by
  the component gate.
- Match scrutinees now use the parser-owned Atom graph as well. Statement
  typing rejects an unresolved scrutinee and cannot recover member types from
  projection text. This is parser/semantic C/LLVM evidence; match is not yet a
  member of the hard DRV-2 source-to-MIR producer frontier.
- MIR surface validation no longer reopens source payloads. Payload presence is
  checked through source-shape predicates, and thread-pool / intent
  observability surface-usage validation consumes MIR `expr0` / `expr1` facts.
- MIR DCE and source-statement emit validation consume source-shape scalar
  facts instead of payload presence for those decisions. Source-statement emit
  predicates and LLVM DEF emit predicates now consume MIR emit facts instead of
  treating source payload presence as a semantic condition. C and LLVM residual
  STMT emission branches consume MIR source-shape / `expr0` facts, and LLVM
  missing-return-value diagnostics use MIR topology diagnostics instead of
  source payload anchors.
  Select dispatch branches carry their readiness channel as a MIR branch
  `expr0` fact; C/LLVM condition emission no longer parses select case payloads.
  Match-case branches carry MIR-captured pattern/guard facts, and C/LLVM match
  condition, body-binding, and remap emission consume those facts instead of
  parsing the match-case source payload.
- Source line/column/stable-id/type seeding and transitional MIR JSON source
  text are now capture-time facts owned by
  `mir_instruction_capture_source_provenance(...)`. `mir_public_surface.c` and
  `mir_lifecycle.c` no longer open source payloads; lifecycle dump emission
  consumes `mir_instruction_source_inline_text(inst)`. Self-hosted `mir_lower`
  now consumes explicit MIR JSON `expr0`/`expr1`/`source_type`/`source_locals`
  facts only for the supported let/statement/return/branch/for subset plus
  selected args/array/string/Bool/Float/file/recursion fixture surfaces,
  straight-line calls, direct integer arithmetic, builtin-name string literals,
  directory walking, exit-guard branches, multiple Void routines with bare-call
  statements, Bool-literal branch reassignment, and loop-control
  `continue`/`break` edge blocks. The MIR JSON parity gate checks
  the `for` header is reconstructed from
  `arg0` plus range bounds rather than treating the lower bound as a branch
  condition, and rejects reintroducing transitional `"ast"` reads. It also
  reconstructs match-case integer branch conditions from `match_patterns`
  facts rather than parsing the source compatibility text, and keeps the
  self-hosted codegen file helpers aligned with the runtime absolute-path
  policy. Option match cases now carry `match_variant` and `match_bindings`
  facts; `Some(v)` lowers to an `IsSome(subject)` branch plus a fact-owned
  `Let: v : Int = UnwrapOption(subject)` binding, while `None` lowers to
  `!IsSome(subject)`, without parsing transitional source text. It also
  classifies phi-bearing loop headers from CFG backedges rather
  than phi presence alone, so nested `if` branches inside loops remain `If:`
  nodes. Array destructuring now consumes MIR JSON `destructure_bindings`
  facts and source-local array type facts to reconstruct typed array-index
  `Let:` bindings without parsing source text. Plain `class` declarations now
  flow through MIR-owned field/method/owner facts and reconstruct `Class:` /
  `Methods:` in the self-hosted MIR-lowering path. Payload-free enum
  declarations flow through MIR-owned variant facts, while payload enum variants
  fail closed from their `param_count` facts. Field-only class/subject/object/
  tobject/vessel declarations now flow through MIR-owned `nominal_kind` and
  field facts and reconstruct their exact AST labels in the self-hosted
  MIR-lowering path instead of being collapsed to a generic class alias. The
  hard gate is now **85
  positive fixtures plus 0 clean-reject fixtures** after
  promoting the already run-equivalent
  trailing-newline Log, nested string concat, string array concat, string
  case/index/trim builtin, string reassignment, two-log, while-break, and
  while-sum surfaces, array pop and array for-each loops, and typed struct
  field declaration/value flow. It also reconstructs break edges after non-empty
  statement blocks from CFG successor facts and consumes the MIR-owned
  `Random()` Int source-local type fact, the match-case integer pattern
  condition surface, the default absolute-path I/O rejection policy, and the
  nested-if-in-loop regression surface that closed the measured `heap`
  self-host via-run timeout, the array destructure binding surface, plain class
  declaration/method lowering, payload-free enum lowering, `Result<Int>` `?`
  early-return flow, `Result<Int>` core constructors/inspection helpers,
  `Option<Int>` value/match lowering, array sort/map/filter/reverse combinators,
  `Join`/`ToFloat` string utility flow, Long scalar flow, array index
  assignment, `Option` `?` propagation, string equality-plus-concat flow,
  C-reserved binding spelling, payload-free enum match comparison projection,
  Float signatures, seeded random flow, and string-array index return flow,
  and the example-origin
  `binary_search` fixture and the Int role operator-dispatch fixture. The
  coverage boundary is now measured
  at **102 PASS / 0 gap plus 0 clean rejects** across the committed
  MIR-lower/codegen/example fixture inventory. Ability declarations now consume MIR
  method signature facts and lower as zero-artifact declaration hosts in the
  self-hosted codegen pre-passes. Role declaration facts are consumed for the
  supported Int/`Arithmetic.Add` operator path, payload enum variants fail
  closed by MIR variant fact, and unsupported
  self-host codegen builtins are
  rejected before C emission, so out-of-subset operator-overload/domain nominal
  semantics and unsupported runtime helper surfaces cannot silently produce
  broken generated C.
- C class/zone collection-specialization scans are MIR-routine based and no
  longer recover method body AST; routine_source_decl_codegen is ratcheted at 0.
- C hosted method body emission binds the linked MIRRoutine body as current
  function context, and `transpiler_host_field_identifier.c` owns current-host
  field identifier lowering so stale field SSA snapshots and field/local
  shadowing do not reopen AST/source-local fallback paths.
- Type-alias target names are MIR declaration-header facts. C and LLVM now use
  the same canonical source-local type fact for alias-backed collection
  contexts; `type_alias_array_context` proves empty `Array<T>` alias literals
  compile and run on both backends.
- MIR source-local type facts are source-name keyed. C MIR-backed function
  prologue setup materializes local bindings from `MIRRoutine::source_local_types`
  instead of rescanning the AST body, and LLVM MIR alloca/type consumers
  normalize SSA-versioned names such as `push_fn.1` back to `push_fn` before
  consuming the fact. For-loop element bindings, `Array<T>.Slice(...) ->
  Slice<T>`, and `SliceCopy(Slice<T>) -> Array<T>` are captured as MIR
  source-local type facts, so branch/phi locals and collection view locals do
  not reopen AST body scans on C or LLVM self-hosted codegen legs.
- Intent retry counts are MIR declaration-header facts. `with retry(n)` is
  parsed, printed, and captured as `MIRDeclHeader.intent_retry_count`; semantic
  checking rejects non-zero retry until C and LLVM retry lowering share the same
  MIR-owned intent body wrapper, so the feature cannot become a silent no-op.
- LLVM generic class specialization type mapping consumes `MIRDeclField`
  type-name metadata before falling back to template AST compatibility.
- LLVM class constructor field arguments consume `MIRDeclField` type-name
  metadata for expected-type context before template AST compatibility.
- C/LLVM class field-slot claim helpers consume `MIRDeclFieldClaim` metadata
  instead of class destructure AST in MIR-active paths.
- C/LLVM role-slot ability tag rendering fills omitted generic actuals from
  `MIRDeclHeader` generic metadata instead of ability source declarations in
  MIR-active paths.
- C party-slot method dispatch now uses ability `MIRDeclHeader` method rows to
  choose the owning ability tag in MIR-active paths; `ast_ability_method_*`
  remains only inside the explicit non-MIR compatibility helper.
- C/LLVM declaration existence checks that only need a yes/no answer now use
  header-backed `*_decl_exists*` seams in MIR-active paths. They no longer
  recover origin AST declarations just to test class, enum, function, intent,
  callable, or constructor presence.
- C/LLVM declaration payload lookup first validates the MIR declaration-header
  row and then searches active inventory; it no longer has a
  declaration-header source_decl accessor to call.
- C projection literal/source-path lowering has a by-name MIR header path for
  ToTObject, projection-borrow materialization, member access, and domain
  provenance refresh.
- LLVM projection-borrow materialization, member access, and domain projection
  value lowering use the same by-name MIR header path for source field paths.
  The remaining declaration work is broader dedicated declaration IR coverage,
  not source_ast/source_decl payload retirement.

Substrate progress.

- DirWalk deterministic directory snapshot added (filesystem_directory_walk
  gate) and verified on C and LLVM. Examples inventory, production size, and
  ast-read-surface self-host tools now consume DirWalk directly, so their clean
  file inventories no longer depend on committed file-list aliases.
- Parser parity compiles the self-host parser through both C and LLVM, including
  deep nested generic type inference fixtures.
- The Pergyra linter, backend output comparator, backend tri-compare,
  AST-read-surface checker, diagnostics catalog checker, doc/example inventory
  checkers, module manifest resolver, production size checkers, AIR graph JSON
  validator, AIR graph consumer checkers, stable subset checker, stdlib
  dispatch inventory checker, and runtime boundary checker all passed their
  self-host preparation parity gates in the dated snapshot.
- `make self-host-preparation-test-smoke` was green on the last verified
  LLVM-enabled Windows build: lexer, parser, semantic, codegen parity, codegen
  bootstrap, backend tri-compare, MIR JSON lowering, production size/header
  checkers, and the self-hosted audit tools all passed their C/LLVM legs.
- Every one of the 22 self-host tool parity gates now exercises both backends
  when the compiler build includes LLVM.
  Previously 12 of them built and ran their Pergyra tool only with the default
  (C) backend, leaving each tool's LLVM compilation ungated. Each now compiles
  its tool with the C and the LLVM backend and writes both native outputs as
  comparable artifacts. The shared
  `tests/self_hosted/parity/llvm_leg_helpers.sh` (`assert_llvm_leg`) now invokes
  the Pergyra `backend_output_comparator` with `c_oracle`/`llvm_oracle`
  projection rows for the `--run` tools; the lexer and backend-output
  comparator keep their inline artifact legs. At the time of writing all 22
  pass, so the gap was harness coverage, not an LLVM backend defect; the gates
  now hold the C/LLVM equality invariant for the whole tool corpus. C-only CI
  builds keep the C leg mandatory and report an explicit LLVM-leg skip from the
  parity harness.
- Five Pergyra-origin AIR graph consumers now run as soft self-host evidence:
  node-id uniqueness, live-dump node-count integrity, live-dump back-reference
  range checking, fixture-shaped edge referential integrity, and root
  reachability via a push-only worklist. They are not compiler-core
  substitution yet because they do not replace
  `src/self_hosted/air/`, but they prove deterministic graph traversal and
  invariant-checking substrate on both C and LLVM.
- The semantic substitution rung has reached rung-2:
  `src/self_hosted/semantic/` checks a bounded function-body subset and now
  splits the checker into source-of-truth owner modules for source-bundle/import
  expansion, source scanning, diagnostic rendering, local environment lookup,
  expression typing, call checking, body/function checking, and program checking.
  `main.pgy` is the CLI/output boundary only. It types expressions
  through unary not, top-level binary operators
  (same-type Int/Long/Float arithmetic preserves its operand type; comparison
  and logical yield Bool), and function calls
  resolved against a signature table seeded with built-ins and the program's own
  `func` return types. It also checks call-argument types positionally against
  each callee's parameter signature, emitting `call_arg_type_mismatch` when a
  known argument type disagrees with the declared parameter type, and checks
  call arity against the parameter count, emitting `call_arity_mismatch` when
  the number of arguments differs from the declaration. It checks binary and
  logical operand agreement in `let` initializers, `return` expressions,
  `if`/`while` conditions, and assignment right-hand sides: comparison and
  arithmetic operators require equal left/right operand types (emitting
  `compare_type_mismatch` / `binop_type_mismatch`) and `&&`/`||` require Bool
  operands (`logical_operand_not_bool`), and a leading unary `!` requires a Bool
  operand (`not_operand_not_bool`), each emitted only when the operand types
  are known and disagree, mirroring the C oracle's `type_equals` rule and
  skipping when either side is Unknown. Arithmetic result typing now preserves
  same-type `Int`/`Long`/`Float` numeric operands, same-type `Bool` arithmetic,
  and C-oracle `String + String` concatenation typing while still rejecting
  mixed `Int + String` as a binary-operand mismatch.
  `tests/self_hosted/parity/semantic_parity.sh` compares its verdicts with the C
  compiler accept/reject oracle on C and LLVM across 107 committed fixtures that
  close the diagnostic matrix for every check across every statement position
  (typed let/return, arithmetic, comparison, call-return, call-argument,
  call-arity, branch-condition, scoped-block, assignment-type, bare-call-statement,
  binary-, logical-, and unary-not-operand-agreement, `let mut`, file IO builtin
  calls, scalar math builtin-table calls, trig/log Float builtin calls, string
  split/join aliases, first-argument scalar utility calls, generated-source
  string literals, String-plus and Bool arithmetic typing, Option payload typing,
  `None()` context typing, concrete
  `Option<T>` requirements for `IsSome`/`UnwrapOption`, and
  undefined-identifier cases), all
  emitted as `pgy.selfhost.semantic.v1` diagnostic blocks through
  `src/self_hosted/lib/diagnostic.pgy` and byte-equal on both backends. It
  now also gates the self-hosted semantic diagnostic-code vocabulary:
  `src/self_hosted/semantic/diagnostic_code_owner.pgy` owns the 17 lower-case
  codes, and the parity harness rejects fixture `Code:` fields or literal
  `SemanticError...("code")` call sites that are not registered there. The same
  owner maps every fixture-emitted self-hosted code to the current C oracle JSON
  root code, and the parity harness rejects invalid fixtures that fall through
  to backend-native failure or report a different C root code. This is still a
  fixture-root-code gate, not a claim that every C semantic diagnostic has a
  one-to-one Pergyra code.
  The same parity matrix checks that `if` / `while` conditions are
  `Bool` (`condition_not_bool`), that a simple local assignment `name = expr`
  matches the variable's declared type (`assign_type_mismatch`), and that
  expression-statement calls (`Foo(args);`) satisfy the callee's arity and
  argument types -- not only calls in `let` / `return` position. Simple
  identifier expressions, including identifiers nested inside compound
  arithmetic/call-argument expressions, now report `undefined_symbol` when
  absent from the local environment. It also checks scoped `if` / `while`
  bodies without leaking block-local `let` bindings into the parent
  environment. The parity set now includes an import-backed fixture, and
  `tests/self_hosted/parity/selfcheck_sources.sh` now consumes the
  `self-host-completeness-semantic-targets` manifest from TestHarness instead
  of owning a shell source list. The current completeness-owned inventory
  accepts 155 real self-host production source rows through the semantic
  checker; split parser/codegen files use the semantic target selected by
  `completeness_ledger_owner.pgy`. This is still source-stage acceptance, not a
  claim that typed self-semantic facts already drive the native backends end to
  end.
- Building the signature table reproduced the array value-semantics finding from
  the linter: a helper that `ArrayPush`es into an `Array<T>` parameter mutates a
  copy, so the table is built inline in the owning function until `inout Array<T>`
  value-result parameters land. This is the second dogfooded motivation for that
  feature.

## Current installed boundary

The complete-source Pergyra MIR-producer/bootstrap fixed point, public MIR/C/
runtime-free LLVM artifact modes, plain default C/LLVM compile/run, and the
compiler-bearing package commands are bounded `SUBSTITUTING` targets. Package
manifest and lock parsing remain C-owned orchestration, but package semantic
verification and backend artifact production no longer call the native
compiler pipeline by default. A missing installed sibling fails before source
processing, and failed package verification cannot publish a refreshed lock.

This is still target-specific rather than whole-product self-hosting. The
native formatter, debugger, REPL session UI/state, package scaffolding/init,
manifest/lock parsing, and unsupported dependency/registry surface are not
promoted by the installed compiler evidence above. Public owned dump modes are
installed-self-hosted, while unowned RIR/AIR/HIR modes fail closed unless the
caller selects the explicit native oracle. The REPL's per-evaluation compiler
call now uses the installed C runner, but that narrow substitution does not
promote the surrounding REPL product.

## Recommended next pass

Keep the fixed point, installed package gate, and REPL evaluation gate green.
No implicit native compiler call remains in the public launcher or REPL after
the bounded REPL substitution; the two launcher calls are declared test/
bootstrap opt-outs, and the package call requires the same explicit opt-out.
Do not manufacture a successor from `init`, `new`, `fmt`, debugger, package
metadata, or whole-REPL product work. A later rung must first name a fresh
production compiler bypass, an existing complete Pergyra owner, and one
executable falsifier. Do not reopen the broad sentinel campaign by raising its
ceiling.

## How to reproduce

    make pgy
    make self-host-preparation-test-smoke

After changing diagnostic rendering, the `SemanticReason` / `SemanticFix`
tables, or fixtures, regenerate the expected verdict blocks from the tool
itself. The regeneration script reads `semantic-parity-paths` from the
TestHarness manifest, compiles the manifest-projected semantic source in place,
and writes the manifest-projected expected directory, so the checker remains
the single source of truth for its own output. Review the diff before
committing:

    tests/self_hosted/parity/regen_expected.sh

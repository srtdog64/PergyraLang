# Current Work Handoff

Updated: 2026-08-04 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. Verify it against the
current source, `git status --short --branch`, the SoT registries, the named
owner, and the named executable gate.

## Active self-host context — String-array push continuation

- Executable checkpoint: `c786778a` on `main`, with this documentation as
  its intended docs-only descendant. The release-profile self-host driver used
  for final evidence is 4,516,693 bytes with SHA-256
  `CD843E59846DD5D0516EC0786FF46FCA13C87E584E633C7ACF9FFF188EA47AAC`.
- Closed executable rung: `src/self_hosted/codegen/fixture/str_array.pgy`.
  Its 8,408-byte installed self-produced MIR has SHA-256
  `4B992433D9D40A7D1FA6DFB4BAE1AF41D2F86FCF57D32C229C0B5341DFCAA75F`.
  The same MIR emits an 813-byte C artifact
  (`75AB8C27C20F1377E987123898A273F4EBECACC53855D6CEF1AC36E7356AD687`)
  and a 3,431-byte LLVM artifact
  (`AAB5F42BEE7A5EBCFF52C666312BDC184DC135359EE43767199C5B3AA8A929DF`).
  Both host-compile and execute exact `alice`, `bob`, `carol`, `BOB`.
- One target-neutral String-array plan owns the join between the canonical
  `Array<String>` local/literal/storage/ABI facts, `ArrayLength` while
  condition, exact indexed reads, admitted `ArraySet`, operation order, CFG
  edges, and ValueId/local identity. C and LLVM are terminal consumers of the
  same plan and do not reconstruct those decisions.
- Dynamic unsigned indexing is admitted only for the exact zero-initialized,
  unit-incremented while induction proved by the plan. The true guard edge has
  one predecessor and dominates dynamic accesses. Literal read/set indices are
  nonnegative and in bounds. Graph readiness rejects stale value/local rows
  before lookup.
- The negative matrix covers display independence, graph-owned initial/set/empty
  String values, set/log order, branch and length identity, dynamic and literal
  index identity, nonnegative induction, guard edge and bypass, receiver/value
  kinds, ABI length/capacity drift, stale collection rows, and absence of a
  legacy retry. All rejected inputs fail before artifact publication for both
  targets.
- Six indexed-String-only owner files were deleted, and their facts moved behind
  responsibility-named collection, plan, readiness, safety, and emission
  owners. The foreach-only collection-definition owner was generalized without
  adding a helper bucket. The prior indexed concat route now consumes this same
  plan and remains green.
- Observed evidence: release driver build 129.2 seconds; new focused parity
  32.2 seconds; prior indexed concat 27.5 seconds; scalar CFG,
  `Array<Int>` foreach, mixed-foreach, and the closed plan regressions green;
  component/removed-path ratchet 242.6 seconds; final cumulative installed
  integration 253.339 seconds.
- Memory was sampled only for the final cumulative integration. The recorded
  `str-array-final-cumulative-20260804` run exited 0 with 0.019 GiB peak
  working set and 0.008 GiB peak private memory. The 2.4 GiB attention threshold
  and 3 GiB hard stop did not fire.
- Fixed caps remain below their hard ceilings without increases: graph admission
  450/450; graph identity 100/100; graph readiness 240/240; String-array plan
  admission 417/420; plan fact 160/180; lookup 155/180; identity 86/100;
  readiness 188/200; append 109/120; binding 121/140; dominance 77/80; index
  safety 89/100; graph shape 125/140; graph readiness 127/140; C 133/140; LLVM
  205/220; mutation script 167/180; focused gate 121/130.
- Classification: bounded `SUBSTITUTING` for one local non-empty ASCII-literal
  `Array<String>` with zero-start/unit-step `ArrayLength` iteration, indexed
  `Log`, a literal in-bounds `ArraySet`, and a later literal indexed
  `Log`. It does not cover generic `ArraySet<T>`, dynamic set indices,
  empty/pushed/reallocated arrays, parameters, aliases, returned arrays,
  arbitrary while induction, or the whole compiler.
- Known independent reds: `python scripts/sot_registry_gate.py` still stops on
  the pre-existing duplicate Coq fact-authority rows, and
  `tests/self_hosted_scaffold_smoke.sh` stops because the pre-existing
  `src/self_hosted/tools/expression_graph_persisted_read_probe/` has no
  `intent.md`. This rung did not modify or weaken either conflict. Full CI,
  public matrices, current gen2==gen3 fixpoint, and proof suites did not run.
- Next objective card: only
  `src/self_hosted/codegen/fixture/str_array_push.pgy` is active. Its
  11,764-byte self-produced MIR has SHA-256
  `947C394AEB05CF69F3518812CEF2BCB3A6337294AE850ABC4D8DD6AE7AFE06FA`
  and should execute `abbccc` then `3`. C and LLVM currently reject it
  before publication with `direct MIR String array collection is invalid`.
- Objective: preserve one collection identity from an empty
  `Array<String>` through three ordered `ArrayPush` operations, then feed
  the result to the already closed length/index/concat path. Priority:
  semantic identity, operation order and length/capacity correctness, fallback
  removal, negative ratchet, artifact parity. Fact owners are the existing
  graph, operation, collection ABI/storage, CFG, and ValueId owners; the last
  legitimate consumer remains `CompileAdmittedDirectMirForTarget`.
  Forbidden fallback: fixture/block-count selection, treating empty as missing,
  capacity-as-length, backend-owned push/reallocation discovery, a second
  ArrayPush compiler, repeated whole-graph validation per operation, or
  weakening the closed literal/mutation claim. First falsifiers: push order,
  pushed String identity, length/capacity drift, empty-vs-missing identity,
  stale collection version, and no legacy retry.

## Historical checkpoint archive — mixed foreach and indexed-String setup

- Executable checkpoint: `c91def868b8c7d45dbb9cc4e212dad2b6095bcd2` on
  `main`. This handoff is its intended docs-only descendant. The installed
  current-source sibling is 4,441,513 bytes with SHA-256
  `A7AAD0A55D6F936E2CE2C7B45916EE4CAA43CFB00DC33F18729368E6DD11110A`.
- Closed executable rung: `src/self_hosted/codegen/fixture/for_each.pgy`. Its
  14,425-byte self-produced MIR has SHA-256
  `1D0771BFE62C6C20A5E671A82F0A0DD956A198D4495F08B43D5D86303DD40397`
  and one typed scalar-CFG plan executes sequential `Array<Int>` and
  `Array<String>` foreach loops through both installed direct targets.
- Final artifacts are 1,996-byte C
  (`BA6830C22D3CD427B07A63196DF7ED30F2CD3370EE9D78D153DB827FE1265D35`)
  and 6,962-byte LLVM
  (`D1BAEE876843D8112A5C3BA4F299C95AC0F4F87BE16738CFD561F63139DE67C2`).
  Both host-compile and execute exact `60` then `abbccc`; graph-only String
  payload mutation executes `xyyzzz` in both targets.
- One scalar type-family owner now classifies `Int`/`String` and their admitted
  Array iteration pairs. Element-neutral array-layout and literal-spine owners
  feed canonical `Array<Int>` and `Array<String>` ABI owners. A
  loop-syntax-keyed companion receipt owns element type and String pool while
  the existing primitive foreach receipt continues to own topology, storage
  identity, length, cursor, and binder identity.
- The sealed plan carries local/result types, typed String literal/copy/phi/
  concat/log operations, the companion foreach receipt, and one selected
  concat runtime ABI identity. C and LLVM operation/foreach emitters consume
  only these receipts. Stable public consumer names remain thin delegates; no
  second String foreach compiler or backend graph reader exists.
- The focused gate pins exact C/LLVM execution, graph-only String changes,
  iteration-order and display-`expr0` byte equality, and thirteen independent
  type, ABI, array-spine, concat-target/edge, LocalRef, and stale-use negatives.
  Invalid claimed programs publish no artifact and never retry Option match.
- The cumulative gate exposed and closed two compatibility seams: typed
  readiness now accepts an already-admitted `AddInt` literal slot without
  reparsing text, and a zero-foreach C plan preserves its previous preamble
  bytes. Single/nested range LocalRef negatives now pin their exact range/local
  inventory owner diagnostics rather than one stale generic message.
- Follow-up `c91def86` removes every sentinel introduced by the mixed rung.
  The C `Array<String>` projection now consumes the element-neutral storage
  owner's absent-index facts instead of comparing four duplicated `-1` values,
  and String element-start lookup returns `Option<Int>` rather than `-1`.
- Final observed evidence after that follow-up: current-source installed build
  124.5 seconds; mixed focused gate 14.0 seconds; corrected current-driver
  cumulative CFG integration 307.8 seconds; final structural component/removed-
  path gate 262.4 seconds. Route is 62/70, foreach set remains 320/320, graph admission
  remains 449/450, typed C is 221/240, typed LLVM is 219/220, and typed
  readiness is 154/180. No existing cap increased.
- Full CI, current gen2==gen3 fixpoint, public driver matrices, pressure
  integration, and Coq/Rocq did not run. Memory was not sampled per focused
  gate. The last accepted final-integration policy remains 2.4 GiB attention
  and 3 GiB hard stop.
- `self_host_pergyra_likeness_smoke.sh` remains known red and was not rebased:
  sentinel debt is 44/22 and compiler-zone-bound steps are 26/29. The mixed
  follow-up reduced the observed sentinel count from 49 to 44, while the other
  metrics include `core_string_munge=78/79`, `ast_string_surface=0/0`, and
  `result_use=3542/2254`. This cross-owner historical debt is not the active
  indexed-String executable rung.
- Classification: bounded local-literal `Array<Int>`/`Array<String>` foreach
  plus the earlier pure returned `Array<Int>` composition are `SUBSTITUTING`
  in installed C and LLVM. Arbitrary element ABI, indexed aggregate reads,
  effectful collection mutation/producers, and whole-compiler replacement
  remain open.
- Next objective card: `src/self_hosted/codegen/fixture/str_array_concat.pgy`
  is the only active rung. Its 7,709-byte self MIR has SHA-256
  `DBE3B8FF0D4DCFBF69A10A1D416BA08AA165C173416478CB4E61560BD428DAEE`
  and should execute exact `xyz`. Both direct targets currently fail before
  artifact publication with `direct MIR CFG single-node literal graph is
  invalid`.
- Priority is exact route ownership, canonical `ArrayLength(parts)` bound fact,
  dynamic `parts[i]` String element receipt, then reuse of the existing String
  concat/local/result plan. Fact owners are persisted expression graphs,
  `Array<String>` ABI, range iteration/LocalRef receipts, and typed ValueIds.
  The last legitimate consumer is `CompileAdmittedDirectMirForTarget`.
- Forbidden fallback: fixture-name or block-count routing, source/`expr0`
  reparsing, treating array capacity as length, backend-specific index
  reconstruction, a second range compiler, weakening the older direct-CFG
  claimant, or adding responsibility to the full foreach-set/graph-admission/
  typed-LLVM owners. The first gate must mutate length/index graph, ABI,
  ValueId, and LocalRef facts and prove no legacy claimant retry.
- Memory remains final-integration-only: attention at 2.4 GiB and hard stop at
  3 GiB. Do not add a cache, shard, worker, or per-test pressure loop before
  identifying a repeated owned operation at the reached boundary.

## Historical checkpoint archive — returned-array foreach composition

- Executable checkpoint: `6122051f` on `main`; this handoff is its docs-only
  descendant and should be clean after commit/push. `bin/pgy.exe` remains
  4,626,988 bytes, SHA-256
  `39798EA50105C9B48F26AE2FCABDB400B54AC153D15DC440B089C4E5E6402F9E`.
  The rebuilt and smoke-verified Pergyra sibling is 4,384,945 bytes, SHA-256
  `F29AC44465FB3B0B576C6716C735294232FEE5C1982CCE334CFA82684331F7B8`.
- Closed executable rung: `foreach_array_int_sum.pgy`. Its 6,761-byte
  self-produced MIR has SHA-256
  `34E49B953F380D3B3909A96FA4D266575C8A351497D1F000DC68468598C9D23E`.
  The source carries one local literal `Array<Int>`, one `Int` binder, a
  mutable total, one header phi, and an exact four-block CFG.
- That immutable MIR now produces a 913-byte C artifact
  (`2AF700B1022F5CA37D45B64E62F3C972C17084EEA89EF4551E70903AEA3EA1C5`)
  and a 3,121-byte LLVM artifact
  (`301654DB872A8FC2F6F133088A7636D38C7C75CE47A4DB24A168B6608828708E`).
  Both compile and execute exact `6`.
- `DirectMirScalarCfgForEachFact` is the target-neutral receipt. It joins the
  loop candidate, typed iteration row, collection ValueId and dominating
  definition, exact Array ABI, persisted literal graph, binding LocalRef,
  header/body/exit, and backedges once. The collection stays outside scalar
  local storage; only the binder obtains a scalar local slot.
- C and LLVM consume that same receipt for storage, ABI-owned length, data
  access, cursor, binder load, and latch increment. There is no `expr0` read,
  block-count route, capacity-as-length guess, hardcoded element count,
  backend collection-protocol reconstruction, or legacy range retry.
- The focused gate pins exact `6`, phi-input permutation byte equality, and a
  graph-only `[4,5]` mutation that executes exact `9` while display `expr0`
  remains unchanged. Twelve ABI, type-row, ValueId, graph, int32-bound, phi,
  and CFG negatives reject both targets before artifact publication.
- Fixed caps are green without increases: foreach fact 184/200, set 274/320,
  append 80/120, admission 264/280, local binding 150/180, graph readiness
  64/100, C projection 151/180, LLVM projection 187/220, general graph
  admission 445/450, C emitter 175/180, LLVM emitter 190/260, loop-flow
  admission 40/40, direct-local operand 40/40, and focused gate 103/180.
- Latest observed evidence: current-source installed build 128.7 seconds;
  focused foreach C/LLVM gate 14.6 seconds; existing scalar/range regressions
  11.0–14.9 seconds; public installed LLVM 22.2 seconds; current-driver
  cumulative CFG/AIR 209.5 seconds; structural component body 232–248 seconds.
  The final monitored component body printed its green sentinel with peak
  working 79,863,808 bytes (0.074 GiB) and private 46,170,112 bytes
  (0.043 GiB); 3 GiB hard stop did not fire. The monitoring wrapper itself did
  not recover the already-exited process code, so do not promote that wrapper
  invocation to an exit-0 claim.
- Classification: this identifier-backed local-literal `Array<Int>` foreach is
  bounded `SUBSTITUTING` in installed C and LLVM. It is not returned-array call
  composition, nested/sequential collection foreach, arbitrary element ABI,
  general foreach, whole-compiler replacement, or a current gen2==gen3 claim.
- Next objective card: `src/self_hosted/mir_lower/fixture/for_each_call.pgy` is
  the only active rung. The installed Pergyra producer emits a 17,155-byte MIR,
  SHA-256
  `F569D00CA64B92042203160511B969B2F695C12EE0EF884EF3C7BA489F269958`,
  with `MakeValues() -> Array<Int>`, nested outer/inner foreach loops, and a
  trailing foreach. Both direct targets fail before publication with
  `direct MIR Array<Int> return program envelope is invalid`.
- Priority is returned-array producer identity and call-result ValueId,
  composition with the existing foreach receipt, nested/sequential CFG
  ownership, then one C/LLVM integration gate. Fact owners are the existing
  Array return plan, call/use graph, typed iteration rows, and scalar-CFG
  collection receipt. The last legitimate consumer is
  `CompileAdmittedDirectMirMultiRoutineForTarget`; the over-broad competing
  claimant is `DirectMirArrayReturnProgramCandidate` when the two-routine
  program is not a bounded return-only consumer.
- Forbidden fallback: source/call expression reparsing, broadening the Array
  return candidate to swallow invalid main graphs, a fixture/topology-specific
  call-foreach compiler, block/routine-count semantics, backend-specific call
  composition, materializing the returned collection three times, or retrying
  another multi-routine family after a claimed invalid plan.
- The first unqualified cumulative CFG invocation still selects a stale
  `.tmp/.../driver_seed.exe` and fails at `unknown source MIR pressure token`.
  The same gate with `PGY_SELFHOST_ONE_MIR_DRIVER_BIN` bound to the installed
  sibling is green. Full CI, full bootstrap, current-source gen2==gen3, and
  Coq/Rocq did not run. `doc_link_checker_parity.sh` did not reach its link
  scan: the public C compile returned nonzero with an empty compile log; its
  cause is `Unknown` and this rung did not broaden into that toolchain repair.
  Memory remains final-integration-only: attention at 2.4 GiB and hard stop at
  3 GiB.

## Historical checkpoint archive — inactive evidence

### Previous pre-foreach snapshot

- Executable checkpoint: `2184c651` on `main`; this handoff is a docs-only
  descendant and should be clean after its commit/push. `bin/pgy.exe` remains
  4,626,988 bytes, SHA-256
  `39798EA50105C9B48F26AE2FCABDB400B54AC153D15DC440B089C4E5E6402F9E`.
  The rebuilt and smoke-verified Pergyra sibling is 4,341,923 bytes, SHA-256
  `F9947165C26884CEAE8A4E427BE2FB3B7ECCACB1291921F9D66A34A7B1486154`.
- Closed executable rung: `nested_iteration_continue_shadow.pgy` carries an
  outer declaration, two same-spelling integer range binders, an inner
  `continue`, and reads after both lexical boundaries. Its 8,363-byte
  self-produced MIR has SHA-256
  `A615CC9CE932217B5E1D752F37875C4167CA505CC6ABC6B2DF3BFAD145665130`.
  The exact nine-block CFG keeps the inner continue and normal fallthrough on
  inner header 3, then the outer latch on header 1.
- That one MIR produces an 898-byte C artifact
  (`2A0510677CB839653F6389EC3B0BA249AA99602D13F611CC618A8C3B62965E6C`)
  and a 2,368-byte LLVM artifact
  (`25A0A1945C252AF6242B2A1CBC814838DA48462A5E8FFA365D08DB4458B0B78A`).
  Both execute exact `0,2,0,0,2,1,40`; independent iteration-type and
  loop-flow row permutations remain artifact-equal.
- The positive program already executed before this change, but three
  falsifying mutations were incorrectly accepted by both direct backends:
  condition use of the outer same-spelling ValueId, inner `continue` retargeted
  to the outer header, and inner normal fallthrough retargeted to that header.
  Therefore output parity alone was not sufficient evidence.
- `DirectMirScalarCfgRangeTransfersReady` now proves each range backedge is
  owned by the innermost active range receipt. Wire-scope admission rejects an
  outer same-spelling SSA use while the inner binder is active. These decisions
  are target-neutral and both C and LLVM consume the same admitted graph; there
  is no backend scope reconstruction or nested-loop topology compiler.
- The focused negative gate also pins exact locals, syntax identities,
  instruction kinds, CFG successors, LocalRefs, range increment counts,
  permutation equality, stable failure diagnostics, and no-artifact behavior.
  Existing single/nested range gates remain in the cumulative integration hook.
- Fixed caps are green without increases: range transfer 53/90, graph admission
  394/450, wire range-scope admission 147/150, focused nested-continue gate
  105/160, cumulative hook 47/240, and public hook 60/70. Manifests remain 285
  driver, 31 core MIR, and 2 examples.
- Latest observed evidence: current-source installed build 119.0 seconds;
  focused nested-continue C/LLVM gate 6.4 seconds; cumulative CFG/AIR integration
  178.5 seconds; public installed LLVM 32.8 seconds; structural component and
  removed-path ratchet 235.7 seconds. All are green.
- Classification: innermost range ownership for nested same-spelling
  `continue` and fallthrough transfers is bounded `SUBSTITUTING` in installed C
  and LLVM. This is not general foreach, arbitrary collections, whole-compiler
  replacement, or a current gen2==gen3 claim.
- Next objective card: `foreach_array_int_sum.pgy` is the only active rung. The
  installed Pergyra producer emits a 6,761-byte MIR, SHA-256
  `34E49B953F380D3B3909A96FA4D266575C8A351497D1F000DC68468598C9D23E`,
  for `Array<Int>` values and a carried `Int` sum. Both direct C and LLVM fail
  before publication with `direct MIR range CFG block inventory is invalid`.
  Priority is producer-carried collection identity, ABI, bounds, index, and
  element facts, then one target-neutral scalar graph and C/LLVM parity. The
  fact owners are the typed iteration rows plus Array ABI/storage owners; the
  last legitimate consumer is the scalar-CFG plan. Forbidden are a
  topology-specific foreach compiler, block-count routing, source/expression
  reparsing, legacy range retry, or target-specific Array guesses.
- Rust's accepted 2026 `Move`/`Forget` direction and the bounded Pergyra
  comparison are recorded in `docs/106_ownership_model_comparison.md`. It is
  inactive research context for this rung: Pergyra has stronger owner/evidence
  composition in covered structured slices, but no general arbitrary-type
  `!Move` or `!Forget` equivalent may be inferred from that design direction.
- Full CI, full bootstrap, current-source gen2==gen3, and Coq/Rocq did not run.
  `tests/sot_authority_edge_smoke.sh` remains independently red on pre-existing
  duplicate Coq fact-authority rows; this rung did not broaden into that repair.
  Memory remains final-integration-only: attention at 2.4 GiB, hard stop at
  3 GiB, and only a final pressure result is recorded.

### Earlier checkpoint archive

### Previous iteration-binding scope snapshot

- Executable checkpoint: `aefebe13` on `main`, one commit ahead of
  `origin/main` before this documentation update. `bin/pgy.exe` remains
  4,626,988 bytes, SHA-256
  `39798EA50105C9B48F26AE2FCABDB400B54AC153D15DC440B089C4E5E6402F9E`.
  The current Pergyra-built sibling is 4,285,412 bytes, SHA-256
  `1A2BDC16E2C84C102B4C0698D3CEECB39221290CB87F442A1D723EBF78DFE091`.
- Closed executable rung: `examples/break_continue.pgy` produces one
  7,796-byte, eight-block MIR, SHA-256
  `B63639FD56B440D3B5B68E94249124E8F159127A7516F25295722932F5C036DD`.
  Header `sum.3 = phi(sum.1, sum.3, sum.9)` binds the preheader, block-5
  continue, and block-6 fallthrough predecessors. Both backedges target header
  block 1; the break targets exit block 7 and the final Log uses `sum.3`.
- The same MIR produces a 732-byte C artifact
  (`AA8FF900CAB41160BC1C817B0A659543AB71C305B24E549C60D7E9FC064EB530`)
  and a 1,950-byte LLVM artifact
  (`275C91A329195644439D0CBEF649491A53306338C579FC222A510DD74041D158`);
  both execute exact `42`.
- `routine_local_predecessor_snapshot_owner.pgy` now owns exact predecessor
  and local-version snapshots for break, continue, and fallthrough transfers.
  `routine_loop_header_backedge_binding_owner.pgy` validates each captured edge
  before binding its version. The former break-specific owner and the `for`/
  `while` post-lowering CFG rescans are removed.
- The general scalar CFG route admits `AST_CONTINUE` only as a use-free,
  unconditional edge to a dominating loop header. Range admission consumes all
  sealed backedge predecessors. Incoming permutation is artifact-equal;
  missing, stale, and retargeted continue rows reject before C/LLVM publication.
- Fixed caps remain green: predecessor snapshots 82/90, header preparation
  37/100, header binding 68/80, loop exit 83/90, for 170/180, while 118/130,
  graph admission 407/450, range receipt 269/280, and focused gate 109/160.
  No cap increased. Manifests own 285 driver rows, 31 core MIR fixtures, and 2
  example MIR fixtures.
- Latest observed gates: current-source driver rebuild 109.2 seconds; focused
  continue C/LLVM gate 6.7 seconds; prior for-break plus break/repeated-break
  gates 16.5 seconds; cumulative CFG/range integration 151.9 seconds; public
  LLVM file/stdout 15.1 seconds; full structural component/removed-path ratchet
  225.4 seconds.
- Classification: the bounded unique range binding with one reachable continue,
  one fallthrough latch, one break, and one outer mutable `Int` is
  `SUBSTITUTING`. This is not general foreach, nested/multiple loops, scoped
  iteration binding, arbitrary multi-phi, or whole-compiler replacement.
- Active objective card: make iteration binding identity survive same-spelling
  outer locals. Priority is semantic binding identity, producer active-scope
  restoration, target-neutral LocalRef identity, C/LLVM parity, then negative
  rejection. The source fact owners are the typed iteration row and complete
  source-local inventory; `routine_for_owner.pgy` owns active MIR scope, and
  `DirectMirScalarCfgRangeIterationFact` is the last legitimate consumer.
- Observed next falsifier: a temporary program declares outer `i: Int = 40`,
  runs `for i in 0..3`, then logs `i`. Native and self producers both emit two
  source-local rows named `i` but bind the body and post-loop Log to `i.2`.
  Direct C and LLVM fail closed with
  `direct MIR scalar CFG range iteration facts are invalid` and publish no
  artifact. This is observed evidence, not a completed fix.
- Forbidden fallback: name-only first/last matching, collapsing duplicate local
  rows, backend-side lexical scope recovery, source/`expr0` reconstruction,
  fixture dispatch, native semantic/AIR/libLLVM re-entry, planner retry, or
  raising a hard cap.
- No pressure probe, full CI, full bootstrap, current-source gen2==gen3, or
  Coq/Rocq suite ran. Memory remains final-integration-only: attention at
  2.4 GiB and hard stop at 3 GiB.

### Previous multi-backedge loop snapshot

- Executable checkpoint: `c27fa4e9` on `main`, one commit ahead of
  `origin/main` before this documentation update. `bin/pgy.exe` remains
  4,626,988 bytes, SHA-256
  `39798EA50105C9B48F26AE2FCABDB400B54AC153D15DC440B089C4E5E6402F9E`.
  The current Pergyra-built sibling is 4,282,817 bytes, SHA-256
  `91AC1AC29970D53DC7E48ACF77EEAD55A11065564D72FFE2EF20C11760DA4538`.
- Closed executable rung: `for_break_exit.pgy` produces one 7,229-byte MIR,
  SHA-256
  `222BBEF69BE73692E7F58A22695A910BB2A2DD793AF9A9239321B963B74048AE`.
  The producer emits header `total.3 = phi(total.1, total.5)`, exit
  `total.8 = phi(total.3, total.5)`, and binds the final Log to `total.8`.
  That same MIR produces a 580-byte C artifact
  (`DA274ADBCCA1873B30E8527960A233D9174520AE08742BD46E85EDEF74FEABA1`)
  and a 1,558-byte LLVM artifact
  (`1F9F69D4BC755CB4F46C8A7921D62DD003707CF25612F7E000DDA12F0196F87F`);
  both execute exact `3`.
- Producer ownership is no longer while-specific.
  `routine_loop_header_phi_owner.pgy` owns the admitted single-latch header
  version, and `routine_loop_exit_phi_owner.pgy` merges the feasible
  completion lane with every captured break snapshot for while and range
  lowering. No backend invents either phi.
- The target-neutral general scalar CFG plan now carries ValueId/LocalRef
  operands plus one sealed range iteration receipt. Fact, identity, readiness,
  route, operation assembly, operand spelling, range effect, and final C/LLVM
  emission have responsibility-named owners. Fixed caps remain 101/310,
  86/100, 225/240, 299/300, 397/450, 40/40, 269/280, 164/180, and 258/260;
  no cap was raised.
- The retired compiler range mini-path
  (`direct_mir_range_cfg_shape_owner.pgy`,
  `direct_mir_range_cfg_plan_fact_owner.pgy`, and
  `direct_mir_range_cfg_emission_owner.pgy`) is deleted. The remaining
  composite CFG plan is schema v9 and has no range plan/digest/emission arm.
  AIR may retain bounded range certificate evidence, but compiler consumers
  cannot retry it.
- Header and exit incoming-row permutations produce byte-identical artifacts.
  Stale duplicated header input, stale duplicated exit input, and a final Log
  that bypasses the exit phi all fail before publication for C and LLVM.
  Simple range bounds, changed bounds, zero-trip, summary/iteration/type/hoist/
  backedge/graph/binding mutations, while, break, and repeated-break regressions
  are green.
- Latest observed gates: final current-source driver graph reused its
  fingerprint in 7.1 seconds; focused for-break C/LLVM gate 6.6 seconds;
  existing break/repeated-break gate 12.3 seconds; cumulative CFG/range matrix
  149.3 seconds; public LLVM file/stdout 14.0 seconds; full structural
  component/removed-path ratchet 226.7 seconds. The manifests now own 284
  driver rows and 31 core MIR fixtures.
- Directly running the cumulative CFG gate without an explicit driver selected
  the stale `.tmp/self_hosted/driver/bootstrap/driver_seed.exe` and failed
  before the range rung with `unknown source MIR pressure token`. Current
  evidence explicitly selected `bin/pgy-self-driver.exe`; stale seed output is
  not current-source evidence.
- Classification: the bounded unique-`i`, outer-`total`, one-fallthrough-latch
  range/break slice is `SUBSTITUTING`. This is not general `for`, arbitrary
  `continue`, foreach, nested-loop, or whole-compiler replacement.
- Active objective card: close multiple actual backedge snapshots at the MIR
  producer. Priority is exact predecessor/version identity, one general loop
  header owner, general scalar CFG consumption, C/LLVM parity, then removal of
  the current single-latch admission bound. The fact owner is
  `routine_loop_header_phi_owner.pgy`; the last legitimate consumer is
  `DirectMirScalarCfgGraphPlan`.
- Next falsifier: add a range loop with both a reachable `continue` edge and a
  fallthrough latch while mutating an outer `Int`, then observe the producer
  header phi before changing it. The current range receipt deliberately rejects
  `backedge_blocks` cardinality other than one, because the producer currently
  snapshots only one aggregate backedge version.
- Forbidden fallback: consumer-side backedge value reconstruction, a
  continue-specific or block-count-specific compiler, duplicate incoming slots
  without predecessor snapshots, source/`expr0` recovery, planner retry,
  fixture dispatch, native semantic/AIR/libLLVM re-entry, or raising a hard cap
  to fit the implementation.
- No pressure probe, full CI, full bootstrap, current-source gen2==gen3, or
  Coq/Rocq suite ran. Memory remains final-integration-only: attention at
  2.4 GiB and hard stop at 3 GiB.

### Earlier checkpoint archive

### Previous for-loop break exit checkpoint

- Executable checkpoint: `6da669a4` on `main`. `bin/pgy.exe` remains
  4,626,988 bytes, SHA-256
  `39798EA50105C9B48F26AE2FCABDB400B54AC153D15DC440B089C4E5E6402F9E`.
  The rebuilt Pergyra sibling is 4,278,544 bytes, SHA-256
  `C274237F9D39B1123F8C6C8F3A75231F1A1008265176F4CD254E86A1C4DE2A44`.
- Closed executable rung: `multiple_break_exit.pgy` produces one 8,040-byte
  MIR with SHA-256
  `C2AF131C2AE49930FC9F9D1D6507DF3300A710771E8BF8BE39795CD40BBCA835`.
  Its exit phi has three predecessor slots `[i.2, i.4, i.4]`; C and LLVM both
  execute exact `2` from that same MIR.
- The pre-fix projector rejected that valid phi with
  `direct MIR scalar CFG predecessor/phi binding is invalid`. A first slot-only
  repair then exposed a false acceptance: forged stale incoming
  `[i.2, i.2]` could consume two distinct slots while omitting the latest
  definition. The landed owner therefore requires both per-predecessor slot
  consumption and the latest same-local definition dominating that edge.
- `routine_definition_dominance_fact_owner.pgy` now owns block dominance,
  strict definition ordering, and definition-to-use dominance. Phi binding
  scans all same-local routine definitions instead of treating the incoming
  list as its own proof. Caps remain 54/70 for dominance, 127/180 for binding,
  and 156/160 for the focused gate; no hard cap was raised.
- Permuting the incoming array to `[i.4, i.2, i.4]` produces byte-identical C
  and LLVM artifacts. The stale duplicate `[i.2, i.2]` and the four existing
  malformed predecessor/phi mutations still fail before publication.
- Green evidence: installed driver rebuild 130.8 seconds; focused repeated-slot
  C/LLVM gate 10.2 seconds; combined public multi-break plus nested scalar
  regression 16.8 seconds; full structural component/removed-path ratchet
  226.6 seconds. The manifest now owns 283 driver rows and 30 core MIR fixtures.
- Active objective card: make a range `for` with an outer mutable local and a
  feasible `break` carry its exit value in producer MIR. Priority is producer
  predecessor/version identity, one general loop-exit merge responsibility,
  C/LLVM parity, then negative rejection of missing or stale exit lanes. The
  current producer owner is `routine_for_owner.pgy`; the last legitimate
  consumer is the general scalar CFG plan through phi predecessor binding.
- Next falsifier: add `for_break_exit.pgy`, mutate an outer `Int` in `0..5`,
  break at a reachable iteration, and observe the installed producer and both
  projectors before changing ownership. `routine_for_owner.pgy` currently
  captures `SelfMirBreakExitFacts` but consumes only its block list when adding
  exit edges; it does not publish the captured local-version snapshots as an
  exit phi.
- Forbidden fallback: a second topology-specific for-break compiler, using the
  break block list without version snapshots, backend-created exit phi,
  fixture/block-count dispatch, source/`expr0` reconstruction, planner retry,
  or native semantic/AIR/libLLVM re-entry.
- No pressure probe, full CI, full bootstrap, current-source gen2==gen3, or
  Coq/Rocq suite ran. Memory remains final-integration-only: attention at
  2.4 GiB and hard stop at 3 GiB. The prover remains unavailable locally.

### Earlier checkpoint archive

### Previous first Array parameter checkpoint

- Executable checkpoint: `f8e91764` on `main`, one commit ahead of
  `origin/main` before this documentation update. Installed public C and LLVM
  artifact/compile/run remain target-specific `SUBSTITUTING` paths owned by the
  Pergyra-built sibling driver; this is not whole-compiler replacement.
- Closed multi-routine frontier:
  `src/self_hosted/codegen/fixture/array_return_literal.pgy` travels through
  source-to-MIR exactly once and the same 6,267-byte MIR feeds C and LLVM. A
  real producer fills caller-owned fixed storage, returns the admitted
  `Array<Int>` aggregate, and `Main` prints exactly `4\n3\n`. LLVM has zero
  `@pgy_` runtime references.
- `DirectMirArrayReturnProgramIdentity` owns exact-one `Main`, strict unique
  routine name/kind/syntax-id/return fields, zero-parameter signatures, typed
  direct-callee resolution, and semantic identity independent of routine row
  order. Row coordinates remain routing receipts, not identity.
- `DirectMirArrayReturnPlan` joins the producer literal, caller definition and
  two exact SSA uses, reachable terminal straight-line blocks, blank Log scalar
  fields, canonical ABI, target capability, and explicit
  `caller_owned_fixed_array` lifetime. C and LLVM emitters cannot reopen MIR.
- Local and returned arrays now consume one canonical captured `Array<Int>` ABI
  predicate, including every field offset, size, and alignment. A forged field
  shape with a correctly recomputed layout ID is rejected before artifact
  publication.
- Root dispatch reads routine cardinality before any row-zero shape. Once a
  program enters the multi-routine owner it cannot retry hello, scalar, local
  Array, Option, or CFG single-routine planners. Routine-order permutation is
  artifact-equal.
- The focused gate rejects thirteen independent mutations covering entrypoint,
  graph-valid unresolved callee, producer instruction/signature including
  missing and duplicate return fields, caller definition/use, ABI offset,
  repaired-ID field size/alignment drift, unreachable/nonterminal blocks, and
  forged Log result facts. It is wired into the LLVM-enabled self-host
  preparation parity aggregate, not left as a standalone smoke.
- Latest installed driver: 3,560,729 bytes, SHA-256
  `350A39D1DA6800657B24A5423B104057B4CFE33787AEDFE0F0442131ABC03EF3`.
  The final current-source DRV-2 rebuild completed in 93.9 seconds. It was not
  pressure-measured, so no memory peak is inferred from earlier builds.
- Latest green: Array-return focused C/LLVM parity and thirteen negatives;
  local Array regression; installed public LLVM compile/run; hard self-host
  contract; full component/removed-path ratchet; staged diff check. The full CI
  matrix, Coq adequacy suite, and current-source gen2==gen3 fixed point were not
  rerun.
- Active objective: compile and execute
  `src/self_hosted/mir_lower/fixture/array_literal_call_argument.pgy`. It has
  three routines (`Double`, `SumPair`, `Main`), passes a fixed `Array<Int>`
  literal into a typed parameter, nests a scalar call, and should print exactly
  `11`. This next fixture is selected but not yet admitted as completed
  evidence.
- Fact owner: strict routine identity/signature facts, typed direct-call targets,
  parameter carriage and Array ABI, caller/callee result-use identity, and the
  nested expression graph. Last legitimate consumer is one target-neutral
  multi-routine parameter plan feeding selected C or LLVM emission.
- Forbidden fallback: routine-name or row-order special cases, flattening the
  calls into constants, treating by-value Array carriage as an unowned raw
  pointer, C-only parameter reconstruction copied into LLVM, native
  semantic/AIR/libLLVM re-entry, or retrying the closed two-routine return plan
  after the three-routine graph is classified.
- Next falsifier: source-to-MIR once, the same MIR projected once per backend,
  exact `11`, routine permutation stability, and pre-artifact rejection of
  parameter type/carriage, call-target, argument-use, result-definition, and ABI
  mutations. Do not build a general query engine or jump to dynamic arrays.
- Memory policy remains one execution per changed semantic target followed by
  the final maximum only. Attention begins at 2.4 GiB and the hard stop is
  3 GiB; a below-threshold run does not redirect the active rung.

### Previous first multi-routine Array-return checkpoint

- Executable checkpoint: `76867abd` on `main`. Installed public C artifact,
  compile/link, and `--run` remain `SUBSTITUTING`. Plain public LLVM binary
  requests route only through the sibling Pergyra-built driver. The sealed
  runtime-free Option and local `Array<Int>` compile/run envelopes are now both
  executable `SUBSTITUTING` slices.
- Closed Array frontier:
  `src/self_hosted/mir_lower/fixture/array_literal_assignment.pgy` travels
  through source-to-MIR exactly once, one typed expression-graph owner, one
  target-neutral array plan, one selected ABI projection, and one C or LLVM
  emitter. Both executables print exactly `3\n10\n`; the LLVM artifact has zero
  `@pgy_` runtime references.
- `direct_mir_array_int_graph_fact_owner.pgy` owns the literal spine,
  assignment target, `ArrayLength`, indexing, and addition graphs.
  `direct_mir_array_int_plan_owner.pgy` owns local/result identities, element
  vectors, latest SSA uses, canonical layout facts, target capability, and the
  plan digest. The plan consumes instruction kind/source type from
  `MirProgramRoutineIndex`; blank scalar-capture display fields are not semantic
  authority.
- One plan drives both backends. The runtime-free representation is one
  stack-backed fixed aggregate with pointer, length, capacity, and owner fields;
  the selected ABI projection alone maps it to C or LLVM. Dispatch classifies
  the Array slice before the scalar slice and cannot retry scalar/hello after an
  Array rejection.
- The focused gate rejects seven pre-artifact mutations: element kind, index
  kind, length target, stale SSA use, ABI offset, source type, and unsupported
  static index. Source-to-MIR executes once, the same MIR is projected once per
  backend, output is exact, and the admitted MIR hash remains
  `9D056A3A9D9063207B9CD3A871E81E60684C0637A3CC4AA870E06952499C618F`.
- The installed public LLVM gate now uses the Array program, observes exact
  `3\n10\n`, and keeps exactly-once, stale-output, missing-driver, malformed-
  artifact, and no-native-fallback negatives. `clang -x ir` remains only the
  host compile/link boundary.
- Bootstrap source inputs are repository-relative. MSYS absolute spellings had
  caused the native compiler's absolute-source authority check to reject the
  import-composed driver before code generation. Output paths and cache
  identities remain explicit; component ratchets reject restoration of the old
  absolute source invocation.
- The refreshed Pergyra-built codegen seed completed in 410.451 seconds at
  2.705 GiB peak working set and 2.841 GiB peak private. This is above the
  2.4 GiB attention threshold but below the 3 GiB hard stop. An intermediate
  current-rung driver build completed in 98.359 seconds at 1.579/1.684 GiB
  working/private; later small rebuilds were not pressure-measured and must not
  inherit that number.
- Installed `bin/pgy-self-driver.exe` is 3,528,807 bytes with SHA-256
  `D3CDA2D90E2018F453DCA8ACE7B374F21E5B62EF7F4DFCB281282D1F86D2BE52`.
  Refreshed `.tmp/self_hosted/codegen/bootstrap/gen2.exe` is 2,257,728 bytes
  with SHA-256
  `BD6D3E074885CCA4C8308F873A212A04DDF4DD22E1C7244E22963B041ADCF28D`.
  The refreshed seed is current-source capable, but gen2==gen3 was not rerun at
  this checkpoint; retain the previous fixed-point result only as historical
  evidence.
- Evidence remains target-specific. General arrays, heap/runtime-bearing LLVM,
  package/dump/check/repl, arbitrary multi-routine calls, and a canonical
  compiler-purpose intent remain open. This is not whole-compiler self-host
  completion.
- Active objective: compile and execute
  `src/self_hosted/codegen/fixture/array_return_literal.pgy`, whose `Build`
  routine returns `Array<Int>` and whose `Main` routine prints `4\n3\n`.
  Installed source-to-MIR succeeds exactly once, producing a 6,267-byte,
  two-routine MIR with SHA-256
  `8AFFE11FE23F78554980FCCAA62E1DE8F024F679EC496702736FC0C47669D6DD`;
  direct LLVM currently fails closed before artifact publication.
- Fact owner: `MirProgramRoutineIndex` routine names, identities, declarations,
  and typed call/return facts. Last legitimate consumer is one target-neutral
  multi-routine plan/emitter; `clang -x ir` remains only the host boundary.
  The direct bypass to delete is hard-coded `admitted.routines[0]` and first-
  routine block/instruction shape dispatch, which currently misclassifies the
  single-instruction `Build` return as the hello slice.
- Forbidden fallback: source-name guessing, a first-routine default, copying a
  C-only call/return reconstruction into LLVM, native semantic/AIR/libLLVM
  re-entry, runtime inference from LLVM text, or scalar/hello retry after the
  multi-routine graph is classified.
- Next falsifier: the same produced MIR must feed C and LLVM once, select `Main`
  by the owned entrypoint/routine identity, carry the `Build` return into the
  caller without row-order reconstruction, execute exact `4\n3\n`, and reject a
  mutated entrypoint, call target, or return use before artifact publication.
  Do not build a general query engine.
- Latest green: Array C/LLVM focused parity plus seven mutations; installed
  public Array LLVM compile/run and exactly-once/stale/failure negatives; hard
  self-host contract; full component and removed-path ratchet; diff check. The
  full CI matrix, Coq adequacy suite, and refreshed codegen gen2==gen3 fixed
  point were not run.
- Memory policy remains one execution per changed semantic target, followed by
  the final summary only. Hard stop is 3 GiB and attention begins at 2.4 GiB;
  attention is recorded but does not redirect the active rung.

The former source-to-MIR timeout card begins below. It was correct for its
checkpoint but is superseded by `76867abd`; it must not be resumed as the active
P0. External reviews that observed `614cb5d5` likewise describe historical
evidence, not the current compiler state.

### Previous source-to-MIR full-bootstrap checkpoint

- Resume scope: read this card, verify the named owner and gate, then continue
  this one executable rung. Sections below `Historical checkpoint archive` are
  lookup evidence only and must not be treated as parallel work queues.
- Verified checkpoint: 8819acae on main, equal to origin/main before this dirty
  vertical slice. Verify exact HEAD and dirty state with Git before resuming.
- Active production entrypoint: driver_bootstrap_main.pgy,
  PgyCompilerWorld.source_mir, DriverSourceMirExecution,
  DriverRung2MirProjectionFromAdmittedAnalysisObserved, then
  SelfMirProgramFactsFromReadyArtifactObserved.
- Closed fact seams: SelfMirProgramFacts owns one immutable semantic expression
  graph. Instruction rows carry root/bounded range handles, and the program
  instruction index owns borrowed routing/text/graph bounds. Per-instruction
  whole-graph storage, graph text reconstruction, and
  SemanticExpressionGraphFactsEqual are forbidden.
- Closed cumulative-graph seam: sequence append and parser bridge carry the
  prior call-return vector and append only the new node fact. Target projection
  does not re-run whole-arena Ready. The final expression-graph fact owner
  validates the cumulative arena exactly once.
- Closed publication seam: stdout mode may materialize one MIR JSON payload,
  but artifact mode consumes verified SelfMirProgramFacts through
  SelfMirProgramJsonWriteArtifactVerified. SelfMirArtifactCommitPayload is
  forbidden in the source-MIR artifact action.
- Fixed-input release evidence: the prior artifact path completed semantic MIR
  work but crossed the 3 GiB stop at 3.098 GiB private after materializing an
  86 MB payload. The streaming path completed in 83.364 seconds at 1.525 GiB
  peak private and 1.404 GiB working set with attention_required=false.
- Current 90,304,012-byte MIR consumer evidence: r54 reached graph row 12,288
  and the 3.009 GiB hard stop at 311.431 seconds. After eliminating cumulative
  graph reconstruction, r55 reached row 28,672 during a 900-second timeout at
  only 0.965 GiB peak private and 0.904 GiB working set. This closes the memory
  defect, not the completion/throughput defect.
- r56 reached row 40,960 and was intentionally stopped after about 1,131
  seconds because another longer wait would not add implementation progress.
  It is incomplete evidence and must not be reported as green.
- The current SubstringWithLen-aware codegen seed-only build exited 0 in 400.6
  seconds. It produced a runnable Pergyra-built gen2 codegen and self parser in
  `.tmp/self_hosted/codegen/bootstrap_8819acae_r2`. This replaces the older
  pre-SubstringWithLen seed evidence. Full current-source gen2==gen3 remains
  open.
- SubstringWithLen now carries an existing length fact through runtime, native
  type/C/LLVM lowering, and self-host builtin signature. Unescaped bounded JSON
  strings and number tokens use one bounded copy instead of one allocation per
  character.
- Runtime-call ABI row 245 records the self-host helper and C/LLVM manifest
  parity is artifact-equal. The fresh gen2 seed and bounded production driver
  now execute that mapping; the earlier filtered str_builtins run still is not
  a PASS because its tool build did not finish inside 300 seconds.
- The first fresh bounded driver run reached the real readiness boundary and
  failed closed with `builtin_signature`. `SemanticBuiltinSignatureRows` had
  gained `SubstringWithLen`, while its readiness function separately mirrored
  the base row count as numeric literal 124. The numeric mirror was removed;
  seed/projection parity remains the owner-derived exact check. The new
  `builtin_signature_registry_owner_parity.sh` rejects numeric count mirrors,
  requires exactly one SubstringWithLen row, and executes the readiness probe
  under C/LLVM artifact parity.
- The repaired Pergyra-built bounded production driver exited 0 in 534.4
  seconds. Self/oracle sample C is byte-identical at SHA-256
  `0E32EC703F3B1237FC8C147BD8F395D89A53106D649F3E8F1AB4C608FC0FF25B`;
  bounded MIR JSON is byte-identical at
  `0C5E32D7E035F96C4F3EFCEFD569DA60EA8BEF98FFA3A11355DD3573C6F56739`;
  the MIR consumer emitted the same C artifact. The long section was the native
  oracle build, observed at about 0.967 GiB RSS, not repeated self-host graph
  validation.
- Emitted-C profile owner: emitted_c_runtime_header_owner.sh. The default
  self-host profile is release with -O3 -fwrapv -fno-strict-aliasing.
  PGY_SELFHOST_CC_PROFILE=test explicitly selects -O0 with the same semantic
  flags for debugging; O0 is not the normal-build benchmark.
- Open test-profile defect: the O0 generated driver reaches routine 397 and
  overflows the Windows stack in nested ApplyPostfixFact lowering because
  generated lowering frames are tens of KiB. Release mode completes that
  computation; do not hide the O0 defect by increasing the process stack.
- Evidence grade remains REACHABLE, not SUBSTITUTING. The bounded codegen
  fixed point exists, but the released default compiler still has no whole-root
  Pergyra replacement. Source files, owners, and green structural gates alone
  do not change that percentage.
- Latest focused green: fresh SubstringWithLen-aware Pergyra gen2/parser seed,
  repaired bounded production-driver sample/MIR producer/MIR consumer parity,
  builtin-signature readiness C/LLVM parity, native pgy incremental build,
  SubstringWithLen C/LLVM
  parity, bounded JSON exact-bound C/LLVM parity, expression-graph projection
  and persisted-read owner gates, MIR routine-index fixture, self-parser owner
  acceptance, source-MIR action negative gate, and the structural component
  contract through the graph/JSON slice. After the final self-host ABI addition,
  runtime ABI parity, shell syntax, line caps, and owner acceptance are green;
  the full component contract was not rerun. No full matrix is implied.
- Memory policy: execute one semantic target once, then read only the final
  peak_private_gib and attention_required summary. The hard stop remains 3 GiB
  and attention starts at 2.4 GiB. Do not poll live samples or optimize memory
  below that threshold without another reached owner.
- Forbidden fallback: graph copies, whole-graph equality, artifact payload
  materialization, default O0 self-host builds, a higher memory cap, repeated
  graph validation, per-character bounded-token strings, a general cache/query
  engine, timeout-only reruns, or unrelated library work.
- Next falsifier: execute the same full source-to-MIR target exactly once under
  the pressure owner using the current green seed and require native-oracle
  byte parity. Only a completed run may advance to current-source gen2==gen3
  evidence. Do not regenerate the seed through the Make dependency before this
  run; reuse the named current artifacts unless their imported source identity
  changes.
- Do not infer the full matrix or fixed point from focused results. If the one
  scheduled full consumer fails, its exact compile/parity diagnostic is the
  blocker. If it stays below memory attention but times out, profile the reached
  JSON/graph owner; do not merely extend the timeout or reopen already closed
  graph/streaming seams.

### Earlier historical checkpoints — inactive unless explicitly referenced

The remaining sections preserve exact revisions and prior falsifiers. They are
lookup evidence, not an ordered TODO list. Do not resume Insere/Zeno, an older
zone/ABI seam, or an architecture proposal unless the active card or the user
explicitly names it.

## Historical checkpoint - exhaustive self-host CI and executable-rung closure

- This continuation started at
  `ef1522821f9de89783f23ebdbcacbc34bec05705`, equal to `origin/main` before
  the current dirty change set. The containing repair commit and push must be
  verified with Git; this note cannot name its own containing revision.
- GitHub Actions run `30535237959` kept the backend comparison shards, formal
  proofs, sanitizers and self-host codegen bootstrap green, while exposing five
  independent contract failures: imported enum variants were absent from the
  lightweight semantic callable table; several standalone sources relied on
  transitive imports; language-word implementation inventory was stale;
  memory-concurrency and production-header gates still named the pre-zone-sync
  owners; and full bootstrap rejected compiler-stage nested intent calls at
  `mir-facts:start` because DIR admitted only direct subject actions.
- The semantic SoT repair projects declared enum variants into the canonical
  callable table with enum return and payload signature. The positive fixture
  covers qualified and bare zero/payload variants. The negative fixture locks
  `ImportedDecision.Missing -> undefined_symbol`; rewriting the driver or
  treating a missing qualified variant as an arbitrary member read is a
  forbidden fallback.
- The exhaustive checker also falsified three older scanner/import assumptions.
  The delimiter owner now distinguishes a spaced comparison `<` from a
  type-argument opener, nominal constructor scanning consumes `let mut`, and
  direct consumers own their imports. The intentional
  `expr_type_owner/result_call_type_owner` recursion remains one declared
  checker cluster rather than being turned into a circular source import.
- Last observed broad semantic evidence for this dirty set is the current
  TestHarness manifest plus C semantic checker accepting all 684 real
  self-host sources. This is source semantic acceptance, not full-bootstrap
  execution. The native
  production-header census passes at 717 headers; the self-host C/LLVM header
  checker is artifact-equal for clean and over-cap cases. The memory-concurrency
  model passes its C-focused path while following
  `pgy_runtime_zone_sync_abi.h` as the lock-diagnostic owner.
- Objective card for the now-reached nested-intent seam:
  - objective: admit the documented `FrontendPipeline -> IntakeSource ->
    SourceUnit.Read` composition without flattening or disguising an intent as
    an action;
  - priority: exact intent identity, action/intent discrimination, return/arity
    carriage, fail-closed negative cases, then bootstrap completion;
  - fact owner: `SemanticAstIntentSignatureFacts` plus the parser-owned
    expression graph; `SemanticAstIntentCallFromGraph` carries the exact chosen
    target and DIR/MIR consume that identity;
  - last consumer: self-host MIR intent routine construction during full
    bootstrap;
  - forbidden fallback: rewrite `stage_intents.pgy`, insert intents into the
    action/function table, accept name-only/ambiguous calls, or silently skip
    the compiler intent cluster;
  - observed falsifier: a fresh C-emitted self driver passed documented nested
    intent, single-step and two-step graph identity parity plus missing,
    ambiguous and wrong-arity target negatives. Classification is `REACHABLE`,
    not `SUBSTITUTING`.
- The adjacent authority seam is also reached: semantic facts now carry exact
  authority, zone, subject-slot and required-ability identities into DIR, and
  `zone_authority_fact_owner.sh` proves the old AST rescan is absent plus
  mutation negatives. It remains `BRIDGE` until the production MIR authority
  transition and runtime plan consume the same owner.
- Parser intent parameters are finalized after declaration/import composition,
  not in source order. Native C tracks the header-binding prefix and rebuilds
  the involved/value views once. The self parser emits neutral
  `IntentBinding` rows and resolves exact final subject/zone identities; its
  matcher is indentation-anchored so embedded contract strings are preserved.
  Native cross-module positive/unresolved-negative fixtures pass, and
  `bin/pgy --ast src/self_hosted/compiler/driver_bootstrap_main.pgy` resolves
  `SourceIntakeZone -> IntentInvolves` and `StagePathManifest -> IntentValue`.
  The focused self resolver reproduction passes; the whole-driver self-parser
  run was policy-stopped after 1,532.042 seconds with zero output. Its last
  observed private memory was 717,144,064 bytes, not an exact peak. Therefore
  full self-parser integration remains incomplete for performance, not a
  semantic PASS.
- Focused green evidence: component contract, zone-authority carriage, fresh
  nested-intent self-driver reachability, language-keyword registry, LSP
  latest-publication C/LLVM parity, changed-owner semantic checks, 684/684 C
  real-source semantic acceptance, and SoT live owner/consumer negative gates.
  Coq/Rocq is absent on this runner, so the formal model was explicitly skipped
  with `PGY_ALLOW_MISSING_COQ=1`; it was not claimed as checked.
- The attached architecture review's query-engine and opaque-admitted-artifact
  proposals remain valuable follow-up candidates, not concurrent owners in this
  repair. Its memory improvement still holds, but the CPU warning remains
  current: the latest full integration reached `mir-facts:start` and was
  stopped after 2,534,272 ms at 2,284.8 MB peak private. The next executable
  falsifier is a profile of that exact owner path with stable revision/query
  keys, followed by a full bootstrap under the unchanged limit. Do not raise
  the timeout or call this incomplete run green.

## Historical checkpoint - exact bootstrap pressure and zone runtime closure

- This material continuation started at
  `3f1416bd1f09864bb45dcea982af611e67fffb5b`, equal to `origin/main` before
  the current dirty change set. The final commit/push must be verified with Git;
  this note cannot name its own containing revision.
- Active executable rung: close self-host zone storage and synchronization from
  the existing declaration, DIR topology and
  `semantic.domain_runtime_assignment` owners. The last consumer is the
  self-host C runtime emitter. Empty sync bodies, name-only reconstruction,
  fabricated MIR/domain-graph identity and a copied native body detached from
  admitted facts are forbidden fallbacks.
- Exact pressure is no longer the blocker. The fixed 5,106,665-byte AST
  (`97EEFA34159BE8AFEA8D15F44BF5F74FB57D5DD1D8C03ABF565AF4A14B8D5190`)
  completed C emission in 158.020 seconds at 1,659.1MB peak private. A fresh
  parser artifact of 5,326,689 bytes
  (`49BFB21900867135FBAF6F51F23364BB108B88A65C62328541D6089DBD64844B`)
  completed in 164.252 seconds at 1,742.1MB peak private. After exact zone
  authority/where/slot carriage, r10d completed the same artifact in 145.719
  seconds at 1,759.6MB peak private and 1,666.9MB working set; its C output is
  byte-identical to r9. All used the unchanged 3,072MiB / 2,400-second pressure
  policy. The earlier 20GB symptom is not reproduced.
- A fully current r11 rerun removes the remaining stale-artifact ambiguity. Its
  fresh AST is 5,324,488 bytes. The zone-sync 9c codegen build completed in
  71.756 seconds at 911.2MB peak private and 865.2MB working set. Exact C
  emission completed in 123.632 seconds at 1,838.6MB peak private and 1,733.1MB
  working set, below the unchanged 3,072MiB limit. The output is 5,368,419
  bytes. The AST contains 20 zone identities and the emitted C contains exactly
  the same 20 `static void *Zone_sync` definitions: 18 compiler-world zones and
  two support zones. Host GCC compilation now succeeds and the executable
  reaches the expected driver argument boundary. The former 15 missing-symbol
  host-compile failure is closed. After the final owner/policy consolidation, a
  fresh current-source codegen repeated the exact emission in 136.249 seconds,
  produced 5,368,053 raw UTF-8 C bytes, preserved the 20/20 bijection and again
  host-compiled successfully. This last repeat was not pressure-sampled; the
  9c peak figures above remain the measured memory evidence.
- The measured closures are admitted constructor proof reuse, binary node-ID
  reads, carried call-return/place facts and one same-epoch global type-row
  index. Dynamic local rows retain their prepend/first-row rule. A general
  cross-revision query engine remains deferred until a stable key/revision owner
  and measured invalidation consumer exist.
- Parser intent parameters now keep one source-order stream. The interleaved
  zone/value/subject/value fixture passes through both native and freshly built
  self-host parsers. Intent codegen derives omitted `who` through the semantic
  actor owner, keeps actor distinct from authority, accepts by-value and inout
  `using`, requires explicit `where` to match that zone, and accepts authority
  only through an exact declared zone subject slot. Exact aliases win; a
  type-only slot match must be unique.
- Evidence grade for the new exact compiler-world zone result is `REACHABLE`,
  not hard `SUBSTITUTING`. It removes the self-host emitter's empty runtime
  fallback and produces executable C, but no released C-owned production route
  has yet been replaced by a freshly installed self driver. The already
  substituting admitted MIR domain-runtime slice remains intact: nonzero typed
  topology continues through its admitted plan, while the semantic-artifact
  fast path accepts only proven zero topology and fails closed before partial C
  on nonzero topology. The currently installed `bin/pgy-self-driver.exe` remains
  stale until the next installed-driver replacement rung.
- Last observed green focused evidence: language keyword registry
  (146 rows, 70 reserved, 76 contextual selectors, 9 fixtures), include-size
  gate, semantic environment lifetime/admission ratchet, source-MIR execution
  action gate, intent-step binding execution contract, MIR machine layer,
  domain-runtime assignment, zero-topology zone-sync execution and full
  self-host component contract. The zone gate proves a declaration/definition
  identity bijection, two generation increments with unchanged object/tobject
  projection tuples, default atomic execution, thread-safe execution under an
  explicit harness-owned init/destroy lifecycle and fail-closed
  semantic-artifact nonzero topology. Language-generated zone constructor/
  destructor lifecycle ownership is still open. The standalone leaf-place
  contract checker ran for more than ten minutes without output and was stopped;
  it is **not a PASS**. The broad C/LLVM real-source semantic selfcheck was also
  sampled only through the first 13 of 673 C targets and stopped because its
  projected duration exceeds the repository's focused integration budget; it
  is a scheduled/CI matrix and is **not recorded as a full PASS** here. The SoT
  adequacy mutation checks pass with `PGY_ALLOW_MISSING_COQ=1`, but the Coq model
  is an explicit skip because this runner has neither `rocq` nor `coqc`; the
  unmodified fail-closed invocation exits nonzero and is not recorded as green.
- The previous GitHub Actions selfcheck timeout on
  `compiler_world_direct_mir_owner.pgy` was reproduced locally beyond 86.383
  seconds. Sealed-length scanner calls and one-pass import-bundle assembly now
  produce the 760,066-byte bundle in 2.978 seconds and return semantic
  `Status: ok` in 2.681 seconds. The faster checker also exposed and closed the
  missing `zone`/`world` nominal-constructor and slot-row semantics; the
  60-second CI budget remains unchanged.
- The focused semantic parity runner completed all 113 C verdict fixtures.
  Its subsequent LLVM leg was stopped after the combined runner exceeded the
  five-minute focused budget; LLVM semantic parity is therefore not recorded as
  a PASS in this checkpoint and remains for CI/scheduled execution.
- GitHub CI run `30524796373` proved that the former 60-second
  `compiler_world_direct_mir_owner.pgy` timeout is closed: exhaustive C
  selfcheck passed that root and advanced to 155/677. It then exposed a
  separate standalone import defect in `direct_mir_llvm_text_format_owner.pgy`
  (`undefined_function: Die`). The owner now imports the existing codegen text
  boundary directly, and the component gate rejects removal of that edge. A
  replacement CI run is required before this fix is called green remotely.
- The attached 2026-07-30 architecture review's 40-minute/1.55GB timeout was
  valid for its older checkpoint, but it is superseded by the measured 9c exact
  result above. A general query/dependency engine is therefore an evidence-led
  future architecture target, not permission to start a parallel cache owner in
  the active rung.
- Next falsifying sequence:
  1. build and install a fresh self driver from the host-compilable exact C;
  2. run installed launcher parity plus live typed-intent execution and reverse
     compensation gates;
  3. prove the installed production entrypoint reaches the world/action owner
     and delete the replaced C-owned route without a dual read;
  4. only then classify that deleted-path replacement as `SUBSTITUTING`;
  5. keep composite-intent full DIR admission and thread-safe lock lifecycle as
     separately named open seams rather than weakening the zero-topology gate.

## Historical checkpoint - body admission and latest-only publication

- Material checkpoint: `835348ac318506031a375d8fc168a55e9ca94eb3`
  (`feat: seal self-host body and publication facts`), based on
  `6f83a7cd40ba6ff06ab1bb429fe5e877d41b1752`. At handoff-writing time `main`
  is one commit ahead of `origin/main` and the only intended dirty file is this
  handoff refresh. Verify the final handoff commit and remote equality with Git;
  this paragraph is not repository-state authority.
- Active self-host seam: producer-time `SemanticAstArtifactAnalysis` now carries
  function scopes, and `AstBodyAnalysisAdmission` performs one identity,
  parallel-row and reconstruction-free structural-shape admission before the
  body stages. Initializer, iteration, call-target, refinement, place,
  assignment, statement and generic owners consume the admitted analysis and
  its existing enum/scope/table facts instead of reopening the whole artifact.
- Fact owners are
  `ast_body_analysis_admission_owner.pgy` and
  `ast_body_analysis_shape_owner.pgy`; the fail-closed witnesses live in
  `ast_body_analysis_admission_contract_owner.pgy`. The last orchestration
  consumers are the body bundle, driver rung 2 and admitted codegen pipeline.
  Forbidden fallbacks are per-stage artifact reconstruction, repeated graph
  validation, and trusting a caller-supplied mutable analysis solely because
  its row counts still match.
- The driver boundary is intentionally split. Fresh analyses use the admitted
  entrypoints. Externally supplied raw analyses retain one deep
  `SemanticAstArtifactAnalysisMatches` proof. The mutable-analysis fixture
  changes a local name without changing shape and is rejected before
  `body-types:start`.
- Canonical MIR identity/epoch projection now obtains nested domain-runtime
  assignment facts through the typed owner accessors while preserving the
  whole borrow. The parity fixture remaps declaration, topology, directive,
  slot, field, path and runtime-assignment epochs atomically. The stale
  compiler-world smoke assertion was updated to require the admitted
  `GenerateCUnitFromReadySemanticFacts` path rather than a deleted fallback.
- Insere adoption is no longer documentation-only. The production self-host LSP
  `Main --document-store-probe` consumes one `LspDocumentRevision` fact owning
  URI, numeric version, exact text and `HostTaskSlot` ticket. Lower versions,
  same-version/different-text changes and stale diagnostics publication are
  rejected without partially mutating the document store. This is bounded
  `REACHABLE`; it does not yet replace the released C LSP loop.
- Zeno adoption remains the existing `SnapshotTicket` plus
  `BinaryProjectionPreflight` slice. It binds slot generation, the existing
  `MirAbiLayoutRowCapture` identity and explicit endian, then runs through C and
  LLVM. It is `REACHABLE` tooling/library evidence, not a second Layout IR and
  not `SUBSTITUTING` compiler progress.
- Last observed focused evidence for this material checkpoint:
  - body analysis admission owner and shape owner self-host semantic checks:
    PASS;
  - standalone admission-contract semantic check exceeded the focused CPU
    budget and was stopped after about 16 minutes: **not a PASS**. The same
    contract is reached by the driver readiness path exercised by the passing
    component gate;
  - raw-analysis mutation admission gate: PASS, fixture build `0 errors, 0
    warnings`;
  - semantic environment lifetime/admission ratchet: PASS;
  - self-host component contract: PASS;
  - compiler-world topology/source-shape contract: PASS;
  - canonical identity/epoch C execution and stale/wrong-kind negatives: PASS;
  - Insere-derived LSP latest-publication C/LLVM parity plus the existing
    document-store/session-state parity: PASS;
  - Zeno-derived binary-projection preflight C/LLVM executable parity: PASS;
  - build-source inventory, documentation quality, shell syntax and
    `git diff --check`: PASS.
- No new 2.9MB or 5,106,665-byte pressure run was performed for this checkpoint.
  The previous exact 5.1MB run stayed under 3GiB but timed out, so it remains a
  performance falsifier rather than an end-to-end PASS. A sampled final focused
  build showed the Pergyra process and `cc1` each near 0.9GiB, but that sample is
  not a formal peak measurement.
- Evidence grade remains `REACHABLE`, not `SUBSTITUTING`: this checkpoint closes
  repeated semantic admission and mutable-boundary defects and adds a real
  self-host LSP consumer, but does not yet delete another C-owned production
  compiler/LSP entrypoint.
- Next falsifiers, in order:
  1. rerun the exact 5,106,665-byte normalized parity fixture under the
     3GiB/2,400-second cap; only if body admission remains below the dominant
     cost, close the measured emission-side linear node-ID lookup seam;
  2. carry the Insere-derived revision ticket through the live read-exact
     diagnostics completion and delete released-C direct document mutation;
  3. derive Zeno-style ABI inspect/diff only from
     `MirAbiLayoutRowCapture`, rejecting offset/size/alignment/endian drift and
     identity collisions without introducing another layout owner.

## Historical checkpoint - admitted semantic artifact emission

- Exact remote base for this continuation was
  `18c105a75894d1b09c66da2cad5b1b380e3c7a73` on `main`, equal to
  `origin/main`. It contains `28e371df` (Insere/Zeno audits), `2c2b3028`
  (semantic admission), `3bc5e724` (cross-platform self-host CI contract
  closure) and `18c105a7` (handoff refresh).
- Latest local checkpoints are `22054b4e` (`fix: import option call target
  owner`) and `9d51ff5d` (`test: refresh source scan owner evidence`), after
  `0984ba77` closed the dynamic ability selfcheck target. At handoff-writing
  time the worktree is dirty only for this handoff refresh and `origin/main`
  remains at `15485120`; push is authorized and pending. Verify final remote
  equality after the handoff commit rather than treating this paragraph as Git
  authority.
- Active executable seam: a Pergyra-built codegen created one
  `SemanticAstArtifactAnalysis`, then C emission called
  `SemanticAstArtifactAnalysisMatches` and reconstructed signatures,
  constructors, locals, assignments, statements, enums, roles, expression
  surfaces/graph, type surfaces and kind surfaces from the whole AST again.
  On the 5.1MB bootstrap AST this was the last observed 3,072MB scaling RED.
- `AstTreeArtifact` payload schema v4 now carries one producer-time
  `identity_digest`. It binds tree text, node count and parser-owned expression
  graph roots/arena. Parser graph injection recomputes it once; deterministic
  owner-kind and destructure arena projections preserve that epoch. Semantic
  verdicts seal the same identity. Emission never recalculates the digest.
- `SemanticAstArtifactAdmissionReady` is the fixed-size comparison boundary:
  verdict status, node count, artifact identity and entrypoint policy only. It
  cannot call AST/graph readiness, hashing, `*FactsMatchArtifact`,
  `*FactsFromArtifact` or `*RowsFromArtifact`.
- The digest is an epoch witness, not an untrusted-input security seal. Fast
  callers are exact-allowlisted and receive owner-produced fact arrays that are
  immutable after admission. Arbitrary or mutable artifact/analysis pairs must
  use the public deep-checked compatibility entry.
- Production entrypoints now use one admitted/Ready graph:

  ```text
  codegen seed
    -> GenerateCUnitFromAstArtifact
    -> SemanticAstArtifactAnalyzeCompactBridge             # exactly one
    -> GenerateCUnitFromAdmittedSemanticArtifact
    -> GenerateCUnitFromReadySemanticFacts                 # reconstruction zero

  source-to-C / admitted MIR-to-C
    -> one semantic analysis + body-type admission
    -> GenerateCUnitFromReadySemanticFacts
  ```

  The unused `GenerateCUnitFromSemanticFacts` checked fallback was deleted.
  The externally callable raw semantic-artifact entrypoint retains exactly one
  deep match before entering the admitted path.
- Fail-closed evidence now includes same-node-count/different-text and
  same-tree/different-expression-graph artifacts. Both receive different
  producer identities and are rejected by admission. The lifetime gate fixes
  direct codegen, source-to-C and integrated driver call edges and forbids
  unbounded proof work inside the Ready core.
- Observed focused evidence for the material checkpoint:
  - `driver_rung2_main.pgy --emit-c`: `0 error(s), 0 warning(s)`;
  - executable `CompilerDriverPipelineReady` probe:
    `semantic-admission-contract:ok`, exit 0;
  - self-host component contract: PASS;
  - semantic environment/admitted emission ratchet: PASS after correcting its
    stale call-target assertion to follow the existing body-environment owner;
  - shell syntax and `git diff --check`: PASS;
  - fresh native-seed codegen build: exit 0, 47,749ms, peak
    1,138.2MB working / 1,190.8MB private;
  - 2,864,634-byte AST emission: exit 0, 1,098,757ms, peak 890.5MB working /
    968.4MB private, 2,785,703-byte C output;
  - 5,106,665-byte AST emission: timeout 124 at 2,400,686ms, peak 1,436.1MB
    working / 1,551.4MB private, no 3,072MB limit breach. This closes the
    observed memory RED but is not an end-to-end emission PASS;
  - compiling the successful 2.9MB C output into gen2: exit 0, 4,710ms,
    peak 244.5MB working / 229.7MB private;
  - gen2 consuming the same 2.9MB AST: exit 0, 1,059,367ms, peak 1,177.7MB
    working / 1,357.3MB private. Raw output differed only by one trailing blank
    line; the repository emitted-C normalization/comparator passed.
- Evidence grade remains `REACHABLE`, not `SUBSTITUTING`. This removes a real
  Pergyra-built codegen hot-path reconstruction and fixes its provenance
  boundary, but does not yet prove a fresh installed self driver or replace a
  new native C semantic owner.
- The 5.1MB CPU audit isolated two remaining costs. Before emission,
  `SemanticAstBodyTypeBundleFromAnalysis` repeatedly deep-matches or rebuilds
  signature/local/iteration/assignment/statement facts. During emission,
  `EmitStmtList` performs nested linear node-id searches for local, assignment,
  statement-kind and expression rows. The fixed fixture has 110,971 nonempty
  AST rows, 4,094 callables, 12,224 locals, 6,958 assignments and at least
  27,675 tracked statements. Local/assignment misses alone imply a
  530,861,850-comparison lower bound; statement-kind lookup is approximately
  3.50 billion comparisons from the current loop order and average row
  position. These are source/census calculations, not runtime counters.
- Next falsifier: add coarse, non-row-level stage markers, close body semantic
  proof admission without reopening whole-artifact reconstruction, then rerun
  the exact 5,106,665-byte fixture under the 3GiB/2,400-second cap with
  normalized-C parity. Only after that seam closes should a separate emission
  rung replace owner-local linear node-id scans with ordered lookup. The timed
  out 5.1MB run remains non-green.
- The body-admission implementation card is now concrete:
  - owner: the existing `SemanticAstArtifactVerdict` identity and analysis
    produced by `SemanticAstArtifactAnalyzeFromExpressionSurfaces`; do not add
    a parallel body authority;
  - last legitimate proof consumer: a new
    `SemanticAstBodyTypeBundleFromAdmittedAnalysisObserved` entry that checks
    artifact identity plus fixed parallel-row shapes before body work;
  - production callers: only `program_admitted_semantic_owner.pgy`,
    `driver_pipeline_owner.pgy` and `driver_rung2_owner.pgy`; probes and
    arbitrary/mutable-pair contracts retain the checked entry;
  - forbidden admitted closure: `SemanticAstArtifactAnalysisMatches`, every
    whole-program `*FactsMatchArtifact` and already-carried plural
    `*FactsFromArtifact`, repeated expression-surface/graph full readiness and
    a `fast ? checked : admitted` dual path;
  - the admitted family cores must receive analysis-owned enums, roles, intent
    signatures/transitions and function tables. The existing checked APIs
    perform their deep proof once and then descend into those cores;
  - hidden cost: `SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact`
    currently rebuilds enum and function-scope facts at match-visible use
    sites from initializer, assignment, statement, call-target, place and
    generic passes. Add an admitted-facts seeder consuming the existing scope
    local view; a single case pattern projection may remain, but plural
    whole-program reconstruction may not;
  - negative gate: stale identity or one malformed parallel row fails before
    `base-initializer:start`; admitted transitive forbidden-call count is zero;
    exact caller allowlist holds. Run component/lifetime and body parity first,
    then 2.9MB normalized parity, then the exact 5.1MB capped falsifier.
  This is still `REACHABLE` performance closure, not a new native-C owner
  substitution and therefore not `SUBSTITUTING` evidence.

## Historical checkpoint - independent CI portability repairs

- `expr_semantic_call_argument_owner.pgy` now directly imports the owner that
  defines `RewriteSemanticExpectedValue`; the final integrated driver source
  compile completed with 0 errors and 0 warnings.
- Raw `test-mir` and the dedicated topology target both derive
  `PGY_DOMAIN_RUNTIME_TOPOLOGY_BACKENDS` from `LLVM_ENABLED`. The inventory
  gate requires exactly those two owner expressions. C-only topology passed.
- HostTask slot/policy comparison no longer assumes Windows has `cmp`/`diff`.
  Bash `read -d ''` preserves trailing newlines before exact text comparison;
  both C focused gates passed.
- The macOS hard-contract false failure no longer uses early-exit `grep -q`
  behind a producer under `pipefail`. Hard contract and build inventory passed.
- GitHub run `30498129265` then exposed three remaining contract drifts:
  - self-host parity checked `expr_semantic_call_type_owner.pgy` outside its
    intentional recursive expression-emission closure. Completeness now maps
    that source to `expr_semantic_graph_emit_owner.pgy`, as it already did for
    the other cyclic cluster members; direct reverse import remains forbidden;
  - the new `SFSemanticAstArtifactAdmission` registry fact had no corresponding
    Coq `SpineFact -> SpineOwner` projection and the registry summary still
    claimed `ACTIVE=0`. The projection is now
    `SFSemanticAstArtifactAdmission => SOSemanticArtifact`, the count is
    `ACTIVE=1`, and enforcement anchors point to executable negatives;
  - the machine-layer projection probe still emitted routine source identity
    zero and omitted let-row ABI type facts after the MIR consumer tightened
    those contracts. It now emits positive identity `1` and producer-owned
    `DeviceSlot<Int>`/`Int` ABI types.
- Valid post-fix evidence:
  - generated selfcheck source-to-target row exact and graph-emission semantic
    checker `Status: ok`, `Diagnostics: none`;
  - full self-host component contract: PASS;
  - SoT authority edge: `62 authorities, 67 derived fact carriers;
    CLOSED=34 BRIDGE=27 ACTIVE=1`;
  - unique repo-relative full machine-layer gate: exit 0; MIR/AIR declaration
    rows and self-host C `DeviceSlot`/`RemoteFuture` lowering wired, malformed
    owner identity rejected.
- GitHub run `30501338487` reached a later self-host parity failure after all
  preceding semantic C/LLVM 113 fixtures and 25/661 C source targets passed.
  Target 26, `expr_semantic_dynamic_ability_call_emit_owner.pgy`, called the
  graph-root-owned `RewriteExprFromSemanticGraph` from the intentional
  `graph -> call -> dynamic ability` recursive emission closure. Completeness
  omitted only this indirect cluster member. The source now maps to
  `expr_semantic_graph_emit_owner.pgy`; a negative gate forbids solving it with
  a dynamic-to-graph reverse import.
- Post-repair evidence for that later failure: current-tree manifest build
  exit 0 with 0 errors/warnings; exact dynamic-source-to-graph-target row;
  graph semantic checker exit 0, `Status: ok`, `Diagnostics: none`; full
  self-host component contract PASS; `git diff --check` PASS. The same remote
  run already has Rocq 9 proofs, sanitizers, TSan and the completed backend
  comparison shards green, but its older self-host parity job remains red by
  definition and the long Linux/Windows/macOS/bootstrap jobs were still in
  progress when this snapshot was written. A fresh pushed run is required.
- Fresh run `30502023063` proved the dynamic source-to-graph target by reaching
  29/661, then exposed a distinct direct-import defect in
  `expr_semantic_option_value_owner.pgy`: it consumed
  `SemanticExpressionGraphCallTargetKind/Name` without importing
  `ast_expression_call_target_fact_owner.pgy`. That direct owner edge is now
  present and ratcheted. Current-tree C and LLVM standalone semantic checks
  both return exit 0, `Status: ok`, `Diagnostics: none`; the LLVM checker build
  has 0 errors/warnings and the full component contract passes. Unlike the
  recursive graph-emission cluster, this file must not use a completeness
  redirect because no import cycle exists.
- The same run's macOS C-only job passed 62/63 staged contracts and failed only
  because source-scan evidence still named pre-keyword-registry/pre-canonical-
  reuse owner hashes. Current CRLF-normalized owner-set hash is
  `2ECB092EA4E5C16B786CE8A6D732A5B958434C8AB748E9E7DB060C9745548DC5`;
  current type-canonical owner hash is
  `E6BD4E6D10612CB019265AD7763DF7FC37BBF748A0F10C919D4EFF5D5D74D859`.
  Only those ratchet identities changed; historical elapsed/peak values were
  not relabeled as new measurements. The full local source-scan contract now
  passes. A bounded follow-on C selfcheck of targets 30-36 found no additional
  `undefined_function`; three large targets reached the explicit 45-second
  audit timeout with no diagnostic and are not counted as PASS.
- One absolute unique-build attempt failed before product execution because an
  MSYS-form absolute path was not stripped to the repository-relative input
  required by self-host I/O policy. One overlapping default-dir run was also
  excluded because two validators briefly shared output names. Neither is
  counted as product evidence; only the clean relative unique run above is.
- One process-management error remains part of the handoff record: while
  narrowing its own 661-file selfcheck, the CI worker incorrectly terminated a
  separate `C:\msys64\usr\bin\bash.exe` process running
  `tests/self_hosted/parity/selfcheck_sources.sh` (PIDs 30812 and 54352,
  parent 49280) around 06:39 KST. No source changed. The parent had
  already exited and cwd/environment were not recoverable, so the exact run was
  not guessed or restarted. Do not count that interrupted external selfcheck
  as a PASS; rerun it from its owning task if that owner still needs the result.

## Previous checkpoint - installed source-to-MIR one-graph closure

- Exact working base is `8da168bc5c3e09f4f31788c133bfc5f053bf8a91` on
  `main`, equal to `origin/main` when this checkpoint began. The tree is dirty
  for the active integration; verify final HEAD and clean state after the
  authorized commit/push rather than treating this snapshot as Git authority.
- Active seam: the installed `bin/pgy --self-driver --emit-mir-json-verified`
  path and the full bootstrap artifact path must share one source-to-MIR
  execution owner. Physical stage folders remain fact-lifetime owners; they do
  not own competing program roots.
- The actual installed graph was traced through the native sibling launcher:

  ```text
  bin/pgy --self-driver
    -> src/compiler/self_host_driver.c -> bin/pgy-self-driver
    -> driver_rung2_main.Main -> RunDriverRung2FromArgs
    -> ProduceSourceMirThroughPgyCompilerWorld
    -> PgyCompilerWorld.source_mir
    -> DriverSourceMirExecution.ProduceSourceMir
  ```

  The previous installed CLI called `CompileSourceToMirJsonVerified` directly;
  that bypass is deleted. The full bootstrap artifact graph is:

  ```text
  driver_bootstrap_main.Main
    -> PublishSourceMirArtifactThroughPgyCompilerWorld
    -> PgyCompilerWorld.source_mir
    -> DriverSourceMirExecution.PublishSourceMirArtifact
    -> SelfMirArtifactCommitPayload
  ```

- `DriverSourceMirExecution` is the single subject/zone owner. One shared
  admission function owns subject/topology identity, pressure mode and exactly
  one typed source-to-MIR producer call. Publication is split only at the real
  authority boundary: `ProduceSourceMir` requires `io_read`; the installed CLI
  therefore does not inherit `io_write`. `PublishSourceMirArtifact` requires
  `io_read, io_write`, rejects an empty path before compilation and commits once.
  Empty-path stdout sentinels, temp-file round trips and caller-side compile or
  commit fallbacks are forbidden.
- The negative gate now follows the installed C launcher, build owner, rung-2
  `Main`, CLI, sole world materializer, world method and both subject actions.
  It uses portable `find+grep` rather than assuming `rg` exists in macOS/Linux
  CI, and it allow-lists every self-hosted `CompileSourceToMirJson*` definition
  and call site so moving a bypass into a helper cannot evade the ratchet.
- Observed focused evidence:
  - both `driver_rung2_main.pgy --ast` and `driver_bootstrap_main.pgy --ast`:
    PASS after capability separation;
  - source action/no-bypass gate: PASS;
  - recursive compiler topology: PASS;
  - compiler-world contract: PASS;
  - first full `make -j2 self-host-compiler` pressure run reached its 1,800s
    time ceiling before final install: exit 124, peak working set 1,144.1MB,
    peak private 1,198.0MB, top process `cc1.exe` 724.2MB. This is not a build
    PASS or memory failure; it is bounded evidence that the 20GB defect did not
    recur. A detached MSYS `bash -> gen1.exe` chain from this exact run was
    identified by PID/start time/command and stopped before re-entry.
  - the complete staged-array full run exited 2 after 5,101,206ms without
    installing: peak working set 1,301.8MB, peak private 1,469.2MB, top
    `gen2.exe` private 1,455.7MB, and `limit_exceeded=false`. Its 483-byte
    `driver.c` was an explicit `initializer_type_unresolved` diagnostic for
    `Clone(admitted.intent_execution_plan)`, not a memory failure.
  - `MirIntentExecutionPlan` is an admission-validated read-only struct carrier.
    A local typed binding removed the old gen2 inference error but violated the
    current compiler's borrowed-member escape rule. The final projection passes
    `admitted.intent_execution_plan` through an explicit typed value parameter;
    it neither broadens the machine receipt to `own` nor uses polymorphic
    `Clone`. The static protocol ratchet rejects a detached local, Clone, plan
    revalidation and expression-graph reconstruction at that boundary. The
    protocol wrapper has an explicit 180-line budget; the owner is 159 lines.
  - the 3,072MB-capped install-only rerun used the already-built gen2/parser
    seeds and consumed the intermediate direct-binding source, so the former
    initializer diagnostic did not recur. It was stopped by the unchanged
    pressure owner
    after 4,605,377ms: peak working set 2,820.5MB, peak private 3,072.0MB, top
    `gen2.exe` private 3,052.8MB, `limit_exceeded=true`. `driver.c` remained
    zero bytes and the installed driver timestamp did not change. This is a
    scaling RED, not an install or launcher-parity PASS. The final typed-value
    source compiled into a fresh focused `driver_rung2.exe`; the broader
    machine-layer gate then stopped at its existing producer/consumer mismatch,
    `MIR machine-layer facts are missing or invalid`.
- Current native/integration evidence after the final typed-value change:
  - `make -j2 test-mir`: `158 passed, 0 failed`; domain topology, destructure
    type, match binding and speculation-fact follow-up gates also PASS;
  - `make -j2 stdlib-test-smoke`: general stdlib C/LLVM plus HostTask lifecycle
    and typed policy C/LLVM PASS;
  - `make -j2 module-test-smoke` and explicit C/LLVM domain runtime topology:
    PASS;
  - source action, topology, compiler world, component, Pergyra likeness,
    artifact transaction, build pressure/inventory, inc sentinel, stdlib
    inventory, object/action, ABI shape, SoT/protocol registries, full UTF-8
    documentation, shell syntax and diff checks: PASS;
  - intent protocol native canonical/multi-routine + 41 mutation corpus: PASS;
    executable self admission remains explicitly BLOCKED because no current
    admission binary was supplied;
  - broader LLVM D&D campaign: C leg compiled/ran, LLVM leg is RED at
    `LLVM hosted method call argument allocation failed`. This is not counted
    as a green backend verdict or silently attributed to the current owner
    changes.
- CI run `30464053512` exposed two independent contract defects now fixed in
  the dirty tree: C-only macOS incorrectly forced the LLVM topology leg, and
  the machine-layer gate checked a moved admission term in the old owner.
  Linux parity and macOS also lacked `rg`; the portable action gate closes that
  failure. Build inventory, shell syntax, backend-selection negatives and the
  exact action gate pass locally. Full platform closure awaits the next push.
- Evidence grade remains `REACHABLE`, not `SUBSTITUTING`: this closes a real
  installed Pergyra orchestration bypass but does not replace a new C-owned
  semantic compiler path. The root compiler `intent` remains `SURFACE`.
- Next falsifier: the admitted semantic-analysis receipt/identity must cross the
  emission boundary without reconstructing the whole artifact fact surface.
  On the same composed AST, analysis construction count must be one and
  emission reconstruction count zero under the unchanged 3,072MB cap. Only
  after a fresh driver is installed may
  `examples/function_clause_order_minimal.pgy` direct/launcher byte parity be
  claimed.

## Historical checkpoint - Insere/Zeno adoption continuation

- `docs/201_insere_zeno_lineage_and_library_adoption.md` remains the canonical
  provenance/adoption contract for the user-authored `F:/insere` and `F:/zeno`.
  Those TypeScript repositories provide falsifiers and design lineage; Pergyra
  owners and executable gates remain semantic authority.
- The first Insere continuation slice is implemented on the existing
  `stdlib/host_task_slot.pgy`, not as a second scheduler. Typed
  `HostTaskApplyPolicy` and one `HostTasks.ApplyPolicy` owner distinguish
  `spawn`, `restart` and `skip`: active skip and duplicate spawn preserve the
  current generation, only restart advances it, vacant start issues the next
  generation, and malformed phase/generation fails closed. Existing `Replace`
  delegates its generation transition to restart policy.
- `tests/host_task_policy_smoke.sh` executes active/vacant/malformed and stale-
  ticket cases through stable `use host_task_slot;` on C and LLVM. The aggregate
  `make stdlib-test-smoke` keeps the unrelated all-module surface fixture
  separate, then runs both the legacy slot and policy gates. This avoids making
  a mixed-module namespace-lowering limitation part of HostTask semantics.
  Focused C/LLVM policy and legacy slot gates, stdlib inventory, object/action
  contract, documentation quality, shell syntax and diff check were observed
  green by the implementation slice.
- This Insere policy is pure immutable admission, so it remains enum/class/func
  rather than ceremonial subject/action/intent or detached `tobject` receipt.
  Without a real host adapter consuming the decision it is `REACHABLE`, not
  `SUBSTITUTING`.
- The completed Zeno-derived baseline remains `SnapshotTicket` plus
  `BinaryProjectionPreflight`: runtime slot generation, existing MIR ABI layout
  identity and explicit endianness are admitted without recalculating offsets
  or defaulting host endian. Its current grade is also `REACHABLE`; normalized
  manifest inspect/diff and a real receipt-consuming binary boundary remain
  the next production falsifiers.

## Previous checkpoint - source-to-MIR world/action reachability

- Exact working base is `ab51d69bff88bd433405461aefdea76031155ccd` on
  `main`. The tree is intentionally dirty for this checkpoint; verify the final
  revision and clean state after commit rather than treating this snapshot as
  Git authority.
- Active seam: production `--emit-mir-json-verified` orchestration. Existing
  typed lexer/parser/semantic/DIR/MIR functions retain semantic fact ownership;
  `DriverSourceMirExecution.EmitSourceMir` owns request/identity admission, one
  payload-owner call, one atomic commit, and the typed outcome.
- Actual call graph:

  ```text
  driver_bootstrap_main.Main
    -> EmitSourceMirThroughPgyCompilerWorld
    -> PgyCompilerWorld.EmitSourceMir
    -> PgyCompilerWorld.source_mir
    -> DriverSourceMirZone.execution
    -> DriverSourceMirExecution.EmitSourceMir
    -> CompileSourceToMirJsonVerified | CompileSourceToMirJsonPressureObserved
    -> SelfMirArtifactCommitPayload
  ```

- `PgyCompilerWorld` has exactly two ordered executable fields: `direct_mir`
  first and `source_mir` second. One
  `PgyCompilerWorldMaterializeExecutableZones` owner constructs both; no second
  world or partial aggregate materializer exists.
- Deleted bypass: `CompileSourceToMirJsonFileVerified` and
  `CompileSourceToMirJsonFilePressureObserved` definitions/calls are absent.
  `Main` no longer compiles or commits the source-to-MIR artifact directly.
- Evidence grade is `REACHABLE`, not `SUBSTITUTING`. The production caller
  invokes the action and consumes its typed outcome, but this replaces a
  Pergyra-internal file-helper orchestration path, not a new C-owned semantic
  compiler path. `CompilePergyraProgram` remains `SURFACE`; source-to-C and
  general MIR-to-C still use direct orchestration.
- Observed gates at this checkpoint:
  - `driver_source_mir_execution_action_gate.sh`: PASS;
  - `build_pressure_contract_smoke.sh`: PASS;
  - `self_host_compiler_topology_smoke.sh`: PASS;
  - `self_host_compiler_world_contract_smoke.sh`: PASS;
  - `self_hosted_component_contract_smoke.sh`: PASS;
  - `self_host_hard_contract_smoke.sh`: PASS;
  - `self_host_substitution_velocity_smoke.sh`: PASS; nine blockers remain
    explicit (five direct, four process/evidence);
  - `self_host_pergyra_likeness_smoke.sh`: PASS with 20 resource zones, two
    world members, and 28 zone-bound transitions;
  - `self_host_progress_metric_smoke.sh`: PASS; implementation volume
    `17.89%`, default native replacement `0%`, explicit DRV-2 `live`;
  - `build_source_inventory_smoke.sh`: PASS, including macOS Bash 3.2
    portability for the new action gate;
  - `make -j2 test-mir`: PASS; MIR suite `157 passed, 0 failed`, followed by
    domain-topology, destructure-type, match-binding, and speculation-fact
    gates. The previously missing `mir_lower_request` and declaration-method
    validator link owners are now present;
  - `doc_link_checker_parity.sh`: PASS for C/LLVM artifact equality and the
    synthetic dead-link negative after refreshing the `docs/INDEX.md` census
    golden to `173` total links and `168` Markdown links;
  - `intent_compression_contract_smoke.sh`: PASS after binding on-receiver
    inference and diagnostics to their split inference/type/sequence owners;
  - `evidence_guard_amortization_smoke.sh`: PASS with the default 50,000,000
    iterations; best preflight/per-access ratio `0.200` and best cached
    preflight/repeated-preflight ratio `0.174`. Generated secure MIR C uses the
    typed `pgy_secure_pin_read_init_Int` ABI and rejects the old return-value
    call shape;
  - `perf_contract_smoke.sh`: PASS; the measured C compile was `301ms` and the
    static contract now follows the split LLVM enum-constructor, C constructor
    argument, and typed pin-init owners;
  - `make -j2 callable-contract-vocabulary-test-smoke`: PASS with the exact
    Make-built `PGY_BIN`. `build_source_inventory_smoke.sh` ratchets both that
    binary identity and the shared Windows path helper;
  - production `driver_bootstrap_main.pgy --ast`: PASS.
- Runtime evidence is not yet claimed. The current falsifier is
  `examples/function_clause_order_minimal.pgy`: it must traverse the production
  action, preserve bounded native/self MIR and C/LLVM parity, and reject wrong
  pressure mode, subject identity, topology identity, or artifact identity
  before publication.
  Two bounded C prerequisites were also attempted: direct production-driver
  build and split file C emission both timed out at 120 seconds with `rc=124`.
  Their logs contained `0 error(s), 0 warning(s)` but no requested executable/C
  artifact; both logs hashed to
  `1a9ded083816fe692fbfc6a0dafe1f90a7e40e4655706a8a0518e20eab74e3a8`.
  Fixture execution and LLVM therefore did not start. No compiler worker
  remained after timeout, and no memory verdict is inferred from these runs.

- Previous GitHub run `30454762165` at the working base exposed additional CI
  defects that are not self-host substitution evidence: the shortened
  `test_mir` link omitted `mir_lower_request.o` and
  `mir_decl_header_method_validate.o`; the region unit omitted
  `ast_async_lambda_accessors.c`; and the doc-link expected artifacts still
  described the older index census. Those three owner/inventory defects are
  fixed and locally falsified in this checkpoint. The same run's stale intent,
  typed pin/evidence, perf split-owner, and Windows callable-vocabulary gates
  are also fixed locally. The complete Windows preparation target advanced
  through those gates and into the long component contract, but the bounded
  local run ended at its 180-second ceiling; no full platform PASS is claimed
  until the next pushed CI result is observed.

## Historical checkpoint - Insere/Zeno three-track reachable slices

- Exact working base is `6e1891f54aa7770880ae1b89276adc90895b61b7` on
  `main`. This checkpoint integrates 17 explicitly named paths; no broad
  `git add -A` or glob staging is permitted.
- Objective and owners:
  - `HostTaskSlot` owns stable host-task key, generation, lifecycle phase, and
    guarded wait/final/cleanup transitions. The host adapter is the last
    consumer; key-only commit/delete is forbidden.
  - `SnapshotTicket` immutably binds slot id/generation, existing MIR ABI
    layout identity, and explicit endianness. Runtime `SlotHandle` generation
    and `MirAbiLayoutIdFromCapture` remain the semantic owners.
  - `BinaryProjectionPreflight` is the sole receipt-admission owner. It consumes
    the existing layout identity and must not recalculate offsets or default
    endianness.
- Current grade for all three tracks is `REACHABLE`, not `SUBSTITUTING`.
  `HostTaskSlot` is a completed active official-library slice; SnapshotTicket
  and BinaryProjection are completed internal library/tooling slices. None
  deletes or replaces a C-owned production compiler path, so this work earns
  no hard self-host progress credit.
- Exact focused evidence observed:
  - `PGY_HOST_TASK_SLOT_BACKENDS=c bash tests/host_task_slot_smoke.sh`: C
    compile/run PASS;
  - `PGY_HOST_TASK_SLOT_BACKENDS=llvm bash tests/host_task_slot_smoke.sh`: LLVM
    compile/run PASS;
  - `PGY_BIN=bin/pgy.exe bash
    tests/self_hosted/parity/binary_projection_preflight_probe_parity.sh`: C
    compile/run PASS, LLVM compile/run PASS, and output parity PASS;
  - `bash tests/self_hosted_scaffold_smoke.sh`: `35 tool(s) gated`;
  - `bash tests/stdlib_inventory_smoke.sh`: inventory/contracts PASS.
- Verification scope is intentionally bounded. The full stdlib surface matrix,
  full self-host parity matrix, CI/platform matrix, and production compiler
  bootstrap suites were not run for this checkpoint.
- Next falsifiers:
  1. a real host adapter must retain the existing task/future handle beside a
     `HostTaskTicket`, re-read the current slot before publish/cleanup, and
     delete every key-only direct commit/delete path;
  2. a public `Slot<T>` generation view may be designed only when a real
     workload proves the need; the current internal ticket protocol must not
     infer or refresh generation;
  3. normalized manifest tooling must derive from the existing MIR ABI tuple,
     reject same-name offset/endian changes, and then a real binary boundary
     must reject receipt-less direct open/truncate/read.
- At checkpoint close, the 17 intended paths are explicitly staged, with no
  unexpected temporary path, compiler output, or binary artifact included.
  Commit remains intentionally pending.

## Historical checkpoint - unified CI recovery and next Pergyra-native rung

- Exact clean base is `0b848787245b1272334c5fd9ef503b988d0ff6b2` on
  `main`, equal to `origin/main` when this checkpoint was written. The
  pre-stage audit recorded 15 tracked modifications, 2 untracked paths, and
  0 staged paths. All 17 paths are now explicitly staged, with no unstaged or
  untracked paths. They are user-approved integration candidates; the three
  driver parity owners and
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md` are no longer protected
  exclusions. This authorization does not replace diff and gate review before
  one intentional commit.
- The actual production `Main` call graph reaches `PgyCompilerWorld` only for
  `--mir-json-backend=c|llvm`:

  ```text
  driver_bootstrap_main.Main
    -> EmitDirectMirThroughPgyCompilerWorld
    -> PgyCompilerWorld.EmitDirectMir
    -> PgyCompilerWorld.direct_mir
    -> DriverRung2DirectMirZone.execution
    -> DriverRung2Execution.EmitDirectMir
  ```

  That world/zone/subject/action path is `REACHABLE`, not `SUBSTITUTING`: it
  replaces a Pergyra `Main` orchestration bypass but does not itself replace a
  C-owned compiler semantic path. The compiler-root canonical real-purpose
  `intent` remains `SURFACE` because production calls none of its imported
  intent declarations.
- Exactly two bounded input-feature slices currently count as true
  `SUBSTITUTING` dogfood:
  1. source -> admitted MIR -> general C binding-slot admission/runtime, which
     consumes exact binding constructors and projection assignments;
  2. admitted v2 typed intent-transition MIR -> self C, which replaced the old
     typed direct/rollback consumer.

  Neither slice makes the whole compiler root, direct-MIR world, or released
  compiler self-hosted.
- Remaining C-owned and incomplete boundaries are explicit:
  - the released/default `pgy` and frozen recovery/oracle seed remain C-owned;
  - production source-to-C, source-to-MIR, and general MIR-to-C modes still
    enter the direct `CompileSourceTo*` / `CompileMirJsonToC*` orchestration;
  - C and LLVM are parallel projections of admitted facts, not an old/new
    fallback chain. Their current parity evidence is bounded to named slices;
    general source -> LLVM through the Pergyra-native compiler root and full
    self-host backend closure remain incomplete;
  - full Stage 2/Stage 3 fixed-point convergence, frozen-seed provenance, and
    the remaining role/domain runtime plan are open.
- The next executable rung is source -> MIR orchestration through the existing
  `PgyCompilerWorld` composition boundary and one compiler-run
  zone/subject/action. Typed lexer/parser/semantic/DIR/MIR `func` owners retain
  pure computation; the action owns admission, one verified MIR artifact
  commit, and the typed outcome. The same rung must delete the production
  `Main` bypasses `CompileSourceToMirJsonFileVerified` and
  `CompileSourceToMirJsonFilePressureObserved`, with no compatibility or
  failure fallback beside the action. The first falsifier is
  `examples/function_clause_order_minimal.pgy`.
- Current CI recovery facts:
  - the explicit MIR object inventory now includes
    `mir_resource_runtime_population`, `mir_decl_header_methods`,
    `mir_intent_step_emit`, and `mir_intent_execution_graph`; the region-escape
    unit build also names the parser constructor/accessor objects it consumes;
  - method metadata preserves the semantic return-type name when there is no
    direct AST return-type node, and the declaration inventory pins that owner;
  - the LLVM MIR region scope now records its owning function, rejects
    cross-function/ambient scope state, and has a focused static owner gate;
  - CFG, MIR inventory, semantic function-table, and the three user parity
    assertion edits are integrated with the current split owners and exact
    diagnostics; the user-authored C oracle/bootstrap contract is in the same
    unified review scope;
  - generated production-header inventory is 716 headers; the formal header
    parity and the changed method translation-unit/ABI shape checks passed;
  - the self-host backend AIR checker generated the clean artifact at 793
    backend C/H files, 12 forbidden terms, 0 hits, and no findings. Its focused
    C/LLVM parity, negative leg, `bash -n`, and `git diff --check` passed.
- OPEN evidence is not runtime green:
  - `semantic_function_table_owner_smoke.sh` was corrected after one failed
    static run and has not been rerun in this checkpoint;
  - `cfg_body_dataflow_smoke.sh` was corrected after its stale split-file path
    failed and has not been rerun;
  - the three driver parity-owner edits are integrated but their executable
    parity legs have not been rerun in this checkpoint;
  - `mir_declaration_inventory_smoke.sh` reached its bounded timeout; no
    executable PASS is claimed;
  - the region Make target was unavailable in the observed shell and a direct
    GCC link probe ended at `collect2` rc5; the new region owner gate is static
    evidence, not a runtime fixture result;
  - the build-source inventory attempt encountered a missing child `make`.
    These are exact open/environmental results, not reasons to weaken owners or
    call the affected runtime gates green.
- The proposed test-header consolidation remains blocked at the exact include
  sentinel `src/test_mir.c:883`, which still includes
  `tests/mir/test_mir_lowering_part_c_3.cases.h`. The 187-line `part_c_3` may be
  moved behind the 146-line `part_c_2` owner as a 333-line unit only in one
  coordinated change that preserves case identity, removes that include,
  deletes `part_c_3`, and reruns the focused MIR inventory/unit gate. No delete
  or include removal has occurred in this checkpoint.
- Dirty-path audit before staging: 5 build/compiler source
  paths, 2 generated clean artifacts, 7 test/gate paths, and 3 documentation
  paths. There were no temporary paths, build products, or binary artifacts in
  the dirty inventory. All 17 paths were added with explicit path arguments;
  the staged tree now has 17 paths and the unstaged/untracked tree has 0.
  Commit remains intentionally pending.

## Post-a54 CI closure follow-up checkpoint

- Base and pushed checkpoint: `a54ae6e78321d39494f50d3145795dac63b12714`
  on `main`, pushed to `origin/main`. The local CI-closure fixes described in
  this section are dirty follow-up work and are not included in that revision.
- GitHub Actions run `30438997058` was still incomplete at the captured
  snapshot: 15 jobs succeeded, 12 failed, 1 was cancelled, and 1 was still
  running. That remote run tests `a54ae6e78321d39494f50d3145795dac63b12714`;
  it does not contain the local fixes below.
- Six locally identified root causes and closures:
  - the HIR region-escape validator was present in the source inventory but its
    object was missing from `HIR_CORE_OBJECTS`;
  - `APPLY_EFFECT` must consume the dedicated effect-pool identity instead of
    reconstructing or borrowing another domain identity;
  - function entry must reset the active region state and restore the prior
    state on exit so one function cannot inherit another function's region;
  - callable `Option<Self>` / `Result<..., Self>` readiness is valid while
    recursive aggregate layout remains a cycle and must still fail closed;
  - generic specialization must consume the generic-binding SoT, and an
    unbound generic base must not bypass specialization binding;
  - the backend fail-closed gate must assert typed `PinReadInit` /
    `PinWriteInit` rows rather than the obsolete raw `PinRead` / `PinWrite`
    spelling.
- Observed local evidence is limited to strict translation-unit compilation and
  focused static-owner/gate checks, all of which passed. The seven fresh
  executable fixtures were not run to completion because GCC attempted to use
  protected Windows temporary storage and the bounded aggregate run reached
  120 seconds. No executable fixture PASS is claimed.
- The backend fail-closed literal audit resolved 543 direct or simple-loop
  positive/negative pairs with 0 missing positives and 0 present forbidden
  literals. Its compound and derived lookup assertions were also checked, and
  `bash -n` plus `git diff --check` passed. This is static evidence only; the
  full executable `backend_fail_closed_smoke.sh` is not recorded as PASS.
- Next falsifier: rebuild the same revision and run exactly
  `zone_effect_pool_runtime`, `forward_ability_order`,
  `class_bump_option_match`, `generic_class_method`,
  `result_chained_method_class`, `result_class_chain_score`, and
  `option_class_self_method`, then run self-host parity, ASan, and the platform
  matrix against that same revision.
- Four protected paths remain outside this follow-up's scope and must not be
  staged or overwritten:
  `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`,
  `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`,
  `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`, and
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.

## Current resume checkpoint - integrated SoT and self-host closure audit

- Exact checkout base is `afefd1a80c25a91ee3557bd798b9c68d4e8f65a9` on
  `main`, equal to `origin/main` when this checkpoint was recorded. Immediately
  before the integrated gate run, the shared tree contained 90 tracked changes,
  49 untracked paths, and 0 staged paths. This is a shared dirty tree; do not
  treat the counts as an invitation to stage or discard unrelated work.
- Four pre-existing protected paths remain outside this checkpoint's ownership:
  `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`,
  `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`,
  `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`, and
  untracked `docs/self_hosted/18_c_oracle_bootstrap_contract.md`. They were not
  modified, staged, or deleted by this work.
- Objective card:
  - objective: keep program-wide MIR fact validation behind one named owner
    while preserving `mir_validate` order and fail-closed behavior, then record
    the actual production self-host reachability rather than syntax counts;
  - priority: semantic identity and one SoT, exact validator order, old-owner
    removal, negative owner gate, then file-size and build-inventory closure;
  - fact owner: `mir_program_fact_validate.c` owns program/routine inventory,
    receiver-carriage, fallback, resource-flow, parameter-flow, and loop-flow
    validators; `mir_validate` is the last orchestration consumer;
  - forbidden fallback: duplicated validators in `mir_program_validate.c`, a
    skipped/overwritten failure return, AST/source topology rereads, or treating
    reachable declarations as substituting execution;
  - falsifier: a freshly rebuilt current `test_mir` must reject all five damaged
    topology identities with a `domain topology row` diagnostic, and the
    component contract must complete against the same current tree.
- Active executable rung and dogfood grades:
  - production entrypoint remains `driver_bootstrap_main.pgy#Main`;
  - only `--mir-json-backend=c|llvm` reaches
    `Main -> EmitDirectMirThroughPgyCompilerWorld ->`
    `PgyCompilerWorld.EmitDirectMir -> PgyCompilerWorld.direct_mir ->`
    `DriverRung2DirectMirZone.execution ->`
    `DriverRung2Execution.EmitDirectMir`;
  - that direct-MIR world/zone/subject/action path is `REACHABLE`; native plan
    execution is also `REACHABLE`; the admitted v2 input-language typed
    MIR-to-self-C transition is the bounded `SUBSTITUTING` slice;
  - source-to-C, source-to-MIR, and general MIR-to-C still use their direct
    `CompileSourceTo*` / `CompileMirJsonToC*` orchestration paths. The canonical
    compiler-purpose root `intent` is therefore still `SURFACE`, not executable
    dogfood. Its next rung must run one real compiler purpose through production
    and delete the named direct bypass; importing or statically gating an intent
    is not a substitute.
- Current structural evidence:
  - the program-validation owner split leaves the top-level call order unchanged,
    keeps `mir_domain_topology_validate` ahead of the moved validators, and
    preserves an immediate `false` return for every failed owner call;
  - strict `gcc -pipe` compilation of both translation units and `ld -r` partial
    link passed; `mir_program_fact_validate_owner_smoke.sh` passed;
  - the current owner-cap audit reports 0 violations, and both the explicit Make
    source inventory and `test_inc_size` passed.
- Exact integrated gate observation: 11 gates passed:
  `build_source_inventory` (explicit Make inventory), `test_inc_size`,
  `mir_program_fact_validate_owner`, `mir_json_expression_graph_owner`,
  `mir_lowering_api`, `abi_ownership_shape`, `memory_string_safety`,
  `dir_domain_identity`, `domain_runtime_topology`,
  `self_host_program_graph_unification`, and `self_host_hard_contract`.
  Existing observed suites remain AIR 144 passed / 0 failed and semantic 2,823
  passed / 0 failed; these were not rerun as part of the 11-gate observation.
- Memory-pressure cause and operating rule:
  - the 20+ GiB observation was aggregate system pressure, not one compiler
    process: six reparented/orphan native workers overlapped after the same
    whole-graph gate was restarted before its prior process tree ended;
  - the earlier roughly 3 GiB defect repeatedly validated/materialized a
    whole-program graph for consumer/local rows where the owner needed one
    bounded validation. Normal observed self-host pressure is roughly
    1.1-1.5 GiB;
  - run only one bounded whole-graph gate at a time, capture wrapper and
    descendant PIDs/command lines, and wait for the complete process tree to
    terminate before another run. Do not infer a single-process peak from
    aggregate Task Manager memory.
- OPEN and environmental evidence:
  - `mir_declaration_inventory` static full audit passed: 3,051 resolved pairs,
    2,976 unique owner paths, and 0 missing/rejected paths; `bash -n` and diff
    checks also passed. Its executable gate was run once and ended rc124 at the
    120-second bound with no orphan worker remaining; no executable PASS is
    claimed;
  - `mir_function_param_flow_summary` reached the pre-existing
    `bin/test_mir` result 155 passed / 1 failed. That PE predates current
    `src/test_mir.c`, both split sources, and their build objects, so it is stale
    baseline evidence, not a current split regression or current gate result;
  - `mir_param_carriage` could not execute its `pgy` artifact: Git Bash saw an
    ELF binary and returned rc126, while the default WSL route had no `bash` and
    returned rc127;
  - `self_hosted_component_contract` timed out at 120 seconds with no output.
    A prior PASS exists, but no current PASS is claimed;
  - the broad Make runtime target attempted unrelated recompilation and was
    stopped at 120 seconds. Plain GCC also attempted protected `C:\Windows`
    temporary storage; focused `gcc -pipe` translation-unit checks are the
    observed evidence instead.
- Resume with one fresh current-tree `test_mir` rebuild, then run the five
  topology-identity mutations (zero identity, foreign-valid identity, wrong
  field kind, stray unused participant identity, and unknown zone owner) and
  require the owned diagnostic. After that, run
  `self_hosted_component_contract_smoke.sh` once under a bounded process-tree
  observation. Do not restart either graph while a prior worker tree exists.

## Current resume checkpoint - self-host consistency closure

- Consistency landing: `1044e3eef0ed3f11c6025a43b9d130d6eca47ddb` on
  `main`. Its verified baseline was
  `fbc728f8ac34eed393e97e639376dc767bbdcdd6`, matching `origin/main` before
  this session. Verify the remote tip before resuming.
- Objective card:
  - objective: close the shared CI inventory/fixture drift and the first real
    self-host codegen bootstrap type-owner failure without adding a source-text,
    constructor-name, backend, or compatibility fallback;
  - priority: existing semantic owner, exact failure identity, focused negative
    gate, generated-owner refresh, then broad CI throughput;
  - fact owner: expression-graph array-literal typing owns the contextual
    `Array<T>` fact; the nominal constructor checker is its last consumer;
  - forbidden fallback: constructor-name exceptions, source/AST text reparsing,
    relaxed assignability, stale generated inventory, or fixture-only parity;
  - falsifier: `AstExpressionGraphRows(true,
    [TypedAstKindBareCallStmtTag()], ...)` must infer `Array<Int>`; a malformed
    element must still fail at nominal argument typing.
- Closed consistency seams:
  - the backend comparison inventory now includes the four valid positive cases
    `list_literal_context`, `region_user_callee`,
    `region_user_callee_bad`, and `zone_layer_projection_state_alias`;
  - the language keyword implementation inventory was regenerated from its owner;
  - AIR validator fingerprint drift was reproduced twice as deterministic
    (`17936981139362554101` -> `11564967125245077598`), the owner fixture was
    refreshed, and live-drift parity passed;
  - expression-graph field typing now consumes the existing array-literal owner,
    and nominal-constructor diagnostics expose constructor and argument index;
  - dogfood grade, intent semantics, mir.execution_graph consumers/fallbacks, and
    the sole typed-intent machine admission boundary were reconciled across the
    project rules, design docs, registry, and OWNERS map.
- Exact observed gates:
  - backend inventory/syntax and the four focused C/LLVM cases: PASS;
  - `region_backend_wiring_smoke.sh`, keyword registry, VS Code graph, AIR JSON
    parity, intent protocol static owner, SoT authority edge, documentation
    quality, and aggregate field policy C/graph parity: PASS;
  - corrected gen0 emitted a 55,720-line gen1 C artifact and GCC compiled it; the
    formal seed script independently regenerated and compiled 2.7 MiB
    `gen1.c`/2.0 MiB `gen1.exe`, passing the former node-32501 boundary.
- Exact OPEN evidence:
  - the formal seed run was stopped during gen2 emission after 20 minutes, so it
    is not recorded as seed PASS; complete gen2/fixpoint remains the next broad
    bootstrap gate;
  - `self_hosted_component_contract_smoke.sh` currently stops on the pre-existing
    `codegen_bootstrap.sh` size cap (617 lines versus 600), not on this owner
    change;
  - the production compiler root still needs one canonical real-purpose Pergyra
    `intent` to replace a named direct orchestration bypass.
- Preserved concurrent/user work remains the three driver parity scripts and
  untracked `docs/self_hosted/18_c_oracle_bootstrap_contract.md` listed below; do
  not stage or overwrite them.

## Current resume checkpoint - admitted typed intent self execution

- Implementation landing: `bf55972ba6492074a4d829bbc1fa704b90e85c78` on
  `main`, pushed to `origin/main`.
- Consistency baseline verified before this session:
  `fbc728f8ac34eed393e97e639376dc767bbdcdd6` on `main` at `origin/main`.
  The remaining pre-session dirty state is exactly the three protected parity
  scripts and untracked bootstrap contract listed below.
- Canonical meaning:
  - `intent` is defined by `docs/01_intent_first_design.md` and
    `docs/173_intent_axis_strengthening.md`: it closes one real-world purpose
    and elaborates a participant/coordination/authority/effect/boundary/
    compensation/trace fact bundle into the verification plane;
  - action count is neither necessary nor sufficient, and this execution plan
    is only a bounded coordination/boundary/compensation projection;
  - `tobject` owns detached immutable payload shape. It does not own intent
    identity, authority, predecessor topology, completion, or compensation.
- Objective card:
  - objective: replace the self-host typed intent MIR-to-C direct/rollback path
    with one admitted v2 execution plan and exact payload identities;
  - priority: semantic identity and one admission, exact enum/variant/tobject
    joins, explicit predecessor/completion evidence, fallback deletion,
    negative ratchet, then projection size;
  - fact owner: native `MIRIntentExecutionPlan` owns the wire plan;
    `MirIntentExecutionPlanReady` is the sole self admission boundary; admitted
    self projections are consumers rather than second semantic authorities;
  - last legitimate consumer: the production self C plan emitter reached from
    `driver_rung2_owner.pgy`;
  - forbidden fallback: v1/name-only payload joins, Bool outcome collapse,
    source/AST/row-order recovery, consumer readiness/digest checks, expression
    graph reconstruction, all-earlier-step rollback, and typed direct emission;
  - falsifier: cross-wire a valid routine/action/enum/tobject/instruction or
    persisted graph identity and reject before any partial C artifact.
- Landed executable slice:
  - native AST->semantic->DIR->MIR->JSON carries exact success/failure and
    terminal payload declaration syntax IDs under
    `pgy.selfhost.mir-intent-execution-plan.v2`;
  - one machine admission validates schema, digest, topology, exact routine/
    action/enum/tobject/instruction joins, and sealed plan-owned expression
    graphs, then supplies a typed carrier to consumers;
  - production self C codegen consumes the admitted plan; old typed direct and
    rollback bypasses are removed and statically forbidden;
  - assignment instructions now carry exact binding mode (`default_param`,
    `inout_param`, `own_param`, `ref_param`, `local`, or `owner_field`) through
    the native MIR boundary instead of being re-decided by self codegen;
  - zero-compensation topology is represented only by an unreachable empty
    scaffold count owned by structure validation; consumers do not infer
    meaning from block IDs or row positions.
- Exact observed evidence:
  - fresh native compiler build and `test_mir`: `157 passed, 0 failed`;
  - native typed transition C/LLVM execution: PASS;
  - v2 canonical digest `1268084794`, multi-routine digest `1173492658`, and
    41 protocol/schema/identity/topology mutations: PASS/fail-closed as expected;
  - fresh Pergyra-built driver
    `.tmp/self_hosted/intent_typed_compensation_final9_20260729_090823_335/driver_rung2_1108.exe`:
    build PASS with 0 Pergyra errors/warnings (the generated C compiler retained
    the known unused match-binding warning);
  - `intent_typed_outcome_compensation_owner.sh`: PASS for success, failure A/B,
    predecessor-only reverse compensation, multiple/duplicate expression, and
    zero compensation; malformed digest/graph/target/scaffold variants reject
    before partial C;
  - canonical and multi-routine self C compile/runtime output: exact parity;
  - self-host component contract: PASS;
  - two fresh driver observations stayed at about 1,530-1,531 MiB aggregate
    private (`pgy` about 791 MiB, `cc1` about 739 MiB, `gcc` about 1 MiB), not
    the historical 20+ GiB repeated-graph-validation symptom.
- Grade:
  - bounded input-language typed intent MIR-to-self-C is `SUBSTITUTING` because
    a Pergyra implementation now replaces the real old consumer path;
  - native plan execution remains `REACHABLE` evidence;
  - compiler organization `intent` remains `SURFACE`: the production compiler
    root still does not call a canonical real-purpose intent.
- Next executable falsifier: use the canonical intent docs to identify one
  actual compiler purpose and its full fact bundle, make the production root
  reach that Pergyra intent, and delete exactly one current direct orchestration
  bypass. Do not invent an intent from stage count or use `tobject` as topology.
- Preserved concurrent/user work: do not stage or overwrite
  `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`,
  `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`,
  `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`, or
  untracked `docs/self_hosted/18_c_oracle_bootstrap_contract.md` without first
  reconciling their separate owner/task.

## Current resume checkpoint - native typed intent plan execution

- Landing parent: `ff7de53c01bbaf6831641a7d6ec52b2dd58c4ec5` on `main`.
  Verify the final landing revision and `origin/main` before resuming.
- Objective card:
  - objective: execute source-declared typed intent outcomes through one exact
    MIR plan while keeping `tobject` limited to detached receipt/problem values;
  - priority: exact routine and declaration identity, enum/variant/payload
    identity, explicit predecessor, success-only completion, one plan owner,
    native dual-backend parity, then self admission and production substitution;
  - fact owners: enum/tobject declarations own payload shape, semantic owns exact
    action/variant/payload resolution, DIR owns step/predecessor identity,
    `mir.intent_step_transition` owns branch/completion/compensation facts and
    `mir.intent_terminal_transition` owns typed exits;
  - last legitimate consumer: target-specific C/LLVM projection of one validated
    `MIRIntentExecutionPlan`;
  - forbidden fallback: Bool collapse, variant/name/type inference, source or row
    order predecessor recovery, call-implies-completed, first-compensation-only,
    AST/source rescan, or making `tobject` own authority/topology/control flow;
  - falsifier: success, failure A and failure B return distinct exact payloads;
    B failure compensates completed A and not B; multiple compensation runs in
    reverse order; cross-wired identities reject before emission.
- Landed executable slice:
  - native MIR carries exact intent return types and stable declaration syntax
    IDs, then produces and validates explicit step/terminal transition blocks;
  - MIR JSON projects `pgy.selfhost.mir-intent-execution-plan.v1` with a nonzero
    digest, exact predecessor identity, branch payload definitions, completion,
    compensation and terminal rows;
  - native C and LLVM consume the MIR plan directly and typed mode does not fall
    through to the legacy Bool emitter;
  - self DIR/MIR now preserve exact `legacy_bool` versus typed result signatures,
    and the in-memory execution owner is split into schema, digest and fact
    responsibilities.
- Exact observed evidence:
  - integrated LLVM-enabled compiler rebuild: PASS with no warnings;
  - `intent_typed_transition_native_execution_smoke.sh`: PASS for native C/LLVM
    success, failure A, failure B and reverse multiple-compensation order;
  - full `test_mir`: 157 passed, 0 failed, including variant, payload type,
    action identity, predecessor, completion and terminal-variant mutations;
  - `intent_typed_transition_frontend_owner.sh`: PASS;
  - `intent_execution_fact_contract_owner.sh`: PASS;
  - `intent_result_signature_carriage_owner.sh`: PASS with the guarded prebuilt
    self driver;
  - `match_binding_type_fact_smoke.sh`: PASS after exact-empty domain-runtime
    normalization; a stray non-empty runtime row rejects before partial AST;
  - `tobject_boundary_execution_owner.sh` and
    `object_action_boundary_contract_smoke.sh`: PASS.
- Exact OPEN boundary:
  - self top-level MIR JSON indexing/admission does not yet cache and cross-seal
    the typed routine return and `MIRIntentExecutionPlan`;
  - admitted self C therefore does not yet consume this plan, and the production
    bootstrap entrypoint has not replaced its C-owned direct orchestration path;
  - Coq step/terminal transition facts remain absent.
- TObject implementation truth: it is the right value carrier for detached
  immutable action receipts/problems. It is not the owner of step identity,
  predecessor topology, authority, completion, compensation order or freshness.
  Canonical method-free enforcement and complete bare/nested/indexed immutable
  write closure remain separate semantic debt.
- Grade: native executable plan/carriage is `REACHABLE`; compiler intent remains
  `SURFACE` for hard self-host scoring because no Pergyra implementation has yet
  replaced the production C-owned entrypoint.
- Next falsifier: the self top-level JSON reader must admit this exact plan once,
  cross-seal the routine result signature, and drive admitted self C through the
  success/failure/multiple-compensation gate without a source or native graft.
- Preserve and do not stage the concurrent edits in
  `driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`,
  `driver_rung2_owner_field_parity_owner.sh`, or untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.

## Current resume checkpoint - executable intent phases and ordered compensation

- Implementation landing: `4b7b6faeb46db975e107d52491ee5ee53c5c881e` on
  `main`. Its parent was `6195ed4f3e82e5f4ca5e41394631f2e940162057`,
  which matched `origin/main` before this session's push.
- Snapshot dirty state after the implementation landing contains only concurrent
  work that this slice did not stage: three tracked parity scripts
  (`driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`) plus untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.
- Objective card:
  - objective: replace the temporary self-host fail-close for
    `guard`/`post`/`compensate` with lossless parser→DIR→MIR→admitted general C
    execution while preserving `tobject` as payload rather than graph owner;
  - priority: exact step/node/graph identity, ordered phase carriage, explicit
    malformed-carrier failure, direct/admitted parity, native C/LLVM runtime
    parity, then the typed transition rung;
  - fact owner: parser owns singleton/ordered surface rows and expression graphs,
    DIR owns exact step/node/range carriage, MIR owns phase wire identity, and
    intent codegen owns completion/failure/compensation control flow. `tobject`
    owns only detached immutable receipt/failure payload;
  - last legitimate consumer: admitted general self C and native C/LLVM intent
    emitters;
  - forbidden fallback: silent clause deletion, AST/source rescan, phase or
    predecessor recovery from source position/text, result/type hidden on a
    non-`on` row, `tobject` authority/rollback ownership, or a second direct
    orchestration path;
  - focused gates: `intent_outcome_frontend_parser_owner.sh`,
    `intent_phase_carrier_negative_owner.sh`,
    `intent_guard_post_compensation_execution_owner.sh`, and
    `intent_typed_outcome_execution_owner.sh`.
- Implemented carriage:
  - parser rejects duplicate `guard`/`expect`/`post`, preserves ordered multiple
    `compensate` rows, and gives every clause its expression graph;
  - DIR preserves exact guard/expect/post node IDs, compensate range/order and
    typed-AST child census;
  - MIR emits `IntentCheck(guard|expect|post)` and
    `IntentEval(compensate)`, with result/type allowed only on `on`;
  - mir_lower rejects unknown/orphan/wrong-step-or-slot phase, duplicate
    singleton/on, check/compensate result-type contamination, on result/type
    asymmetry and missing graphs before partial C.
- Implemented runtime meaning follows the existing native contract: an action is
  marked complete before `guard -> expect -> post`. Predicate failure therefore
  compensates the current completed step; completed steps run in reverse order
  and each step's compensate rows run in reverse order. Success compensates
  nothing, and first-step failure excludes all future steps and their cleanup.
- Observed gates with the same already-built current-source self driver:
  - parser lossless on-binding AST + duplicate/ordering negatives: PASS;
  - phase order + 9 admitted-MIR mutations with no partial C: PASS;
  - success, first-step guard failure, and second-step guard/expect/post failure,
    direct/admitted byte parity, ordered compensation and native C/LLVM/self
    runtime parity: PASS;
  - enum<tobject> exact-once binding + MIR negatives: PASS;
  - component contract and object-to-action boundary contract: PASS;
  - SoT authority edge: `61 authorities`, `64 derived carriers`,
    `CLOSED=34`, `BRIDGE=27`, `ACTIVE=0`;
  - build-source inventory and documentation quality: PASS.
- Grade: the bounded input-language intent phase/compensation slice is
  `REACHABLE`. Production bootstrap still calls no compiler intent, so compiler
  organization `intent` remains `SURFACE` and this is not hard self-host
  substitution progress.
- Known blockers/debts:
  - current self machine admission requires a non-empty admitted domain runtime
    plan; empty legal topology remains a separate blocker. The runtime fixture's
    object/tobject refresh/publish scaffold satisfies that admission boundary and
    does not make tobject the rollback owner;
  - mir_lower still joins an `on` carrier to its executable row through global
    expression-text equality, so identical action text in different steps is the
    next stable-identity negative debt;
  - explicit intent `success`/`failure`, `concurrent`/`retry` carriage is outside
    this legacy phase rung.
- Next executable falsifier: land `mir.intent_step_transition` with source-
  declared typed success/failure variant and payload bindings, success-only
  completion, DIR-owned predecessor identity and failure-payload-driven
  predecessor compensation. A typed action failure must leave the current step
  incomplete and compensate only completed predecessors; it must not reuse the
  current-step rollback rule for a post-action predicate failure.

## Current resume checkpoint - typed intent action outcome binding

- Landing parent: `5c942ee5` on `main`, aligned with `origin/main` at the start
  of this slice. The landing commit will replace this parent; verify exact HEAD
  and dirty state after commit/push.
- Objective card:
  - objective: bind one exact `subject.action` result in an intent step and
    consume its enum/tobject outcome without evaluating the action twice;
  - priority: exact action identity and return type, scoped immutable binding,
    native/self MIR wire parity, C/LLVM/self execution, fail-closed mutations,
    then typed variant branches and compensation;
  - fact owners: semantic resolves the exact action return type; DIR carries
    binding/type/action stable identity; MIR carries
    `IntentOutcomeBinding + IntentEval`; C/LLVM/self emitters are last consumers;
    `tobject` owns only detached payload;
  - forbidden fallback: Bool/literal collapse, variant spelling inference,
    payload type reinference, action re-evaluation in expect, result hidden in a
    subject/global field, type hidden in runtime ABI/uses, AST/source rescan,
    missing-carrier success, or treating payload as authority/freshness/
    predecessor/rollback evidence;
  - focused gates: `intent_outcome_frontend_parser_owner.sh` and
    `intent_typed_outcome_execution_owner.sh`.
- Implemented source form: `on outcome: worker.Run(...);`. Legacy `on:` still
  discards the result. The binding is available only after `on` in the same
  step's `expect`/`post`/`compensate`, not in `pre` or later steps. The bounded
  rung requires one `on`; outcome names are unique across the intent rollback
  lifetime.
- Canonical MIR wire: `IntentOutcomeBinding` carries
  `result=slot_anchor=outcome`, `arg0=<action source_syntax_id>`, `arg1=<step>`,
  `abi_type_name=<exact return type>`, `source_type=AST_INTENT_STEP` and no
  runtime-call ABI or string-encoded identity. `IntentEval(on)` carries the same
  result/type. Native/self validators and mir_lower exact-join the action routine
  stable identity, return type and expression graph.
- Observed executable evidence:
  - full native `make -j2 compiler` completed;
  - parser binary rebuilt and its complete test run exited 0, including outcome
    binding and duplicate-binding cases;
  - new native semantic outcome tests and MIR carrier/drift tests passed;
    complete semantic remains `2821 passed, 2 failed` on committed HEAD's
    unrelated Option/Result match-destructuring baseline, and complete MIR
    remains `155 passed, 1 failed` on the unrelated committed topology mutation
    baseline;
  - a fresh current-source `driver_rung2_main.pgy` build completed with 0 Pergyra
    errors/warnings. Windows 200ms process sampling observed 1,575.1MiB combined
    peak private and 1,485.3MiB working set (`pgy` 708.0MiB + `cc1` 867.0MiB),
    not 20GB;
  - after the final responsibility splits, the current source rebuilt
    `.tmp/self_hosted/intent_typed_outcome/driver_rung2_landing.exe` with 0
    Pergyra errors/warnings; the complete typed-outcome execution/parity/
    negative gate passed again with that exact driver;
  - frontend lossless AST + invalid/duplicate negatives passed;
  - direct self source C and admitted self MIR C are byte-equal; self C, native
    C and native LLVM all print `accepted=true`, `calls=1`, `rejected=false`,
    `calls=2`;
  - missing binding, binding result/type/action identity drift, duplicate binding
    and eval-result drift all reject before partial C.
- Adjacent fixes proven by this rung: parser test now links its directly consumed
  callable vocabulary object; self MIR has an independent `abi_type_name`
  scalar; `IntentCheck` call expressions are classified before the generic
  statement-call allowlist.
- The final self-host module split keeps each responsibility below its existing
  component cap: intent parameter/outcome environments, exact action contract,
  DIR outcome validation, MIR scalar append, MIR-lower carrier/cleanup/action
  admission, and typed C outcome emission now have named owners. The component
  contract, SoT edge (`61 authorities`, `63 derived carriers`), build-source
  inventory, MIR declaration inventory, and documentation-quality gate passed.
  SoT adequacy live owner/consumer and mutation checks passed; the Coq model was
  explicitly skipped because no `rocq`/`coqc` executable is installed.
- Grade: the bounded input-language outcome-binding feature is `REACHABLE`.
  Compiler organization `intent` remains `SURFACE` because the production
  bootstrap entrypoint does not call a compiler intent and no C-owned path was
  replaced.
- Next falsifier: the `.todo` two-action fixture must add source-declared typed
  success/failure branches, success-only completion, exact DIR predecessor
  carriage and B-failure compensation where A undo executes once, B undo zero
  times and the failure tobject payload changes the observed result. Do not infer
  these facts from source order or variant spelling.
- Preserve and do not stage the three concurrent parity edits
  (`driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`) or the untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.

## Current resume checkpoint - fallible action tobject outcome consumption

- Landing parent: `49c097b2dbd42f1387349b4b1d751881a7a5dd27` on `main`,
  aligned with `origin/main` at the start of this slice. After landing, verify
  the exact revision with `git rev-parse HEAD`.
- Objective card:
  - objective: preserve the production direct-MIR action's typed success or
    failure payload through world composition to bootstrap Main and make the
    final caller consume the payload;
  - priority: exact outcome variant; transaction stage/status/recovery facts;
    exact target/path receipt; action/world/caller carriage; authority and
    freshness negatives; then typed intent transition outcome binding;
  - fact owner: `artifact_transaction_owner.pgy` owns receipt/failure facts;
    `DriverRung2Execution.EmitDirectMir` owns the terminal transition;
    world/composition only carries it and Main is the last consumer;
  - forbidden fallback: `ok + stage` double tag, Bool collapse before Main,
    failure-tag-only handling, diagnostic string recovery, raw writer/retry,
    unknown status, known-but-wrong target, or receipt-derived authority,
    source freshness and topology identity;
  - verification: `driver_rung2_fallible_tobject_outcome_owner.sh`, the action
    and atomic-transaction static gates, runtime transaction matrix, object/
    action boundary contract and the SoT registry gate.
- `DriverRung2ExecutionOutcome` now has distinct executed receipt, ordinary
  rejection and artifact failure variants. The old result struct and detached
  `execution_identity` field are gone. Success checks schema, exact target and
  output path, atomic visibility and non-durability. Failure preserves exact
  stage/status/prior-final/temp-cleanup facts to Main.
- The C type declaration scheduler now consumes hosted method/action by-value
  return and parameter facts. It does not create false cycles for implicit
  self, pointer-carried mutual subject parameters, or the host's own direct
  return/parameter type.
- Observed focused evidence:
  - incremental native compiler rebuild passed;
  - native C, native LLVM and production self C execute the action outcome
    probe as `ok=7`, `error=9`;
  - mutual subject action parameters compile without a false by-value cycle,
    while a host-self `ValueTool` method returns the executed value `3`;
  - a fresh current-source bootstrap driver publishes the success artifact and
    Main distinguishes a real begin failure exactly as schema v1, begin-temp,
    status 1, prior final preserved and temp removed, with no partial output;
  - the fresh build peak observed about 808 MiB in `pgy` plus 1.03 GiB in
    `cc1`, roughly 1.8 GiB combined rather than the old multi-process 20 GiB
    symptom.
- Grade: receipt and failure reach `OUTCOME_CONSUMED`; overall `tobject`,
  subject/action, zone and world remain `REACHABLE`, not `SUBSTITUTING`.
  Compiler `intent` remains `SURFACE` because intent lowering still discards
  action results and accepts only literal-success `expect`.
- Next falsifier: add typed outcome binding and success/failure branching for
  two real production actions in an intent, execute compensation with exact
  predecessor evidence, then consider root-intent takeover.
- Preserve and do not stage the three concurrent parity edits
  (`driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`) or the untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.

## Current resume checkpoint - tobject publication and domain admission boundary

- Landing parent: `553af9793433798a8b7c6bdea3badc80b1d345a6` on `main`, aligned
  with `origin/main` at the start of this slice. The landing commit contains
  this handoff; use `git rev-parse HEAD` and `git status --short --branch` after
  landing for the exact revision and dirty state.
- Objective card:
  - objective: keep `object`/`tobject` as topology-materialized projection
    destinations, make caller-supplied domain admission explicit, and prevent
    declaration initializers or constructor arguments from becoming a second
    materialization authority;
  - priority: projection identity and one materialization owner; explicit
    admission roles; native/self field-kind parity; exact DIR/MIR identity;
    C/LLVM execution; then examples and tooling projections;
  - fact owners: the language-word registry owns `binding`; nominal field-kind
    vocabulary owns `binding_slot`; domain declarations own slot identity;
    `dir.domain_graph` and `semantic.domain_runtime_assignment` own topology and
    exact projection assignments; MIR is a carrier;
  - last legitimate consumers: native C/LLVM constructor and topology
    renderers, and the self source-to-MIR producer/admission path;
  - forbidden fallback: object/tobject slot initializer, projection/layer/shared
    constructor injection, object slot used ambiguously as both admission and
    projection, subject-only endpoint validation, source-name recovery, or a
    detached tobject reused as a fresh projection source;
  - verification: `tobject_boundary_execution_owner.sh`, language-word and MIR
    field-kind registries, object/action boundary ratchet, VS Code grammar gate,
    and focused C/LLVM backend execution.
- `binding slot` is the explicit object-valued zone admission surface. Zone
  constructors accept source-order `subject slot`/`binding slot` values;
  relation/effect constructors accept only their `for ...` participants.
  `object`/`tobject` projection destinations and layer/shared storage are
  materialized after construction by topology/runtime owners.
- Native semantic and self parser now reject `object slot ... = ...` and
  `tobject slot ... = ...`. Those initializers previously parsed and type
  checked but had no DIR/MIR/backend carrier, so runtime silently observed
  zero-filled storage.
- The self path carries `BindingSlot` through typed AST, declaration
  vocabulary, DIR graph census, exact topology source identity, MIR declaration
  verification and runtime projection assignment. The effect/relation header
  label is produced by one immutable classifier; this avoids the observed
  generated-C SSA carry defect that erased the default subject participant.
- Observed focused evidence before landing:
  - a clean native `make compiler` rebuild completed successfully, then
    `tobject_boundary_execution_owner.sh` passed with the rebuilt `pgy` and the
    current Pergyra-built self driver;
  - production `CompileSourceToCVerified` emits byte-equal C from the binding
    source and its admitted MIR, and that C executes `door=5`, `key=9`,
    `view=5`, exactly matching native C and LLVM;
  - valid positive field IDs remain unchanged while `door` is mutated from
    `binding_slot` to `object_slot` and `key` from `binding_slot` to
    `tobject_slot`; both mutations fail in the nominal constructor policy with
    `expected: at_most_1`, `actual: 2` before any partial C artifact;
  - `language_keyword_registry_smoke.sh`,
    `mir_decl_field_kind_vocabulary_smoke.sh`,
    `object_action_boundary_contract_smoke.sh`, and
    `vscode_language_graph_smoke.sh` all passed after regenerating their owned
    projections;
  - a fresh Pergyra-built driver from the current source repeated the complete
    binding production gate after the collection-lane cleanup;
  - self MIR for `intent_callable_execution` is 46,384 bytes and for
    `binding_slot_constructor_source_order` is 10,394 bytes; the latter carries
    exact `binding_slot` identities and projection source IDs;
  - four self negatives reject projection constructor injection, detached
    tobject source reuse and unowned projection initializers;
  - native C/LLVM preserve interleaved source-order zone admission and execute
    `alpha=7`, `beta=9`, `view=7`, `receipt=9`; the binding fixture executes
    `door=5`, `key=9`, `view=5` on both backends;
  - the keyword registry now contains 145 words, including contextual
    `binding`, with one generated self/LSP/TextMate projection.
- The hard self-host contract was realigned with current owners: the MIR
  fixture inventory belongs to `driver_rung2_mir_manifest_owner.pgy`, resource
  receiver traversal uses the expression-graph accessor, and body call-target
  resolution consumes the shared expression-environment owner. Collection
  mutations were removed from `SelfMirSimpleStatementKind`; ArraySet secondary
  graph attachment now exists exactly once in the graph-owned collection lane.
- Focused current-source indexed-assignment evidence is green: direct-source C
  and admitted-MIR C are byte-equal, runtime prints `2`, and removing the target
  graph fails with `MIR instruction expression graph is missing or invalid`.
  The broader filtered `driver_rung2_body_parity.sh` run was not green: its
  oracle canonicalization stopped earlier at `MIR machine-layer facts are
  missing or invalid`. Do not report that broader runner as executed past this
  pre-existing admission blocker.
- Grade only the bounded binding admission/runtime slice `SUBSTITUTING`:
  production self source -> admitted MIR -> general C now replaces the C-owned
  oracle for this fixture, has byte-equal direct/MIR artifacts, exact native
  C/LLVM output parity, and valid-ID negative ratchets. Do not promote the
  compiler-organization grade of `object`, `tobject`, `zone`, `world`, or
  `intent`; their independently recorded grades remain unchanged.
- This next falsifier is completed by the newer fallible action outcome
  checkpoint at the top of this handoff. Typed intent transition outcome
  binding remains open.
- The stale tracked `testall_run.txt` transcript was removed and is ignored;
  generated builders remain under `.tmp/`.
- Preserve and do not stage the three concurrent parity edits
  (`driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`) or the untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.

## Current resume checkpoint - intent execution and tobject boundary

- Landing parent: `57cbc9d5bd600bb37fa0c1a56d7feeb60f6993aa` on `main`.
  The landing commit contains this handoff; after landing use `git rev-parse
  HEAD` and `git status --short --branch` for the exact revision and dirty state.
- Objective card:
  - objective: carry exact DIR intent facts to typed MIR and execute the bounded
    successful `Checkout` action through the general self-host C consumer while
    keeping `tobject` a detached payload rather than a second graph authority;
  - priority: participant/action identity; lossless MIR carriage; one admitted
    execution path; projection synchronization and caller writeback; tobject
    constructor/source negatives; then fallible intent semantics;
  - fact owners: semantic action/call owners decide callable contracts; DIR owns
    intent purpose/participants/ordered step graph and `dir.domain_graph`; MIR is
    a carrier; intent lowering/emission is the final bounded C consumer;
  - forbidden fallback: intent-as-func, source/AST rescan, native MIR graft,
    action beside an old direct path, projection storage as constructor input,
    or a published tobject reused as fresh projection source;
  - verification: `intent_callable_execution_owner.sh`,
    `tobject_boundary_execution_owner.sh`, the earlier intent reachability gate,
    and the self-host component contract.
- Observed executable evidence:
  - native `make -j4 compiler` rebuilt `bin/pgy.exe`; a fresh Pergyra-built
    `driver_rung2_main.pgy` completed with 0 Pergyra errors and 0 warnings;
    generated C retained only the three known unused-variable warnings and two
    unsupported warning-option notes;
  - direct-source self C and admitted-self-MIR C are byte-equal for the exact
    successful intent slice;
  - self C, native C and native LLVM all execute `Checkout` and print
    `buyer.total=3`, `payment.total=3`, ready projections, `Mina`, and ready world;
  - intent kind, commit, participant type, zone alias, authorization and rollback
    identity mutations fail before partial C;
  - native C/LLVM preserve subject input order across interleaved object/tobject
    zone storage, while a second projection constructor argument is rejected;
  - native source, self source and a valid-ID mutated self MIR all reject a
    detached tobject as projection source.
  - `self_hosted_component_contract_smoke.sh`, object/action boundary,
    documentation quality, build-source inventory, MIR declaration inventory,
    domain runtime topology, SoT edge, and single-owner gates passed. The
    registry reports 61 authorities, 62 derived carriers,
    `CLOSED=34 BRIDGE=27 ACTIVE=0`;
  - Coq/Rocq is unavailable, so `sot_authority_adequacy_smoke.sh` ran with the
    explicit `PGY_ALLOW_MISSING_COQ=1` declared skip. Live owner/consumer binding
    and negative mutations passed; proof compilation is not claimed.
- Grade: the bounded successful input-language intent path is `REACHABLE`, not
  whole-intent `SUBSTITUTING`. Fallible `expect`, compensation/effect outcome and
  `PgyCompilerWorld` root intent takeover remain open. Compiler-organization
  intent remains `SURFACE` because the real bootstrap entrypoint does not call
  a production root intent with a real purpose/fact bundle.
- Next executable falsifier: carry an actual fallible `expect` result, branch to
  explicit failure/compensation, observe the effect/outcome, and reject missing
  predecessor/rollback evidence. The root intent may replace the direct
  bootstrap bypass only after a real compiler purpose and its elaborated fact
  bundle reach that production path; action count is not the criterion.
- `tobject` owns only immutable materialized payload. Source identity, freshness,
  edge and authority remain with the enclosing directive plus
  `dir.domain_graph`; zone constructors accept only subject/binding inputs.
- Preserve and do not stage the three concurrent parity edits
  (`driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`) or the untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.

## Current resume checkpoint - explicit projection-map substitution

- The landing parent is `880a83c348021f3e126176a2f71ff0ad872e8223` on
  `main`, aligned with `origin/main` at the start of this slice. The landing
  commit contains this handoff; use `git rev-parse HEAD` after landing for the
  exact revision.
- Objective card:
  - objective: preserve `map { target <- source }` as a typed child of its
    refresh/publish directive, resolve it once to exact target/source
    declaration-field identity and semantic assignability, and execute that
    fact through production self C and native C/LLVM;
  - priority: syntax parent/entry identity, exact path and type verdict,
    canonical identity-epoch preservation, no explicit-to-implicit fold,
    direct-source execution, negative ratchet, then patch size;
  - fact owner: parser owns map spelling and parent structure;
    `semantic.domain_runtime_assignment` owns the resolved assignment. Self
    semantic `SemanticDomainProjectionTypeAssignable` owns compatibility;
    DIR/HIR/MIR are carriers and `MirDomainRuntimePlan` is a one-time admission
    receipt;
  - last legitimate consumers: native C/LLVM domain runtime renderers and the
    general self C method-prologue view reached by production
    `CompileSourceToCVerified`;
  - forbidden fallback: explicit map folded to implicit same-name, target/source
    string equality as type policy, backend source/AST rewalk, missing-source
    zero fill, native MIR graft, map-child omission followed by node-ID offset
    repair, or fixture-specific output;
  - verification gate:
    `tests/self_hosted/parity/domain_runtime_explicit_map_execution_owner.sh`,
    invoked by `domain_runtime_assignment_execution_owner.sh`, plus component,
    object/action, source/MIR inventory and SoT gates.
- Parser emits a typed `ProjectionMap:` child for effect/relation/zone
  refresh/publish directives. DIR binds each entry to its exact directive and
  rejects duplicate targets. The self runtime producer resolves the selected
  source path and calls the semantic assignability owner; the fixture uses
  `Int -> Long` deliberately, so byte-equal type strings cannot satisfy it.
- The canonical MIR identity epoch reconstructs admitted explicit-map children
  before dependent callable IDs are issued. It does not compare raw native and
  self producer IDs. General self C emits the exact assignments
  `life <- hp` and `label <- name` from admitted runtime rows.
- Observed executable evidence on this source tree:
  - a fresh DRV-2 self compiler build completed with 0 Pergyra errors and 0
    warnings; GCC emitted only the pre-existing unused-variable and unsupported
    warning-option notes;
  - native C, native LLVM and production self C for
    `zone_layer_projection_explicit_map_runtime` all printed `7` and `dst`;
  - production direct-source self C and explicit self-MIR C were byte-equal;
  - no-map, type mismatch, missing source and duplicate target variants all
    failed before an artifact;
  - the explicit gate, the combined implicit/explicit runtime gate, the full
    self component contract, object/action contract, build/MIR inventory,
    144-row keyword registry, targeted backend comparison, documentation
    quality and SoT edge/adequacy live-binding checks passed;
  - the SoT edge audit found and closed a pre-existing registry/Coq projection
    omission for `SFDomainRuntimeAssignment`. The final projection reports
    `59 authorities, 60 derived fact carriers; CLOSED=34 BRIDGE=25 ACTIVE=0`;
  - `make -j2 all` reported no pending native/LSP work. Coq proof compilation
    was a declared skip because neither `rocq` nor `coqc` is installed; live
    owner/consumer and mutation checks still ran and passed.
- Grade the explicit effect/relation eager method-entry map path
  `SUBSTITUTING`: it replaces a real source -> self MIR -> admitted plan -> C
  execution path and is checked against both native backends. Keep the whole
  `semantic.domain_runtime_assignment` family `BRIDGE`: self still produces
  the resolved semantic family at the MIR boundary, declaration-level source
  IDs, pool/materialization, dirty/epoch/detach/unlink/state scheduling and one
  shared native/self runtime plan remain open.
- The reusable `tobject -> object -> vessel -> subject -> action` rule is not a
  nominal promotion ladder. It is a set of orthogonal protocols: detached
  transfer, local observation, stable owned state, authority-bearing identity,
  and observable transition. At every boundary use the same closure pattern:
  semantic identity -> typed fact -> lossless carrier -> one admission receipt
  -> last production consumer -> negative ratchet. `effect`/`relation` bind
  exact destination roles and projection members; `zone` owns resource/lifetime
  frontier; `action` owns the observable transition; `intent` closes a real
  purpose and attributes its elaborated cross-axis facts.
- The next executable falsifier is
  `world_zone_projection_visibility`. Its renamed maps (`label <- displayName`,
  `user <- displayName`) must first become reachable through the self semantic
  artifact/world path; the current first blocker is the semantic initializer
  artifact for that world/intent source, not projection codegen. The current
  driver reports `ast_artifact_invalid`, `node_count: 96`, owner
  `SemanticAstInitializerTypeFacts`. Do not bypass it with native MIR or a
  fixture-specific reduced program.
- Preserve and do not stage the three concurrent parity edits (indexed
  assignment `1/0`, match `2/2`, owner-field `3/3`) and the untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`. They are not part of
  this slice.

## Prior checkpoint - callable receiver carriage substitution

- The final parent for this executable slice is `6837a34d` on `main`. During
  the slice, the concurrent language-word task landed `206e0697`, `8d4c34d4`,
  and `6837a34d` above the original `ca01b7c0` base; this receiver commit
  preserves those commits and updates the SoT keyword evidence from 145 to 144 rows. The
  landing commit contains this handoff; after landing, use `git rev-parse HEAD`
  for the exact revision.
- Objective card:
  - objective: replace self general C's by-value identity receiver path with one
    callable-owned `none | value | mutable-identity` fact carried from native or
    self MIR through exact machine admission into signature and call emission;
  - priority: callable identity, exact declaration join, mandatory wire fact,
    semantic place/addressability fact, output-before-failure negatives, then
    patch size;
  - fact owner: `semantic.callable_receiver_carriage`, with the current self
    policy in `callable_receiver_carriage_policy_owner.pgy`; MIR rows and
    codegen views are projections, not second semantic owners;
  - last legitimate consumer: general self C function/prototype emission and
    member-call receiver argument emission;
  - forbidden fallback: missing or unknown carriage success, owner-name-only
    join, mutable identity by value, `Leaf || MemberAccess` addressability
    reconstruction, address-of-temporary, role/non-role guessing, or use of the
    callable fact as a general parameter ABI decision;
  - verification gate: receiver admission native/self parity plus production
    `zone_layer_projection_runtime` hard emission, exact canonical IDs, pointer
    signature/address call, carried-value mutation, temporary receiver negative,
    generated-C syntax, routine-index C/LLVM regression, and component caps.
- Native and self MIR now emit mandatory routine `receiver_carriage` rows.
  Admission binds each row to a positive unique routine `source_syntax_id` and,
  for methods, one exact declaration owner. The current wire values are `none`,
  `value`, and `mutable-identity`.
- The production canonical identities observed by the focused gate are
  `27 | method | BattleZone | Show | mutable-identity` and
  `35 | function | Main | none`. General self C emits
  `BattleZone_Show(BattleZone *self)` and calls it as
  `BattleZone_Show(&(battle))`; a `value` mutation is rejected before C output.
- Stable-address eligibility must come from the semantic expression place fact.
  Node-kind reconstruction such as `Leaf || MemberAccess` is forbidden because
  `factory().field` is a member node but not stable storage. The focused gate
  carries an executable temporary-receiver negative.
- Role-erased local ABI preserves a concrete mutable target as `T *self` behind
  `void *_pgy_raw_self`, and its direct-call projection requires a stable
  address. This is a local owner closure only: native semantic currently rejects
  a direct `Player.TakeDamage` lookup, while the observed native/self role
  method source IDs are `13` and `6`. Do not count the synthetic/local role gate
  as production reachability or substitution. A role body `return self.health`
  also fails closed at `statement_type_unresolved`, so close call-target
  resolution, the canonical role callable identity epoch, and role-body field
  type facts first.
- This is `SUBSTITUTING` progress for the self MIR -> general C receiver path.
  The registry remains `BRIDGE` because native C/LLVM and general parameter ABI
  still reuse the broader `uses_pointer_self` compatibility policy.
- The routine-index regression encountered during integration was a missing
  mandatory `reachable` fact in positive fixtures. The fixtures now state it;
  validation was not weakened. The C/LLVM routine-index smoke and the full
  self-host component contract are green, with the main index owner at its
  600-line cap.
- Last observed focused evidence on the final source tree:
  - fresh `driver_rung2_main.pgy` C build: 0 self-host errors and 0 warnings;
  - hard `zone_layer_projection_runtime` receiver gate: one MIR fixture PASS,
    including carriage mutation, semantic-place temporary receiver rejection,
    role owner positive/three negatives, ordinary self-codegen role definition,
    and generated-C GCC syntax;
  - MIR receiver admission: native/self value and mutable rows plus ten
    fail-closed mutations PASS;
  - routine-index C/LLVM smoke and self-host component contract PASS;
  - SoT authority live owner/consumer and negative gate PASS; the Coq compile
    was explicitly declared skipped because neither `rocq` nor `coqc` is
    installed on this runner.
- The protected concurrent user changes remain unstaged and must preserve these
  numstats exactly: indexed-assignment `1/0`, match `2/2`, owner-field `3/3`.
  They are not part of this executable commit.
- The concurrent language-word registry task is now committed in the three parent
  revisions above. Its separate untracked oracle-bootstrap document is not part
  of this receiver commit. The receiver registry gate and component contract
  passed from an isolated 54-path staged-snapshot worktree: the registry
  observed 58 authorities, 57 derived fact carriers, `CLOSED=34`, and
  `BRIDGE=24`; the protected parity edits were absent from that snapshot.
- The next falsifying runtime fixture remains `zone_layer_projection_runtime`
  output `7` and `dst`. Receiver identity is no longer its blocker. The active
  next seam is exact projection member assignment plus effect bearer/relation
  source-target destination roles, followed by layer materialization and
  refresh/publish synchronization in one admitted runtime plan.

## Prior checkpoint - domain runtime assignment boundary audit

- Exact checkout at the start of this supporting slice is
  `e8440ac3cf1bcdb5469a8dff75041bc416078714` on `main`, aligned with
  `origin/main`. The landing commit contains this handoff; after landing, use
  `git rev-parse HEAD` for the exact revision.
- Objective card:
  - objective: fix the repeated runtime boundary protocol from `tobject` through
    `object`, `vessel`, `subject`, `action`, `effect`, `relation`, and `zone`,
    then define the first lossless runtime-assignment carrier without creating
    another backend or AST-text authority;
  - priority: exact source/destination identity, owner-specific fact lifetime,
    callable receiver carriage, explicit lifecycle/materialization, one
    admitted target-neutral plan, then patch size;
  - fact owners: semantic callable ABI for `CallableReceiverCarriage`, domain
    semantic/DIR for participant roles and projection member assignments, domain
    runtime owner for lifecycle/materialization operations, MIR as lossless
    carrier, and `VerifiedDomainRuntimePlan` only as an admission receipt;
  - last legitimate consumers: general self-host C codegen view and direct C/LLVM
    target renderers after machine admission has built and fully validated the
    plan once;
  - forbidden fallback: backend same-name member search, missing-source
    `.field = 0`, first/0/1 bindable destination selection, `by participant` as
    destination role, aggregate zero as successful layer materialization,
    by-value zone identity, backend lifecycle AST rewalk, or old-epoch plan
    reuse;
  - verification gate: explicit/implicit map carrier parity; wrong valid member
    ID/type, bearer role and relation destination mutations; missing receiver,
    materialization and sync operations; structural fallback ratchet; final
    self MIR -> C and shared-plan C/LLVM execution output `7` and `dst`.
- Boundary judgment is now fixed in
  `docs/200_object_to_action_boundary_patterns.md`. These constructs are not a
  promotion ladder: tobject owns detached transfer, object local observation,
  vessel subject-owned state, subject stable identity, and action observable
  transition. Effect/relation/zone repeat the same identity-carriage-binding-
  operation-outcome protocol at the domain frontier.
- The native fact-lifetime audit found five distinct missing families:
  `DomainParticipantRoleFact`, `DomainProjectionMemberAssignment`,
  `DomainLifecycleOperation`, `DomainLayerMaterialization`, and
  `CallableReceiverCarriage`. A future `domain_runtime_assignments` namespace
  may carry them together, but one nullable mega-row or a DIR-owned receiver
  decision is forbidden.
- Projection mapping is currently lossy. Native semantic resolves explicit and
  implicit same-name paths only locally; native MIR retains explicit names in
  memory but does not serialize them. C and LLVM therefore re-decide mapping,
  and C hides a missing path with zero while LLVM fails. Implicit same-name
  remains a sound Pergyra default only when semantic resolves it once into exact
  field ID/type/path rows.
- Effect bearer and relation source/target destinations do not have role facts;
  native C/LLVM use first and 0/1 ordinal bindable slots. Receiver carriage is
  also lost from the MIR JSON wire, and self general C can emit
  `BattleZone_Show(BattleZone self)`. Zone layer storage can remain aggregate
  zero without a proved materialize/bind/sync operation.
- A trial parser/DIR-only `ProjectionMap:` patch was deliberately discarded.
  It inserted public compact-tree rows that change native parity/source IDs and
  still disappear at the MIR JSON consumer. The coherent implementation slice
  must start at semantic ownership and preserve all dependent identities through
  canonicalization; no source reparse or public-tree side channel is allowed.
- The next implementation order is: add lossless explicit/implicit member and
  participant-role facts; add callable-specific receiver carriage; serialize
  lifecycle/materialization operations; remap declaration/member/directive IDs
  atomically; index `domain_runtime_assignments` once in `MirDocumentFactIndex`;
  build and fully validate one runtime plan immediately after topology admission;
  pass it into the general C codegen view and direct C/LLVM renderers.
- No compiler source was landed in this audit slice. The observed evidence is
  source/read-path inspection plus document contract gates and `git diff
  --check`; the prior executable `e8440ac3` remains the last substitution
  checkpoint. This is supporting commit one after that executable checkpoint.
- The next executable falsifier remains self MIR -> C `7`/`dst`. A parser-only
  map row, plan comment/digest, zero-filled `.poison/.trust`, or by-value zone
  receiver cannot satisfy it. The three protected concurrent user parity files
  remain unstaged and unchanged by this slice.

## Current resume checkpoint - distinct apply topology and boundary contract

- Checkout base before this executable slice is
  `820e1ec32960a78ed73b37bd4f4046f0ba6270a9` on `main`, aligned with
  `origin/main`. The landing commit contains this handoff; after landing, use
  `git rev-parse HEAD` for the exact executable revision.
- Objective card:
  - objective: preserve `apply` as a distinct self-host-produced lifecycle fact
    through DIR/MIR/canonical admission while fixing the canonical
    `object/tobject -> vessel -> subject/action -> effect/relation/zone/intent`
    authoring boundary and naming the exact runtime facts still missing;
  - priority: distinct source identity, exact field-kind join, atomic canonical
    epoch, no-edge graph admission, honest dogfood grade, then patch size;
  - fact owner: native/self DIR domain-topology rows for directive identity,
    declaration-field identity for slot kind, MIR as the lossless carrier, and
    `MirDomainTopologyGraphPlan` only for the target-neutral dependency plan;
  - last legitimate consumer: machine admission and the one C/LLVM graph-plan
    attachment immediately before target projection;
  - forbidden fallback: folding apply into maintain, deriving runtime lifecycle
    from graph adjacency, source/AST reparse, same-name projection-member join,
    0/1 ordinal destination binding, `by participant` as effect bearer,
    zero-filled layer storage as success, or by-value zone receiver identity;
  - gates: native topology smoke, focused self producer, canonical identity
    epoch, one-plan C/LLVM consumer, component/grammar/keyword/object-action/SoT
    contracts, shell syntax, and `git diff --check`.
- `zone_layer_projection_runtime` now produces four exact rows from the
  production self source path: `Poisoned.refresh`, `TrustedLink.publish`,
  `BattleZone.apply-effect`, and `BattleZone.link-relation`. `apply-effect`
  exact-joins the `poison` effect slot and `player` subject slot and remains
  distinct from `maintain-effect`; native/self MIR agree on the four-kind
  sequence. Stale IDs, a valid relation-slot ID substituted for `poison`, a
  relation used as the apply layer or target, and a non-subject participant all
  fail closed.
- Native `apply stateAlias` is normalized by semantic ownership into the exact
  effect/target slots before DIR collection, and a focused fixture proves that
  it emits the same typed row. DIR no longer drops an unresolved apply or
  reduces the expected row count. The production self source parser still
  fail-closes this shorthand; typed state-declaration/alias carriage is an open
  parser/DIR parity seam, not a name-lookup fallback opportunity.
- Apply is a one-shot lifecycle/materialization transition, not a persistent
  recomputation dependency. Native and self graph builders therefore admit the
  exact kind while adding no edge. The BattleZone graph remains exactly
  `nodes=3 edges=2 depth=2 pass_limit=2`; maintain continues to own the
  layer-to-target dependency edge. Unknown kinds are not ignored.
- Evidence grades remain deliberately split: the four-row self source ->
  DIR/MIR producer is a narrow `SUBSTITUTING` C-owner replacement; the admitted
  target-neutral graph plan is `REACHABLE`; direct-MIR world/zone/subject/action
  is `REACHABLE`; object/effect/relation/vessel/intent bootstrap declarations do
  not become runtime dogfood merely from syntax or import reachability. The
  layer materialization/projection-sync runtime remains `BRIDGE` and RED.
- The canonical boundary pattern is now fixed in
  `docs/200_object_to_action_boundary_patterns.md`: `object` is a same-process
  refreshable read projection, `tobject` a detached immutable transfer value,
  `vessel` subject-owned passive state, `subject` the identity-bearing authority
  host, and `action` its observable transition. Effect is a temporal layer,
  relation an identity edge, zone the membership/lifetime/frontier owner, and
  intent is used only when multiple production actions share a real
  success/failure/compensation purpose. These are orthogonal boundary protocols,
  not a nominal promotion ladder and not a keyword-density target.
- Exact runtime blocker: the wire still lacks projection member paths and field
  types (`view.hp <- bearer.hp`, `packet.name <- target.name`), effect-bearer and
  relation source/target destination roles, receiver carriage, layer
  materialization/state/synchronization, and refresh/publish value operations.
  The self parser currently skips projection `map { ... }` bodies, so the next
  owner must first preserve them as typed facts. A separate DIR-owned
  `domain_runtime_assignments` family should carry exact directive/owner/slot/
  path IDs and types into one target-neutral runtime plan. C/LLVM only render
  admitted operations.
- The next executable falsifier remains self MIR -> C output `7`/`dst`, but only
  after those exact facts exist. Changing one member ID/type or relation
  destination role must reject the artifact; `.poison`/`.trust` zero storage or
  a by-value zone receiver must never be accepted as the target result.
- Last observed focused evidence is green: native build and topology C/LLVM
  smoke; native/self exact four-row MIR; focused self producer hard gate;
  canonical stale/wrong-kind negatives; one target-neutral C/LLVM plan with the
  unchanged 3/2 graph; component, language-word registry, grammar,
  object/action and single-Gate-SoT contracts; shell syntax and `git diff
  --check`. The previous fresh pressure build remains the current broad memory
  evidence at peak working set 1,038.0 MiB/private 1,132.4 MiB under 3,072 MiB;
  the 35-minute pressure build was not repeated for this bounded row/admission
  change.
- This is an executable producer replacement slice, so the supporting-only
  commit counter resets here. The only protected concurrent user files remain
  the three unstaged parity owners for indexed assignment, match, and owner
  field. Do not stage or edit them.

## Previous resume checkpoint - non-empty topology producer and one graph plan

- Checkout base before this executable slice is
  `09e00d29a82584e912534ed1e4cb8eefafe23ab0` on `main`, aligned with
  `origin/main`. The landing commit contains this handoff; after landing, use
  `git rev-parse HEAD` for the exact executable revision.
- Objective card:
  - objective: replace the first non-empty native C topology producer decision
    with self-host typed DIR/MIR facts, then let one ID-keyed target-neutral plan
    reach both production backends;
  - priority: exact producer identity, atomic canonical epoch, one admitted
    plan, bounded backend receipts, runtime blocker honesty, then patch size;
  - fact owner: `SelfDirDomainTopologyRows` and `SelfMirDomainTopologyFacts`
    for source production, `MirDomainTopologyGraphPlan` for the admitted plan;
  - last legitimate consumer: self-host C/LLVM plan attachment immediately
    before target projection;
  - forbidden fallback: native topology graft, source/provenance reparse,
    name-only or offset identity repair, non-empty-to-empty downgrade,
    backend plan rebuild, repeated whole-plan readiness, generic zero-fill of
    layer storage, or plan trace claimed as runtime execution;
  - gates: focused non-empty producer, canonical identity epoch, one-plan
    C/LLVM consumer, component contract, object/action boundary contract, and
    the unchanged 3 GiB pressure owner.
- Production DRV-2 now produces `zone_layer_projection_runtime` topology from
  self source through typed AST/DIR/MIR. Exact identity is
  `domain_graph_id=14937235025281185444` with three rows:
  `Poisoned.refresh(bearer -> view)`,
  `TrustedLink.publish(target -> packet)`, and
  `BattleZone.link-relation(player, enemy -> trust)`. This bounded non-empty
  producer is `SUBSTITUTING`; it replaces the native C-owned producer decision.
- Canonical reconstruction issues nominal owner, directive, and declaration
  field identities in one epoch. Restoring a stale raw ID or pairing the
  canonical `player` name with the canonical `enemy` ID fails. Numeric equality
  across native/self epochs, ordinal repair, and name-only joins are absent.
- Machine admission creates and fully validates exactly one
  `MirDomainTopologyGraphPlan`. Production C/LLVM consumers check only its
  bounded graph/digest/cardinality receipt. The exact BattleZone plan is
  `nodes=3 edges=2 depth=2 pass_limit=2` with `trust <- player` and
  `trust <- enemy`; forged edges and a gate-only digest mutation fail closed.
  This plan path is `REACHABLE`, not yet runtime `SUBSTITUTING`.
- Zone constructor policy now distinguishes caller-supplied
  subject/object/tobject/binding slots from effect/relation layer storage.
  Caller arity remains two for BattleZone; the layer fields remain in layout
  and must be materialized by the topology/runtime owner.
- Exact runtime blocker: `apply poison to player` is identity-checked in DIR
  but is not carried as a MIR topology row; no runtime owner materializes
  `.poison`/`.trust` or executes refresh/publish value synchronization.
  Therefore self MIR -> general C output `7`/`dst` remains RED and is the next
  executable falsifier. A zero-filled layer field must be rejected rather than
  accepted as a successful runtime result.
- Last observed focused gates are green: non-empty producer hard DRV-2,
  canonical identity epoch, one target-neutral C/LLVM plan, component contract,
  language-word registry, object/action boundary contract, and `git diff
  --check`. The fresh pressure-owned self-host compiler build also installed
  `bin/pgy-self-driver.exe` and passed its smoke in 2,138,300 ms, with peak
  working set 1,038.0 MiB and private memory 1,132.4 MiB under the unchanged
  3,072 MiB cap. The one-plan gate compiles and runs the C and LLVM
  artifacts with `Hello, Pergyra!`; it intentionally does not claim the open
  zone runtime result. After that pressure run, the focused plan rebuild also
  proved the final `own` graph-schedule transfer and absent-plan residual-array
  rejection. The 35-minute full pressure build was not repeated for those two
  bounded owner/gate changes.
- Hard self-host guard accounting resets here: this is the executable
  replacement required after the two supporting commits. Documentation,
  registries, tests, and the plan are supporting evidence around the actual
  non-empty self producer substitution.
- The only protected concurrent user files remain the three unstaged parity
  owners for indexed assignment, match, and owner field. Do not stage or edit
  them.

## Current resume checkpoint - declaration field exact identity

- Checkout base before this supporting slice is
  `d6fb4a61328394329fbf71ca736bedfb70a305ae` on `main`, aligned with
  `origin/main`. The landing commit contains this handoff; use
  `git rev-parse HEAD` for its exact revision. The last executable
  substitution checkpoint remains `0ac210261d199f5b188fbc66d5dfdcbdec4c223d`.
- Objective card:
  - objective: make every domain-topology field reference prove the exact
    declaration field `(owner, name, source_syntax_id, field_kind)` before a
    backend can consume it;
  - priority: field identity carriage, one declaration index, forged valid-ID
    rejection, producer-local identity honesty, then patch size;
  - fact owner: native `MIRDeclField` and self-host
    `MirProgramDeclarationFieldIdentityIndex`, scoped to one MIR revision;
  - last legitimate consumer: native MIR topology validation and self-host
    `MirDomainTopologyFacts` admission;
  - forbidden fallback: name-only lookup, declaration rescans per edge,
    native/self raw-ID equality, numeric offset repair, AST-text reparse, or
    non-empty canonicalization with stale topology IDs;
  - gates: MIR unit mutation, `domain_runtime_topology_smoke.sh`,
    `domain_topology_admission_owner.sh`, and focused hard DRV-2
    `function_clause_order_minimal`.
- Native MIR declaration fields capture their parser-assigned stable ID and
  serialize it in `pgy.mir.v1`. Validation rejects missing/global-duplicate
  field IDs and exact-joins topology fields by name, ID, and expected semantic
  kind. The unit mutation with `player` name + valid `enemy` ID fails; the
  same name/ID with subject kind changed to object kind also fails.
- The self-host semantic/MIR producer carries field IDs through constructor
  facts, declaration rows, validation, and JSON projection. `mir_lower` builds
  one declaration index with one flattened field-identity child index, then
  topology admission consumes it without reopening `declarations[]` for every
  row. Missing/zero/duplicate IDs, duplicate owner/name, wrong kind, and the
  forged `player`/`enemy` join fail closed.
- Raw native and self ID numbers are not compared. Native preorder identity
  and the current self-host compact typed-arena identity are different
  producer/revision epochs. A future lossless self parser identity graph must
  close that convergence; constant offsets and provenance-string parsing are
  forbidden. MIR-to-AST canonicalization must regenerate declaration IDs and
  every dependent topology ID atomically. Non-empty topology remains rejected
  until that remap exists.
- Evidence status is `REACHABLE` supporting, not a new `SUBSTITUTING` slice.
  The last observed focused hard DRV-2 gate passed one producer-first MIR
  fixture through self MIR, canonical reconstruction, emitted-C compile and
  execution. Native MIR 155/0, native C/LLVM topology, self topology admission,
  component contract, object-to-action contract, and six focused MIR-JSON
  declaration fixtures were also observed green during this slice.
- The dedicated `generic_default_contracts` gate rebuilt its driver but is RED
  before MIR emission at the existing bounded self DIR error
  `relation/party/world/event production is not implemented` because the
  fixture declares `StorageParty`. Its field JSON expectation was updated and
  shell/component checks are green, but this gate is not claimed green. Do not
  weaken the self DIR fail-close merely to exercise the downstream assertion.
- Hard self-host guard accounting: the documentation refresh `d6fb4a61` was
  supporting commit one after executable `0ac21026`; this exact-identity slice
  is supporting commit two. The next commit must land executable replacement
  evidence, not another SoT-only cleanup.
- Next executable falsifier: produce non-empty
  `zone_layer_projection_runtime` topology in self-host, canonicalize it into
  a new identity epoch by remapping declaration/topology IDs together, and
  feed one ID-keyed graph plan to the production C/LLVM path. Replacing one
  canonical topology ID with the old raw native ID and pairing `player` with
  canonical `enemy` ID must both fail. Owner declaration ID, vessel-slot
  carriage, apply/state/layout/sync facts remain open.
- The only protected concurrent user files remain the three unstaged parity
  owners for indexed assignment, match, and owner field. Do not stage or edit
  them.

## Previous resume checkpoint - self-host empty DIR graph substitution

- Exact executable checkpoint is
  `0ac210261d199f5b188fbc66d5dfdcbdec4c223d` on `main`, pushed to
  `origin/main`. The worktree is dirty only in the three protected concurrent
  user parity owners named below; none belongs to this checkpoint.
- Objective card:
  - objective: make the production self-host MIR producer replace the native
    C DIR census/anchor for the first proved-empty topology document;
  - priority: exact graph identity, typed authority, non-empty fail-close,
    one-shot admission, then patch size;
  - fact owner: `SelfDirDomainGraphFacts`, projected once into
    `SelfMirDomainTopologyFacts`;
  - last legitimate consumer: the DRV-2 MIR writer and admitted MIR consumer;
  - forbidden fallback: declaration-count ID, constant graph ID, native-oracle
    grafting, provenance-text directive recovery, or non-empty-to-empty
    downgrade;
  - gate: focused `function_clause_order_minimal` hard producer/consumer parity
    plus `domain_topology_nonempty_rejected`.
- Production self-host source-to-MIR now classifies zone `Authority` and nine
  distinct domain directive kinds in the typed arena. The bounded DIR owner
  joins declarations, role/ability completion, effect/zone slots and ordered
  authority abilities. For `function_clause_order_minimal` it independently
  reproduces the native census `nodes=9, edges=16` and exact uint64 decimal
  anchor `14937235029576152731`, then emits `domain_topology.rows=[]`.
- The same self-produced MIR passes admission, canonical native/self parity,
  emitted-C compilation and execution; the observed program output is
  `clause-order-minimal`. This bounded empty-topology producer is
  `SUBSTITUTING`: its production source path no longer needs native C DIR to
  create the graph identity. It does **not** make non-empty topology, the graph
  plan/runtime consumer, or the whole `dir.domain_graph` family `CLOSED`.
- `Refresh`, `Publish`, projection `Bind`, `Maintain`, `Link`, `Apply`,
  `Detach`, `Unlink`, and `State` retain distinct typed identities. The current
  bounded producer rejects every one rather than claiming an empty row set.
  The committed negative `apply layer to actor` fixture fails at the self DIR
  owner and emits no MIR document.
- MIR canonicalization now reads one `MirMachineLayerAdmittedJsonInput` and
  carries its already-admitted empty topology into the reconstructed MIR. It
  does not run a second document/graph admission, and it does not recompute a
  graph from the lossy MIR-to-AST declaration projection (which omits zone
  authority today).
- Last observed green gate:
  `PGY_SELFHOST_DRIVER_MIR_FIXTURE_FILTER=function_clause_order_minimal` with
  the Pergyra-built hard driver, reporting one producer-first source/MIR
  parity fixture. A direct emitted-C compile/run also printed
  `clause-order-minimal`. The broad `test-transpile` RED remains the independent
  expression `identifier -> same name` null/`strcmp` failure from the previous
  checkpoint and was not rerun here.
- The only protected concurrent user files remain the three unstaged parity
  owners for indexed assignment, match, and owner field. Do not stage or edit
  them.
- Next falsifier is still the declaration-field exact join: a topology row
  with name `player` and the valid `enemy` field ID must fail. Then add typed
  non-empty directive rows and the ID-keyed target-neutral graph/runtime plan
  for `zone_layer_projection_runtime`. Owner declaration stable identity,
  apply/state/layout/sync facts remain open; no AST/source compatibility path
  is permitted.

## Previous resume checkpoint - MIR JSON topology admission (superseded)

- Checkout base before this supporting slice is
  `da26dc09d0ad5c04ee94b122bb23e18f6073a611` on `main`. The last hard
  substitution checkpoint remains
  `c66e22ca6dd34b50ff2a7a3a8e183852943d3a9a`: native C/LLVM zone frontier
  topology consumes MIR instead of the deleted AST graph entrypoints. After
  landing this slice, use `git rev-parse HEAD` for its exact revision without
  relabeling it as a substitution boundary.
- The only protected concurrent user files are
  `driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`. They must remain unstaged and
  outside this checkpoint commit.
- Native `pgy.mir.v1` now carries `relation` declarations plus optional
  `domain_topology: { domain_graph_id, rows }`. Domain rows carry graph,
  owner, directive, participant/layer/endpoint slot identity values. A
  non-domain scalar document retains its exact five-field root shape.
- Self-host `mir_lower` indexes the topology object once and admits it as typed
  `MirDomainTopologyFacts`. Missing topology for a domain declaration, unknown
  kind, duplicate directive identity, damaged null/name-ID pairs, owner or
  field-kind mismatch, and invalid relation cardinality fail before backend
  emission. No source/AST recovery or compatibility read exists. This is not
  yet an exact name-to-ID proof: declaration JSON fields do not carry their
  `source_syntax_id`, so a valid field name paired with another field's valid ID
  cannot currently be rejected by a declaration-field identity join.
- Relation identity is also connected through the self-host declaration,
  typed-AST and semantic-constructor projections, so `TrustedLink` reconstructs
  as a relation with two subject slots and one tobject slot.
- Evidence status for the JSON/admission delta is `REACHABLE`, not
  `SUBSTITUTING`. The native C/LLVM frontier remains `SUBSTITUTING`, while the
  whole `dir.domain_graph` family remains `BRIDGE`. Admission and canonical
  reconstruction do not yet execute a Pergyra-owned graph plan.
- Last observed green gates: MIR 155/0; native
  `domain_runtime_topology_smoke.sh`; self-host
  `domain_topology_admission_owner.sh`; object/action boundary contract;
  `mir_lower` source compile; positive verify-input and relation reconstruction.
  The broad `test-transpile` remains independently RED before domain tests at
  expression `identifier -> same name`, where a null emission reaches `strcmp`.
- Focused DRV-2 `function_clause_order_minimal` producer parity now reaches the
  new fail-closed boundary and is RED because the self-host MIR producer does
  not own or emit a proved empty/domain topology fact. Do not weaken admission
  or graft the native oracle row onto self output. This exact producer gap is
  part of the next executable rung.
- Hard-guard accounting: this is the second consecutive supporting/SoT-only
  checkpoint after `c66e22ca`. The next commit must land executable replacement
  evidence; do not insert another documentation, registry, or admission-only
  commit.
- Exact `BLOCKED` record for the next rung: missing facts are declaration-field
  name/`source_syntax_id` identity join, self-host producer-owned typed topology,
  the target-neutral `MirDomainTopologyGraphPlan`, and the fixture's apply,
  state-count, hidden-layout and sync-operation facts. `dir.domain_graph` owns
  topology identity; its last legitimate consumers are the plan and self-host
  C/LLVM emitters. AST/source recovery, native-oracle grafting, and
  count-floor-only success are forbidden fallbacks.
- A bounded experiment to carry the missing topology through the current
  semantic graph was rejected and reverted: the `own` variant compiled, then
  the diagnostic executable panicked out of bounds while satisfying
  `graph_shape`. The current constructor path also conflates storage-field and
  exposed-parameter counts. No experimental source change or inferred topology
  fact remains in the tree.
- Next executable falsifiers: first, a forged row with name `player` and the
  valid `enemy` field ID must fail admission. Then `zone_layer_projection_runtime`
  must make the general DRV-2 C production path consume exact
  `nodes=3 edges=2 depth=2 pass_limit=2`, `trust <- player`, `trust <- enemy`,
  and the exact mutation/state/layout/sync result from one typed plan.
- Resume with `docs/200_object_to_action_boundary_patterns.md` sections 2.1 and
  4.2.1, the `dir.domain_graph` registry row,
  `tests/domain_runtime_topology_smoke.sh`, and
  `tests/self_hosted/parity/domain_topology_admission_owner.sh`.

## Previous compiler world/action boundary checkpoint (superseded)

The following section records the immediately preceding checkpoint. It is
historical context; the current executable state is the section above.

- Exact implementation checkpoint:
  `90ed9f82ae1b3af966739f2e324a989ccc3f4863` on `main`, with parent
  `806d2eb1a861b50f4edd5c9302a1cb33a1f9b5a0`. This handoff refresh follows as
  a documentation-only commit; use `git rev-parse HEAD` for the checkout
  revision while retaining `90ed9f82` as the executable boundary.
- Dirty state at the implementation checkpoint contains only three protected
  concurrent user files, none staged or included in `90ed9f82`:
  `driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`.
- Active executable rung: `REACHABLE`, not `SUBSTITUTING`. Production direct
  MIR now follows exactly one graph:
  `driver_bootstrap_main.Main -> EmitDirectMirThroughPgyCompilerWorld ->
  PgyCompilerWorld.EmitDirectMir -> PgyCompilerWorld.direct_mir ->
  DriverRung2DirectMirZone.execution -> DriverRung2Execution.EmitDirectMir`.
  This removes Main's direct action/backend bypass but does not yet replace a
  C-owned compiler semantic path.
- Hard-substitution accounting is `BLOCKED` at the next rung, not complete:
  `dir.domain_graph` must own one typed `DomainRuntimeTopology` carrying stable
  field/layer identity, relation endpoints, pool capacity,
  refresh/authority/state/lifecycle and action transition binding. The current
  native carrier is `MIRDeclHeader`; the last legitimate consumers are the
  target-neutral topology plan and self-host C/LLVM runtime emitters. Backend
  AST/source topology rereads are the forbidden fallback. The next falsifying
  fixture is `zone_layer_projection_runtime`.
- The production import closure is 450 files with no missing import. Reachable
  Pergyra-native declarations include func 3,617, struct 179, enum 6, object
  18, tobject 3, subject 17, action 17, zone 19, world 1, and intent 14.
  Keyword/declaration counts are topology
  evidence only; only the direct-MIR world/zone/subject/action call chain is a
  production execution witness.
- `docs/200_object_to_action_boundary_patterns.md` is the canonical authoring
  contract for the value-to-authority boundary. Values remain in `struct` /
  `object` / `tobject`; identity-bearing state belongs to `subject`; an
  `action` owns the public authority/state/effect transition; the current
  direct-MIR `zone` owns its authority/lifetime boundary; the compiler `world`
  delegates once. The full audit grades `struct` as a reachable supporting
  construct, not an independently substituting feature; `class/object/vessel/intent`
  remain surface; artifact receipt/failure `tobject` values and only one
  subject/action/zone/world slice are reachable. The next
  source-to-MIR action must reuse/generalize the active execution boundary
  rather than mechanically add a zone per compiler stage. Root `intent`
  takeover follows only when a real compiler purpose binder and its fact
  bundle are executable through production and replace the direct bypass.
- Raw file-handle I/O had a real capability escape. Semantic analysis now
  refines literal `FileOpen` modes (`r`/`w`/`a`/`+`) and conservatively requires
  read+write for dynamic modes. Native C-inline and LLVM-linked runtime twins
  enforce actual open mode plus `FileRead`, `FileWrite`, and `FileExists` at
  runtime. Shell and PowerShell manifest gates cover read/write
  under-declaration; the runtime gate covers grant/deny and denied-write
  zero-artifact behavior.
- Compiler artifacts no longer use those raw handles. One shared runtime core
  now owns same-directory exclusive temp creation, checked write/flush/close,
  atomic replace, cleanup, and generation-tagged transaction handles for both
  C-inline and LLVM-linked output. The Pergyra owner maps scalar status
  immediately to `tobject SelfMirArtifactReceipt`/`SelfMirArtifactFailure`;
  the typed executed variant requires the receipt. It claims atomic visibility only,
  never crash durability. Production MIR JSON, direct-MIR action, bootstrap
  outputs, and rung-1 CLI outputs have no raw-final writer fallback.
- The source-to-MIR production path validates `SelfMirProgramFacts` once and
  calls `SelfMirProgramJsonWriteArtifactVerified`; the writer no longer repeats
  the whole graph validation that contributed to the multi-GiB symptom. The
  raw compatibility writer retains exactly one validation at its boundary.
- The codegen bootstrap's independent `0xC00000FD` failure was parser stack
  depth, not another multi-GiB graph allocation. A manually duplicated 123-row
  builtin-signature `&&` contract produced 123 nested precedence frames while
  reading the 2.46 MiB `main_ast.txt`. The signature registry now verifies its
  projection with one bounded owner loop, and the expression environment
  consumes that verifier. `make self-host-codegen-bootstrap-seed-test-smoke`
  is green through gen2 seed readiness with the normal 2 MiB PE stack reserve;
  the observed gen0/gen1 private-memory range was about 490/560 MiB.
- The next integrated-driver failure exposed a real self-host grammar gap:
  top-level dispatch recognized `export` but not native `public`/`private`, and
  nominal AST emission did not carry explicit visibility. It now maps
  `public`/`export` to the same `[export]` fact and `private` to non-export via
  `LanguageWordId`. Native/self-host AST is byte-equal for the committed
  `top_level_visibility_decl` witness, and the production `public zone
  DriverRung2DirectMirZone` parses through the self-host parser.
- The `selfhost.action_contract` supporting seam now has one semantic owner,
  `SemanticAstActionContractFacts`, keyed by callable `SyntaxNodeId`. The
  self-host parser preserves distinct Action/Function identity and exact
  requires/within/causes/authorized/caps/effects/body nodes; native and self
  MIR declarations emit the same `callable_kind + contract` wire; `mir_lower`
  validates it once and reconstructs the exact Action rows. Codegen does not
  skip clauses to find `Body:`. `semantic.callable_contract_vocabulary` now
  owns the 9 capability and 9 effect rows, canonical order, mask-symbol link,
  manifest spelling, and `local` zero-exclusive policy. Native/self/runtime
  consume direct or generated projections. The gate rejects missing/unknown
  fields, duplicates, noncanonical order, and `local + nonlocal` in both orders
  before backend output. The old AST node 88972 `Within:` / `expected Body:`
  result is retained in troubleshooting as the historical falsifier.
- `selfhost.action_contract` and its semantic vocabulary are now `CLOSED` as a
  declaration-carriage fact family. This still does not replace a C-owned
  compiler path. Production direct-MIR remains `REACHABLE`, not `SUBSTITUTING`;
  source-mode `Main -> CompileSourceTo*` is not deleted.
- The focused C shard now carries `Damage` as explicit `effect/effect` identity,
  requires `causes Damage` to resolve to that declaration, preserves
  `Damage.bearer=subject_slot` and `BattleZone.damage=effect_slot`, reconstructs
  exact domain-slot AST rows, and completes emitted-C compile/run. Zero-explicit
  parameter role impls also retain their implicit self ABI. This closes typed
  effect declaration plus C value-ABI admission only; relation declaration,
  stable field identity, pool capacity and zone runtime operations remain open.
- `mir_decl_field_kind_vocabulary.def` owns 14 stable wire spellings and AST
  labels, including distinct general/shared fields. Native C consumes the
  registry directly and self-host consumes a checked generated projection.
  Missing/unknown/invalid host kind, subject/effect-slot flattening and loss of
  the effect's exactly-one subject participant fail before backend output.
  `semantic.nominal_field_kind` remains `BRIDGE`, not `CLOSED`.
- The unfiltered `valid_array_builtins` failure was a separate runtime-header
  SoT omission. Array runtime emission already owned `uses_array` but did not
  pass it to header selection, while emitted owned-String helpers require
  `<string.h>` and `pgy_runtime_panic_contract.h`. Header selection now consumes
  `uses_array`; array-only output receives only those narrow dependencies and
  does not falsely claim String surface use or checked-arithmetic ownership.
- Match-case pattern identity no longer has a second physical graph. Typed
  `MatchCase` AST atoms feed one bounded HIR fact; `AstTreeArtifact` payload v3
  carries executable expression graphs only. The parser partition owner,
  `match_pattern_graphs`, and ordinal join are deleted, and the component gate
  rejects their return. Malformed/or-pattern/string/duplicate-binding patterns
  fail closed. The owner row remains `BRIDGE` only because four codegen helpers
  still structure a passed pattern string instead of receiving the typed fact.
- Authority evidence is deliberately bounded. `MIRDeclMethod` owns declaration
  clauses and `MIRDeclZoneAuthority` owns zone topology. The current C/LLVM
  world hook supports only the exact direct `world -> zone -> subject` receiver
  with one `authorized by self`; named, multiple, or indirect world-action
  authority shapes fail closed. The C helper separates "no check applies" from
  check-materialization failure with `bool + out`, so allocation failure cannot
  silently emit an unchecked call. Runtime validation currently proves
  non-null zone/participant presence, not identity-token or ability
  authorization.
- Nested construction is owner-preserving inline materialization with no
  surviving source alias. It is not a physical zero-copy/stable-address proof.
  Likewise one compiler world declaration/composition graph is not a runtime
  singleton; each composition call materializes a value aggregate. The world
  has one executable `direct_mir` member, while the other 18 declared zone
  types remain target topology. This removes the former 19-argument aggregate
  zero-fill fallback and keeps construction exact-arity.
- Hosted method scheduling is declaration-inventory owned. C emits nominal
  forwards/layouts, then domain value layouts, then nominal hosted bodies.
  LLVM registers nominal/domain layouts, then method signatures, then bodies.
  A later-declared by-value object fixture is green on both backends; missing
  metadata fails closed instead of guessing an opaque/scalar layout.
- The 3+ GiB semantic spike was a real native compiler defect. Each of 28,233
  dependency edges retained `bool[N] + size_t[N]` graph-sized scratch until
  context destruction for a 27,807-node graph. Per-edge path probing is now
  removed; the completed graph is validated once, and pass 2 revalidates only
  when node/edge generation changes. Exact-source C peak private memory fell
  from 3,522.4 MiB to 1,566.4 MiB; LLVM completed at 1,226.0 MiB under the
  unchanged 3,072 MiB cap.
- Last observed native build: incremental UCRT64 `make -j4` completed and linked both
  `bin/pgy.exe` and `bin/pgy-lsp.exe`. Current `world.pgy --emit-c` completed in
  28.1 seconds at 564.1 MiB peak private under the unchanged 3,072 MiB cap.
  Current `driver_bootstrap_main.pgy --backend=c` completed in 104.2 seconds at
  1,560.6 MiB peak private; `--backend=llvm` completed in 181.3 seconds at
  1,225.0 MiB peak private under the same cap. Both fresh drivers passed the
  hello/`let_log`/`multilet` one-MIR C/LLVM projection gate. Topology,
  compiler-world/component contracts, object/action, execution-action, C/LLVM
  authority ABI including unsupported named/multiple/indirect shapes, hosted
  later-value-object parity, and AIR 144/0 are green.
- Current focused capability evidence is green in both C and LLVM:
  `run_manifest.sh`, `run_manifest.ps1`, and `run_runtime_enforce.sh` cover
  literal read/write/update modes, dynamic-mode conservative inference,
  `FileExists`, host grant denial, and denied-write zero artifact. The
  object/action boundary, documentation-quality, and recursive compiler
  topology gates are also green.
- Current declaration evidence: the isolated native compiler rebuild, field-kind
  vocabulary projection, self-host component contract, semantic declaration
  identity, documentation/object-action and SoT edge/single-owner gates are
  green. Focused `function_clause_order_minimal` C DRV-2 observes native/self
  canonical MIR, seven effect/field-kind negative mutations, implicit role-self
  ABI, emitted C compile and runtime parity. The broad MIR JSON gate had three
  stale schema/harness expectations repaired, then reached the unchanged
  `for_continue` negative where a wrong-predecessor self phi is still accepted;
  the full gate is therefore RED and must not be reported as passed. No Coq
  prover is installed, so `SoTAuthority.v` reports explicit `DECLARED SKIP` and
  was not theorem-checked on this runner.
- Known unrelated RED: native semantic suite is 2,800 passed / 2 failed in the
  pre-existing Option/Result match-destructuring direct unit cases. The graph
  cycle/provenance cases pass; the full `type_resolution_dag_smoke.sh` wrapper
  inherits the same two failures. The likeness ratchet's stale
  `core_string_munge=72` and `sentinel=0` ceilings were audited against the
  exact pre-change HEAD, which already measured 79 and 11. The gate records
  those existing debts without adding a new String-to-String function or
  sentinel, and requires 19 declared zone types but only one
  production-reachable world member. Existing MIR
  inventory/link gates retain their separately documented pre-existing
  failures. `mir_json_parity.sh` additionally remains RED at the pre-existing
  `for_continue: wrong-slot self phi was accepted` negative. Do not weaken any
  semantic gate for this rung.
- The prior `valid_array_builtins` emitted-C failure has an owner-level fix:
  `uses_array` now reaches runtime-header selection and supplies `<string.h>`
  plus the panic contract. The focused emitted-C compile/run is green; the full
  unfiltered DRV-2 matrix must be rerun at the integration boundary.
- The artifact falsifier is now green: a pre-existing sentinel is preserved
  under injected open/write/flush/close/publish failure, no temp remains, no
  success receipt is issued, and C-inline/LLVM-export status agrees. The next
  falsifying fixture for ActionContract carriage and typed effect declaration
  is now green through self-host source -> native/self `pgy.mir.v1` ->
  `mir_lower` -> focused C compile/run plus field/vocabulary mutations. The
  shared caps/effects and field-kind vocabularies have single owners, but only
  the callable vocabulary is `CLOSED`. The next executable rung is typed
  `DomainRuntimeTopology` on `zone_layer_projection_runtime`; it must remove
  backend AST topology reads as its direct bypass. Production source-to-MIR
  action substitution and `Main -> CompileSourceTo*` deletion follow only after
  that runtime fact is executable. Root-intent takeover comes later.

The remainder of this file preserves earlier v63-v74 evidence as history. If a
historical statement below conflicts with this checkpoint, current source,
registries, and executable gates win and the stale statement must not be used
as a continuation fact.

## Historical v74 resume checkpoint

- Exact v74 executable revision: `bce4ae6f75a36dc014e19515732468a5de0de245`
  on `main`. Its direct parent is the v73 handoff correction `a9f5dfaa`; the
  v73 executable boundary remains `ed9fd179`. This handoff refresh is a
  documentation-only follow-up, so use `git rev-parse HEAD` for the checkout
  revision while retaining `bce4ae6f` as the loop-break boundary.
- v74 satisfies the hard executable-progress guard. One unchanged 7,054-byte
  `break_after_stmt.pgy` MIR has SHA-256
  `cb2d4f9fad6411ae9ce54e2d072d038735c29d2499a960909a09fae8eb59efbf`.
  C and LLVM compiled from that identity and matched normalized native output
  `3`, `3`.
- Certificate and plan schemas are v6. One break fact binds typed
  preheader/header/decision/break/empty-continuation/exit roles, one while
  summary, the actual `b4` continuation predecessor separately from the
  `i.4@b2` definition, one header phi, exact break row, two Log uses, and
  normal-exit `i.2` versus break-exit `i.4` lanes. Repaired digests cannot
  legitimize topology, SSA, or exit-selection drift.
- Ownership remains split by Pergyra responsibility: common fixed certificate
  identity/readiness, loop-break topology and SSA, target-neutral break shape,
  fixed break plan, and one break text emitter containing both C and LLVM. The
  common dispatcher remains the last artifact-producing full-plan consumer;
  emitters receive no MIR, JSON, index, or full plan. LLVM's exit phi is marked
  backend-only materialization and is not a second MIR fact.
- The fresh Pergyra-built bounded bootstrap is green: generated seed and
  native oracle match on sample C, MIR production, and bounded MIR consumption.
  That seed passes hello/`let_log`/`multilet`, every CFG predecessor through
  `forloop`, and original/late-break/zero-trip break execution. Phi storage
  permutation is byte-identical and all break/topology/SSA/graph/plan negatives
  reject before artifact acceptance.
- `src/lexer/language_keyword_registry.def` owns 145 sorted identities and
  all native/self-host stable IDs and metadata projections. Native lexer/debug,
  generated self-host projection, 27-row native/self-host completion, hover,
  and exact 92-row TextMate spelling/scope are registry-directed. The row stays
  `BRIDGE`: generated implementation census records typed native+self-host
  selectors for 80 words, direct-string-only self-host selectors for 18,
  native-only selectors for 46, no parser selector for `channel`, and 37 raw
  direct selectors across 34 words. Support flags, fixtures, and tooling do not
  promote implementation status.
- Pergyra-native dogfood status is now explicit. The bootstrap import closure
  has 403 files with no missing import; its non-fixture/generated/probe
  reachable declaration set has 2,664 `func`, 175 `struct`, four `enum`, one
  `subject`, and one `action`. `world`/`zone`/`intent`/`role`/`ability`/`effect`
  remain zero. `DriverRung2Execution.EmitDirectMir` is the first production
  `REACHABLE` action; `world.pgy`, `stage_intents.pgy`, and
  `authority_owner.pgy` remain unreachable, and the 16 declared compiler-world
  actions still consume readiness facts only. The world remains
  `SURFACE`/`BRIDGE`, not the executable root.
- The combined action ABI prerequisite is green on C and LLVM: subject action,
  aggregate request, enum-bearing aggregate result, capabilities, and action-
  internal `WriteFile`/`ReadFile` produced `ok / artifact-written / 17` and the
  same `driver-action-abi` file. The production direct-MIR action also owns
  requested -> target-admitted -> artifact-written/rejected, target admission,
  exact artifact acceptance, and the final `WriteFile`. `Main` no longer calls
  `CompilerTargetProjectionFactFromOwner`,
  `CompileMirJsonToDirectBackendVerified`, or direct-mode `WriteFile`.
- The production action exposed a native C declaration-order bug: hosted method
  bodies were emitted before early-eligible file-scope prototypes. The early
  function/intent prototype pass now precedes nominal method-body emission.
  `subject_action_global_helper` reproduces the old implicit/conflicting
  declaration failure and now passes C/LLVM with output `12`. The current
  current driver compiles with 0 errors/0 warnings, and the direct one-MIR
  hello/`let_log`/`multilet` C/LLVM parity plus negatives remain green.
- This action rung is `REACHABLE`, not `SUBSTITUTING`: it replaces a Pergyra
  `Main` orchestration bypass but does not yet replace another C-owned compiler
  path. Released/default replacement therefore remains 0%.
- The historical 20+ GiB / 3 GiB symptom came from cumulative graph copying
  and repeated whole-arena/readiness validation. The accepted direct CFG path
  keeps one typed admission/certificate issuance followed by fixed-size
  identity checks. During the successful v74 seed emission, an observed
  non-peak sample was 944.3/847.7 MB private/working set; it is not peak proof,
  but shows no 20 GiB-class recurrence.
- This is a real Pergyra-owned replacement for the bounded direct-CFG path.
  It does not replace the released/default C-owned `pgy`; released/default
  replacement remains 0%.

## Exact dirty state at this handoff

The exact executable/dogfood implementation checkpoint for this handoff is
`62d601f5e296aa88ecdbce9bbc88edad7b595c21`; the handoff-only refresh commit
`fce6b14654efb48acc0370a5ea97f9ecd4479d21` follows it without changing
compiler semantics. The object-to-action boundary audit is the next child of
that checkpoint; use `git rev-parse HEAD` for its exact commit after landing.
The v74 executable boundary
remains `bce4ae6f`; the later language-word/dogfood work is a supporting
SoT/contract checkpoint, and the direct-MIR production action is a reachable
dogfood boundary. Neither changes released/default replacement.
The following unstaged files are concurrent user work and must remain
unmodified and excluded from task commits:

- `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`.

No registry, dogfood-contract, action-rung, codegen-ordering, ABI-probe, v74
implementation, gate, or documentation file should remain dirty.

## Object-to-action boundary audit checkpoint

- `docs/200_object_to_action_boundary_patterns.md` is the canonical authoring
  matrix for `struct`, `class`, `object`, `tobject`, `vessel`, `subject`, and
  `action`. Parser/semantic/MIR/codegen owners and executable gates remain the
  semantic authority.
- Function-parameter carriage and hosted receiver are separate. `vessel` is
  value-carried but pointer-self when hosted; `subject` is identity-referenced
  and pointer-self; `class`/`object`/current `tobject` are value-self.
- `func` is not synonymous with pure and `action` is not synonymous with
  impure. Use action only for a subject-owned public state/authority/resource/
  stage transition with an explicit failure boundary.
- The current bootstrap closure reaches only one `subject` and one `action`.
  Unreachable `compiler/world.pgy` declares object 18, tobject 1, subject 16,
  zone 18, world 1, and action 16; all 16 actions are readiness facades and do
  not count as `REACHABLE` or `SUBSTITUTING`.
- The source-backed static ratchet is
  `tests/object_action_boundary_contract_smoke.sh`. It pins six nominal kinds,
  subject-only action, struct hosted-func rejection, subject/vessel pointer-self,
  object/tobject immutability, and the honest current tobject-helper debt.
- Open falsifiers: tobject hosted method, object bare-field mutation, class
  mutator persistence, subject bare/`self.` mutability drift, temporary subject
  action receiver, full action-contract MIR carriage, and duplicated C/LLVM
  post-action sync.
- This is the second consecutive supporting SoT/docs checkpoint after
  `fce6b146`. The next commit must be an executable zone/world replacement delta
  or record the exact blocking fact; do not start a third SoT-only commit.

## Completed Pergyra-native direct-MIR action objective card

- Objective: move the actual direct-MIR C/LLVM target admission and artifact
  write transition from `driver_bootstrap_main.pgy` into one production-
  reachable Pergyra subject action.
- Priority: stable target identity; requested -> target-admitted -> artifact-
  written/rejected state; actual bootstrap reachability; direct bypass deletion;
  unchanged MIR/certificate/plan facts; C/LLVM/native parity; then world/zone
  attachment.
- Fact owner: current MIR, semantic, ABI, target-projection, certificate, plan,
  and emitter owners remain authoritative. The new execution owner owns only
  CLI request-to-target admission, action state, failure, and the final output-
  write handoff.
- Last legitimate consumer: bootstrap execution action immediately before the
  artifact sink. `Main` owns argument spelling only; the direct backend owner
  continues to own artifact generation.
- Forbidden fallback: `Main` calling
  `CompilerTargetProjectionFactFromOwner` or
  `CompileMirJsonToDirectBackendVerified`; action failure re-entering the old
  path; target strings re-owned outside current owner; separate C/LLVM action
  or world graphs; semantic/MIR/ABI reconstruction inside the action.
- First falsifying fixture: unknown or corrupted direct-MIR target admission
  reaching `WriteFile`, or a rejected action still producing an artifact.
- Acceptance gate: the production import/call graph reaches the action; the two
  direct `Main` calls are statically forbidden; fixed MIR identity and all
  target/graph/certificate/plan negatives still reject before artifact; C,
  LLVM, and native outputs remain equal.
- Result: complete at `REACHABLE`. The current native driver compiles with
  0 errors/0 warnings; the static no-bypass gate, component contract,
  subject/action ABI parity, targeted C/LLVM global-helper regression, and
  hello/`let_log`/`multilet` one-MIR parity/negative gate are green.

## Active zone/world attachment objective card

- Objective: attach the reachable direct-MIR execution action to a real
  target/artifact zone boundary, then let one compiler world compose that
  boundary without copying MIR, ABI, target, certificate, plan, or artifact
  facts.
- Priority: preserve current target/artifact identity and action transitions;
  bind an actual resource/authority/lifetime boundary; reject missing authority;
  keep one C/LLVM-neutral graph; only then connect a root intent.
- Fact owner: existing typed target and artifact owners remain authoritative.
  The zone/world owns orchestration and authority only.
- Last legitimate consumer: the execution action at the artifact sink; the
  zone may admit and observe it but may not become another emitter owner.
- Forbidden fallback: importing the entire 5,919-LOC world closure merely to
  raise keyword counts; separate C/LLVM worlds; readiness-only action; direct
  `Main` re-entry; source/MIR JSON re-scan inside a zone.
- Falsifying case: missing or wrong zone authority still reaches the action, or
  a rejected action leaves an accepted artifact.
- Blocker: root-intent follow-up is blocked on six missing `authorized by`
  bindings:
  `bin/pgy.exe src/self_hosted/compiler/world.pgy --emit-c` currently exits 1
  with 6 errors/5 warnings. Do not import the whole world to fake progress.

## Current measured evidence

| Slice | Exit/time | Peak private / working set | Result |
| --- | ---: | ---: | --- |
| v63 observed current-driver build | 0 / 54,476 ms | 2,593.7 / 2,582.8 MB | Current parser/interpolation owners compiled below the cap. |
| C-oracle full MIR producer | 0 / 767,407 ms | 844.3 / 762.8 MB | 54,205,046-byte verified MIR emitted. |
| Pergyra gen2 full MIR producer | 0 / 1,210,574 ms | 1,091.0 / 963.4 MB | Byte-identical to the C-oracle MIR; no partial output. |
| full MIR consumer to gen2 C | 0 / 1,774,216 ms | 1,714.8 / 1,590.9 MB | Complete 3,378,704-byte C emitted. |
| gen2 host compile | 0 / 4,721 ms | 302.1 / 316.4 MB | `driver_gen2_v63.exe` created. |
| gen2 to gen3 C | 0 / 800,248 ms | 2,033.2 / 1,867.9 MB | Same MIR consumed; gen3 C byte-equal to gen2 C. |
| gen3 host compile | 0 / 4,942 ms | 337.0 / 351.6 MB | `driver_gen3_v63.exe` created. |
| fresh v64 codegen/parser seed refresh | 0 / 412,649 ms | 1,107.9 / 1,123.6 MB | Isolated current gen2 codegen and parser seeds created. |
| rewired full-bootstrap runner | 0 / 3,770,822 ms | 2,658.0 / 2,667.1 MB | Pergyra/C MIR parity, gen2 compile/bounded preflight, and gen2/gen3 C equality all passed. |
| v65 bounded integrated-driver rebuild | 0 / not separately timed | not separately sampled | Pergyra-built seed includes the backend-neutral direct MIR projection owner. |
| one-MIR direct C/LLVM gate | 0 / 12,596 ms | not separately sampled | One MIR SHA remained stable; both artifacts compiled, ran, and matched the native C oracle; graph/kind/target negatives passed. |
| v66 bounded integrated-driver rebuild | 0 / not separately timed | 2,108.9 / 2,096.3 MB observed sample | Pergyra-built seed includes typed instruction-use and scalar graph admission; bounded MIR consumer parity passed. |
| v66 hello + let_log direct C/LLVM gate | 0 / 17,371 ms | not separately sampled | Both MIR identities remained stable; C/LLVM compiled and matched native output; result/use/missing-use/operator/call-target negatives passed. |
| v67 final r3 bounded integrated-driver rebuild | 0 / not separately timed | 764.8 / 673.3 MB observed sample | Final source, document carrier, one-pass graph schema, and owner-directed ABI projection compiled into the Pergyra-built seed; bounded MIR consumer parity passed. |
| v67 hello + let_log + multilet direct C/LLVM gate | 0 / 24,462 ms | not separately sampled | All three MIR identities remained stable; C/LLVM compiled and matched native outputs; local/use/operator/order/ABI/reindex/bridge/target negatives passed. |
| v68 bounded integrated-driver rebuild | 0 / not separately timed | 882.5 / 782.0 MB observed sample | Current certificate, plan, and combined C/LLVM emission owner compiled into the Pergyra-built seed; bounded seed/oracle and consumer parity passed. The sample is not a peak. |
| v68 scalar regression + CFG/AIR plan gate | 0 / not separately timed | not separately sampled | Hello, `let_log`, and `multilet` remained green; one unchanged `ifelse` MIR drove one certificate/plan and both compiled backends with native-equal `pos`; CFG, AIR, certificate, plan, and target negatives rejected before output. |
| v69 bounded Pergyra-built r2 bootstrap | 0 / 441,708 ms | root-only summary invalid for gen2 | Seed/oracle MIR and bounded consumer parity passed. Git Bash reparented the native gen2 worker, so the root-only 27.7/9.8 MB summary is not memory evidence. |
| v69 detached-worker-aware gen2 seed emission | 0 / 355,226 ms | 1,022.1 / 937.2 MB measured peak | `gen2.exe` top private 1,005.8 MB; 3,366,105-byte C output SHA `ef8f0be...06637` was byte-identical to the bounded seed; 3,072 MB cap not exceeded. |
| v69 native-current + Pergyra-built r2 focused gate | 0 / not separately timed | not separately sampled | hello/let_log/multilet/no-phi ifelse/phi if_else_assign all green; C/LLVM/native output matched and CFG/phi/certificate/plan negatives rejected before output. |
| v70 Pergyra-built bounded bootstrap | 0 / not separately timed | 875.2 / 776.5 MB observed sample | Fresh generated driver seed matched the native oracle on sample C, MIR producer, and bounded MIR consumer. The memory row is an in-flight sample, not a peak. |
| v70 Pergyra-built direct-false CFG gate | 0 / not separately timed | not separately sampled | `reassign_block` MIR SHA stayed `c891...b223b`; C/LLVM/native output `10` matched and edge/predecessor/phi plus certificate/plan mutations rejected pre-artifact. |
| v71 final r2 Pergyra-built bounded bootstrap | 0 / not separately timed | not separately sampled | Fresh generated driver seed matched the native oracle on sample C, MIR producer, and bounded MIR consumer with certificate/plan v3. |
| v71 final r2 Pergyra-built nested CFG gate | 0 / not separately timed | not separately sampled | `nestedif` MIR SHA stayed `20e5...b3db0`; C/LLVM/native output `big` matched and inner-use/edge/merge plus repaired certificate/plan mutations rejected pre-artifact. |
| v72 native-current loop CFG gate | 0 / not separately timed | not separately sampled | `whileloop` MIR stayed 4,692 bytes / `c48c...e50fb0`; C/LLVM/native output `0`, `1`, `2` matched, phi-order permutation was byte-identical, and loop-summary/topology/SSA/graph/assignment-target mutations rejected pre-artifact. |
| v72 final r2 Pergyra-built bounded bootstrap | 0 / not separately timed | not separately sampled | Fresh generated driver seed matched the native oracle on sample C, MIR producer, and bounded MIR consumer with certificate/plan v4. |
| v72 final r2 Pergyra-built loop CFG gate | 0 / not separately timed | not separately sampled | Scalar rungs and every CFG predecessor remained green; the fresh seed passed `whileloop` C/LLVM/native execution, phi-order permutation, and all pre-artifact mutations. |
| v73 native-current range CFG gate | 0 / not separately timed | not separately sampled | `forloop` MIR stayed 3,197 bytes / `02a6...61720`; C/LLVM/native output `0`, `1`, `2` matched, generalized `2..5` and zero-trip `3..3` passed, and range fact/topology/graph/policy mutations rejected pre-artifact. |
| v73 Pergyra-built bounded bootstrap | 0 / not separately timed | 988.4 / 887.8 MB largest observed sample | Current generated seed matched the native oracle for sample C, MIR producer, and bounded MIR consumer with certificate/plan v5. The memory value is an in-flight sample, not a peak. |
| v73 Pergyra-built range CFG gate | 0 / not separately timed | not separately sampled | Scalar rungs and every CFG predecessor remained green; the fresh seed passed original/generalized/zero-trip range execution and all pre-artifact mutations. |
| v74 Pergyra-built bounded bootstrap | 0 / not separately timed | 944.3 / 847.7 MB observed sample | Current generated seed matched the native oracle for sample C, MIR producer, and bounded MIR consumer with certificate/plan v6. The memory value is an in-flight sample, not a peak. |
| v74 Pergyra-built loop-break CFG gate | 0 / not separately timed | not separately sampled | Scalar rungs and every CFG predecessor remained green; the fresh seed passed original/late-break/zero-trip execution, phi permutation, and all strengthened pre-artifact mutations. |
| direct-MIR action native build | 0 / not separately timed | not separately sampled | The current driver compiled with 0 errors/0 warnings after early global prototypes moved ahead of hosted method bodies. |
| subject action global-helper regression | 0 / not separately timed | not separately sampled | A subject action calling a nominal-return file-scope helper passed C/LLVM and produced `12`; the pre-fix C order reproduced implicit/conflicting declarations. |
| reachable direct-MIR action one-MIR gate | 0 / not separately timed | not separately sampled | hello, `let_log`, and `multilet` kept fixed MIR identities; direct C/LLVM outputs and all existing negative mutations passed through the action-owned artifact handoff. |

## Current gates and artifacts

Green:

- focused parser interpolation graph contract and 188-row parser manifest;
- native/self-host/fixture AST byte parity for `pipe_and_try`;
- DRV-2 C build and executable `let_log` readiness;
- native current-source `driver_bootstrap_main.pgy` C build: 0 errors and
  0 warnings;
- Pergyra-built bounded `tests/self_hosted/parity/driver_bootstrap.sh`, with
  seed/oracle production and bounded MIR consumer parity;
- `tests/self_hosted/parity/one_mir_dual_backend_projection.sh` using the
  current driver for hello, `let_log`, and `multilet`;
- `tests/self_hosted/parity/one_mir_cfg_air_plan_projection.sh` using the fresh
  Pergyra-built v74 seed for `ifelse`, `if_else_assign`, `reassign_block`,
  `nestedif`, `whileloop`, `forloop`, and `break_after_stmt`, including
  CFG/phi/nested/while/range/break/AIR/certificate/plan mutations;
- `tests/self_host_preparation_smoke.sh`;
- `tests/self_hosted_component_contract_smoke.sh`;
- `tests/language_keyword_registry_smoke.sh`: 145 registry rows, 71 reserved
  lexer rows, stable native/self-host word identity and metadata, native
  lookup/debug probe, eight generated projection owners below 600 lines, and
  exact generated-inventory drift checks;
- `tests/self_hosted/parity/parser_language_word_registry_parity.sh`: 80 typed
  word IDs reached by the current selectors, `action/impl/ref/own/type` native-
  selfhost AST parity, and matching rejection of unregistered `systemic`;
- `tests/self_hosted/parity/lexer_parity.sh`: all 9 sources byte-equal on C,
  LLVM, and live-native comparison;
- `tests/lsp_completion_registry_smoke.sh`: 27 registry-owned native/self-host
  completion rows; the old independent `items:[]` path is rejected;
- `tests/lsp_hover_registry_smoke.sh`: 25 lowercase language rows plus 7
  builtins, with C/self-host runtime parity and decoded multiline Markdown;
- `tests/vscode_language_graph_smoke.sh`: 92 exact highlighted rows, one full
  grammar, and no grammar ownership in the thin client;
- `tests/self_hosted/parity/driver_execution_action_abi_parity.sh`: C/LLVM
  subject/action aggregate ABI, enum result, capabilities, `WriteFile`/`ReadFile`,
  stdout, and artifact byte parity;
- `tests/self_hosted/parity/driver_rung2_execution_action_gate.sh`: production
  action reachability, requested/target-admitted/artifact-written/rejected
  transitions, exactly one backend-owner call and action-owned write, and no
  direct `Main` bypass;
- `tests/compare_backends.sh tests/cases/backend_compare/subject_action_global_helper`:
  C/LLVM output `12`, with the file-scope nominal-return helper prototype ahead
  of the subject action body;
- current `driver_bootstrap_main.pgy` native C build: 0 errors/0 warnings;
  the action-rung driver passed hello, `let_log`, and `multilet` one-MIR direct
  C/LLVM projection plus the existing graph/kind/target/ABI negatives;
- `tests/tooling_conformance_smoke.sh` and `make -j2 test`;
- `python scripts/sot_registry_gate.py`: 52 authorities, 54 derived carriers,
  `CLOSED=31 BRIDGE=21 ACTIVE=0`;
- `python scripts/protocol_registry_gate.py`: 7 protocol rows;
- `tests/build_pressure_contract_smoke.sh`;
- `tests/self_host_ci_profile_smoke.sh`;
- `PGY_DOC_QUALITY_FULL_UTF8=1 tests/documentation_quality_smoke.sh`;
- `PGY_ALLOW_MISSING_COQ=1 tests/formal_semantics_smoke.sh`: structural gate
  green, explicit missing-prover skip; 41 proofs not machine-checked;
- `git diff --check`;
- gen2/gen3 complete C byte equality and bounded gen2/gen3 parity.
- the rewired `tests/self_hosted/parity/driver_bootstrap.sh` full-fixpoint body
  with fresh isolated seeds under the 3,072 MB pressure owner.

Environment omission:

- `tests/formal_semantics_smoke.sh` passed its structural registry/load-path
  checks with `PGY_ALLOW_MISSING_COQ=1`, then declared the missing prover skip.
  No Coq/Rocq binary is installed, so the 41 proofs were not machine-checked;
  do not report this as proof-kernel success.
- `make` is not on the default PowerShell/Git-Bash PATH, but
  `C:\msys64\usr\bin\make.exe` is available. The action-rung native compiler
  rebuild used that MSYS2 make with `-j2` and succeeded. This does not
  retroactively prove an unrun full-fixpoint wrapper target.

Known RED, unchanged and not weakened:

- `tests/self_host_pergyra_likeness_smoke.sh` reports the newly explicit
  production reachability facts (`world_entry_imports=0`,
  `world_entry_refs=0`) but still fails its pre-existing smell baseline:
  `core_string_munge=79 > 72` and `sentinel=11 > 0`. The same 79 string-munge
  matches exist at the pre-task `HEAD`; this work did not loosen the ratchet.
- `bin/pgy.exe src/self_hosted/compiler/world.pgy --emit-c` exits 1 with 6
  errors/5 warnings because six authority-bearing intent steps omit required
  `authorized by` actors. AST/topology gates do not supersede this RED.

- `tests/self_host_hard_contract_smoke.sh` stops only because
  `driver_rung2_owner.pgy` lacks the pre-existing literal
  `"tests/cases/backend_compare/device_slot_machine_layer/main.pgy"`.
- `tests/self_host_compiler_world_contract_smoke.sh` still expects the retired
  `CompileSourceToMirJsonVerified(` spelling while the current entrypoint owns
  the pressure-observed/verified file variants. This mismatch predates v65 and
  was not weakened or folded into the active direct-backend rung.
- The separate full `self_host_compiler_build.sh` path stops before this CFG
  slice because its older gen2 seed does not recognize the current
  `ArrayPushOwnedString` builtin (`undefined_function`). The bounded integrated
  driver bootstrap used for v71 is green; do not conflate the stale full-build
  seed failure with the direct CFG implementation.

Current ignored evidence:

- `.tmp/instruction_writer_pressure/driver_source_v63_interpolation_graph.mir.json`;
- `.tmp/instruction_writer_pressure/driver_source_v63_gen2_owned.mir.json`;
- `.tmp/self_hosted/driver_bootstrap/v63_full.c`;
- `.tmp/self_hosted/driver_bootstrap/v63_gen3.c`;
- `.tmp/self_hosted/driver_bootstrap/driver_gen2_v63.exe`;
- `.tmp/self_hosted/driver_bootstrap/driver_gen3_v63.exe`.
- `.tmp/self_hosted/codegen/bootstrap_v64_formal/`;
- `.tmp/self_hosted/driver/bootstrap_v64_formal_r3/`;
- `.tmp/build-pressure/self-host-codegen-seed-v64-formal.summary.json`;
- `.tmp/build-pressure/self-host-driver-fixpoint-v64-formal-r3.summary.json`.
- `.tmp/self_hosted/driver/bootstrap_v65_one_mir/`;
- `.tmp/self_hosted/driver/one_mir_v65_formal/`.
- `.tmp/self_hosted/driver/bootstrap_v66_let_log/`;
- `.tmp/self_hosted/driver/one_mir_v66_formal/`;
- `.tmp/self_hosted/v66_falsifier/`.
- `.tmp/self_hosted/driver/bootstrap_v67_multilet_r3/`;
- `.tmp/self_hosted/driver/one_mir_v67_formal_r3/`;
- `.tmp/self_hosted/v67_falsifier/`;
- `.tmp/self_hosted/driver/bootstrap_v68_ifelse_native/`;
- `.tmp/self_hosted/driver/bootstrap_v68_ifelse_r1/`;
- `.tmp/self_hosted/driver/one_mir_cfg_air_plan/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v68_native/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v68_r1/`;
- `.tmp/self_hosted/driver/one_mir_v68_native/`;
- `.tmp/next_cfg_rung_audit/`.
- `.tmp/self_hosted/driver/bootstrap_v69_phi_native/`;
- `.tmp/self_hosted/driver/bootstrap_v69_phi_r2/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v69_native_fixed/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v69_r2/`;
- `.tmp/build-pressure/selfhost-v69-phi-r2.*`;
- `.tmp/build-pressure/selfhost-v69-phi-gen2-r2.*`;
- `.tmp/next_cfg_rung_audit_v69/`.
- `.tmp/self_hosted/driver/bootstrap_v70_reassign_r1/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v70_reassign_r1/`;
- `.tmp/reassign_rung_audit/`;
- `.tmp/next_cfg_rung_audit_v70/`.
- `.tmp/self_hosted/driver/v71_native_audit/`;
- `.tmp/self_hosted/driver/bootstrap_v71_nested_r1/`;
- `.tmp/self_hosted/driver/bootstrap_v71_nested_r2/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v71_native_r2/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v71_seed_r1/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v71_seed_r2/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v71_seed_final/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v71_full_r1/`.
- `.tmp/self_hosted/driver/v72_native_dev/`;
- `.tmp/self_hosted/driver/v72_native_final/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v72_native/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v72_native_r2/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v72_native_r3/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v72_native_final/`;
- `.tmp/self_hosted/driver/bootstrap_v72_loop_r1/`;
- `.tmp/self_hosted/driver/bootstrap_v72_loop_r2/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v72_seed_r2/`;
- `.tmp/next_cfg_rung_audit_v71/`;
- `.tmp/next_cfg_rung_audit_v72/`.
- `.tmp/self_hosted/driver/bootstrap_v73_range_r1/`;
- `.tmp/self_hosted/driver/bootstrap_v73_range_r2/`;
- `.tmp/v73_range_native_gate/`;
- `.tmp/v73_range_self_gate_r2/`;
- `.tmp/v73_range_self_gate_final/`;
- `.tmp/v73_forloop_audit/`.
- `.tmp/v74_break_native_r5/`;
- `.tmp/self_hosted/driver/bootstrap_v74_break_r1/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v74_seed_final/`;
- `.tmp/v74_break_inspect.mir.json`;
- `.tmp/v74_break_inspect.ll`.

Current open boundary:

- `CompilerEmissionArtifact` still does not carry the verified plan revision
  and digest as a repository-wide artifact fact. The direct v74 emitter checks
  the plan immediately before artifact creation, so the bounded path is closed,
  but global artifact carriage remains open and must not be inferred from this
  fixture gate.

## Historical execution directive: gen2 takeover before global SoT closure

Effective 2026-07-26, freeze broad SoT expansion and new fixture breadth until
the integrated gen2 driver exists and takes over the compiler-source build.
This is a scheduling boundary, not permission to bypass an owner or weaken a
fail-closed check. The planning estimate for attempting to close the remaining
SoT globally is approximately one year because the unresolved ownership seams
are individually difficult; treating that global closure as a prerequisite
would prevent the executable bootstrap from reaching a terminus.

Count the active bootstrap in this order:

1. the existing C-owned seed consumes the complete compiler source and emits
   the full `driver_gen2.c`;
2. the native C compiler builds that artifact into the integrated gen2 driver;
3. gen2 consumes the same complete compiler source and emits `driver_gen3.c`;
4. only then compare gen2/gen3 artifacts and behavior for the fixed point.

The first hard self-host threshold is step 3: gen2 must take over the complete
compiler-source build currently performed by the C-owned seed. A bounded
component fixed point, additional owner document, registry closure, or fixture
count does not satisfy that threshold.

Apply SoT work only when the current executable rung exposes a concrete missing
fact. Name that fact, its owner, its last legitimate consumer, the forbidden
fallback, and the falsifying case; close only that blocking seam, then resume
the same gen2 run. Do not sweep unrelated `BRIDGE` rows or pursue global
registry closure. Do not add breadth fixtures. A new fixture is allowed only
as the smallest reproducer for the blocker observed on the active complete
gen2 path, and it must not become a substitute for rerunning that path.

Reassess the remaining SoT and fixture backlog only after gen2 has consumed the
same complete source successfully. Until then, executable artifacts and their
observed gates outrank SoT percentage, document volume, fixture count, and
bounded-only parity as progress evidence.

## Post-gen2 Coq gap audit (queued; not the active executable rung)

Do not start a broad proof expansion before the gen2 takeover above. Commits
`ae638458` and `58b3830d` establish the first vertical spine: 41 registered
`.v` files now include shared root `PergyraCore.v`, importers
`PergyraCoreComposition.v`, `UnifiedCore.v`, and
`PergyraCoreZoneBridge.v`, plus foundation-first/load-path wiring in the kernel
gate. The source audit found no `Admitted` or Coq `Axiom`, and only the two
declared `SlotCalculus` interface parameters (`MaxSlotId` and `verify_token`).
`tests/formal_semantics_smoke.sh` now registers all 41 files and compiles them
from the same sibling-module load path. No local Coq/Rocq binary was available,
so both new proofs and the migrated capstone remain pending the dedicated Rocq
9 kernel CI; the local structural run was an explicit prover skip, not proof
success.

The important proof gaps are refinement gaps, not unfinished `Qed` blocks:

1. the new shared core is not yet comprehensively bound to the live
   parser/semantic/AIR/MIR owner facts used by the integrated compiler;
2. the parser-to-AST boundary is still outside the machine-readable pass/loss
   manifest;
3. `IntentStepSoundness.v` proves a linear authority-guarded fragment, not the
   composed types/generics/world/zone/effect/slot/async language core;
4. exceptional and cancellation exits are not covered by the pin/resource
   cleanup proof;
5. the transitive world/zone/projection frontier scheduler and its termination
   are not closed;
6. cross-axis generic carriage and full call-site evidence attribution remain
   outside the current mechanized bindings.

The first post-gen2 Coq unit must therefore bind the exact gen2-accepted
compiler path to live owner facts and a negative adequacy gate. Do not add
another independent abstract law before that refinement bridge exists, and do
not turn whole-language soundness into the next global-closure project.

## Historical v60 resume checkpoint

- Implementation checkpoint: `3418b0f3` (v60 structured expression occurrence
  identity) on `main`. Structured MIR-to-AST emission carries
  `(global instruction row, AST lane, derived ordinal)` occurrences into one
  final graph arena. Repeated CFG visits repeat the producer key and receive a
  fresh range; source text is only an assertion. Required MIR producer coverage
  fails closed, the intermediate persisted sequence view is deleted, and the
  native range branch now projects its stop expression while loop-init retains
  the start. The complete run passed the v59 positional mismatch, completed
  graph construction and semantic analysis, and reached assignment body typing
  below 1,131 MB private. It advances the executable rung but is not gen2 or
  hard substitution.
- The v60 predecessor is `a4738c25`, following `7eef684b` (v59 prefix
  readiness) and `19ecce41` (linear expression arena assembly). v59 removed
  cumulative `place_kinds` rebuilding, per-append whole-arena readiness, and
  program/routine-index reconstruction, then exposed the positional identity
  mismatch at `ParsePrimaryFact` instead of crossing the 3 GiB cap.
- The accepted predecessor is `195d9b64` (v58 single-consumption loop branch
  projection) on `main`. It removes the second per-block branch
  selection and second per-branch scalar read from loop-summary readiness,
  preserves exact routine/block/span identity and FOR range/foreach semantics,
  and materially improves the adjacent v57 normalized markers through routine
  1,728. The accepted v57 predecessor is `ab3f9066` (direct match-local
  routine-index consumption). The preceding v56 implementation is
  `6f5c373d`, reverted by `c9e8011a`; its separate instruction-alignment pass
  remained slower than the adjacent v48 control after MIR-start normalization.
  v57 removes that redundant pass, retains one routine-index owner and one
  instruction loop, and materially improves the shared normalized markers.
  Do not add a third match-local read shape. The rejected v55 implementation
  is `2eeeec13`, reverted by `1f77b0bc`; focused gates and disassembly proved
  the local transformation, but the fixed full run regressed materially. The
  rejected v52 implementation
  is `8c49f74f`, reverted by `40037e52`.
  The successor-pair seam is abandoned after its first measured shape; do not
  re-express it as another pair struct, wrapper, or carrier. The rejected v51
  implementation is `e6abdeaa`; the rejected v50 carrier is `530682af`,
  reverted by `c5ee6e62`. Accepted compiler source
  retains `5e12cf43`'s isolated stray runtime-row fail-closed correction. Its
  accepted performance baseline remains `8074d6c8` branch selection plus that
  correction. The resource ABI performance seam is now abandoned after both
  carrier and local-scan shapes regressed materially. The earlier rejected v49
  implementation is
  `80a54268`, reverted by `85cee4ff`. Its phi-prefix
  admission predecessor is
  `a05aaf06` (`admit MIR phi prefixes once per routine`). Its phi-prefix carrier
  predecessor is `99e76e76` (`carry
  MIR phi prefixes in routine facts`). Its branch-row predecessor is `4ee29ce2` (`carry MIR
  branch rows in routine facts`). Its CFG negative predecessor is `ec4b9eef`
  (`cover invalid CFG backedge batch results`), with CFG owner implementation
  `73133678` (`batch MIR CFG backedge facts per routine`). Its scalar-key
  predecessor is `dfc8e406`, its optional ABI scalar predecessor is
  `bf8a56b8`, its
  exact ABI witness predecessor is `0da9c5c2`, its ABI
  row-capture predecessor is `a5d56f42`, its
  routine-scalar predecessor is `dd68d6f3`, its
  instruction-view predecessor is `06f6994d`, its
  evidence predecessor is `84f68161`, its
  admitted-structure predecessor is `190d0dbf`, its document-index predecessor
  is `67502f50`, its
  routine-consumer predecessor is `d62553ee`, its
  exact-span predecessor is `157c340b`, its
  machine-admission predecessor is `0857899e`, and the complete artifact
  predecessor is `6329356f` (`bound-mir-json-string-leaf-lifetime`).
- The verified driver now proves semantic readiness once and enters
  `SelfMirProgramFactsFromReadyArtifact`; the independently callable checked
  entrypoint still owns the complete validation contract.
- Direct local assignments still require local/target type equality. Member
  and indexed assignments validate the root local separately and no longer
  compare that root type with the final selected member/index type.
- Production `--emit-mir-json-verified` writes through
  `SelfMirProgramJsonWriteFile` instead of materializing one whole-program
  `String`. Program/routine/block and instruction-local unbounded graph/list
  rows are streamed. Escaped/quoted string leaves use a call-local allocator
  pool released immediately after synchronous `FileWrite`; numeric and fixed
  bounded projections remain unchanged.
- Initializer local visibility now advances through
  `SemanticAstInitializerEnvironmentCursor`. Function-base rows are seeded
  once, lexical locals are appended/popped in source order, destructure rows
  publish atomically, and the two per-row full-function local scans are absent
  from the production loop.
- Pergyra semantic and canonical MIR facts remain the SoT. C and LLVM remain
  peer native compiler projections with their existing execution/reference
  roles; self-hosted artifacts must be compared against the declared C/LLVM
  oracle class. The Pergyra-built DRV-2 is still a bounded self-host replacement
  lane; this checkpoint does not claim a fully self-hosted driver or a
  Pergyra-owned LLVM emitter. It does establish the first complete current
  full-driver MIR artifact below 3072 MB.
- The MIR consumer now creates one typed machine admission and carries the
  exact declaration and routine index used by that proof. Exact-bound JSON
  readers accept only structure-owner spans; declaration phases and the first
  AST reconstruction reuse their inventories instead of rebuilding root facts.
- Routine headers, match/destructure arrays, render/ABI facts, and phi result
  identity now consume one exact routine/instruction owner. CFG structural
  merge is a pure `mir_cfg_graph_owner.pgy` query with branch-local blocked
  reachability; the routine index no longer runs candidate-local BFS.
- The hard MIR input builds one `MirDocumentFactIndex` and carries its root and
  top-level array bounds through schema, capture, routine, and machine
  admission. Exact-bound string materialization no longer calls
  `Substring(json, ...)`, and null tokens use `SubEqualsWithLen`.
- The admitted `MirProgramRoutineIndex` captures the program-order
  routine/block/instruction structure, instruction kind/source type, and raw
  machine spans. Machine admission and `MirRoutineFactIndex` consume this
  derived `pgy.mir.v1` view. Whole-program readiness is proved once at
  admission; per-routine construction uses an O(1) row guard.
- Routine reconstruction now consumes a typed instruction view and a canonical
  CFG block-id projection from that admitted structure. Common no-layout and
  no-resource instructions are decided from exact bounds without repeatedly
  validating the same instruction object and rediscovering its field bounds.
- CFG successor identity is decoded once into `Array<Int>` rows. Missing edges
  alone use the internal negative sentinel; an explicit negative wire target
  fails closed at `cfg_successor` and is exercised through both C and LLVM.
- MIR phi `uses` is treated as the producer-owned incoming-value inventory, not
  a predecessor-indexed native phi table. Its accepted arity is
  `2 <= use_count <= predecessor_count`, and a self-result input requires a
  CFG-proven incoming backedge.
- Each `MirRoutineInstructionFactBundle` construction now captures `result`,
  `expr0`, `expr1`, `arg0`, `arg1`, `slot_anchor`, `abi_type_name`, and
  `match_variant` plus raw ABI value spans in one pass over a routine's
  program-owned spans. It remains
  routine-local rather than turning the program index into a second
  global/local aggregate. Render,
  match, graph, assignment, and phi consumers use that bundle. A malformed
  count cannot cross into the next routine, and duplicate or non-string scalar
  fields fail closed.
- Required ABI rows no longer rebuild a generic object table for every field
  and then repeat the same work during identity hashing. The ABI owner captures
  one nested row and its field rows, applies canonical hash order to that
  capture, and owns both producer and final-consumer identity. The old
  instruction-span validator and repeated-scan hash path are absent.
- One MIR-to-AST execution retains only successful exact ABI validation
  witnesses. A required hit needs the raw type value, canonical decimal ID,
  required state, and complete raw layout payload. ID-only and cross-run reuse
  are forbidden; a changed payload is revalidated and fails closed.
- The routine scalar pass carries whether the ABI type value was one valid
  string or exact optional `null`. The ABI owner remains the semantic owner and
  uses that observation only with exact optional `id=0`/`layout=null` tokens.
  Required tuples still take the complete raw witness path; wrong-kind or
  noncanonical values are not repaired or guessed.
- The same scalar owner scans each key for an escape and dispatches plain keys
  to their raw-length comparison group. Escaped keys retain full semantic
  comparison and duplicate detection. No scalar carrier, helper, cache, or ABI
  semantic owner was added.
- The existing CFG graph owner computes backedge headers once per routine from
  one entry-reachability result and one avoiding traversal per reachable
  distinct incoming target. The fact index consumes that result; the old
  per-edge function is deleted. Invalid batch input is an empty typed result
  and a nonempty consumer reports `cfg_backedge`. Structural merge and phi are
  unchanged.
- The routine-local instruction fact bundle now carries each block's unique
  branch global row from its existing scalar pass. Condition, loop-transfer,
  and match-binding consumers select that row through the admitted routine fact
  index instead of searching the block or repeating full bundle admission.
  Routine/block identity, local/global range, scalar span, and final branch kind
  are checked. Duplicate, out-of-block, scalar-span-mismatched, or non-branch
  rows fail closed; the old bundle accessor and routine-lowering search cannot
  return as fallbacks. The program index remains structure/identity-only rather
  than becoming a second global/local scalar aggregate.
- `BuildMirMatchBindingLocalFacts` now consumes the already-admitted
  `MirProgramRoutineIndex` row directly. One row-readiness proof and bounded
  block/instruction ownership checks precede one instruction loop; only
  canonical `AST_MATCH_CASE` branch rows contribute match local names/types.
  Invalid owners, zero-block parallel-array gaps, wrong-kind match rows, and
  name/type count mismatches fail closed, while forged non-match local arrays
  are ignored. No second graph, carrier, cache, backend split, or old-read
  fallback was introduced.
- `LoopFlowSummaryProjectionReady` consumes each block's owned branch global
  row once. Positive rows receive one exact branch selection and one scalar
  capture; no-branch rows use exact `-1`. Routine identity, block spans,
  instruction offsets/counts, malformed sentinels, FOR fields, and foreach
  iteration facts fail closed before projection. `BlockHasLoopTransfer` and
  rendered `BlockCond`/`"for "` classification are absent, and no second graph,
  cache, carrier, helper, backend split, or fallback was added.
- The same routine-local bundle carries each block's leading phi count. A phi
  after the first non-phi is an invalid sentinel. The phi semantic owner scans
  only that prefix while retaining predecessor, arity, result, incoming-use,
  and backedge validation. It admits routine identity, exact block counts, and
  bundle shape once at entry, then directly reads the prefix array. The one-use
  per-block accessor is deleted. Missing/invalid prefix facts cannot fall back
  to a whole-block scan or JSON kind recovery.
- A direct `EmitBlockStmts` block-slice experiment passed its fail-closed gates
  but regressed the fixed run by 8,169 ms at routine 1,920 and lost routine
  1,984. It is explicitly reverted. Current source retains the accepted v48
  block-accessor shape; the failed v49 shape is evidence, not an active
  fallback.
- A later resource-runtime experiment captured four top-level fact families in
  every instruction scalar and expanded the routine bundle. It removed about
  145.6 MB of repeated resource top-span reading by static estimate but built
  in 62,385 ms and reached only routine 1,728 at 296,959 ms. `c5ee6e62`
  reverts it. The review-discovered stray wrong-kind runtime row fail-open is
  retained alone in `5e12cf43`; a non-resource instruction can no longer treat
  an explicit runtime row as absence.

## Historical v60 dirty state

The semantic implementation checkpoint is `3418b0f3`; its handoff-only
successor carries no semantic change. After that checkpoint is pushed,
`main` and `origin/main` are synchronized and no task-owned implementation or
documentation change is dirty. These unstaged files are concurrent user work
and must remain unmodified and excluded from task commits:

- `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`.

## Historical v60 executable objective card

- Objective: finish MIR-to-AST lowering for the completed admitted full-driver
  MIR artifact, emit and compile the integrated gen2 driver, and immediately
  make gen2 consume the same complete compiler source to emit gen3.
- Priority: preserve the exact `pgy.mir.v1` artifact identity, keep the MIR
  consumer and semantic owners fail closed, stay below the fixed pressure cap,
  complete the gen2 takeover, then establish the fixed point. Do not widen SoT
  or fixtures before that takeover.
- Fact owner: the verified `SelfMirProgramFacts` producer and its completed
  `pgy.mir.v1` artifact. At the current boundary,
  `SemanticAstAssignmentTypeFactsFromArtifact` owns assignment body-type
  derivation and `SemanticAstBodyTypeBundle` is its receiving boundary. The
  structured occurrence order and final expression arena are already admitted
  inputs; they must not be rebuilt inside assignment typing.
- Last legitimate consumer: current `driver_oracle.exe --mir-json` emitting
  `driver_gen2.c`, followed by the native C compiler only as the bootstrap
  object-code boundary.
- Forbidden fallback: regenerating a native oracle MIR per generation,
  backend-specific JSON reads, source-text or graph-text recovery, a second
  expression graph/order/cache, per-assignment whole-program reconstruction,
  `new ? old` authority, or raising the 3,072 MB / 1,800-second bounds.
- Focused falsifier: on the same 51,807,108-byte MIR artifact, progress from
  `semantic-body-type-stage assignment:start` to `assignment:done` under the
  fixed limits, or expose the exact assignment row and repeated owned read that
  prevents completion. Do not reopen graph identity or broaden fixtures.
- Acceptance gate: pressure-owned full MIR consumption emits `driver_gen2.c`,
  that artifact builds, and the resulting gen2 consumes the same complete
  compiler source to emit `driver_gen3.c`. The bounded preflight remains a
  focused diagnostic, not a prerequisite track that may delay this takeover;
  compare gen2/gen3 only after both complete artifacts exist.

## Historical measured evidence through v60

The original 20+ GiB observation was dominated by repeated graph/readiness
validation. Closing those repeated validations brought the current driver into
the fixed 3 GiB pressure window. Sequential instruction projection plus
call-local string-leaf lifetime now completes the full artifact in that same
window. The latest fixed-cap observations are:

| Slice | Peak private | Peak working set | Last observed state |
| --- | ---: | ---: | --- |
| `mir-fact-ready` | 2865.8 MB | 2359.0 MB | Reached MIR lowering; exposed the composite-assignment invariant at syntax node 5290. |
| `assignment-composite-ready` | 3233.9 MB | 2716.4 MB | MIR facts completed; crossed the cap after `json:start`. |
| `json-builder-ready` | 3195.6 MB | 2680.9 MB | MIR facts completed; whole-program JSON still crossed the cap. |
| `json-file-ready` | 3290.1 MB | 2775.6 MB | Wrote 20,013,056 bytes before routine-string materialization crossed the cap. |
| `json-block-file-ready` | 3197.3 MB | 2678.8 MB | Wrote 20,901,888 bytes; per-instruction/field strings still accumulated. |
| `initializer-cursor-ready` | 3117.9 MB | 2601.7 MB | All 8,229 initializer rows and MIR facts completed; crossed after `json-write:start` with 13,709,312 bytes. |
| `instruction-stream-ready` | 3092.7 MB | 2574.5 MB | Unbounded instruction/graph rows streamed; crossed with a 40,263,680-byte partial artifact because leaf strings remained result-lived. |
| `instruction-string-pool-ready` | 3064.3 MB | 2544.9 MB | Exit 0; complete 51,807,108-byte artifact and `json-write:done`. |
| `full-mir-consumer-admitted` | 53.0 MB | 66.1 MB | Input schema/capture completed; timed out at machine admission. |
| `full-mir-consumer-bounded-cursor` | 54.8 MB | 67.8 MB | Timed out while building the routine index; cursor-only `strlen` debt remained in field reads. |
| `full-mir-consumer-exact-bound` | 59.3 MB | 72.0 MB | Reached `routine-index:done`; timed out after `instruction-scan:start`. |
| `full-mir-consumer-machine-twofield` | 63.6 MB | 76.0 MB | One-pass two-field instruction read; still timed out after `instruction-scan:start`. |
| `full-mir-consumer-key-compare` | 57.1 MB | 69.9 MB | Machine/input admission completed; timed out after `mir-to-ast:start`. |
| `full-mir-consumer-exact-span` | 58.0 MB | 70.7 MB | Declaration fields and routine ends consume carried spans; reached `declarations:done`. |
| `full-mir-consumer-routine-fact-exact` | 58.0 MB | 70.8 MB | Routine fact bundle consumes exact spans; reached `first-top-level-routine-fact-index:done`. |
| `full-mir-consumer-routine-indexed` | 58.0 MB | 70.7 MB | Result/match facts consume one routine index; first top-level routine completed, no gen2 output. |
| `full-mir-consumer-cfg-owner` | 57.8 MB | 68.7 MB | Structural merge uses branch-local blocked reachability; first top-level routine completed, no 16 marker or gen2 output. |
| `mir-document-index-driver-build-v2` | 2319.9 MB | 2322.4 MB | Integrated C driver compiled in 57,528 ms below the fixed cap. |
| `full-mir-consumer-document-index` | 63.4 MB | 74.0 MB | Timed out at 300,554 ms after the 16-routine marker; no gen2 output. |
| `mir-program-instruction-index-driver-build-v3` | 2405.9 MB | 2409.3 MB | Integrated C driver compiled in 50,974 ms below the fixed cap. |
| `full-mir-consumer-program-instruction-index-v3` | 85.2 MB | 93.6 MB | Timed out at 300,606 ms after the 16-routine marker; no gen2 output or cap crossing. |
| `full-mir-consumer-borrowed-fact-v9` | 82.6 MB | 92.8 MB | `ref` accessors alone did not help; routine 16 completed at 133,593 ms. |
| `full-mir-consumer-bounds-fast-v10` | 82.7 MB | 91.1 MB | Exact-bound common paths cut routine 16 to 69,919 ms, then exposed `FindTopLevelComma` phi inventory drift. |
| `full-mir-consumer-phi-inventory-v11` | 88.5 MB | 96.7 MB | Passed the phi counterexample and reached routine 64 at 99,411 ms; timed out with no gen2. |
| `full-mir-consumer-direct-block-v12` | 88.5 MB | 96.5 MB | Direct canonical block rows preserved behavior; routine 64 at 99,803 ms. |
| `full-mir-consumer-int-cfg-v13` | 88.6 MB | 96.6 MB | Timed out at 180,056 ms; routine 64 at 99,447 ms and routine 128 at 164,457 ms; no gen2. |
| `mir-int-cfg-negative-ratchet-driver-build-v14` | 2442.7 MB | 2430.8 MB | Final-source integrated C driver compiled in 48,451 ms below the cap. |
| `full-mir-consumer-int-cfg-v14-300s` | 94.3 MB | 102.1 MB | Timed out at 300,324 ms; routine 192 at 235,898 ms; no gen2. |
| `mir-routine-scalar-bundle-driver-build-v23` | 2509.8 MB | 2498.5 MB | Current-source integrated C driver compiled in 47,746 ms below the cap. |
| `full-mir-consumer-routine-scalar-bundle-v23` | 87.0 MB | 95.3 MB | Timed out at 180,343 ms; routine 64 at 96,607 ms and routine 128 at 160,331 ms; no gen2. |
| `full-mir-consumer-routine-instruction-detail-v37-300s` | 92.2 MB | 100.1 MB | Timed out at 300,186 ms; required ABI rows dominated and routine 248 completed at 290,268 ms. |
| `full-mir-consumer-abi-bounds-v38-300s` | 92.1 MB | 100.0 MB | Outer-bound capture alone was a negative result; routine 248 regressed to 293,877 ms. |
| `full-mir-consumer-abi-row-capture-v39-300s` | 134.7 MB | 140.8 MB | Timed out at 300,560 ms; routine 192 at 102,775 ms, routine 448 at 231,271 ms, and routine 640 at 298,374 ms; no gen2. |
| `full-mir-consumer-abi-owner-v40-build` | 2565.3 MB | 2554.5 MB | Exact final-source integrated C driver compiled in 55,007 ms below the fixed cap. |
| `full-mir-consumer-abi-exact-reuse-v41-build` | 2346.8 MB | 2336.6 MB | Exact-source integrated C driver compiled in 52,722 ms below the fixed cap. |
| `full-mir-consumer-abi-exact-reuse-v41-300s` | 157.2 MB | 162.3 MB | Timed out at 300,227 ms after routine 640 at 228,455 ms, routine 704 at 238,884 ms, and routine 896 at 288,574 ms; no gen2. |
| `full-mir-consumer-abi-optional-fast-v42-build` | 2515.0 MB | 2503.6 MB | Exact-source integrated C driver compiled in 53,265 ms below the fixed cap. |
| `full-mir-consumer-abi-optional-fast-v42-300s` | 214.4 MB | 216.6 MB | Timed out at 300,115 ms after routine 704 at 162,849 ms, routine 896 at 192,157 ms, routine 1,600 at 241,729 ms, and routine 1,920 at 293,147 ms; no gen2. |
| `full-mir-consumer-key-dispatch-v43-build` | 2523.0 MB | 2511.6 MB | Exact-source integrated C driver compiled in 52,451 ms below the fixed cap. |
| `full-mir-consumer-key-dispatch-v43-300s` | 215.1 MB | 217.1 MB | Timed out at 300,268 ms after routine 704 at 162,255 ms, routine 896 at 190,875 ms, routine 1,600 at 239,277 ms, and routine 1,920 at 290,054 ms; no routine 1,984 or gen2. |
| `full-mir-consumer-cfg-backedge-batch-v44-build` | 2433.5 MB | 2427.0 MB | Exact-source integrated C driver compiled in 52,316 ms below the fixed cap. |
| `full-mir-consumer-cfg-backedge-batch-v44-300s` | 202.7 MB | 205.0 MB | Timed out at 300,682 ms after routine 704 at 162,403 ms, routine 896 at 191,236 ms, routine 1,600 at 240,535 ms, and routine 1,920 at 291,308 ms; CPU negative/noise versus v43, no routine 1,984 or gen2. |
| `full-mir-consumer-branch-row-bundle-v45-build` | 2534.1 MB | 2522.6 MB | Exact-source integrated C driver compiled in 52,025 ms below the fixed cap. |
| `full-mir-consumer-branch-row-bundle-v45-300s` | 204.8 MB | 206.9 MB | Timed out at 300,345 ms after routine 704 at 161,510 ms, routine 896 at 189,756 ms, routine 1,600 at 238,576 ms, routine 1,920 at 288,324 ms, and the first routine 1,984 marker at 298,381 ms; no routine 2,048 or gen2. |
| `full-mir-consumer-phi-prefix-bundle-v46-build` | 2556.9 MB | 2546.0 MB | Exact-source integrated C driver compiled in 52,507 ms below the fixed cap. |
| `full-mir-consumer-phi-prefix-bundle-v46-300s` | 202.1 MB | 204.3 MB | Timed out at 300,163 ms after routine 704 at 163,937 ms, routine 896 at 193,024 ms, routine 1,600 at 242,500 ms, and routine 1,920 at 293,716 ms; CPU negative/noise versus v45, no routine 1,984/2,048 or gen2. |
| `full-mir-consumer-phi-prefix-admission-v47-build` | 2535.7 MB | 2524.3 MB | Exact-source integrated C driver compiled in 51,436 ms below the fixed cap. |
| `full-mir-consumer-phi-prefix-admission-v47-300s` | 207.7 MB | 209.7 MB | Timed out at 300,384 ms after routine 704 at 158,438 ms, routine 896 at 186,805 ms, routine 1,600 at 234,127 ms, routine 1,920 at 283,594 ms, and routine 1,984 at 293,201 ms; recovered v46 and improved on v45, no routine 2,048 or gen2. |
| `full-mir-consumer-branch-index-admission-v48-build` | 2567.8 MB | 2557.0 MB | Exact-source integrated C driver compiled in 51,479 ms below the fixed cap. |
| `full-mir-consumer-branch-index-admission-v48-300s` | 206.3 MB | 208.3 MB | Timed out at 300,615 ms after routine 704 at 158,817 ms, routine 896 at 187,672 ms, routine 1,600 at 235,166 ms, routine 1,920 at 285,333 ms, and routine 1,984 at 295,075 ms; CPU negative/noise versus v47, no routine 2,048 or gen2. |
| `full-mir-consumer-block-slice-admission-v49-build` | 2587.7 MB | 2578.1 MB | Rejected exact-source experiment compiled in 60,860 ms below the cap but materially slower than v48. |
| `full-mir-consumer-block-slice-admission-v49-300s` | 202.3 MB | 205.0 MB | Rejected experiment timed out at 300,269 ms after routine 704 at 166,252 ms, routine 896 at 194,769 ms, routine 1,600 at 243,264 ms, and routine 1,920 at 293,502 ms; 8,169 ms later than v48 and no routine 1,984/gen2. Reverted by `85cee4ff`. |
| `full-mir-consumer-resource-raw-capture-v50-build` | 2445.2 MB | 2438.9 MB | Rejected exact-source experiment compiled in 62,385 ms below the cap but 10,906 ms slower than v48. |
| `full-mir-consumer-resource-raw-capture-v50-300s` | 178.2 MB | 182.3 MB | Rejected experiment timed out at 300,680 ms after routine 704 at 189,951 ms, routine 896 at 222,884 ms, routine 1,600 at 279,085 ms, and routine 1,728 at 296,959 ms; no routine 1,792/2,048 or gen2. Reverted by `c5ee6e62`. |
| `full-mir-consumer-resource-local-scan-v51-build` | 2576.8 MB | 2565.8 MB | Rejected exact-source experiment compiled in 56,417 ms below the cap but 4,938 ms slower than v48. |
| `full-mir-consumer-resource-local-scan-v51-300s` | 192.6 MB | 195.6 MB | Rejected experiment timed out at 300,614 ms after routine 704 at 173,196 ms, routine 896 at 204,052 ms, routine 1,600 at 255,976 ms, routine 1,728 at 272,517 ms, and routine 1,792 at 287,519 ms; it lost v48's routine-1,984 marker and produced no gen2. Reverted by `6879f0c0`. |
| `full-mir-consumer-block-successor-pair-v52-build` | 2591.5 MB | 2580.9 MB | Rejected exact-source experiment compiled in 67,265 ms below the cap, 15,786 ms slower than v48. |
| `full-mir-consumer-block-successor-pair-v52-300s-observed` | 172.9 MB | 176.6 MB | Rejected experiment timed out at 300,560 ms after machine routine-index completion at 83,531 ms and routines 704/896/1,600/1,664 at 198,093/233,293/291,565/298,472 ms; no routine 1,728/2,048 or gen2. Reverted by `40037e52`. |
| `full-mir-consumer-llvm-performance-v53-build` | 2399.0 MB | 2389.0 MB | Accepted-source LLVM projection compiled successfully in 139,295 ms below the cap and preserved focused C/LLVM semantics. |
| `full-mir-consumer-llvm-performance-v53-300s-observed` | 214.0 MB | 210.8 MB | LLVM projection timed out at 300,518 ms after machine routine-index completion at 73,014 ms and routines 704/896/1,600/1,856 at 172,586/202,127/250,313/295,125 ms; it was slower than C v48 and produced no gen2. |
| `full-mir-consumer-c-clang-v54-build` | 2557.6 MB | 2546.5 MB | Accepted-source C projection compiled with the explicit Windows clang host toolchain in 42,649 ms, 8,830 ms faster than GCC v48, with byte/failure parity preserved. |
| `full-mir-consumer-c-clang-v54-300s-observed` | 206.0 MB | 208.0 MB | clang-built C projection timed out at 300,665 ms after routines 704/896/1,600/1,920/1,984 at 160,553/188,638/237,074/286,528/296,279 ms; build-time win but runtime negative/noise versus GCC v48, no gen2. |
| `full-mir-consumer-json-ascii-constants-v55-build` | 2516.9 MB | 2505.4 MB | Rejected exact-source experiment compiled in 51,536 ms; focused C/LLVM, bounded SHA, and wrong-ABI behavior remained exact. |
| `full-mir-consumer-json-ascii-constants-v55-300s-observed` | 202.9 MB | 205.3 MB | Rejected experiment timed out at 300,480 ms after routines 704/896/1,600/1,920 at 162,958/191,199/240,394/291,112 ms; 5,779 ms later than v48 at routine 1,920, no routine 1,984/gen2. Reverted by `1f77b0bc`. |
| `full-mir-consumer-match-owner-filter-v56-build` | 2587.0 MB | 2576.3 MB | Rejected exact-source experiment compiled in 69,158 ms; focused C/LLVM, component, bounded SHA, and wrong-ABI behavior remained exact. |
| `full-mir-consumer-match-owner-filter-v56-300s-observed` | 166.2 MB | 170.5 MB | Timed out at 300,772 ms after routine 1,408 at 296,916 ms. After adjacent-v48 MIR-start normalization it was 2,420/2,929/5,767 ms slower at routines 256/704/896; reverted by `c9e8011a`. |
| `full-mir-consumer-v48-current-control-300s-observed` | 174.2 MB | 177.9 MB | Adjacent unchanged-source control under the current load: MIR-to-AST start at 83,190 ms, routines 704/896/1,600/1,664 at 198,926/233,149/290,131/296,995 ms; no gen2. |
| `full-mir-consumer-match-routine-owner-v57-build` | 2588.3 MB | 2577.6 MB | Accepted exact-source C driver compiled in 56,640 ms; focused C/LLVM, component, bounded SHA, and wrong-ABI behavior passed. |
| `full-mir-consumer-match-routine-owner-v57-300s-observed` | 197.5 MB | 200.4 MB | Timed out at 300,609 ms after routines 704/896/1,600/1,664/1,728/1,792/1,856 at 172,807/202,276/251,736/258,128/267,628/281,858/296,651 ms. Normalized gains over adjacent v48 are 17,102/21,856/29,378/29,850 ms at 704/896/1,600/1,664; accepted, no gen2. |
| `full-mir-consumer-match-routine-owner-v57-adjacent-v58-control-300s-observed` | 177.5 MB | 181.1 MB | Adjacent accepted v57 control timed out at 300,250 ms; MIR-to-AST started at 80,208 ms and routines 256/704/896/1,600/1,664/1,728 completed at 104,993/191,418/224,809/280,783/287,747/298,614 ms; no gen2. |
| `full-mir-consumer-loop-branch-owner-v58-build` | 2587.9 MB | 2577.0 MB | Accepted exact-source C driver compiled in 60,952 ms below the fixed cap. |
| `full-mir-consumer-loop-branch-owner-v58-bounded` | 0.0 MB sampled | 0.0 MB sampled | Exit 0 in 1,688 ms; the process finished between 100 ms samples, output remained 414 bytes with the established SHA. |
| `full-mir-consumer-loop-branch-owner-v58-wrong-abi` | 0.0 MB sampled | 0.0 MB sampled | Exit 1 in 1,672 ms with the owned ABI diagnostic and no output; the process finished between samples. |
| `full-mir-consumer-loop-branch-owner-v58-300s-observed` | 197.3 MB | 200.0 MB | Timed out at 300,470 ms after routines 704/896/1,600/1,664/1,728/1,792/1,856 at 173,630/202,723/252,244/258,345/267,970/282,271/297,340 ms. Normalized gains over adjacent v57 are 13,115/17,413/23,866/24,729/25,971 ms through 1,728; accepted, no gen2. |
| `mir-lower-loop-branch-owner-v58-llvm-build` | 315.5 MB | 318.3 MB | Focused LLVM `mir_lower` compiled in 4,104 ms; C/LLVM valid output and invalid-ABI failure were byte-equal. |
| `full-mir-consumer-loop-branch-owner-v58-integration-completion` | 3072.1 MB | 2459.3 MB | Reached MIR-to-AST completion at 387,029 ms, then stopped on the unchanged memory limit at 1,059,616 ms inside expression graph construction; no output. |
| `full-mir-consumer-expression-arena-linear-v59-ready-proof-build` | 2590.1 MB | 2579.1 MB | Exact-source v59 driver compiled in 66,274 ms below the fixed cap. |
| `full-mir-consumer-expression-arena-linear-v59-ready-proof-bounded` | 0.0 MB sampled | 0.0 MB sampled | Exit 0 in 1,336 ms; 414 bytes with the established SHA. |
| `full-mir-consumer-expression-arena-linear-v59-ready-proof-wrong-abi` | 0.0 MB sampled | 0.0 MB sampled | Exit 1 in 486 ms with the owned ABI diagnostic and no output. |
| `full-mir-consumer-expression-arena-linear-v59-integration-completion` | 801.8 MB | 749.4 MB | Reached MIR-to-AST completion at 429,211 ms and failed closed at 1,645,538 ms on the positional graph/surface identity mismatch; no output and no memory-limit crossing. |
| `v59-expression-surface-count-probe-full` | 230.4 MB | 233.2 MB | Completed in 498,952 ms: 41,299 surfaces, 35,638 persisted-required lanes, and 1,758 parser-only lanes. Flat MIR contains only 34,962 roots. |
| `full-mir-consumer-structured-occurrence-v60-build` | 2480.3 MB | 2473.7 MB | Exact-source v60 C driver compiled in 69,368 ms below the fixed cap. |
| `full-mir-consumer-structured-occurrence-v60-observed-build` | 2575.8 MB | 2564.5 MB | Observed bootstrap driver compiled in 65,293 ms below the fixed cap. |
| `full-mir-consumer-structured-occurrence-v60-integration` | 1130.3 MB | 1041.1 MB | Expression graph done at 1,673,958 ms, semantic analysis done at 1,674,754 ms, then timed out at 1,800,768 ms during assignment body typing; no graph error, cap crossing, or gen2 output. |

The cursor run completed in 869,913 ms before the pressure owner stopped it
inside routine `SemanticExpressionGraphNodeKind`. `e5587bee` then removed the
complete production instruction and graph Strings. Its first fixed-cap run
completed all current 8,266 initializer rows and MIR facts, started JSON near
2,956 MB, and advanced to 40,263,680 bytes before escaped/quoted leaf results
crossed the cap at 810,472 ms.

`6329356f` moves only those file-boundary leaves into a call-local pool and
destroys it after synchronous `FileWrite`. The successor run exited 0 in
675,355 ms. Peak private was 3,064.3 MB, with `driver_oracle.exe` at
3,063.1 MB; two processes and no compiler/link subprocess were observed. The
artifact is valid `pgy.mir.v1` with 2,345 routines, 142 declarations, and
SHA-256
`1621adf4070bc778dd90493e29db857c22f13722d951bea8a94d1241e9ee884e`.
The full JSON parse and closing `]}` were observed. The production gate is
green, but its 7.7 MB sampled margin is narrow and does not close the broader
semantic/MIR live-state debt.

The consumer measurements are CPU failures, not memory failures. The first
cursor implementation called generated `strlen(json)` at least three times per
routine/block/instruction row, implying about 8.8 TB of avoidable length
walking before field reads. Exact-bound readers removed that debt and reached
`routine-index:done` for the first time. Allocation-free normal-key comparison
then completed the instruction scan, machine admission, and input boundary.
`157c340b` next removed about 2.45 TB of logical declaration-field walking and
at least 118.9 TB from the routine fact prefix. `d62553ee` captures routine
headers, instruction results, and instruction-local arrays once, then moves
structural-merge selection from worst-case O(B^3) candidate-local BFS to
O(B^2) branch-local BFS. The full artifact contains 20,022 blocks, 34,091
instructions, 3,532 phi rows, and 214,151 expression-graph nodes. Its first
top-level routine is only 2,063 bytes with one block/instruction, so the fixed
window is dominated by the admitted machine path and accumulated routine
work, not by that routine or memory.

`67502f50` closes another observed hidden length path. The 34,091 null
machine-layer tokens performed about 1.766 TB of whole-document length walking,
and the minimum kind/name routine decode added about 243 GB, because bounded
reads still materialized through native `Substring(json, ...)`. The common
JSON owner now uses the caller limit while materializing strings, and machine
null reads use `SubEqualsWithLen`. The unchanged 300-second run advanced from
the first routine to 16 routines at only 63.4 MB peak private. This remains
RED: no run opened a partial gen2 C artifact.

`190d0dbf` closes the next structural duplication. The admitted program view
captures 2,345 routine, 20,022 block, and 34,091 instruction spans once and
carries kind/source type plus machine contact/layer spans. Machine admission
and per-routine fact construction no longer rescan nested structure. Review
also found and removed a whole-program `StructureReady` call from every routine
builder; the component contract rejects its return. The v3 fixed-window run
still ended at the 16-routine marker, so the removed work was real but not the
dominant remaining cost. Routines 1-64 contain only 274,581 of 51,741,503
routine-object bytes (0.531%); neither marker is completion. Peak private was
85.2 MB, `limit_exceeded=false`, and no gen2 file was opened.

`06f6994d` closes the instruction-local repeat-scan seam reached by that run.
Merely changing fact-table accessors to `ref` did not improve the v9 timing.
Generated-C inspection corrected the earlier diagnosis: `String` is passed as
a `char *`, and `JsonObjectFactTable` stores that source pointer plus bounds;
it does not deep-copy 51.8 MB into every table. The real cost was repeatedly
revalidating the same instruction object and rediscovering fields/bounds from
the same 51.8 MB-backed source view. Exact-bound ABI/resource common paths
avoid those repeated object/table reads: the observed
instruction ABI step fell from 492 ms to 9 ms, the resource step from 646 ms to
0 ms, and routine 16 from 133,593 ms to 69,919 ms. The next real producer-wire
counterexample was `FindTopLevelComma`, whose loop header has seven CFG
predecessors but two incoming inventory values. The phi owner now preserves
that wire meaning and v11 passed it.

The v13 full-artifact run kept `output_capture_complete=true`,
`limit_exceeded=false`, and only 88.6 MB peak private while reaching routine 64
at 99,447 ms and routine 128 at 164,457 ms. This is a CPU bottleneck, not a
return of the 3 GiB memory defect. The final v14 driver build stayed below the
cap and its bounded output remained exactly 414 bytes with SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
No run reached `consumer:mir-to-ast:done` or opened a complete
`driver_gen2.c`.

`dd68d6f3` closes the next measured routine-local seam. Each routine fact-index
construction now captures the render/result fields in one strict scalar pass,
while the admitted program index remains structure/identity-only. The active
MIR-to-AST reconstruction reuses that bundle, but the later expression-graph
and assignment post-passes still reconstruct a routine index and remain an
open re-entry seam. Phi context is computed lazily
only for blocks that actually contain a phi, and its incoming-backedge fact is
read from the canonical routine index instead of recomputing dominators. The
current v23 build completed in 47,746 ms below 3 GiB and preserved the exact
414-byte bounded SHA. Its 180-second run used 87.0 MB peak private / 95.3 MB
working set and moved routine 128 from the v14 300-second run's 165,019 ms to
160,331 ms. The improvement is real but modest; repeated scalar reads were not the
dominant remaining cost. `output_capture_complete=true`,
`limit_exceeded=false`, and no gen2 output was opened.

`a5d56f42` closes the required ABI-layout repeated-scan seam exposed by the
v29-v37 observation ladder. The v38 outer-bound-only experiment did not improve
the required row cost, proving the nested object/field validation and second
identity walk were dominant. The ABI owner now captures the nested row once,
validates at most eight fields, and hashes the captured values in canonical
semantic order. Raw instruction value spans remain location evidence, not a
second ABI authority. The producer compatibility entrypoint delegates to the
same captured identity owner, and component/ABI gates reject the deleted path.

The v39 300-second run used 134.7 MB peak private / 140.8 MB working set and
moved routine 192 from v38's 233,517 ms to 102,775 ms. It reached routine 640 at
298,374 ms, versus v38 ending near routine 248. The exact final-source v40
driver built in 55,007 ms below 3 GiB and preserved the exact 414-byte bounded
SHA. A bounded wrong-ID tuple exits 1 with the owned ABI diagnostic. This is
material executable progress but remains RED for bootstrap completion: no
`consumer:mir-to-ast:done` marker and no gen2 file exist.

`0da9c5c2` closes the identical-required-row revalidation seam without making
the 28-bit layout ID a cache authority. Before routine 640, 580 required rows
reduce to five complete tuples. The ABI owner remembers a tuple only after the
full order-independent capture and canonical hash succeed. Reordered JSON is a
safe miss and full revalidation; the same ID with a changed nested offset is a
miss and rejection. The focused C/LLVM fixture locks down both cases.

The exact-source v41 driver built in 52,722 ms at 2,346.8 MB peak private /
2,336.6 MB working set. Its 1,251 ms bounded result remains exactly 414 bytes,
and the wrong-ID input exits 1 without opening output. The full fixed-window
run moved routine 640 earlier by 69,919 ms (23.4%) relative to v39, passed the
old routine-704 falsifier, and reached routine 896 at 288,574 ms. It timed out
at 300,227 ms with 157.2/162.3 MB peak private/working set. This remains RED:
there is still no `consumer:mir-to-ast:done` marker or gen2 file.

`bf8a56b8` closes the duplicate optional ABI wire-read seam. The existing
routine scalar scan now carries type-value readiness, while the ABI owner keeps
the sole semantic decision and accepts the common optional case only with exact
raw `0`/`null` tokens. The v42 driver built in 53,265 ms below 3 GiB, preserved
the exact 414-byte bounded SHA, and rejected the wrong-ABI input in 551 ms with
no output. Its fixed-window run reached routine 704 at 162,849 ms, routine 896
at 192,157 ms, and routine 1,920 at 293,147 ms before timing out at 300,115 ms.
Peak private/working set was 214.4/216.6 MB. This is 76,035 ms and 96,417 ms
earlier at the shared 704/896 markers and 1,024 routines farther than v41, but
still RED for bootstrap completion: no `consumer:mir-to-ast:done` or gen2 file.

The v42 interval census covers all 29 completed 64-routine intervals. Interval
time versus instruction count has R-squared 97.43%; the remaining 425 routines
contain 7,873 instructions. The measured linear projection places
`top-level-routines:done` near process timestamp 355.9 seconds, before the still
unmeasured string join and AST inventory cost. That is a projection, not green
evidence and not permission to enlarge the 300-second diagnostic window. The
next measured CPU owner is `BuildMirRoutineFactIndex`: focused samples spend
1,051 of 1,464 ms (71.8%) in fact-index construction. Inside its scalar scan,
34,091 instruction objects expose 852,275 keys and currently trigger eleven
semantic key comparisons per key (9,375,025 calls). Dispatching plain keys by
their already-owned raw length reduces that to about 1,159,094 calls while an
escaped-key fallback preserves JSON equivalence and duplicate rejection. This
is the first minimal executable seam because it changes no fact owner, bundle,
or ABI decision. If linear cost remains after that, the broader candidate is
the second full instruction-object scan from
`BuildMirRoutineInstructionFactBundle` into
`MirRoutineInstructionScalarCaptureWithin`, after the admitted program index
already scanned every instruction for identity. A separate CFG census found
15,940 tail BFS calls but could not distinguish them from the strongly
collinear instruction/block volume; do not introduce a CFG cache or move phi
ownership on correlation alone.

`dfc8e406` executed the smaller falsifier first. Plain scalar keys now run only
their matching raw-length comparison group, while escaped keys retain the full
semantic fallback. The exact-source v43 driver built in 52,451 ms below 3 GiB,
preserved the 414-byte bounded SHA, and rejected the wrong-ABI input with no
output. The fixed-window run reached routine 1,920 at 290,054 ms, 3,093 ms
(1.06%) earlier than v42, then timed out at 300,268 ms without routine 1,984.
Peak private/working set was 215.1/217.1 MB. The comparison-count reduction is
real but not dominant. The next owner-directed move is inside the existing CFG
graph owner: compute the routine backedge result once, migrate the fact-index
consumer, and ratchet the per-edge dominator call. Keep structural merge and
phi unchanged for this slice.

`73133678` performs that owner migration and deletes the old edge-local
function; `ec4b9eef` proves the malformed result reaches an explicit consumer
failure. The static remaining-tail model reduces backedge BFS calls from 9,144
to 4,128, but the fixed-window v44 result is a CPU negative/noise observation.
The exact-source driver built in 52,316 ms below 3 GiB and preserved the bounded
SHA and wrong-ABI rejection. It reached routine 1,920 at 291,308 ms, 1,254 ms
(0.43%) later than v43, before timing out at 300,682 ms. Peak private/working
set was 202.7/205.0 MB. No routine 1,984, `mir-to-ast:done`, or gen2 file exists.
The single CFG owner and negative ratchet remain useful, but this evidence does
not authorize structural-merge or phi caching as the next CPU track.

`4ee29ce2` closes the next measured routine-lowering seam in the existing
routine-local fact bundle. Its scalar pass records one unique branch global row
per block, and condition, loop-transfer, and match-binding consumers no longer
reconstruct typed instruction views to search each block. The complete input
contains 20,022 blocks, 34,091 instructions, and 8,387 branch blocks; the three
mandatory searches removed at least 77,112 repeated view reconstructions.
Duplicate branches and forged row identity fail closed, and a component ratchet
forbids the old call in `routine_lower.pgy`.

The exact-source v45 driver built in 52,025 ms below 3 GiB, preserved the
414-byte bounded SHA, and rejected the wrong-ABI input without opening output.
The fixed-window run reached routine 1,920 at 288,324 ms and the first routine
1,984 marker at 298,381 ms before timing out at 300,345 ms. Peak
private/working set was 204.8/206.9 MB. That shared 1,920 marker is 2,984 ms
(1.02%) earlier than v44. This remains RED for bootstrap completion: no routine
2,048, `consumer:mir-to-ast:done`, or gen2 file exists.

`99e76e76` closes the remaining explicit whole-instruction phi scan. The
existing routine-local bundle records the leading phi count per block and a
late-phi invalid sentinel. The phi semantic owner reconstructs only those rows;
program-owned kind, predecessor, arity, result, incoming-use, and backedge
checks remain intact. The full artifact view count falls from 34,091 rows to
3,532, and the active 1,984-through-2,048 interval falls from 1,161 rows to 104.
The old all-instruction loop and fallback are statically rejected.

The exact-source v46 driver built in 52,507 ms below 3 GiB, preserved the
414-byte bounded SHA, and rejected the wrong-ABI input without opening output.
The fixed-window run reached routine 1,920 at 293,716 ms before timing out at
300,163 ms with 202.1/204.3 MB peak private/working set. That marker is 5,392
ms (1.87%) later than v45, and v46 did not recover v45's routine-1,984 marker.
This is a CPU negative/noise result rather than a speedup. The owner closure
remains, but the same revision must not be rerun for a favorable sample and the
window/cap must not be enlarged.

`a05aaf06` removes the v46 read-path regression at its exact boundary. The phi
owner admits program-row identity, block counts, and the routine-local bundle
once, reads block prefix counts directly, and rejects invalid counts. The
one-use accessor is deleted. This cuts full-artifact admission from 20,022
block calls to 2,345 routine calls, removing 17,677 admissions and at least
406,571 shape checks without adding a cache or global/local aggregate.

The exact-source v47 driver built in 51,436 ms below 3 GiB, preserved the
414-byte bounded SHA, and rejected the wrong-ABI input without opening output.
The fixed run reached routine 1,920 at 283,594 ms and routine 1,984 at 293,201
ms before timing out at 300,384 ms with 207.7/209.7 MB peak private/working
set. Routine 1,920 is 10,122 ms earlier than v46 and 4,730 ms earlier than v45;
routine 1,984 is 5,180 ms earlier than v45. This is measured CPU progress, but
routine 2,048, `consumer:mir-to-ast:done`, and gen2 output remain absent.

`8074d6c8` moves branch selection from the bundle accessor to the admitted
routine fact index. The branch row stays in the existing bundle, while the new
boundary checks routine/block identity, local/global range, scalar span, and
final program-owned kind. The old accessor is deleted and all three consumers
use the index owner. The full validation-loop lower bound removes 21,910 full
admissions and at least 503,930 shape checks without adding a cache or
aggregate.

The exact-source v48 driver built in 51,479 ms below 3 GiB, preserved the
414-byte bounded SHA, and rejected the wrong-ABI input without opening output.
The fixed run reached routine 1,920 at 285,333 ms and routine 1,984 at 295,075
ms before timing out at 300,615 ms with 206.3/208.3 MB peak private/working
set. Those markers are 1,739 and 1,874 ms later than v47. This is an
owner/fallback closure and CPU negative/noise result, not a speedup. Routine
2,048, `consumer:mir-to-ast:done`, and gen2 output remain absent.

`80a54268` tested the next larger static candidate by replacing
`EmitBlockStmts`' three checked accessors with one block-boundary guard and
direct instruction/scalar construction. Its C/LLVM cross-block negative and
component ratchet passed, but generated-code cost dominated the eliminated
shape checks. The driver build regressed from v48's 51,479 ms to 60,860 ms.
The full run reached routine 1,920 at 293,502 ms, 8,169 ms later than v48, and
lost routine 1,984 before timing out at 300,269 ms. Peak private/working set was
only 202.3/205.0 MB, so this was a CPU/code-shape regression, not memory.

`85cee4ff` reverts that experiment. `git diff 7dd78069..85cee4ff` is empty, so
the v49 revert restored byte-for-byte v48 source while the failed attempt
remains auditable in history. Do not reintroduce the same direct block
aggregate construction or equate lower static check count with lower
generated-program cost.

`530682af` then moved resource runtime ABI top-field capture into every routine
instruction scalar and bundle row. The focused C/LLVM and bounded gates were
green, but the driver build regressed to 62,385 ms. The full run reached only
routine 1,728 at 296,959 ms and timed out at 300,680 ms with 178.2/182.3 MB peak
private/working set. Even the machine routine-index marker moved from v48's
67,567 ms to 80,353 ms, so the regression is broader generated-program cost,
not resource-row validation alone or memory pressure. `c5ee6e62` reverts the
carrier experiment. `5e12cf43` keeps only the independently found correctness
ratchet: a non-resource instruction carrying a stray runtime ABI value now
fails closed, with current-source C/LLVM negatives and the component contract
green.

The focused instruction-writer gate now compares raw, unnormalized
String/file bytes for five small, graph-heavy, match, destructure, and
ABI/optional fixtures through both C and LLVM, then compares C/LLVM file bytes.
It also corrupts instruction row count and proves the sentinel output is not
opened or truncated. The earlier 11,262-byte small fixture SHA remains
`007d5dacdd8157a0d5dd0f87975f82c7abe2fa4987983afb3945bd61b29efc09`.
`FileOpen` failure is observable and fails closed; the current runtime does not
return a `FileWrite` status, so the writer must not claim write-error detection
that the runtime cannot provide.

Broad runs remain explicit RED evidence. `mir_machine_layer_smoke.sh` reaches
the MIR consumer and then fails at the existing `local declaration is missing
its MIR ABI type fact`. `mir_json_parity.sh` expects an enum variant substring
without the current `param_types:[]` field. A filtered `dir_walk` /
`break_after_stmt` attempt stops earlier because reconstructed C lacks current
`PGY_RUNTIME_PANIC` declarations. Update those owners only when their
executable slice is active; none is a green CFG/runtime verdict.
The current focused DRV-2 body attempt also stopped while compiling
`valid_array_builtins` because emitted C omitted `<string.h>` and runtime panic
declarations. The separately isolated `nested_if_in_loop` current-driver run
is green, and a forged one-predecessor header phi is rejected with
`MIR phi facts are missing or inconsistent`; this does not relabel the broad
body gate green.

## Historical observed gates through v60

Green on implementation checkpoint `3418b0f3` plus the retained predecessor
measurements:

- `tests/self_hosted_component_contract_smoke.sh`;
- `tests/self_hosted/parity/driver_rung2_structured_expression_order_owner.sh`;
- `tests/self_host_program_graph_unification_smoke.sh` with
  `phase=unified structural_owners=1`;
- focused native/self `forloop` `mir_json_parity.sh`: range loop-init graph is
  start `0`, range branch graph is stop `3`, and a start-graph regression is
  rejected;
- v60 exact-source and observed driver builds: exit 0 in 69,368/65,293 ms at
  2,480.3/2,575.8 MB peak private;
- v60 bounded consumer: exit 0, 414 LF-normalized bytes, established SHA;
- v60 wrong-ABI and missing/invalid graph mutations: exit 1 with owned
  diagnostics and no output;
- v60 full integration: graph and semantic completion observed before the
  1,800-second timeout in assignment body typing, with 1,130.3/1,041.1 MB peak
  private/working set and no memory-limit crossing;
- `tests/self_hosted/parity/json_bounded_string_owner_smoke.sh` (C/LLVM,
  plain, escaped, empty, and truncated exact-bound strings);
- `tests/self_hosted/parity/mir_program_routine_index_owner_smoke.sh` (C/LLVM,
  partitions, direct-field spans, malformed scalar tails, missing structure,
  corrupted counts, invalid row guards, explicit negative CFG successor
  rejection, missing/unique/duplicate/forged/out-of-block branch-row facts, and
  leading/late/truncated phi-prefix facts, plus invalid match owners,
  zero-block parallel-array misalignment, wrong-kind match rows, match
  name/type count mismatch, forged non-match local arrays, malformed FOR
  scalar rows, invalid branch sentinel, same-endpoint scalar range, and
  no-branch block-span mutation);
- `tests/self_hosted/parity/mir_cfg_graph_query_owner_smoke.sh` (C/LLVM,
  diamond, re-entry, unrestricted-ranking, self-loop, tie, fallback, and
  detached-component witnesses);
- `tests/self_hosted/parity/driver_rung2_mir_abi_layout_negative_owner.sh`;
- `tests/abi_ownership_shape_smoke.sh`;
- `tests/protocol_registry_smoke.sh`;
- `tests/gate_sot_single_owner_smoke.sh`;
- integrated `driver_bootstrap_main.pgy` C build under the 3072 MB pressure
  owner (`full-mir-consumer-loop-branch-owner-v58-build`): exit 0, 60,952 ms,
  2,587.9 MB peak private / 2,577.0 MB peak working set;
- v59 readiness-proof integrated C build: exit 0, 66,274 ms, 2,590.1 MB peak
  private / 2,579.1 MB peak working set;
- v59 bounded MIR consumer: exit 0 in 1,336 ms, 414 bytes, established SHA;
- v59 wrong-ABI mutation: exit 1 in 486 ms with the owned diagnostic and no
  output;
- v59 full completion attempt: MIR-to-AST done at 429,211 ms, fail-closed at
  1,645,538 ms, 801.8/749.4 MB peak private/working set, no gen2;
- v59 surface-count probe: 41,299 surfaces, 35,638 persisted-required lanes,
  1,758 parser-only lanes, proving the flat-root count mismatch;
- bounded MIR consumer byte check: 414 bytes, SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
- bounded wrong-ABI mutation: exit 1 with the owned ABI diagnostic and no
  output file;
- focused current-source resource runtime ABI negatives through C- and
  LLVM-built drivers, including missing/identity/payload/aux rows and a stray
  wrong-kind row on a non-resource instruction;
- `tests/build_pressure_contract_smoke.sh`;
- focused current-driver `nested_if_in_loop` MIR production/consumption plus a
  forged one-predecessor header-phi rejection;
- `tests/self_hosted/parity/module_manifest_resolver_parity.sh` (C/LLVM,
  clean plus malformed/missing manifest negatives);
- `tests/self_hosted/parity/air_graph_json_validator_parity.sh` (C/LLVM,
  clean, missing-key, and live-drift negatives);
- `tests/self_hosted/parity/mir_json_instruction_writer_byte_parity.sh`
  (C/LLVM, five raw String/file and cross-backend fixtures, plus invalid
  pre-open sentinel rejection);
- `instruction-string-pool-ready` pressure shard: exit 0, complete JSON below
  3072 MB;
- `tests/self_hosted/parity/semantic_initializer_environment_cursor_owner_smoke.sh`;
- `tests/self_hosted/parity/semantic_expression_environment_owned_lifetime_smoke.sh`;
- `tests/self_hosted/parity/initializer_projection_probe_parity.sh` (C/LLVM,
  including shadow/exit/destructure positives and self/sibling negatives);
- `tests/self_hosted/parity/driver_rung2_iteration_graph_use_owner.sh`;
- `python scripts/protocol_registry_gate.py`:
  `7 protocol rows valid; no authority duplicated`;
- `python scripts/sot_registry_gate.py`:
  `49 authorities, 41 derived fact carriers; CLOSED=29 BRIDGE=20 ACTIVE=0`;
- `git diff --check` and `git diff --cached --check`.

`tests/self_host_hard_contract_smoke.sh` remains RED at the unrelated existing
manifest assertion that `driver_rung2_owner.pgy` contain
`tests/cases/backend_compare/device_slot_machine_layer/main.pgy`. This was not
weakened or relabeled as success.

The shell gates must use `C:\Program Files\Git\bin\bash.exe` in the current
Windows environment. `C:\Windows\System32\bash.exe` resolves to WSL and fails
because `/bin/bash` is unavailable; that is an execution-environment failure,
not a project gate result.

## Historical temporary artifacts through v60

The ignored temporary tree is diagnostic evidence, not semantic authority.
During the 2026-07-28 cleanup, a command intended for exact probe binaries
traversed ignored `.tmp` paths and removed a broader set of ignored diagnostics.
No tracked file or protected dirty file was touched, but the historical
`.tmp/instruction_writer_pressure/driver_source_pool.mir.json` (formerly
51,807,108 bytes), `.tmp/driver_rung2_topology.exe`, and
`.tmp/native_zone_topology.c` are no longer present and are not recoverable from
Git. Any resumed v60 pressure run must regenerate and hash-check the exact full
MIR from its owner before use; the 40,263,680-byte RED partial must never be
substituted. The names below are historical references and their existence must
be checked rather than assumed. Pressure evidence was recorded under
`.tmp/build-pressure/instruction-stream-ready.*` and
`.tmp/build-pressure/instruction-string-pool-ready.*`. Consumer progression is
captured by `full-mir-consumer-admitted.*`,
`full-mir-consumer-exact-bound.*`,
`full-mir-consumer-machine-twofield.*`,
`full-mir-consumer-key-compare.*`, `full-mir-consumer-exact-span.*`, and
`full-mir-consumer-routine-fact-exact.*`,
`full-mir-consumer-routine-indexed.*`, and
`full-mir-consumer-cfg-owner.*`, and
`full-mir-consumer-document-index.*`, and
`full-mir-consumer-program-instruction-index-v3.*`,
`full-mir-consumer-int-cfg-v14-300s.*`, and
`full-mir-consumer-routine-scalar-bundle-v23.*`,
`full-mir-consumer-abi-bounds-v38-300s.*`, and
`full-mir-consumer-abi-row-capture-v39-300s.*`, and
`full-mir-consumer-abi-exact-reuse-v41-300s.*`,
`full-mir-consumer-abi-optional-fast-v42-300s.*`, and
`full-mir-consumer-key-dispatch-v43-300s.*`, and
`full-mir-consumer-cfg-backedge-batch-v44-300s.*`, and
`full-mir-consumer-branch-row-bundle-v45-300s.*`, and
`full-mir-consumer-phi-prefix-bundle-v46-300s.*`, and
`full-mir-consumer-phi-prefix-admission-v47-300s.*`, and
`full-mir-consumer-branch-index-admission-v48-300s.*`. The rejected/reverted
v49 evidence remains under
`full-mir-consumer-block-slice-admission-v49-300s.*`. The rejected/reverted v50
evidence remains under
`full-mir-consumer-resource-raw-capture-v50-300s.*`. The rejected/reverted v51
evidence remains under
`full-mir-consumer-resource-local-scan-v51-300s.*`. The rejected/reverted v52
successor-pair evidence remains under
`full-mir-consumer-block-successor-pair-v52-{build,bounded,wrong-abi,300s,300s-observed}.*`;
only the `300s-observed` run has valid routine-marker evidence. The v53 LLVM
projection evidence remains under
`full-mir-consumer-llvm-performance-v53-{build,bounded,wrong-abi,300s-observed}.*`.
The v54 explicit clang-via-C evidence remains under
`full-mir-consumer-c-clang-v54-{build,bounded,wrong-abi,300s-observed}.*`.
The rejected v55 local-call evidence remains under
`full-mir-consumer-json-ascii-constants-v55-{build,bounded,wrong-abi,300s-observed}.*`.
The rejected v56 evidence remains under
`full-mir-consumer-match-owner-filter-v56-{build,bounded,wrong-abi,300s-observed}.*`;
its adjacent unchanged-source control is
`full-mir-consumer-v48-current-control-300s-observed.*`. The accepted v57
evidence remains under
`full-mir-consumer-match-routine-owner-v57-{build,bounded,wrong-abi,300s-observed}.*`.
The adjacent v57 control for v58 is
`full-mir-consumer-match-routine-owner-v57-adjacent-v58-control-300s-observed.*`.
The accepted v58 evidence is
`full-mir-consumer-loop-branch-owner-v58-{build,bounded,wrong-abi,300s-observed}.*`;
its focused LLVM build is
`mir-lower-loop-branch-owner-v58-llvm-build.*`.
The first completion continuation is
`full-mir-consumer-loop-branch-owner-v58-integration-completion.*`; it reached
expression graph construction and stopped at the 3,072 MB cap. v59 evidence is
`full-mir-consumer-expression-arena-linear-v59-{integration-completion}.*` and
`full-mir-consumer-expression-arena-linear-v59-ready-proof-{build,bounded,wrong-abi}.*`.
v60 evidence is
`full-mir-consumer-structured-occurrence-v60-{build,observed-build,integration}.*`.
The current diagnostic executables are
`.tmp/self_hosted/driver_bootstrap/driver_rung2_v60_structured_occurrence.exe`
and
`.tmp/self_hosted/driver_bootstrap/driver_bootstrap_v60_structured_occurrence.exe`.
The temporary count
probe source/executable were deleted after their result was recorded; its
pressure evidence remains under `v59-expression-surface-count-probe-full.*`.
The latest full consumer evidence passes the former 35,638-vs-34,962
positional mismatch, completes graph construction and semantic analysis below
1,131 MB private, and times out at assignment body typing. The requested
`v60_full.c` does not exist because output is committed only after verified
completion. The rejected v50
executable is
`.tmp/self_hosted/driver_bootstrap/driver_rung2_v50_resource_raw_capture.exe`;
its 414-byte bounded result is
`.tmp/self_hosted/driver_bootstrap/v50_bounded.c`. These files are diagnostic
evidence only, not semantic authority or commit content.

## Historical v60 next executable work

1. The resource ABI and block-successor pair read seams are abandoned. Their
   focused correctness gates passed, but their carrier/local-scan/pair shapes
   materially regressed generated-driver CPU. Do not try another representation
   of either read consolidation.
2. The accepted-source LLVM v53 projection is connected and semantically
   byte-equal, but it is slower than C v48 and reaches only routine 1,856 in the
   fixed window. Keep LLVM's general performance-primary direction, but do not
   use the current LLVM-built DRV-2 as the active bootstrap executable and do
   not change semantics to make that positioning claim pass.
3. The explicit clang-via-C v54 projection improves integrated driver build
   time but is runtime negative/noise against GCC v48 and produces no gen2.
   Keep the existing Windows GCC-first default and do not confuse host compile
   speed with generated compiler progress.
4. The v55 JSON ASCII experiment removed the expected generated calls, but
   routine 1,920 regressed by 5,779 ms and routine 1,984 was lost. It is
   reverted. Do not retry literal constants, a shared ASCII helper, backend
   intrinsics, or unchecked character access; the static call-count hypothesis
   did not identify an integrated dominant cost.
5. The v56 match-local filter is reverted because its extra alignment pass
   regressed adjacent-v48 normalized markers. Accepted v57 directly consumes
   the routine-index owner; accepted v58 then consumes each loop-projection
   branch row once and improves every adjacent-v57 normalized marker through
   routine 1,728. Keep both closed shapes; do not add a third match-local read,
   a second branch pass, or rendered-condition fallback. v58 still produces no
   gen2, so count it as owner closure and generated-driver CPU improvement, not
   hard substitution progress or completion.
6. v60 closes the structured graph occurrence seam. Keep its repeated-key
   semantics, one final arena, producer coverage, deleted sequence view, and
   native range-stop producer ratchet. Do not reopen raw positional pairing,
   text lookup, deduplication, or a second graph/order.
7. The active seam is `SemanticAstAssignmentTypeFactsFromArtifact`, entered at
   `semantic-body-type-stage assignment:start` after graph and semantic
   completion. Add narrow stage/row evidence only as needed to locate repeated
   owned work; do not start a broad assignment fixture campaign.
8. Rerun the same complete artifact under the unchanged 1,800-second / 3,072 MB
   pressure gate. Acceptance for this slice is `assignment:done`, or one exact
   assignment row, owner read, and falsifying case if it still cannot finish.
9. Continue the same run through statement/body verification. If it emits a
   complete `driver_gen2.c`, compile that C as the bootstrap object-code
   boundary; do not regenerate another oracle MIR.
10. Make the generated gen2 driver consume the same complete compiler source
   and emit `driver_gen3.c`. Do not divert into global SoT closure or fixture
   expansion; close only a concrete owner seam that blocks this exact run.
11. Compare complete gen2/gen3 artifacts and behavior. Use the existing bounded
   MIR fixture only as a focused falsifier when diagnosing a failure on this
   path, not as an independent breadth campaign.
12. Keep the separate foreach assignment-binding, ABI-type, stale enum-parity,
   and reconstructed-runtime-header failures out of this active CPU seam. Do
   not raise the fixed integration time or memory limits as a substitute for
   closing the owner path.

## Historical v60 resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, `src/self_hosted/OWNERS.md`,
   `docs/180_compiler_logical_spine_handles_gates.md`, and
   `docs/semantics/sot_owner_spine_registry.md`.
2. Verify HEAD/origin, `git status --short --branch`, and the three protected
   dirty files above.
3. Re-run the component, structured-expression-order, program-graph, and
   focused native `forloop` MIR parity gates through Git Bash before a broad
   build.
4. Confirm no unrelated `pgy`, `genN`, `driver_oracle`, `gcc`, `cc1`, or
   `clang` process is active before the pressure gate; concurrent broad builds
   invalidate attribution.
5. Regenerate and hash-check the exact v60 full MIR first; the former frozen
   temporary artifact was removed in the ignored-temp cleanup. Then continue
   the v60 executable under the fixed 1,800-second/3,072 MB pressure gate; the
   first required marker is `semantic-body-type-stage assignment:done`.
6. Treat current source, registries, and executable gates as authoritative if
   this snapshot disagrees with them.

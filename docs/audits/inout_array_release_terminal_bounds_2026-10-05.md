# Terminal effect bounds: P0 integration falsifier

Observed: 2026-10-05 KST. Base HEAD:
`d0fa49ea95928c007a6670f2e2fbfb7b7479441b`, with uncommitted P0 owners.
This is read-only analysis evidence, not compiler authority or a closure receipt.

## Observed evidence

- Official seed v5 still refused MIR-root node 51162:
  `unproved_shallow_mutation_entry`, before generation 2. Evidence:
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.jnMU3p`.
- The small `shallow-error-return-probe.pgy` independently reproduced the
  boundary: the exact fresh `names` descriptor had exclusive storage, while
  a terminal error-result handoff projected unknown/retirement bound zero.
- A subsequent terminal-site candidate passed 6 positives and 11 refusals per
  C/LLVM analyzer in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.aA3bHJ`.
- That candidate **wrongly admitted**
  `.tmp/inout-array-release/terminal-hides-later-retention-negative.pgy`
  under both analyzers. An earlier conditional return was the minimum negative
  site; ignoring that site also hid a later, reachable nonterminal handoff and
  mutation of the same descriptor. No supplied unsafe program was emitted/run.
- Both analyzers correctly refused
  `.tmp/inout-array-release/terminal-return-later-read-negative.pgy` at
  `unproved_indexed_read_entry`. This separate refusal does not cure the
  hidden-normal-effect counterexample.

## Required boundary

Terminating and continuing effects must not compete for one earliest bound if
consumers later ignore the terminating winner. Preserve every relevant normal
negative obligation and same-return conflicts behind an exact owned fact seam.
Do not reconstruct a hidden later event in a consumer or grant permission from
callee/local spelling. Alias/return retention, rebind, missing facts and detached
execution remain independent negative obligations.

The former `inout_index_after_unknown_negative.pgy` used a fully observed shallow
mutator rather than an opaque callee. Its negative now contains actual deferred
execution; `inout_index_after_shallow_positive.pgy` preserves the legitimate
current-descriptor read as a positive. Both were independently observed with the
earlier C source analyzer; they are not installed-driver evidence.

At this observation Root kept semantic writes frozen while the other writer
finished its candidate. The user subsequently interrupted Main and selected
sole source/integration/Git completion here. P0 requires fresh official seed/driver, actual default C/LLVM execution
of the Alrescha inout-release repro, preserved-artifact negatives and exact-head
CI. No SoT row or GUI readiness is closed by these focused observations.

## Later integration evidence

- Continuing retention is now an observed successful-call transition of the
  exact storage definition, not an inference from initial Unknown provenance
  or a future negative bound. The terminal-tail guard applies to borrowed
  effects; existing ordered own-transfer validation still owns consumption.
- Tail calls through logical negation or short-circuit RHS are legitimate;
  an earlier compound-expression effect cannot use that exception.
- Official seed v9 passed those boundaries and reached the final return of
  `SemanticAstGenericParameterFactRowsFromOwnerNode`. Terminal constructor
  inputs have no continuing escape bound. Ordered aggregate reservation now
  checks their admitted identity and current owned state rather than inventing
  a continuing effect. Prior escape/retirement and deep-element provenance
  requirements are unchanged.
- C/LLVM observers each passed 11 positives and 19 compile-only refusals in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.M0zIPw`, receipt
  `.tmp/inout-array-release-borrowed-read-gate14.log`. This includes borrowed
  aggregate return, duplicate storage and post-transfer source-use negatives.
- Collection integration7 still timed out at its final 60-second actual
  producer unit. A separate phase diagnostic took 152.48 seconds and refused
  an element-use boundary. Neither is full collection integration success.
- The later context-bound formal retention lookup removed that measured
  repeated global scan. Integration9 passed the full C leg, including its
  unchanged 60-second actual producer unit, but the LLVM observer compilation
  then exceeded the unchanged 120-second limit. Its receipt is
  `.tmp/inout-array-release-collection-integration9.log`; no LLVM execution
  or full integration success is claimed.
- Seed v11 reached owned mutation followed by a current local read. The
  terminal-entry proof now checks exact storage and absence of sibling or
  same-expression constructor capture. Seed v12 passed that case and reached
  direct inout formal forwarding. The latter uses complete formal-use and
  absent-retention facts rather than mode-only permission.
- Focused gate18 passed 13 positives and 21 compile-only refusals for each
  C/LLVM analyzer in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.pDJC5H`. Earlier
  hidden-continuation, terminal retention, shallow sibling, aggregate borrow,
  duplicate transfer and constructor-capture counterexamples remain refused.
  Official seed v13 and installed-driver execution are still pending.
- Seed v13 subsequently passed direct terminal forwarding and reached a
  conditional local use of a fresh outer-loop generation at node `51211`.
  Exact mapping uses the actual MIR-root control source, not the separate
  `mir_lower/main.pgy` entrypoint:
  `.tmp/inout-array-release-bootstrap-v13-exact-boundary-context.log`.
  Call admission now consumes its existing conditional-generation owner;
  the narrower duplicated definition check was removed.
- Focused gate19 passed 14 positives and 23 compile-only refusals per C/LLVM
  analyzer in `.tmp/self_hosted/collection-borrowed-descriptor-read.6KckA7`.
  The added conditional-generation positive succeeds; intervening repeated
  own transfer and deferred same-generation negatives remain refused.
  Seed v14 and collection integration10 are running, not completed receipts.
- Seed v14 later refused generic-call capture inside its work loop at node
  `51242`, `unproved_indexed_read_entry`. Integration10 timed out at the
  unchanged 60-second actual C producer unit; its isolated unchanged seven-row
  rerun passed in 46.35 seconds, not a full integration PASS.
- Gate20 rejected generation-relative retirement because terminal branch
  cleanup poisoned the normal continuation. Gate21 preserved that scope but
  refused the early own entry ahead of a later continuing retirement. The
  revised call-entry owner may precede a future bound, never erase a prior one.
  Ordered same-scope transfer use is still required. Gate22 passed 15 analyzer
  positives and 26 compile-only refusals per C/LLVM, including nested repeated
  reads, nested consumption, post-transfer read and hidden normal consumption.
  Evidence `.tmp/self_hosted/collection-borrowed-descriptor-read.X2bzgH`.
  Official seed v15 and installed/default execution remain OPEN.
- Seed v15 later refused the same node 51242. The nested-function-exit
  candidate passed gate24: 16 analyzer positives/29 compile-only refusals per
  C/LLVM in `.tmp/self_hosted/collection-borrowed-descriptor-read.n32M5a`.
  Inner Break/Continue remains unproved entry; same-scope reuse reaches ordered
  `owned_argument_use_after_move`. The source generation owner stays 86 counted
  lines and does not grant storage or element authority.
- Seed v16 then passed names/types read and refused `actual_type_names` at
  `unproved_inout_copy_entry`. This is a reached producer-contract mismatch:
  GenericSpecializationAppend used shallow ArrayPush even though the later
  capture needs owned String elements. It now uses ArrayPushOwnedString on
  the original input rather than manufacturing a grant from Concat. Gate25
  subsequently passed 17 analyzer positives/30 compile-only refusals per C/LLVM
  in `.tmp/self_hosted/collection-borrowed-descriptor-read.Kj2gOq`. It is not
  installed, seed or full integration success.
- Seed v17 passed the accumulator and reached node 51578 at an unproved
  indexed String input to the generic symbol encoder. Copying that scalar
  through the established builtin avoids a new user-call permission grant.
  Gate26 passed 18 analyzer positives/31 compile-only refusals per C/LLVM in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.3gwTzQ`; raw indexed
  scalar return stays refused. Native C/LLVM each passed four exact naming
  assertions, recorded in the generic-symbol-copy SHA-256 manifest.
- Seed v18 passed naming and reached node 52506 at the same missing scalar
  call contract in runtime-value type lookup. The two indexed lookup inputs
  now use the already tested copy boundary without changing ABI layout policy.
  Native C/LLVM each passed five exact presence/preamble assertions, bound by
  `.tmp/inout-array-release-runtime-value-scalar-copy.sha256`. Caller-side
  returned String lifetime remains conservative. Official seed v19, fresh
  installation, full integration and current-head CI are still OPEN.
- Seed v19 passed runtime lookup and refused node 54508 at the participant
  alias lookup's raw selected String return. The production query now copies
  only the selected scalar; exact slice/unique-match meaning is unchanged.
  Fresh gate27 passed 19 analyzer positives/32 compile-only refusals per C/LLVM,
  including the equivalent raw-return falsifier, in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.b3RKyW`. Native C/LLVM
  each passed five exact lookup assertions, recorded in the participant-copy
  manifest. Seed v20/default installation/full integration/CI remain OPEN.
- Seed v20 then reached node 54573 in inherited contract-name collection.
  The reached producer retained a raw indexed String locally and shallowly
  inserted it in another array. Scalar materialization and owned insertion
  now use their existing, separate builtin contracts. Gate28 passed 20
  analyzer positives/33 compile-only refusals per C/LLVM in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.kkJRKH`; the original
  raw/shallow counterpart stays refused. Native C/LLVM each passed five exact
  mapping/duplicate/range assertions. Their contract-name-copy manifest owns
  value evidence only. Seed v21 is running; default/full integration/CI are OPEN.
- Seed v21 subsequently refused the same node 54573 at indexed-read entry:
  the contract table was a local aggregate alias. The step resolver now takes
  the canonical contract facts as a required direct ref input from its single
  validated caller; the alias is deleted and member permission is unchanged.
  Member gate4 passed 4 positives/9 refusals per C/LLVM. Production DIR oracle
  parity passed exact rows and negative mutations. Native C/LLVM lookup/name
  probes were refreshed against the new owner; nested ref forwarding executed
  `true` on both. Fresh member gate5 is running after the fixture nominal was
  renamed from Input to ContractEnvelope following an AIR capability refusal.
  Gate5 subsequently passed 4 positives/9 refusals per C/LLVM in
  `.tmp/self_hosted/member-indexed-read.4gaBJr`. Official seed v22 is running;
  default/full integration/CI remain OPEN.
- Seed v22 passed indexed-read proof and refused the same call at its value
  sequence boundary: default/callee/ordinal 0 identify the non-ref names
  formal. That input is now explicitly ref; no permission policy changed.
  Fresh member gate6 passed 4 positives/10 refusals per C/LLVM in
  `.tmp/self_hosted/member-indexed-read.Kv8Ttd`, including the independently
  observed value-formal refusal with exact diagnostic facts. DIR parity gate2
  is running; seed v23/default/full integration/CI remain OPEN.
- DIR parity gate2 subsequently passed; current-ref-names native C/LLVM
  lookup/name/ABI receipts were rebuilt and passed. Official seed v23 is
  running, not a completion receipt. Default/full integration/CI remain OPEN.
- Seed v23 later refused requires_names at mutable entry. Derived mutable
  requires/authorized arrays now use the existing String-array Clone contract,
  which duplicates both storage and String elements. Gate29 passed 21
  positives/34 refusals per C/LLVM; the current named-ref positive subsequently
  executed on both native backends, preserving the input after cloned storage
  was dropped. This does not prove returned String lifetime or installation.
  Fresh gate30 is running following that fixture correction; DIR gate3 and
  seed v24/default/full integration/CI remain OPEN.
- Fresh gate30 passed in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.eWeKz1`; DIR gate3 also
  passed current-source oracle rows and its negative mutations. Official seed
  v24 is running; default/full integration/CI remain OPEN.
- Seed v24 subsequently refused participant assembly at node 55663, boundary
  ArrayPush. Alias/type-name accumulation now uses owned String insertion;
  fresh gate31 passed 22 positives/35 compile-only refusals per C/LLVM in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.OgVPGU`. The identical
  shallow member accumulator stays refused at owned_string_drop. These are
  analyzer results, not installed/default or whole-compiler proof.
- DIR gate4 and native copied-member values passed, but seed v25 still refused
  node 55663 at ArrayPushOwnedString. Current formal-effect observation proved
  the resolver's participant aliases/type-name formals are unproved-element
  effect 5; adjacent read helpers have 3. The source's retained/user-call
  indexed scalars now copy through the existing builtin; pure comparisons do
  not change. An exact repeated caller/resolver pair is admitted with scalar
  copies and refused with raw retained scalars by both prior analyzers. Gate32,
  current native values/formal observation, DIR gate5 and seed v26 run serially;
  default/full integration/CI remain OPEN.

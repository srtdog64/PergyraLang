# Indexed String: exclude a proved non-owning local risk

Status: BOUNDED NATIVE REPAIR INDEPENDENTLY APPROVED; canonical/CI OPEN.
Base: main `1dae4522189bb48cf09dd299ca165c2807d8c228`, code checkpoint
`7d08b9db575a1a06c46b64b77a8cae6c61802a51`. A separate uncommitted
`docs/01_intent_first_design.md` change is outside this edit scope.

## Objective and complete chain

- Objective: remove a false local payload-retirement risk only when the
  current typed whole-body history proves that exact fresh empty Array never
  acquires String payload ownership. This is not a String source, an entry
  loan, result independence, a deep-release premise or a language extension.
- Priority: semantic identity and current evidence, fail-closed consumer,
  immutable handoff and behavioral negatives, then patch size.
- Existing owner: collection ownership / indexed String risk predicate in
  `semantic_indexed_string_borrow_call_argument`. It already excludes known
  borrowed-element arrays. UNKNOWN itself is never evidence of non-ownership.
- Production entry: native type checking on the unchanged full canonical
  program. A pending typed String of UNKNOWN source encounters each current
  local String-Array cleanup candidate. Existing source-specific checks and
  existing field/private-push recipes retain precedence. Only the otherwise
  opaque failure can issue this conditional candidate exclusion.
- Producer: bind the exact current empty LET, caller, initializer, type and
  local risk fact while scopes exist. Before successful semantic completion,
  prepare the existing routine index and verify its complete current claims.
  Every receiver use must be a direct read, checked ArrayLength, exact current
  private SHALLOW ArrayPush effect (receiver binding and source tuple), or an
  existing checked terminal nominal return. Reject aliases, captures, local
  carrier stores, rebindings/index writes, owned effects, deep release,
  unmodelled calls, own/inout handoffs and every unknown use. Check every
  branch/loop claim, not just the prefix before the diagnostic.
- Each exclusion also binds its exact current typed String value and exact
  non-NULL call/actual slot. Context-free shallow-retention checks retain their
  old guard; this repair does not invent their missing sink. Missing
  source-current facts never become current
  UNKNOWN. Preserve these use edges with the candidate's original claims at
  the immutable handoff, not just a boolean on the empty LET.
- A shallow push never grants ownership of its String payload. Other local
  risk candidates must still be checked. A later owned push in any branch or
  loop invalidates this entire exclusion. General source validity and the
  legacy aggregate-parameter release boundary are not proved by this repair.
- Last legitimate consumers: final semantic risk validation, then the same
  opaque collection evidence snapshot and actual HIR/MIR call/definition
  consumers. Move original claims once, after fallible validation; never keep
  a second AST scan or semantic context alive as a fallback authority.
- Forbidden fallback: UNKNOWN-to-source0, guessed formal independence,
  routine/builtin-name whitelist without its typed receipt, read-only masks
  as storage liveness, skipped risk candidates, copied user Strings, or new
  manual drops/annotations. Known concrete sources keep their old checks.

## Independent edit scopes on this one repair

- A: cohesive producer and whole-claims non-owning-risk replay, behind the
  existing indexed String / collection family. No HIR/MIR, tests, Git or builds.
- B: immutable snapshot ownership and actual HIR/MIR consumption of this
  premise; no producer or source fixtures. Preserve failure-before-MOVE.
- C: minimal source positives and alias/rebind/branch/loop/owned-effect
  negatives, plus phase/MIR withdrawal and coherent-forgery falsifiers in
  existing test owners. No compiler production files or process control.
- Root: shared private vocabulary, pending-risk hook/finalizer/destructor,
  Makefile, integration, all builds/WSL/process control, independent review,
  commits and remote publication. Agents do not edit shared scope files.

## Shared verification and remaining obligations

- First reproduce an otherwise rejected synchronous String use with an exact
  fresh empty local risk. Positive variants must not add a String copy/drop.
- Negative axes: alias then owned insertion, rebind to owned storage, owned
  insertion in one branch or later loop iteration, owned-producing payload
  effect, nonterminal carrier, another actual handing off the risk, capture,
  erased/replaced effect/claim/candidate identity. Unsafe inputs are analysis
  only; verify refusal at the new premise rather than an earlier unrelated
  parser error. Withdrawn facts fail; restoration recovers the positive.
- Additional review falsifiers: a missing/stale String source receipt; a safe
  and an unsafe risk in both declaration orders; original claims retained
  while an extra unclaimed actual alias/carrier/call/owned effect/drop is
  added in HIR/MIR; a nested capture's return presented as terminal. This
  premise never upgrades generated cleanup classification.
- Budgets: static gates60s, focused parity300s, integration shard30min.
  No larger budget/input reduction. Root identifies exact source/binary and
  verifies D/Linux equality, phase/MIR and strict C/LLVM controls before the
  unchanged fixed/current canonical. An observer which clears diagnostic state
  is never an acceptance compiler.
- Full canonical observer WvJGJz traversed4273 scopes and observed164
  error-bearing scopes (155 opaque-source primaries); these are diagnostic
  groups, not an authoritative remaining-defect count or progress metric.
- Full canonical, actual bootstrap/gen2==gen3, installed C/LLVM driver and
  required CI on one published SHA remain the finish line. This native repair
  is a prerequisite, not Pergyra substitution or whole-language safety.

## Integrated observations (2026-10-18 KST)

- Candidate native SHA256
  `a9cf6e3dd92709becea17a88fd7c771a0db8ec641504aaf93b90405e631d9684`
  in `/root/pgy-nonowner-20261018.5t1W96`; approved3a8bea source and binary
  remain untouched in their earlier validation directory. Pair8g9CYE verifies
  all94 changed/new source, test and Makefile paths against D byte-for-byte.
- Deliveryo1KDVC passes the unchanged599 static cap, build, phase12053/0,
  MIR468/0, exact9 refusal Counters and the full strict C/LLVM suite. Runtime
  WnCtbE separately observes `9\n0\n` and `8\n8\n2\n` for both backends.
  Alias/capture/rebind/local-carrier cases retain their existing earlier
  guards; branch, later-loop, later-actual and both risk orders reach the new
  non-owning-history failure. No negative program executes.
- A real issuer mismatch was fixed: the LET execution row issues reachability
  only, not a caller. Pending/current binding now joins the existing collection
  function identity and typed initializer caller, with whole-index LET
  membership, instead of demanding an unissued LET caller. One test now
  mutates that real initializer issuer. Empty vacuous return-element ownership
  and shallow-mutation withdrawal retain their original cleanup meaning.
- Full fixed/current canonicalZzFauK remains RED: both have18435 errors and16
  warnings with MIR0. Fixed wall35.60s/1699200KiB; raw current resource receipt
  is in that lab. This is not a defect-count reduction. ObserverBS3Dyb locates
  the next first error in the same routine at141:54, after the former80:61
  boundary, at a reassigned String whose initializer-only lineage is retired.
  Do not revive that old initializer or interpret unavailable as source0.
- Broader acceptancenToSX3 PASS: old-six currency groups, semantic3230/0,
  transpile1012/0, AIR156/0, Map24 C/LLVM positives plus58 analysis-only
  negatives, and four IR flow gates. Separate sanitizerhf8i6T passes
  phase12053/0 and MIR468/0 with a live-UAF witness; leak detection is OFF.
  Unit refreshMvLmyo and separate sanitizer refreshCcNTZs then pass the final
  phase12059/0 and MIR471/0. The terminal falsifier first requires a real
  nonowner receipt with exactly one nominal terminal. In phase tests an
  admitted lambda and the outer return swap live AST parents with original
  claims retained; a separate mutation swaps only the sealed claim edges.
  Both refuse and restoration succeeds. MIR uses a lambda-free admitted
  control, inserts an unclaimed nested lambda parent around the original
  terminal return, and observes evidence/full/emission refusal plus recovery.
  Unrelated lambda admission in the general source visitor is not proved by
  this test; the initial decoy-lambda control correctly stayed RED and was
  not counted. Frozen independent review is pending these exact receipts.
  The new risk exclusion is not yet called approved,
  canonical-closed, bootstrap-complete or CI green.

## Next reached blocker: read-only proposal, not implemented

The first current failure is `ArrayPush(variant_names, variant_name)` at
`ast_enum_fact_owner.pgy:141:54`. Its declaration-only lineage is correctly
withdrawn by the branch assignment. Do not revive that old initializer.

The bounded sufficient condition is independence of **every definition** of
one exact String binding: its initializer and all possible assignment RHSs
must each have an existing current source proof equal to0. Then branches and
loop order cannot select an array-backed version. This does not require a new
reaching-definition/SSA model. It also does not grant payload ownership.

The existing typed control claims are the inventory owner, not an AST rescan.
A complete repair must bind the definition, every write and each protected
use; reject hidden inout/own, aliases, captures, unsupported writes/control,
self/mutual source cycles and any nonzero/unknown RHS; preserve the original
claims and result-source dependencies through the existing opaque collection
snapshot to actual HIR/MIR DEF/ASSIGN/use consumers. Old deferred-read, return,
drop and cleanup permission checks remain unchanged. Missing/withdrawn new
evidence must not fall back to a retired initializer.

Diagnostic leaf probeZw1aFQ parses the six exact production declarations
(including whitespace/window dependencies) into a small program, reassigns
their identities before typing, and observes existing ready result queries:
DelimitedRangeText=0, alias initializer=0, TrimSourceRangeReuse=0, but the
reassigned binding's original-lineage read correctly fails. Ten checks pass.
The latter function can return its input; this observation queries its full
existing result proof, not just Substring. This is a bounded real-declaration
probe, not the full canonical's current proof or an admission compiler.

Still to establish: full-program dependencies at prepare, all write forms
in the reached typed-claim contract, and branch/backedge hidden-write tests.
No source annotations, manual copies or canonical rewrites are selected.

## Reopened actual-role integration boundary

Frozen review v2/a9cf is REJECTED, not an approved checkpoint. Root reproduced
an original risk identifier reused as a new MIR assignment RHS: evidence,
whole-MIR and emission all accepted (fZYwNj). The first d58 correction rejects
all three (VmyyVJ), but the direct RETURN role has the same missing coupling.
Do not start the reassigned-String proposal above until this boundary closes.

The complete same-owner repair is actual-role consumption, not another syntax
membership check. A risk descriptor cannot be a DEF/ASSIGN RHS or direct
RETURN. An admitted terminal nominal constructor can be used only at its exact
original direct RETURN, not a definition RHS or discarded STMT. String element
reads are not descriptor transfers and must keep their existing admission.
New calls/own/inout/channel parents still require original exact child/sink
claims; no current query creates evidence or a cleanup permission.

One explicit source-evaluation query rejects a bare risk descriptor and an
admitted terminal constructor in non-return operand-root roles. Definition
queries consume it for their RHS. The return query additionally takes the
actual statement, rejects a bare risk descriptor and joins a protected
constructor to its original caller/RETURN edge. HIR actual CFG statements and
MIR actual operand roots consume those distinct roles; generic recursive
expression traversal remains a membership check, not permission to change a
node's role. No fake NULL assignment or old-read fallback is selected.

Source read APIs may move together into a cohesive source-snapshot TU to stay
within the existing cap. The original opaque snapshot's one struct definition
moves to its private header; identity, allocation, immutable payloads, MOVE
and destruction stay with the same owner. Root updates the build list only.

Independent edit scopes: A public role signature plus HIR/MIR consumers; B
the same snapshot's source queries/private layout extraction; C original-AST
and original-claims retained, type-correct RHS/RETURN/terminal-STMT falsifiers
and payload-read positive. Root owns protocol agreement, Makefile, identified
builds and all process/Git/review actions. No other production edits overlap.

Acceptance is the four earlier axes plus each real role mutation: direct
collection evidence, whole MIR and emission refuse, exact restoration passes.
Retain the terminal nominal-return control and payload-read positive. Then
rerun the original focused strict C/LLVM gate, broader regression, sanitizer
and full unchanged canonical; bounded green is still not canonical or CI
closure. Budgets and no-known-red-push policy above are unchanged.

### Repaired-role observations

- Candidate native55c332cdebaabc21c4a89810ab5a1ad652923036f8f438f610d05e6674b279f2
  passes deliveryLMqXEA: size/build, phase12059/0, MIR478/0 and full strict
  C/LLVM, with the final source/binary manifests unchanged. The earlier
  delivery1e3R7y is incomplete: a const-only test update invalidated its final
  source hash after its tests; LMqXEA reruns the complete frozen slice.
- Current ETh3ky observes exact RHS-reuse evidence/full/emission refusal and
  restoration, unlike rejected a9cf/fZYwNj. Formal typed-Array assignment,
  direct Array RETURN, terminal constructor DEF and terminal constructor STMT
  mutations all refuse at these three boundaries. Normal terminal nominal
  return and shallow-pushed String element read remain admitted. The earlier
  current-source receipt, two risk orders and nested-capture tests still run.
- Post-reviewLsRnZz passes the six currency groups, semantic3230/0,
  transpile1012/0, AIR156/0, Map24 C/LLVM positives/58 analysis-only negatives,
  four IR flow gates and separate ASan/UBSan phase12059/0 + MIR478/0.
  Live-UAF witnesshf8i6T is retained; leak detection is OFF. Existing adjacent
  compiler/test warnings remain; this is not a warning-free claim.
- PairRlkd8L verifies96 source/test/Makefile paths D/Linux byte-equal. Runtime
  ZtlSeu verifies both safe source outputs C/LLVM with empty stderr. Source
  observations2pRjbp retain2 positives/9 refusals on this exact native.
- Full fixed/current AMbb4V is still RED:18435 errors plus16 warnings, MIR0,
  wall31.29s/31.23s and peak1698852KiB/1697336KiB. Observer sCdSlF confirms
  first prior_error=0 remains141:54. This role repair changes the reached
  safety boundary, not the remaining canonical error count or self-host
  substitution. Bootstrap, installed driver, published-SHA CI remain OPEN.
- Independent frozen review APPROVED this bounded v3 repair only. Bundle
  `E:\PergyraValidation\nonowner-20261018-roles-v3` rehashes98 source/nav
  files and117 evidence files with mismatch0. Source-manifest SHA256 is
  `446709ce5ae2b0ec701c40b8126d1ec22118eb1dc88e3019fed5534626912b74`;
  evidence-manifest SHA256 is
  `534f4c0389f04e00dd96b4ccea0b54eee7b02532c0b6a77c2e0b5f8faac66516`.
  Approval covers local preservation, not a known-red push, canonical closure,
  bootstrap/installed-driver acceptance or whole-language memory safety.

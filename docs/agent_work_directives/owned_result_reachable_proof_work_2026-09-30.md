# Reachable owned-result proof work

Date: 2026-09-30 (Asia/Seoul)
Status: IMPLEMENTATION COMPLETE — focused acceptance, not whole P1 closure
Base revision: `7a9fe09d8293170c4ce676de6464486cd25ca348`
Starting direct-MIR owner hash: `B2DB3A6E2BB25BAB2CDE4DB65ECA70658CF5A4FB9D8CA143F0123A1A43D08A80`.

The previous packet added executable falsifiers and verified the concurrent
depth-unit repair. It was progress, but did not complete the user's code-fix
goal. The active collection-ownership P1 and its registered authority remain.

## Shared objective card

- Objective: ownership admission validates each reachable callable body once
  per query, independent of the number of paths reaching that same body.
  Stop recursive proof expansion and reject active cycles without native-stack
  growth. Keep exact producer identity and all terminal-return obligations.
- Priority: ownership semantics and stable callable handles; fail-closed input;
  completed fact reuse within the current immutable plan; bounded work; caps.
- Fact owner: existing direct-MIR owned String result owner, a projection of
  `semantic.hashmap_collection_ownership`. No new authority or global cache.
- Last consumers: collection transition readiness and owner-handle call-result
  argument admission, then the existing C/LLVM projection paths.
- Forbidden fallback: type/name guesses, active-node-as-completed reuse,
  skipping a borrowed return arm, C production retries, increased depth/cap
  algorithm allowances, or a copied alternative ownership policy. The generated
  compiler-internal caller projection may grow only by the exact new lifetime
  row (99 -> 105 lines); its generated-data bound follows that row, not proof
  complexity. The reached result owner remains below its existing 180-line cap.
- Gate/falsifier: a two-terminal shared-callee DAG, a long callable chain,
  callable/local cycles, a fresh/borrowed return join, exact-source mutations,
  and unchanged prior composition controls. Input is one immutable plan; count
  distinct reached routine bodies separately from path multiplicity.

## Independent scopes

- Root: sole implementation track in the existing direct-MIR owned-result
  owner; only its two current admission consumers if signatures must migrate.
  Keep the previous depth-unit repair's intent and all unrelated dirty work.
  Root owns integration, gate registration and handoff.
  The exact query-scratch retirement boundary adds one canonical internal caller
  row, its generator-owned projection and registry-derived call-site ratchet.
  This is not a public drop API or widening of external caller authority.
- `proof_cost_tests`: new test-only owner probe under
  `tests/self_hosted/fixtures/` and a new focused runner under
  `tests/self_hosted/parity/`; optional diagnostic scratch. No compiler source,
  Makefile, existing gate, shared caches, installed binaries or semantic owner.
- `proof_contract_review`: read-only graph/identity/cycle/cleanup review;
  findings under `docs/audits/` only. Do not open native-C or semantic-producer
  implementation tracks; report their separate issues as unclosed evidence.

## Validation and outputs

- Integration owner: root. One acceptance entry is the reached-owner proof
  probe plus the existing composed owned-result controls on the changed
  projection; source producer receipts must remain exact.
- Allowed: scoped apply_patch edits, source/hash/status reads, unique scratch,
  native MSYS2 UCRT64 compilation and focused execution. No commit, push,
  installed publication, network model calls, broad cleanup or whole matrix.
- Budgets: static60s, focused5min, integration shard30min. Do not repeat the
  failed whole-driver seed or raise memory allowance. Build the smallest real
  reached owner/projection needed for the falsifier; record its limited scope.
- Preserve MIR/projected output on refusal; public binary invalidation follows
  the existing binary-output owner. Do not replace either policy generically.
- Outputs are implementation candidate, executable unit/integration evidence
  and review observations. No P1 CLOSED, full world, fixed-point or performance
  completion may be inferred from one probe.

## Observed focused acceptance

Root integrated iterative per-query routine states and an explicit pending
stack in the existing owner, SHA-256
`D9179E99A53001B0BBC8BD8572E202B1E6F850281EA6C8FF45DB84B0214EB974`,
177 lines / cap180. The two admission consumers use the same owner; the exact
scratch retirement caller is registered once in the canonical registry and
its generated projection. The data-only projection is 105 lines / cap105.

The reached-owner gate passed five positive and fifteen negative cases plus
wrong-path internal-caller refusal. DAG6/10/14 body work changed from baseline
recursive query entries63/1023/16383 to candidate terminal inspections12/20/28;
chain4096 ran without recursive proof calls. Each query retired its two scratch
backings. Real changed-owner scalar projection passed six programs / twelve
C+LLVM runtime outputs; root also ran DAG14 in both backends and ten mutated
MIR refusals that preserved prior projected artifacts. These counts are scoped
evidence, not whole-driver asymptotics or public installed-driver acceptance.

Retained failures, source/artifact hashes, commands and limitations are in
`docs/audits/owned_result_reachable_proof_implementation_2026-09-30.md`.
The next serial repair is the three routine-build backing omissions under
`routine_build_last_consumer_retirement_2026-09-30.md`. Body-bundle Bool/leaf
coverage and repeated cross-boundary plan proof remain separate open evidence.

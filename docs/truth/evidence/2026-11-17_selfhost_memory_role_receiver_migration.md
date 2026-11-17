# Memory dependency: general role receiver ABI

Local validation on 2026-11-17, parent main `9f5573f18`. This records the
candidate carrying this file; required CI for that candidate is still pending.
The [execution plan](../../agent_work_directives/selfhost_memory_role_receiver_execution_2026-11-17.md)
maps the affected owners and consumers.

## Reproducible generation

Windows `D:\PergyraLang` and WSL `/root/mg-main` have identical normalized
production Pergyra maps: 2,838 files, SHA-256
`9d81fa5952055cb8fe8077571bf6016cff105a2da3ee0002d5937c3075787a96`.
WSL Git remains a `5b0d354fb` validation base with an explicit source mirror;
it is not the publication checkout.

| Artifact | SHA-256 |
|---|---|
| Native `bin/pgy` | `f44310784eb710ac708abea794e9db517b94a1f08096b2088e55c62ad628878a` |
| Codegen `gen2.exe` | `42d0d7756fd2eb90cc936b0300a704e67ddae94a7b0d1fb3c9f91d0bbbc872ff` |
| Installed `bin/pgy-self-driver` | `8980901e91fd17fc561ccc55244ddc20d1dafbf8aaf0c59a97fa39ad83f463c7` |
| Installed driver C, 235,160 lines | `b13e14743b64b28d42f42387c121bb70b58be0def5d2a5a2f51f444e2a857223` |
| Preserved String-generation driver | `74e0dcb2ea2fe8c2ac5d44d357ec4eadbf067b2a24e6cad4b13b9a890f35bb79` |

## Implemented boundary

- The resolved receiver target carries erasure through the existing method
  signature, instance inventory, routine identity and sealed GraphPlan
  partition. Every actual constructor, reseal and reached probe preserves it.
- C emits an erased first parameter and binds the admitted concrete pointer
  inside the body. LLVM retains its pointer ABI and admitted target layout.
- The existing role declaration owner joins represented role/ability methods,
  impl ranges, module paths and exact routine identities. Ordinary nominal
  declarations omit params: their joined routine signature owns the prototype.
  Explicit and omitted source `self` normalize to the same receiver ABI.
- Mutable identity parameter storage now consumes the existing mode. It does
  not invent an initial SSA definition; both local initializers reuse the
  existing cell-parameter receipt. Existing cell-store and predecessor checks
  remain authoritative. Readonly modes gain no mutable storage permission.
- Four old override-route owners and their classification/emission branches
  are deleted. The general emitter handles the former override fixture.
  Value-target operator handling remains a separate existing route.
- Ordinary member/direct calls cannot manufacture a role binding by changing
  both callee name and ID. Missing represented role facts terminate dispatch.

No receiver/ability SoT row is declared CLOSED: source constraints and
generic/default facts absent from MIR are not proved by this ABI migration.

## Observed checks

- Current-source codegen bootstrap: gen2 == gen3, 123,822 lines; fresh
  Pergyra-built installed driver build passed (`role-final-codegen.log`,
  `role-final-driver.log`). The native seed reported 17 warnings, no errors.
- `role_override_mir_replacement.sh` passed on that installed driver: native/
  self AST and canonical-MIR parity, C/LLVM runtime outputs, three declaration/
  routine permutations, original source negatives, 17 original/extended MIR
  negatives and six multi-role negatives on both targets (46 refusal pairs).
- The artifact ABI harness compiled typed erased slots and executed two
  caller-cell mutations on each target. Its LLVM test only exports the tested
  definitions for the C harness; the function bodies and layouts are unchanged.
- Removing exactly the two ordinary-call binding guards from a scratch emitted
  driver admits both forged calls on C and LLVM. The member-call mutant prints
  `override` instead of `base` on both targets. The unmodified driver refuses
  these inputs before artifact publication. Scratch compiler warnings include
  two discarded-const diagnostics; this is not a warning-free build claim.
- A copied-receiver artifact fails the mutation harness with exit 71. Replacing
  the erased C signature with a concrete pointer fails the typed slot compile.
  C and LLVM receiver harnesses pass ASan at -O0 with leak checks enabled.
  Local Clang 18 lacks its ASan runtime archives: LLVM IR and the C harness
  were instrumented separately and linked with GCC's ASan runtime. The first
  Clang final-link failure is an environment limit, not a sanitizer pass.
- Reached constructor consumers passed: owned-string reachable proof and
  legacy intent binding/signature tests, including 49 intent refusals.
  String runtime allocation checks retain all 22 value/OOM/unchecked controls.
- Component caps/generator drift, keyword inventory, build inventory, docs
  quality and SoT registry checks passed. The registry remains 95 authorities,
  214 derived carriers, 70 CLOSED / 23 BRIDGE / 2 ACTIVE.

## Full fixed input and memory

The complete frozen DRV-2 MIR has 953 declarations, 9,623 nongeneric routines
and 15 roles. SHA-256:
`e7767d3059972797baf41654ececd912bfb91a0755547afea6336e6af8bbdf19`.
It previously stopped at `role-declaration`. The new installed consumer passes
that boundary and refuses at `collection_ownership_transition`, ordinal 1054:
`AstMatchCasePatternFactFromText`, source row 1053 / syntax ID 32525,
`src/self_hosted/hir/ast_match_pattern_fact_owner.pgy`.

Peak RSS is 3,614,356 KiB. The owned receipt records child failure and a drained
process group under the unchanged 14 GiB address-space / 1,800 second limits.
No C artifact is published. This is the next falsifier, not a successful full
compile or a performance comparison against the earlier shorter refusal.

The existing memory gate passed, `.tmp/self_hosted/selfhost-memory.eeYw0S`:

| Complete input | Peak KiB | Required ceiling KiB |
|---|---:|---:|
| Semantic MIR consumer | 797,672 | 819,200 |
| MIR-lower source-to-C | 5,559,260 | 8,388,608 |

Source-to-C and source-to-MIR-to-C bytes are equal, SHA-256
`5e4fd4ac4615b875a82ed19c2bd48ecbfc1aa0b0069a0f1fc538f3c5a3954f58`.
The whole one-process DRV-2 source-to-C goal is not established by these checks;
its last measured String generation failed with OOM. It was not remeasured
after this ABI-only dependency. Owned-value/retire S1 and S2 remain OPEN.

## Preserved baseline failures

Both the saved String driver and the new driver fail the optional mutable-
nominal gate's raw `source_locals` comparison (binding IDs 9 vs 8), and both
fail the older role-operator gate with `erased declaration is invalid`.
The operator test was rerun with its explicit Linux driver variable. Neither
failure was skipped or weakened to manufacture a green result.

Raw logs, the full-process receipts, controlled mutants and ordinal-header
census are under WSL `.tmp/selfhost-memory-20261117/`, prefixed `role-`.
The constructor migration census covers 20 actual calls. A source measurement
over the 33 changed tracked production Pergyra files, with strings/comments
masked and only function parameter headers counted, changes ref 135 -> 126,
own 1 -> 1, inout 7 -> 7 (122 -> 102 functions). This is bounded ceremony
accounting, not a global DX score or hard-substitution numerator.

# Compiler-world embedded-zone lifecycle directive

Status: LOCAL CANDIDATE — PUBLICATION PENDING

Exact base: `52021bbf3912e65b7066d2b0adce832150699a76`, equal to
`origin/main` when this directive opened. Protected unrelated untracked paths
listed in `docs/current_work_collaboration.md` stay outside inspection, edit,
and staging.

This is a temporary coordination artifact. Current source, semantic owner
facts, the SoT registry, and executable gates remain authoritative.

## Shared objective card

- Objective: replace the production `PgyCompilerWorld` embedded-zone C guard
  with one semantic resource-carriage plan and executable thread-safe
  lifecycle. `driver_bootstrap_main.Main` materializes one fresh compiler
  world; downstream compiler routes carry that same identity by an explicit
  mutable borrow instead of returning or copying a world value.
- Priority order: preserve one world/zone identity; prevent every hidden
  `pthread_rwlock_t` copy; consume owner-directed nominal field-kind facts;
  delete the backend-only guard and factory return; add a negative ratchet;
  then minimize the patch.
- Fact owner: `SemanticAstNominalConstructorFacts` owns nominal kind, ordered
  field type, and `world_zone` field kind. The semantic zone-carriage verdict
  owns which fresh local node IDs carry zone resources, their exact embedded
  zone paths, and which parameter node IDs are admitted in-place mutable
  resource borrows.
- Last legitimate consumer: the admitted body-type codegen view. Statement
  emission consumes exact resource paths for lock init/destroy; function
  emission consumes admitted mutable-borrow parameter IDs. Neither consumer
  may rediscover resource policy from C layout or source spelling.
- Forbidden fallback: `PgyCompilerWorld` returned by value, default/by-value
  carriage of a world containing zones, copy-in/copy-out lowering for an
  admitted resource `inout`, raw lock copy/reinitialization, a
  `PGY_ZONE_THREADSAFE` compile-time rejection, a backend-local world-name
  exception, heap/reference-count lifetime invention, or a fixed source-level
  world lifetime.
- Verification gate and falsifier: a focused world-zone carriage gate must
  prove direct fresh world construction plus one admitted `inout` chain in
  single and thread-safe C, exact nested lock init/destroy, and semantic
  rejection with no C artifact for world copy/reassignment/default parameter
  or lock-bearing world return.
  A fresh current-source DRV-2 `driver.c` must contain one caller-owned
  `PgyCompilerWorld`, no factory return or embedded-zone guard, compile with
  `PGY_ZONE_THREADSAFE`, and execute a production installed-driver request.

## Production rung

- Production entrypoint:
  `src/self_hosted/compiler/driver_bootstrap_main.pgy::Main`, reached through a
  fresh current-source `pgy-self-driver --emit-c-artifact-verified` build.
- Direct bypass to delete:
  `src/self_hosted/codegen/emission/nominal_struct_emit_owner.pgy` emits
  `Pergyra embedded zone requires an admitted transfer plan`; the current
  compiler composition factory also returns `PgyCompilerWorld` by value and
  every route reconstructs it.
- Pergyra fact owner: the already-admitted nominal constructor field-kind rows,
  extended through the existing semantic zone-carriage bundle rather than a
  second backend inventory.
- Last orchestration consumer: installed CLI/read/artifact execution owners
  receive the one `inout compiler_world` created by `Main` and pass it to the
  existing world methods.
- Focused parity/negative gate: new world-zone carriage execution/admission
  gate, then current-source compiler-world/static gates and an isolated
  thread-safe DRV-2 request.

## Edit scope and forbidden overlap

- Semantic/resource carriage: the existing zone-carriage verdict, body bundle
  receipt/readiness/codegen view, and stable diagnostic registry only as
  required for exact resource rows.
- C projection: statement local lifecycle, admitted resource-`inout` parameter
  binding, and deletion of the embedded-zone guard.
- Production composition: compiler `Main`, installed request routing, and the
  compiler-world route owners needed to carry one `inout PgyCompilerWorld`.
- Gates/docs: focused parity/negative gate, existing topology/world ratchets,
  generated language inventory, this directive, collaboration ledger, and
  handoff.
- Forbidden overlap: general zone return/move syntax, arbitrary recursive
  ownership, world semantics unrelated to embedded zone resources, LLVM
  redesign, query/cache work, binary hardening, performance sharding, and any
  other SoT row.

No parallel implementation scope is open. The primary task is the sole edit,
integration, commit, push, and CI owner for this rung.

## Validation budget

- Static owner/negative gates: 60 seconds each.
- Focused semantic and single/thread-safe execution: 5 minutes.
- Fresh current-source DRV-2 build and one installed request: 30 minutes.
- Full matrices run only at the publication/merge boundary.

Outputs before focused execution are observations or implementation
candidates, not completion evidence. This directive may become `PUBLISHED`
only after the exact published revision is green in CI.

## Local candidate evidence

- `driver_bootstrap_main.Main` now constructs the production compiler world
  directly. The former by-value factory is deleted, and installed/read/artifact
  routes carry the same identity through admitted `inout` parameters.
- Semantic admission carries exact resource paths and mutable-resource
  parameter IDs. Statement emission initializes and destroys each carried
  nested zone lock; function emission binds admitted resource `inout` directly
  to the caller object without copy-in/copy-out.
- The focused world-zone gate passes exact `7` in single and thread-safe C and
  rejects copy, reassignment, default parameter carriage, and lock-bearing
  world return before artifact publication. Aggregate zone, topology,
  compiler-world, intent takeover,
  source-MIR, and component structural gates are also green locally.
- A fresh production DRV-2 candidate has SHA-256
  `3c5b2b9c690529e5fc236e73a9acdceb19153ba033a3071d4bde9b9038f4c6d1`.
  Its generated C has SHA-256
  `9fa10649498fe4fca84dd069dc0f99f8d7194925d2ddf087674eacd25cdadde1`,
  one caller-owned `PgyCompilerWorld`, zero embedded-zone guards, zero factory
  returns, four exact nested lock init/destroy pairs, and no world
  copy-in/copy-out. The same C compiled with
  `PGY_ZONE_THREADSAFE` to SHA-256
  `b1345fc43fb6a0a1095ecb1188e684fd8cee647609185171e5bd992563cbb340`
  and executed a production verified-artifact request with exact output `0`;
  the same driver rejected lock-bearing world return with no artifact.
- General zone return/move remains a distinct open seam. The SoT census stays
  `88/183` and `55/32/1`, and the integrated forecast stays 83%.

# Collection Field Ownership Continuation Review

Status: AUDIT COMPLETE; implementation not started by this packet.
Observed: 2026-09-30, 21:45-22:02 KST.
Base HEAD: `667f11ec01d361f1f1c17bbb3b0dab46de3c4190`.

Root prepared the continuation card and directive; `collection_field_proof_review`
and `collection_integration_gate_review` independently inspected read-only scopes.
Root reread the material owner/runtime/consumer/gate/build boundaries below and
rechecked the reported binary and gate hashes. Review completion is not compiler
acceptance, a source substitution, or registry closure.

The checkout is moving: 64 dirty status entries at the starting checkpoint,
71 after the directive addition and concurrent direct-CFG/gate edits, before
this audit was added. Root changed no compiler, runtime, fixture, test, Makefile,
or registry file. Preserve all pre-existing/concurrent changes.

## Executed baseline, deliberately narrow

Installed `bin/pgy-self-driver.exe --emit-mir-json-verified` returned 0 and emitted
MIR for each of the following fixtures at 21:45 KST:

- `borrowed_string_array_shallow_copy`: a current valid binding-move control.
- `unknown_string_array_alias_without_drop`: a current valid binding-move control.
- `borrowed_string_array_move_use_after_move`: must refuse moved-source use under
  the landed source contract; installed acceptance is a mismatch.
- `collection_parameter_member_move_deep_drop`: the pending borrowed aggregate
  formal-field falsifier; installed acceptance leaves this boundary exposed.

Evidence directory: `.tmp/collection-continuation-baseline.unFHlE`. Each run
preserved stdout, stderr, and MIR. No unsafe generated executable was run.
This observation does not establish current-source semantic/backend behavior.

Binary SHA-256 values were rechecked unchanged during preparation:

```text
A3525D2C302A4912648C2C3984793B5BCDB2A10A674E242070F7961FE351860A  bin/pgy.exe
8AB5C9D75040195862FC67407638AE44DAB53C2B65BEB7A98EA417D239FDB1B7  bin/pgy-self-driver.exe
```

The older private alias-refusal driver in the historical handoff is not evidence
for the subsequently committed binding-move or HashMap lifetime implementation.

## Field proof: observed source and missing fact

`ast_expression_environment_owner.pgy:256` creates three empty arrays, populates
them through `SemanticAstExpressionFunctionTables` and intent rows, and returns
`SemanticAstExpressionFunctionTableFacts(ok, names, returns, params)`. Inspected
table and builtin seed insertion paths use `ArrayPushOwnedString`.
`pgy_runtime_builtin_storage_inline.h:52` duplicates the input String; its paired
deep drop releases those duplicates and backing storage. Borrowed input metadata
does not by itself make these duplicated table elements borrowed.

The missing fact is the stable field-specific element lifetime across producer
inout effects, aggregate construction/return, caller extraction, formal release,
and emptied-descriptor writeback. `inout`, nominal type, and readiness `ok` do not
prove this lifetime.

`ast_collection_ownership_member_move_owner.pgy:32` resolves only scoped local
roots. A formal root therefore lacks the source binding/field identity used to
classify `member_move`, and can escape that UNKNOWN rejection branch. Native
`collection_ownership_fact.c:782-790` does carry the formal identity but admits
the UNKNOWN deep drop solely because its source is a current parameter. This is
the exact reached bypass; neither source observation is an executed fix.

`ast_expression_function_table_fact_owner.pgy:31` extracts and deep-drops the
three fields, writes emptied descriptors back, and invalidates readiness. The
last consumers are not simply the release function's comment:

- Initializer analysis: `ast_initializer_type_fact_owner.pgy:438-449`, after the
  analysis that borrows the tables.
- DRV-2 MIR projection: `driver_rung2_owner.pgy:180-195`, after projection JSON
  has consumed the derived facts.
- DRV-2 source-C: `driver_rung2_owner.pgy:361-383`, after
  `CodegenAdmittedCViewFromFactsOrDie`, including source intent admission. Do not
  retire the tables earlier merely because body analysis has completed.

### Candidate, not an approved architecture or implementation

Extend the existing identity/transition owners to identify formal roots by
signature SyntaxId without fabricating a local row. Carry reached field element
requirements and release postconditions by callee/formal/field identity, and
check them against real caller lifetimes. Use the existing exact builtin target
identity query in `ast_collection_ownership_identity_owner.pgy:14-30`, not names
alone. Reflect producer inout effects and retired/empty release writeback in the
same lifetime, then remove the native parameter exemption.

This is a candidate seam, not permission to start a general interprocedural
engine. First falsify the concrete table path. If it needs an unavailable fact,
record that exact owner/consumer/fixture rather than adding a type/name exemption
or claiming formal-root resolution alone completes the fix. Constructor, call
return, nested extraction, failure/partial production, and alias obligations
remain unresolved until executable evidence exists.

## Gate review: observations versus acceptance

The current semantic script includes valid binding moves, member move/restore,
Clone, MapKeys, and moved-source negatives. Neither it nor the carrier gate names
the pending borrowed-formal-field fixture or executes a real table-producer/
release positive. Structural table/lifetime grep smokes do not fill that gap.

The direct-backend probe is native-built from current imported Pergyra owners,
but its input MIR is made by the selected driver. A mixed old-driver/current-
backend probe does not prove one current-source compiler pair. The probe opens
its output only after `CompilerEmissionProduced`; the HashMap alias test checks
both C/LLVM sentinel preservation and refusal identity `program_readiness=27`.

Two narrower gaps must not be overstated as green coverage:

- The borrowed/UNKNOWN binding-move direct probe executes C only; the same MIR
  needs a direct LLVM control, including destination cleanup and source retirement.
- The HashMap early-return fixture inserts the key it then tests, so runtime
  reaches only the early exit. Two textual cleanup instructions are structural
  coverage of two exits, not runtime evidence that both paths release once.

The carrier mutation corpus tests the self-host reader, not every direct C/LLVM
consumer. Add missing/wrong formal-root and field provenance, forged OWNED,
borrowed `inout`, borrowed table construction, shadowed builtin, and post-release
use/drop refusals only as the concrete proof path reaches those obligations.
The shared integration gate must also preserve prior artifacts on refusal.

## Private-pair verification proposal — not executed

The semantic Make target has broad runtime/admission/carrier/build prerequisites.
`self-host-compiler` may replace the output driver and machine manifest. A private
output alone is insufficient: use private native, seed, object, and driver-build
directories as well. Use Make `compiler`, not its `pgy` alias, which can copy a
private compiler into repository `bin`. The config-stamp rule also cleans its
build directory, so the supplied directory must be newly owned by the run.

The semantic script currently clears its fixed WORK_DIR at lines 22-23,62;
an external WORK_DIR variable cannot override it. The carrier already uses
`mktemp`. Before running the shared semantic gate, root must give its run a unique
owned directory without overwriting the concurrently edited script or existing
evidence. No such test edit was made in this preparation packet.

The following MSYS2 sequence is an unexecuted candidate for a stable source tree,
after that run-directory boundary is fixed. Verify GCC/Clang/LLVM availability
first; failure is not a reason to silently skip a lane. Hash the full reached
source graph, dirty/untracked inputs, scripts, fixtures, and resulting artifacts
before/after each evidence boundary; drift invalidates a source-current claim.

```bash
set -euo pipefail
export PATH=/ucrt64/bin:/usr/bin:$PATH
export MSYS2_ARG_CONV_EXCL='*'
COLLECTION_RUN="$(mktemp -d "$PWD/.tmp/self_hosted/collection-current.XXXXXX")"
export PGY_BIN="$COLLECTION_RUN/bin/pgy.exe"
export PGY_SELF_DRIVER_BIN="$COLLECTION_RUN/bin/pgy-self-driver.exe"
export PGY_SELFHOST_CODEGEN_BUILD_DIR="$COLLECTION_RUN/codegen"
export PGY_SELFHOST_COMPILER_BUILD_DIR="$COLLECTION_RUN/driver"
export PGY_SELFHOST_CC=gcc PGY_SELFHOST_CLANG=clang
make LLVM_ENABLED=1 BUILD_DIR="$COLLECTION_RUN/obj" BIN_DIR="$COLLECTION_RUN/bin" compiler
PGY_SELFHOST_CODEGEN_SEED_ONLY=1 bash tests/self_hosted/parity/codegen_bootstrap.sh
PGY_SELFHOST_CODEGEN_SEED="$COLLECTION_RUN/codegen/gen2.exe" \
  bash tests/self_hosted/parity/self_host_compiler_build.sh
bash tests/self_hosted/parity/collection_ownership_fact_carrier_owner.sh
# Only after the semantic gate owns a unique directory and the new controls:
bash tests/self_hosted/parity/collection_ownership_semantic_owner.sh
```

Preserve per-stage exit codes/stdout/stderr, source graph/seed/driver receipts,
flags, toolchain identities, and native/driver/parser/gen2/probe/runtime-object/
manifest hashes. Seed-only ends before gen3/fixed-point/full matrices. Apply the
60-second static, 5-minute focused parity, and 30-minute integration-shard budgets.
Private acceptance, installed/public acceptance, fixed point, and CI are distinct.

### Inspected input hashes, not an immutable-tree claim

```text
2C1FE4058BA60DD4300913D805E5A53A9D48B0D198EA5942EB463DE472DDAA39  src/semantic/collection_ownership_fact.c
3504AB775DB78CA434399CE5BCB9000501D92A15805EA68D1FD4B3DD51B9E02E  src/self_hosted/semantic/ast_collection_ownership_member_move_owner.pgy
CDD2DD93093A40BC64573093B2EB7E9C20269B838B6709529FD9C3B1F0C614F4  src/self_hosted/semantic/ast_expression_function_table_fact_owner.pgy
F3EDDEF784F5B08FFE2B5BB06742E1A2A456CC1430133376987C9EEB7E38E04F  src/self_hosted/semantic/ast_expression_environment_owner.pgy
1CA4851C7BCC2676A61A50532E39D5A29B7707B61241B357AA5CF43A3A33F1CB  tests/self_hosted/parity/collection_ownership_semantic_owner.sh
B420561777E94D048CD11B245EC04E7AF8F89E97F4CE49309FB98DFAE8742E03  tests/self_hosted/parity/collection_ownership_fact_carrier_owner.sh
006447A010162C2685E081001BA0F96AF5BD45E1105974A6753E6E6C8685B65B  tests/self_hosted/parity/fixture/collection_ownership_binding_move_direct_c_probe.pgy
523D15232473BA2026E96401EB7E4522532EDF17AA877ABE5C40D3F1D7B85FF8  Makefile
80D7E3C5415E35D2CFADC4833131D27123CF6C791B6B884279208DB44FD3EC7A  tests/self_hosted/parity/self_host_compiler_build.sh
```

No compiler fix, fresh private-pair green result, installed publication, full
ownership closure, fixed point, or CI is claimed. The continuation directive is
ready; the registry family remains ACTIVE. Authoring review kept pure lifetime
facts in existing `func`/`struct` owners and did not propose extra user syntax or
ceremonial world/zone/action constructs.

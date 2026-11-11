# Merged semantic-owner probe build, measured on 2026-11-11/12

Measured by the Claude root lane in WSL `Ubuntu-C-WSL` (16 threads, 31 GB)
with native `bin-q/pgy` built from `d01c2388a` plus the merged-probe work
tree. This record is not edited after it is written.

## Build

`tests/self_hosted/fixtures/semantic_owner_probe.pgy`: 16 merged probes,
`--native-pipeline --opt=dev`, C and LLVM built at once.

| Condition | C | LLVM |
|---|---:|---:|
| Quiet machine (load below 0.5) | 522 s | 528 s |
| Producer run inside the step-4 make chain | 637 s for both | |
| Six other heavy jobs alongside | 704 s | |

The make producer's limit is set to 1,200 s, about 2.3 times the quiet
measurement, because hosted runners varied by about 1.5 times on
2026-11-11. Standalone builds of single probes, for comparison: formal key
198 s, focus recovery 490 s, member identity 194 s, arity probe (LLVM)
130 s.

## Defects the build exposed

None of these ran on CI before, because every probe build timed out first.

- `collection_inout_effect_identity_probe`: passed one argument more than
  `SemanticAstCollectionOwnedArgumentVerdict` takes.
- `collection_member_identity_probe`: copied the inout analysis graph,
  mutated the copy and returned before writing it back; native refused it.
  It now mutates the graph in place (45 of 45 modes still answer `true`).
- Native LLVM: an `UnwrapOption` first array element and a name reused with
  another type after its block closes (fixed in `87312eb7f`, `d01c2388a`).
- `collection_copy_field_identity_unit` imported unadopted duplicate owners
  ([decision](../decisions/2026-11-12_unadopted_split_copy_owners_are_deleted.md)).

## Step-4 chain, local

`make -k self-host-collection-ownership-semantic-test-smoke` (906 s) after
the fixes above: nominal field identity, public array drop, hashmap
admission, owned result, member identity, aggregate entry requirement and
constructor field input pass. Failing:

- formal key consumption: the `map-override` run differs from baseline;
- binding move prefix: `borrowed-exclusive` output changed;
- member indexed read permission: native accepts
  `member_capture_constructor_negative`;
- focus recovery: "exact focus declaration premise failed";
- aggregate release source: `aggregate_release_while_exit_positive` fails;
- native storage escape: native accepts `field_ctor_clone_drop_negative`.

The collection inout effect gate did not run because its prerequisites
failed.

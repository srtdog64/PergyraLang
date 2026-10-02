# Reached JSON fragment lifetime and literal completion review

Reviewed base: dbc8cd4e59539d12b5d9e05210da16bab526bf5e.
Implementation checkout: attached collection-assignment-lifetime worktree.
This is an audit of supporting executable corrections, not a semantic owner,
a compiler-completeness claim, or a new collection ownership grant.

## Objective and delivered boundary

The active executable rung is the real JsonOwnedFragmentWriteFile route:
an owned scalar String enters a one-element array and reaches
ArrayDropOwnedStrings. The objective card and read-only review boundaries are
in agent_work_directives/owned_string_literal_transfer_2026-10-03.md.
Root alone edited and built; three agents reviewed scalar lifetime, complete
MIR consumers, and the actual caller/runtime boundary independently.

Three supporting corrections are present:

- Allocated empty content is no longer treated as borrowed storage by an
  early return. The real production writer still writes synchronously and
  retires its one-element array through the public deep-drop operation.
- Absent domain topology and absent/zero-row runtime assignment prefixes no
  longer enter the owned writer. Their producers return borrowed empty
  literals. The public writer's existing Ready boundary remains authoritative
  for invalid facts; there is no guessed default or new allocation/copy.
- The existing ordered lifetime graph fold now emits ArrayElement completion
  after its value. Call-prefix suppression still applies to call completion,
  not to literal element completion. This notification grants no ownership.

The production source wrapper, receipt, ABI-layout and machine-null branches
retain their existing checks. No cleanup was removed, no private retirement
privilege was added, and no old origin/receipt validator was relaxed.
Pure facts remain func/struct; routine callers gain no proof-strategy syntax
or ornamental world/subject binder.

## Observed executable evidence

All evidence paths below are managed-checkout local retained artifacts.

1. `.tmp/self_hosted/json-owned-empty.OTVTdL`: whole official runtime gate
   observed exit0. Native C and LLVM emitted the actual production writer.
   The observer interposes real frees only during each writer invocation;
   empty and nonempty results are both allocated before either is retired.
   Each backend freed each argument exactly once and published exact payload.
   Restoring the old empty-content guard in the generated writer failed with
   exit77 and `watches=2 empty=0 text=1` on both backends, while publishing the
   same payload. This is a leak falsifier, not a pre-publication refusal test.
   The mutation inserts the empty guard at writer entry, not at the original
   post-write position; empty-write failure/transaction order and NULL/OOM
   behavior are outside its evidence. The read-only observer review found no
   concrete false-PASS path within this bounded runtime claim.
   Source/native/runtime before-and-after hashes matched. Runtime compilation
   retained six ATOMIC_VAR_INIT deprecation warnings and each LLVM variant
   retained one target-triple override warning. Earlier instrumentation or
   observer attempts are not the final evidence.
2. `.tmp/self_hosted/mir_json_writer_byte_parity.WjaD8E`: whole official writer
   gate observed exit0 with `backends=c llvm fixtures=8`. Current production
   Pergyra writer/probe code was built via the explicitly selected native
   bootstrap pipeline, once per backend, and reused for eight inputs. All
   String/file pairs and cross-backend file bytes matched. Inputs include
   absent topology, present topology with empty rows/no assignments, and a
   populated runtime assignment. Invalid domain assignment and instruction
   facts preserved the pre-open sentinel. Domain invalidation runs before the
   existing instruction mutation, whose array payload aliases the original.
   Compiler logs retained17 Pergyra warnings each and a C unused-variable
   warning. This is not installed-driver/source-MIR admission evidence.
   Final read-only review found that the populated input alone would not
   falsify OR becoming AND. The gate now includes a ninth roles-only input.
   `.tmp/self_hosted/mir-json-writer-roles-only.ASA4Fn` executed that input on
   both already-issued, unchanged production probe binaries: exact String/
   file/cross-backend bytes, empty stderr, unchanged invalid sentinels, one
   participant role and zero projection members all PASS. This is an explicit
   incremental reuse, not a claim that the final nine-input gate rebuilt its
   probes. An earlier HasLayer input refused at builtin_native_pipeline_only
   and is not passing evidence. Sentinel equality observes the committed
   target; the Ready-before-Begin source boundary, not the sentinel alone,
   establishes the pre-open ordering. Temporary-file abort behavior is not
   separately instrumented.
3. `.tmp/self_hosted/collection-inout-effect.0VBfAm`: whole official gate
   observed exit0 on both native-C and native-LLVM analyzer/probe builds:
   216 source admission cases,75 diagnostic locations,3 observer-mode checks,
   51 identity/boundary units,35 occurrence/root/element-order units,
   4 completion receipt units,10 storage producer units,19 constructor units,
   4 constructor CLI refusals and7 actual-formal units. Seven new literal
   units cover empty, single, multiple, nested call, repeated physical leaf,
   prefix, and malformed self-child cases. Source cases are analyzed, not
   emitted/executed, so this is diagnostic/order evidence, not runtime closure.
   Owner, input, native and probe manifests still match. The whole-import
   manifest differs only at the subsequently edited writer test tool, which
   neither analyzer/probe nor these input programs import. That later tool
   revision is covered by item2, not by the earlier whole-import snapshot.

Static evidence: structural writer lifetime ratchet PASS; changed shell syntax
and diff checks PASS; component checker self-tests14 PASS; narrow new source
inventory assertions PASS. Full component inventory timed out at60 seconds
after reaching the match-pattern-placement checkpoint. It is NOT full PASS.
The component inventory remains structural; no behavior is inferred from it.
No complete bootstrap, installed-driver, fixed-point, performance or full
platform matrix was run for this supporting slice.

## Three independent review findings

### Scalar current value and occurrence lifetime

The strict formal identity join owns function, physical ordinal, parameter
syntax identity, type and mode. An own String formal is an entry obligation,
not proof of allocator domain, current value, exclusivity or last use.
Existing collection assignment definitions do not carry scalar String current
values; result facts can describe an initializer without discharging aliases.
The new literal completion notification belongs in the existing ordered fold,
not in a second root/whole-program traversal. It is only a prerequisite.

The next grant must refuse unproved alias-after-transfer, repeated literal
transfer, reassignment to borrowed storage, conditional/loop/deferred paths,
and nested calls where an earlier argument still retains a pointer until the
enclosing call completes. Prefix/leaf order alone cannot prove those facts.

### All MIR consumers and definition-time materialization

A formal-owned literal requires a disjoint versioned fact/receipt contract.
Do not reuse EmptyLiteral, Clone, MemberMove, or the existing call-result
origin. Its definition key must identify the completed Let/value operation and
join the exact scalar formal/source and array receiver identities.
CFG state must be NotMaterialized -> completed-Let Owned -> exact-drop Retired;
an early return before the literal must remain NotMaterialized.

Required fact/receipt coverage must derive from the admitted owning operation,
not merely from rows still present in a supplied artifact. Removing the entire
fact/count/receipt family or downgrading it to UNKNOWN must not erase an
obligation. Native HIR/MIR producers and validators, self-host artifact/wire
writers/readers, parameter/local-ref projections, CFG admission, and C/LLVM
retirement consumers all need the same identity and falsifying mutations.
Native UNKNOWN, untracked cleanup and unconditional readiness paths cannot
remain compatibility authority after substitution.

### Actual callers and allocation domain

The bounded source review found51 Json calls in6 files:20 numeric ToString,
25 allocating JSON expression forms and6 conditional/field sources. Int,
Long, Float and Double ToString allocate on both native backends; String
ToString preserves a pointer and Bool differs between C and LLVM. String
concatenation uses region storage, so a String result type is not individual
deep-free permission. The six special cases require their actual producer
branch/field facts, not a renderer name whitelist.

AllocatorResult TextBuilderFinish may return allocated empty storage; the
allocator's pool destruction does not retire that result. This is the actual
leak fixed here. A separate OOM issue remains: numeric allocation and its
fallback can both fail to NULL, and a writer may then succeed without emitting
the value. No nonnull/always-fresh claim or OOM repair is made in this slice.

## Explicit open executable rung

Status: BLOCKED at the full substitution boundary, not the overall task.
Missing facts: carried actual-caller scalar allocation domain/current value,
exclusive one-time literal transfer/no later alias use, and completed-value
materialization with mandatory versioned receipt coverage across all readers.
Fact owners: resolved signature/current-expression and ordered lifetime facts,
owned String result facts, collection verdict, MIR fact/receipt and CFG owners.
Last reached consumer: JsonOwnedFragmentWriteFile deep-drop admission, then
the production MIR collection/cleanup admission chain.
Next falsifier: the exact CI input
`tests/self_hosted/parity/fixture/mir_collection_receiver_root.pgy`, followed
by actual caller alias/current/after-use and wrong/missing/duplicate receipt
mutations. Earlier node5251 is a retained base diagnostic, not the syntax ID
of the changed source. A fresh current-source trace is still required.

The base's CI37062145672 completed FAILURE in Linux self-host bootstrap at
owned_string_drop/borrow_boundary_escape for the real Json route. Windows,
macOS C-only, TSan, Rocq and classification succeeded; downstream Linux jobs
were skipped. This patch has not yet obtained new CI evidence:
https://github.com/srtdog64/PergyraLang/actions/runs/37062145672

C-owned compiler path substitution delta0; no registry row is promoted.
Registry remains70 CLOSED /23 BRIDGE /2 ACTIVE, including ACTIVE
semantic.hashmap_collection_ownership. Actual aggregate Release, all3 drops,
all3 field and2 outer writebacks remain a later consumer boundary, unchanged.
Native runtime PASS, ordered events and documentation do not close this rung.

Before another SoT-only commit, implement a real executable replacement or
retain this exact missing-fact/owner/last-consumer/falsifier card as BLOCKED.
No general cache/query/library or independent semantic cleanup track is opened.

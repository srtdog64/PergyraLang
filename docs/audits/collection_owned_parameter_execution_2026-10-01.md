# Reached owned-parameter execution evidence

Status: PARTIAL IMPLEMENTATION; not aggregate-field or family closure.
Base observed during implementation: `3dc2e097e483060c89769ba63e4f54246b1cc520`.
Existing unrelated dirty source/docs/tests remain preserved.

The reached Pergyra owner now derives deep-release obligations from exact
builtin identity, propagates them along stable own-formal call edges, refuses
required arguments without admitted element facts, and retires forwarded
formal IDs as well as local IDs. Receiver offsets are consumed from the source
signature fact, not reconstructed from callable spelling. No MEMBER_MOVE state
is promoted to OWNED and the aggregate UNKNOWN bootstrap exemption is intact.

## Observed component execution

Private evidence: `.tmp/self_hosted/collection-sot-current.8612gf/`.
Native LLVM compiler built successfully; its Windows source-root relink used
`PROJECT_ROOT=D:/PergyraLang`. The first POSIX-root build could not find runtime
C files. A global MSYS argv-conversion exclusion also broke an absolute output
path in an existing control; rerunning the harness without that exclusion
resolved that environment failure. These failures are not green evidence.

Private native SHA-256:
`20E06E85FF94A2D08E1C516ABA0AC09F3FF1E668F56E1D8A35F5754FA518BCA3`.
The source-fingerprinted seed-only bootstrap completed (not full fixed point).
Gen0 SHA-256:
`F4D0A5DED1E8F4FA91AE9E5BA0F76D24227FC3899F43D68397DABD79767E8279`.
Gen2 SHA-256:
`900518B8A2E21C8DE32B4A9778FE32E4664B6D3A52CF4B66548A30EE6420A368`.
Existing argument-graph controls observed 2 executions/12 pre-emission refusals;
nominal-array controls observed 4 executions/8 pre-emission refusals. Gen0
bootstrap compilation reported 0 errors and 17 unreachable-statement warnings;
this packet does not resolve or hide those warnings.

`owned-owner-probe/` contains actual gen0 Pergyra owner execution:

- borrowed three-hop Forward -> Relay -> Retire: `borrow_boundary_escape`;
- required member actual: `borrow_boundary_escape`;
- inline literal actual: `named_value_boundary_argument_required`;
- own-formal double forward and post-forward read: `move_from_released`.

All five returned 1 with the exact diagnostic and no C preamble/publication.
No negative binary was compiled or executed. The owned wrapper and named Clone
controls returned 0, their generated C compiled, and execution produced exactly
`owned-wrapper-retired` and `named-clone-retired`. The same-spelling user drop
callable returned 0 and its generated C compiled; it was not executed.
An earlier `Clone(["borrowed"])` control was invalid under current initializer
inference; the final control first binds a typed source, then calls Clone.

## Limits and next falsifier

This is not installed-binary, full fixed-point, CI or family-closure evidence.
The gen2-built DRV-2 completed in `driver-build/` with output only under
`driver-bin/`; installed binaries were not replaced. Its SHA-256 is
`AF815E11989FC389D9C4BA5FF4A9CF50F81110CED9C16BB123814160AA4D62EB`.
The initial native MIR oracle incorrectly admitted the known-borrowed three-hop
wrapper (exit 0); no binary for that negative was executed.

The native bootstrap now records exact typed stdlib deep-drop seeds, stable
own-call targets, formal forwarding edges and caller element-state snapshots
before the existing storage consumer. One post-Pass2 fixed point rejects known
BORROWED actuals. This is a refusal ratchet, not UNKNOWN ownership permission;
general UNKNOWN and aggregate-formal exemptions are still bootstrap debt.
Private candidate native SHA-256:
`F3F516DDC6C03C8AECA0DE51700B91AD879E1233118FF3109C0E9ABD03CBFA99`.
The native build succeeded with six warnings in existing unrelated semantic
files; no new owner warning was observed. The prior seed receipts still name
the initial native bootstrap hash, not this later candidate.

`owned-parameter-strict-json.log` and unique run
`.tmp/self_hosted/collection_ownership_semantic_owner.LRQRI9/` observed all five
driver MIR refusals with preserved sentinels and all five public C/LLVM plus
native C/LLVM pre-artifact refusals. Native JSON codes were BORROW_ESCAPE for
the wrapper, TYPE_MISMATCH with the exact named-variable requirement for
member/inline, and MOVE_FROM_RELEASED for the two reuse cases. Native text mode
omits these stable codes; the gate now requests JSON for native negatives.

In `native-requirement-positives/`, owned wrapper and named Clone compiled and
ran on native C/LLVM with their exact expected output (four executions).
The shadow callable compiled on native C/LLVM without execution. The strict
gate also admitted shadow MIR and compiled it on both public backends.
The private carrier gate (`collection-carrier.log`) passed native/self-host
producer-consumer parity and 36 malformed-row refusals.

The default semantic integration shard was rerun with the script frozen at
SHA-256 `551B0431979DDA549A6EA5B052D461A3C08E73E8FFC3DF3F4314B66F07AD3488`.
`collection-semantic-all-stable.log` returned 0 and the final ownership parity
PASS; its unique evidence is
`.tmp/self_hosted/collection_ownership_semantic_owner.fXtCKb/`. The script hash
was unchanged after completion. This includes the retained default negatives,
borrowed/UNKNOWN binding-move C/LLVM execution, Clone controls and both actual
HashMap early-return/normal-exit execution. Clang module-target-triple warnings
were visible. The log's legacy "installed self-host" wording means the selected
private driver in its first line here; repository installed binaries were not
replaced, and this is not installed acceptance or the new own-formal selector.

The first default run (`collection-semantic-all.log`, `5YTGUv`) is not overall
green evidence: root edited the script while it was running, and it stopped at
the final negative-array section with a command-not-found error. The current
array declaration parsed independently; the fully frozen rerun above passed.
No source-stability claim is made for the first run.

The strict gate did not pass overall: public C refused the owned wrapper after
verified MIR publication with `direct MIR scalar program extension ... code=19`.
The unchanged final script was rerun in `owned-parameter-strict-final.log`
with evidence `.tmp/self_hosted/collection_ownership_semantic_owner.xsqhqo/`;
the typed negative checks passed and the same public C positive failed.
An independent public LLVM probe (`public-wrapper-llvm.out/.err`) also returned
1 with code 19 and did not publish its requested executable.
The same failure was observed in the self-host-only selector before native
integration. This is a real positive falsifier, not a generic diagnostic match.
The owned-array-string move admission accepts only local expression sources,
and its coverage requires a local definition operation. Forward/Relay carry
formal parameters, whose lifetime starts at routine entry. Merely relaxing
ExprLocal would leave last-use/exit coverage and the digest/plan join unproved.
Current C/LLVM cleanup is local-only; no formal cleanup should be added as a
substitute for the missing transfer fact. LLVM own-array mutation uses the
`.local` descriptor while parameter reads can still use the original SSA value;
grow-and-forward needs a consistent descriptor owner as well. Main was sent
this exact consumer boundary. No direct-backend source was edited here.

Lexical owner sizes observed: 66, 41, 88, 146 lines versus caps 100, 100, 160,
180. Shell syntax and whitespace checks passed. The 14 component-checker unit
tests passed, but the overall structural script did not complete successfully
within its Windows static budget, at repeated responsibility-size lookup; no
overall structural PASS is claimed.

The native lexical-size gate also ran and returned 1 for four unchanged
pre-existing owners: `collection_ownership_fact.c` 873,
`type_checker_builtins_ownership_nominal.c` 611,
`type_checker_call_generic_where.c` 655 and
`type_checker_intent_step_sequence.c` 689, against 599. Those files have no
diff in this implementation; unrelated splitting was not opened as a second
track. The new native owner is 326 lexical lines, its header 27, and the two
hooked program/call owners are 559/298. No overall native-size PASS is claimed.

Future aggregate negatives/actual producer controls have an explicit
`aggregate-field` selector. Default established cases are preserved. The
`owned-parameter-self-host` selector names the production Pergyra/public scope;
`owned-parameter` also requires native parity. Neither implies full closure.
Registry `semantic.hashmap_collection_ownership` stays ACTIVE. The next full
field falsifier remains borrowed Bundle/callable-table formal extraction versus
actual FromArtifact table cleanup through nested DRV-2 analysis and empty
writeback. Producer/inout/return/field-path evidence is still required.

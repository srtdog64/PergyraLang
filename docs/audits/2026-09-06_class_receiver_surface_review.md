# Value receivers and language-surface reduction: bounded review

Status: PROPOSAL REVIEW + EXECUTED COUNTEREXAMPLES; NO LANGUAGE CHANGE.

Observed HEAD: `16b2f894cec5e0478639d68e89a6fb98e9168dea`.
Installed native SHA-256:
`0F9F4F30255D6850B5A773E21D5815F776B305E5C01A7A2C3DF6D373BB15A29E`.
Installed self-driver SHA-256:
`7928ED6BE2D38A9C36FF6B09BA3F1BFCDAB3D4FEDD0B5CC006506F3788BBACFB`.
These are the existing installed binaries, not rebuilds from the observed HEAD.
This audit does not own semantics, authorize word removal or open a second
compiler implementation rung beside the active ability/enum integration seam.

## Receiver proposal: compatible direction, incomplete implementation

The proposed distinction is useful when stated narrowly: caller-visible
mutation of a value receiver is explicit `inout`; an identity receiver follows
its existing identity/authority contract. It must not change a class into an
identity nominal merely to obtain a pointer ABI. Identity receivers are not
limited to subject: `callable_receiver_carriage_policy_owner.pgy` also names
vessel, party, roster, world, relation, effect, role and zone.

`docs/10_role_interface_design.md` distinguishes class values with methods from
subject identity. `src/parser/parser_decl.c` already accepts `inout self`.
`docs/mut_borrow_parameters.md` owns copy-in/copy-out on normal exit, named
addressable arguments and rejection of duplicate inout arguments. This is not
a live mutable borrow; passing an lvalue address alone is not its full meaning.

Two new bounded probes were executed from
`.tmp/self_hosted/class_receiver_review_16b2f894/`. The mutable-binding probe is:

```pergyra
class Counter {
    let mut n: Int;
    func Bump(inout self) -> Void { self.n = self.n + 1; }
    func Next(self) -> Counter { return Counter(self.n + 1); }
}
func Main() -> Void {
    let mut c = Counter(0);
    c.Bump();
    let next = c.Next();
    Log(ToString(c.n));
    Log(ToString(next.n));
}
```

The second probe keeps only Bump, uses plain `let c`, then prints `c.n`.
The proposal expects the first to print `1`, `2` and the second to be refused.
Actual observations (`beea5f`, completed `e1dbda`):

| Route | Mutable binding | Plain binding |
| --- | --- | --- |
| Native C | Compile/run exit 0, prints `0`, `1` | Compile/run exit 0, prints `0` |
| Native LLVM | Exit 1: value-result `self` has no registered copy-out boundary | Same refusal |
| Public C | Exit 1: parser-owned signature is missing `parameter_kind` | Same refusal |
| Public LLVM | Exit 1: parser-owned signature is missing `parameter_kind` | Same refusal |

These eight outcomes refute "only checker and address-passing changes are
needed" as an established implementation claim. They do not establish that a
new IR mechanism is necessary. Existing value-result carriage should be reused
where valid, with its receiver producer/consumer gaps explicitly closed.

Reached source anchors:

- `semantic/ast_signature_fact_owner.pgy` carries parameter modes;
- `semantic/ast_callable_receiver_source_parameter_owner.pgy` identifies the
  source-declared receiver offset;
- `mir/routine_receiver_carriage_owner.pgy` derives current routine carriage
  from declaration `uses_pointer_self`, not a completed mutable-value mode;
- `codegen/emission/member_call_receiver_carriage_owner.pgy` consumes value or
  mutable-identity carriage and stable-address evidence.

All four paths are under `src/self_hosted/`. A correction must reconcile these
facts with native and LLVM copy-out ownership, not teach each backend a class
name special case. Required controls include plain-self writes, immutable
fields, mutable/plain caller bindings, temporary receivers, receiver/argument
alias overlap, every normal return, and failure exit behavior. Existing inout
rules do not automatically authorize nested lvalues or a new local-mutability
policy.

## Unsafe: incomplete capability checking is not a no-op

The deletion matrix's one sequential source pair has equal output. That does
not establish the broader no-semantic-effect claim. Current native owners
`src/semantic/type_checker_unsafe_block.c` and
`type_checker_flow_statement_kinds.c` record `EFFECT_UNSAFE` and refuse unsafe
blocks in parallel task bodies. The parser retains a distinct unsafe AST node
and optional capability label.

Primary ran a source-admission control using a parallel sum whose body contains
`unsafe { }` followed by `give x`. Removing only that empty unsafe block yields
the positive control. On the installed native `--mir-json --error-format=json`
path (`fe0ee4`):

- without the block: exit 0 and one `pgy.mir.v1` document;
- with the block: exit 1, `PGY_SEM_PARALLEL_SECURE_FORBIDDEN`, no MIR document.

Sources are `parallel_without_unsafe.pgy` and `parallel_unsafe_marker.pgy` in
the same temporary review directory. Neither parallel program was executed.
This independently falsifies an unconditional "no-op boundary" description;
it does not prove that all scoped raw/FFI capabilities are implemented.
The lexical-boundary wording in docs/132 and docs/204 must be read with this
existing effect/admission behavior, not as evidence that deleting the block
always preserves compiler guarantees.

## Reduction recommendations, not an implementation queue

| Surface | Bounded recommendation / remaining condition |
| --- | --- |
| `retry` | The recorded public single-execution acceptance contradicts native's explicit unsupported-lowering refusal. Make both production paths refuse unsupported retry before treating word deletion as the repair; do not add a native fallback. |
| `timeout`, `backoff` | Preserve an explicit unsupported diagnostic unless deletion is separately chosen. Review support metadata against its declared meaning; a support bit is not proof of executable substitution. Removing rows is not required merely to report unsupported execution honestly. |
| `move ... to` | The parser shares transfer alias fields with `transfer:`. Consolidation is plausible, but migrate sources and consumers and retain a retired-spelling negative gate. Do not remove unrelated `to` contexts. |
| `guard` | A single failing guard/expect/post can leave the same state. Their ordered evaluation and distinct failure labels remain observable; migrate trace consumers, combined clauses and compensation behavior before retiring a spelling. |
| `unsafe` | Do not remove it or issue an unconditional no-op warning on the disproved no-effect premise. Separate existing unsafe effect/parallel refusal from missing capability enforcement. |
| local `mut` | docs/134 explicitly records beta local-reassignment compatibility while fields are enforced. Stronger local enforcement is a migration decision, not implied by field rules. Current self-host owners use plain let counters followed by reassignment; do not invalidate that compiler source in one unbounded sweep. |

`src/self_hosted/lexer/language_word_row_projection_owner.pgy` is generated,
not an independent hand-maintained registry. Its header names
`scripts/render_language_keyword_registry.py` and the `.def` source of truth.
Any chosen removal must update the registry/parser contract, regenerate all
derived projections and validate the existing keyword gates. Derive counts
from the exact chosen set; do not precommit to `146 -> 143` while the proposal
still offers different removal/unsupported-state alternatives.

No compiler source, keyword registry, generated projection or CI profile was
changed in this review. The prior matrix intake reconciliation remains separate
from implementing this receiver proposal or deleting a language feature.

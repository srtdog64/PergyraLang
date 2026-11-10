# Mutable bindings and compound assignment

Status: ACCEPTED by the user on 2026-11-08; implementation is not thereby closed.

The user requested numeric `+=`, `-=`, `*=`, `/=` and `%=` and both `var` and
`let mut` declarations. Both declaration spellings carry one mutable-binding
fact. `let` fixes the value after initialization: rebinding, collection writes,
struct-field writes and writable argument passing cannot erase that permission.
Existing readonly borrows, immutable fields and ownership boundaries still apply.
Identity hosts and resource operations retain their existing authority contracts.
Ownership transfer and retirement are not writes to a live value.

The user separately chose "let 불변 검사와 기존 코드 이행까지 포함" and then
"기존 계약대로 값 변경까지 금지 (권장)". Migrate declarations at compiler-resolved
write sites, including the self-host source. This does not authorize replacing
every `let` or changing the expected result of an ownership refusal.

Compound assignment evaluates its place and prior value once before its RHS,
then applies the existing arithmetic rules and writes once. Existing event
subscription/unsubscription keeps its type-directed meaning. A backing-changing
RHS cannot invalidate an indexed place while its address is held.

This explicitly requested language extension is separate from the main lane's
frozen V.001 publication candidate. Evidence and remaining obligations are in
[the binding lane state](../state/mutable_bindings.md); the implementation plan is
[the work directive](../../agent_work_directives/mutable_bindings_and_compound_assignment_2026-11-08.md).

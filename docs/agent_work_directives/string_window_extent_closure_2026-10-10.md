# String-window extent closure

Status: **FOCUSED_GREEN, landed without push (Claude, sole lane since the
user stopped the main lane on 2026-10-10)**. Base `main @ 85fff5aa`; committed
together with the stopped lane's ownership-cutover work, as the handoff card
records. Finding and reproduction: [audit](../audits/string_window_extent_audit_2026-10-10.md).

Evidence on the final tree: the gate passes on both routes (8 accept, 17
reject fixtures each); the native check of the driver has 0 errors; a scratch
self-host build accepts the whole compiler source; 1488 programs keep
byte-identical MIR, C and diagnostics across the migration. Open: step 5
(recorded gap) and the installed-driver chain, which stops at the
pre-existing aggregate release-plan refusal (handoff P1b).

## Objective card

- Objective: no source program can make a string-window builtin read outside
  its source string; an unproven extent is a compile-time refusal.
- Priority: one admission rule shared by both compilers; fail-closed refusal
  with a source span; no runtime fallback; no new syntax; patch size last.
- Fact owners: native `src/semantic/string_window_extent.c`; self-host
  `src/self_hosted/semantic/string_window_extent_owner.pgy`. They implement the
  same rule; a parity fixture set pins equal verdicts.
- Last consumers: every backend reached from source. The rule runs in semantic
  analysis, before the legacy C, direct-MIR C/LLVM and nested-intent routes split.
- Forbidden fallback: a checked runtime variant chosen silently, a pointer-keyed
  length cache, name-only matching across sibling bindings, an allowlist of
  compiler files, or accepting an unresolved call to a routine with a requirement.
- Gate: `tests/string_window_extent_smoke.sh` (native and self-host verdicts on
  the fixture set, plus the reproduction programs).
- Falsifiers: the two audit reproductions; reassignment of the source or the
  extent; inout passing of the source; a helper called once with a wrong
  extent; a requirement-carrying routine reached without a resolved callee.

## Rule

Window builtins `W`: `CharCode`, `CharAtN`, `SubstringWithLen`,
`SubEqualsWithLen`, `SubContainsWithLen`, `SubIndexOfWithLen`,
`SubStartsWithLen`. Argument 0 is the source, argument 1 the source extent.

A binding is **stable** when it is a routine parameter or a plain `let`
binding, no assignment anywhere in the program targets it, and no call passes
it to an inout parameter. Loop, match and destructured bindings and host fields
are never stable: the self-host graph identifies them only by spelling, and one
rule must give both compilers the same answer. Identity is the declaration
syntax id, never the spelling.

A (source, extent) pair is **proven in F** by exactly one of:

- **P1 same-value length:** the extent is `StringLength(x)` and `x` and the
  source are the same stable binding, or the same string literal.
- **P2 stable witness:** the extent is a stable binding whose single
  declaration is `let n: Int = StringLength(x)`, with `x` the source's binding,
  and the source is stable.
- **P3 literal:** the source is a string literal and the extent is an integer
  literal no larger than its byte length.
- **P4 empty prefix:** the extent is the integer literal 0 or 1. Index 0 is in
  storage for every string, the empty one included.
- **P5 requirement:** the source and the extent are stable parameters `p_i`,
  `p_j` of F, and F is not a method. F then carries the requirement `(i, j)`.
- **P7 record field pair:** the source and the extent are `r.f` and `r.g` over
  the same member path down to one binding root that no call passes as inout
  (the root may be reassigned). The pair `(f, g)` becomes a record invariant:
  every construction of every record declaring both fields, by call or by
  struct literal, must prove `(f, g)`, and no write may target a field of
  either name through a receiver of unknown type or of a record declaring both.

- **P8 readonly element:** the source is `A[K]` where `A` is a readonly `ref`
  parameter of F or a field path over one, and `K` is a stable binding or an
  integer literal; the extent is `StringLength(A[K])` over the same path and
  index, or a stable `let` declared with it. A readonly array's elements cannot
  change while F runs, so the element is observed in place and never bound or
  copied out (binding it would be a formal element use the self-host ownership
  owner refuses).

A call to a window builtin is admitted when its pair is proven. A resolved call
to a routine G is admitted when, for every requirement `(i, j)` of G, the
argument pair `(arg_i, arg_j)` is proven in the caller; P5 there adds the
requirement to the caller. Requirements and field pairs are the least fixed
point over all routines and constructions. A call whose callee is unresolved
and whose name is the name of a routine with a requirement is refused, as is a
reference to such a routine that is not a direct call. Everything else is
refused with `string_window_extent_unproven` and a fix naming
`StringLength(source)`.

## Measured scope and ceremony

On the stopped lane's tree the native rule refused 646 window or requirement
sites in `driver_bootstrap_main.pgy`. All were rewritten, none exempted.

| Form | Count | Reason it exists |
| --- | ---: | --- |
| `<p>_extent: Int` parameter after `p: String` | 244 | the routine reads `p` per byte and is called per byte or per token; measuring `p` inside would make the caller quadratic |
| `let <x>_extent: Int = StringLength(<x>);` witness | 91 | one measurement serves every window over an unchanged binding |
| `<f>_extent: Int` record field | 6 | records that carry a source string across routines (`JsonObjectFactTable`, `LexerTokenFact`, `MirProgramRoutineIndex`, `CodegenTypeEnv`) |
| `StringWindow*` call (bounded window over a proven extent) | 73 | the call bounds its reads by a window end that is not the string's length |

The codemod first added 324 parameters. A routine that measures its source on
every call anyway keeps the witness local instead (`Trim`, `ExpectOpt`,
`SemanticReadIdent`, `CodegenTypeEnvFromRows`, `JsonDocumentObjectFactTable`
and the routines that only forwarded into them): 80 parameters and 736 call
arguments were removed that way.

- Missing fact: a runtime `String` carries no O(1) byte length
  (`StringLength` is `strlen`), so the extent is a separate value.
- Last consumer: the seven window builtins and the record constructions that
  carry a field pair.
- Removal condition: a `String` value that carries its byte length (a runtime
  ABI change), or an extent fact the compiler attaches to a string binding
  itself. Either one deletes every row of the table above.

## Chain and change set

1. Native owner and hooks: function entry, let declarations, assignment
   targets, every `AST_CALL` in the expression dispatcher, function-value
   identifiers, and program finalization in `type_check_program`. Diagnostic
   code in `diag_codes.h`, `diagnostic_code_owner.pgy` and docs/72.
2. Native unit cases and fixtures; run the native rule over the self-host
   compiler sources to get the exact refusal list.
3. Rewrite the refused self-host call sites so that they are proven. A rewrite
   must preserve observable results, including the -1/"" out-of-window answers
   of bounded helpers.
4. Self-host owner with the same rule, wired into the self-host semantic
   verdict before MIR; parity fixtures on an isolated gen1 builder.
5. External `--mir-json` input bypasses semantic analysis. **Recorded gap:**
   the direct-MIR scalar route admits `CharCode`, `CharAtN`,
   `SubstringWithLen`, `SubIndexOfWithLen` and `SubEqualsWithLen` from MIR
   JSON without an extent proof. An in-expression P1/P3/P4 readiness check
   alone would refuse the source-produced MIR of
   `tests/self_hosted/fixtures/direct_mir_scalar_bool_sub_equals_short_circuit.pgy`,
   whose window is proven by P5 across routines, so closing the gap means
   re-admitting P1-P7 over MIR identities at the untrusted boundary. Falsifier:
   a hand-written MIR document whose window extent exceeds its literal source.
6. Runtime header comment: extents reach these primitives only after admission.

## Non-claims

Official binaries are not replaced before I8. No claim covers other string
builtins, FFI strings or whole-language spatial safety.

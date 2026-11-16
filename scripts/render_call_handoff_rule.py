#!/usr/bin/env python3
"""Validate the call hand-off rule and render its C header and Pergyra owner.

The source is src/semantic/call_handoff_rule.def. The rendered C header is
read by the native semantic owners and the rendered .pgy owner by the
self-host ones; --check fails when either output differs from a fresh render.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from itertools import product
from pathlib import Path
import re


MACROS = {
    "PGY_CALL_HANDOFF_MEMBER": 2,
    "PGY_CALL_HANDOFF_KIND": 3,
    "PGY_CALL_HANDOFF_AXIS": 2,
    "PGY_CALL_HANDOFF_ACTION": 2,
    "PGY_CALL_HANDOFF_DIVERGENCE": 4,
    "PGY_CALL_HANDOFF_NATIVE_SUMMARY_BIT": 2,
    "PGY_CALL_HANDOFF_SELF_HOST_EFFECT": 2,
    "PGY_CALL_HANDOFF_SELF_HOST_EFFECT_DIVERGENCE": 3,
    "PGY_CALL_HANDOFF_ROW": 6,
    "PGY_CALL_HANDOFF_DIVERGENCE_ROW": 7,
}
AXES = ("ORIGIN", "LATER", "CONTEXT", "ELEMENTS")
NATIVE_SUMMARY_BITS = (
    "SLOT_PARAM_SUMMARY_READ", "SLOT_PARAM_SUMMARY_WRITE",
    "SLOT_PARAM_SUMMARY_RELEASE", "SLOT_PARAM_SUMMARY_RETURN_ESCAPE",
    "SLOT_PARAM_SUMMARY_CALL_ESCAPE", "SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE",
)
NAME = re.compile(r"[A-Z][A-Z0-9_]*")
REQUIRED_MEMBERS = ("READ", "RETURN", "GROW", "KEEP", "RELEASE", "UNPROVED")
REQUIRED_ACTIONS = ("LEND_FOR_CALL", "HAND_OFF", "LOAN_TO_RESULT", "CLONE_ARGUMENT",
                    "OWNED_ELEMENT_CLONE", "REFUSE", "KEEP_ROW_STATE")
REQUIRED_AXES = {
    "ORIGIN": ("FRESH", "LOCAL", "CALLER_FORMAL", "BORROWED"),
    "LATER": ("ONCE", "USED_AGAIN"),
    "CONTEXT": ("SYNC", "DETACHED"),
    "ELEMENTS": ("AS_GIVEN", "OWNED_REQUIRED"),
}


@dataclass(frozen=True)
class Kind:
    name: str
    any_of: int
    only: int


@dataclass(frozen=True)
class Divergence:
    name: str
    front_end: str
    form: str
    slice: str


@dataclass(frozen=True)
class Row:
    divergence: str | None
    kinds: frozenset[int]
    axes: tuple[frozenset[int], ...]
    action: int


@dataclass
class Rule:
    members: list[str]
    moving: list[bool]
    kinds: list[Kind]
    axes: dict[str, list[str]]
    actions: list[str]
    divergence_only: set[str]
    divergences: list[Divergence]
    native_bits: list[tuple[str, int]]
    effects: dict[int, int]
    effect_divergences: list[tuple[str, int, int]]
    rows: list[Row]
    divergence_rows: list[Row]

    @property
    def all_members(self) -> int:
        return (1 << len(self.members)) - 1

    def kind_of(self, members: int) -> int:
        for index, kind in enumerate(self.kinds):
            if (kind.any_of == 0 or members & kind.any_of) and members & ~kind.only == 0:
                return index
        raise ValueError(f"class set {members:#x} matches no kind")

    def cells(self):
        sizes = [range(len(self.kinds))] + [range(len(self.axes[a])) for a in AXES]
        return product(*sizes)

    def lookup(self, cell: tuple[int, ...], divergences: set[str]) -> int | None:
        for row in self.divergence_rows:
            if row.divergence in divergences and row_matches(row, cell):
                return row.action
        for row in self.rows:
            if row_matches(row, cell):
                return row.action
        return None


def row_matches(row: Row, cell: tuple[int, ...]) -> bool:
    return cell[0] in row.kinds and all(v in s for v, s in zip(cell[1:], row.axes))


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//.*?$", "", source, flags=re.MULTILINE)


def parse_calls(path: Path) -> list[tuple[str, list[str], int]]:
    text = path.read_text(encoding="utf-8")
    source = strip_comments(text)
    pattern = re.compile(r"\s*([A-Z_]+)\(([^()]*)\)")
    calls: list[tuple[str, list[str], int]] = []
    offset = 0
    while offset < len(source):
        match = pattern.match(source, offset)
        if match is None:
            if source[offset:].strip():
                line = source.count("\n", 0, offset) + 1
                raise ValueError(f"unexpected call hand-off rule text at line {line}")
            break
        macro, body = match.group(1), match.group(2)
        line = source.count("\n", 0, match.start(1)) + 1
        if macro not in MACROS:
            raise ValueError(f"line {line}: unknown macro {macro}")
        fields = [field.strip() for field in body.split(",")]
        if len(fields) != MACROS[macro]:
            raise ValueError(f"line {line}: {macro} expects {MACROS[macro]} fields")
        calls.append((macro, fields, line))
        offset = match.end()
    return calls


def declare(names: list[str], name: str, what: str, line: int) -> None:
    if NAME.fullmatch(name) is None or name in ("ANY", "ALL", "NONE"):
        raise ValueError(f"line {line}: invalid {what} name {name}")
    if name in names:
        raise ValueError(f"line {line}: duplicate {what} {name}")
    names.append(name)


def name_set(text: str, names: list[str], what: str, line: int, wildcard: str) -> frozenset[int]:
    if text == wildcard:
        return frozenset(range(len(names)))
    picked: set[int] = set()
    for part in text.split("|"):
        part = part.strip()
        if part not in names:
            raise ValueError(f"line {line}: unknown {what} {part}")
        if names.index(part) in picked:
            raise ValueError(f"line {line}: {what} {part} listed twice")
        picked.add(names.index(part))
    return frozenset(picked)


def member_mask(rule: Rule, text: str, line: int, empty: str, full: str | None) -> int:
    if text == empty:
        return 0
    if full is not None and text == full:
        return rule.all_members
    return sum(1 << i for i in name_set(text, rule.members, "member", line, "\0"))


def load(path: Path) -> Rule:
    rule = Rule([], [], [], {a: [] for a in AXES}, [], set(), [], [], {}, [], [], [])
    kind_names: list[str] = []
    divergence_names: list[str] = []
    for macro, f, line in parse_calls(path):
        if macro == "PGY_CALL_HANDOFF_MEMBER":
            if f[1] not in ("MOVING", "NOT_MOVING"):
                raise ValueError(f"line {line}: member role must be MOVING or NOT_MOVING")
            declare(rule.members, f[0], "member", line)
            rule.moving.append(f[1] == "MOVING")
        elif macro == "PGY_CALL_HANDOFF_KIND":
            declare(kind_names, f[0], "kind", line)
            rule.kinds.append(Kind(f[0], member_mask(rule, f[1], line, "NONE", None),
                                   member_mask(rule, f[2], line, "\0", "ALL")))
        elif macro == "PGY_CALL_HANDOFF_AXIS":
            if f[0] not in AXES:
                raise ValueError(f"line {line}: unknown axis {f[0]}")
            declare(rule.axes[f[0]], f[1], f[0].lower(), line)
        elif macro == "PGY_CALL_HANDOFF_ACTION":
            if f[1] not in ("RULE", "DIVERGENCE_ONLY"):
                raise ValueError(f"line {line}: action role must be RULE or DIVERGENCE_ONLY")
            declare(rule.actions, f[0], "action", line)
            if f[1] == "DIVERGENCE_ONLY":
                rule.divergence_only.add(f[0])
        elif macro == "PGY_CALL_HANDOFF_DIVERGENCE":
            if f[1] not in ("NATIVE", "SELF_HOST") or f[2] not in ("ROW", "PROJECTION", "INPUT"):
                raise ValueError(f"line {line}: divergence needs NATIVE|SELF_HOST and ROW|PROJECTION|INPUT")
            if re.fullmatch(r"S[1-9]", f[3]) is None or f[3] == "S1":
                raise ValueError(f"line {line}: divergence must name a later slice S2..S9")
            prefix = "NATIVE_" if f[1] == "NATIVE" else "SELF_"
            if not f[0].startswith(prefix):
                raise ValueError(f"line {line}: {f[1]} divergence name must start with {prefix}")
            declare(divergence_names, f[0], "divergence", line)
            rule.divergences.append(Divergence(*f))
        elif macro == "PGY_CALL_HANDOFF_NATIVE_SUMMARY_BIT":
            if f[0] not in NATIVE_SUMMARY_BITS or any(b == f[0] for b, _ in rule.native_bits):
                raise ValueError(f"line {line}: unknown or duplicate summary bit {f[0]}")
            rule.native_bits.append((f[0], member_mask(rule, f[1], line, "\0", None)))
        elif macro == "PGY_CALL_HANDOFF_SELF_HOST_EFFECT":
            code = int(f[0]) if re.fullmatch(r"0|[1-9][0-9]*", f[0]) else -1
            if code < 0 or code in rule.effects:
                raise ValueError(f"line {line}: invalid or duplicate effect code {f[0]}")
            rule.effects[code] = member_mask(rule, f[1], line, "NONE", None)
        elif macro == "PGY_CALL_HANDOFF_SELF_HOST_EFFECT_DIVERGENCE":
            code = int(f[1]) if re.fullmatch(r"0|[1-9][0-9]*", f[1]) else -1
            if code not in rule.effects:
                raise ValueError(f"line {line}: effect divergence names an undeclared code {f[1]}")
            rule.effect_divergences.append((f[0], code, member_mask(rule, f[2], line, "NONE", None)))
        else:
            divergence = f[0] if macro == "PGY_CALL_HANDOFF_DIVERGENCE_ROW" else None
            f = f[1:] if divergence is not None else f
            if divergence is not None and divergence not in divergence_names:
                raise ValueError(f"line {line}: undeclared divergence {divergence}")
            if f[5] not in rule.actions:
                raise ValueError(f"line {line}: unknown action {f[5]}")
            if divergence is None and f[5] in rule.divergence_only:
                raise ValueError(f"line {line}: {f[5]} is for divergence rows only")
            row = Row(divergence, name_set(f[0], kind_names, "kind", line, "ANY"),
                      tuple(name_set(t, rule.axes[a], a.lower(), line, "ANY")
                            for t, a in zip(f[1:5], AXES)),
                      rule.actions.index(f[5]))
            (rule.divergence_rows if divergence else rule.rows).append(row)
    validate(rule)
    return rule


def validate(rule: Rule) -> None:
    if not rule.members or len(rule.members) > 16:
        raise ValueError("the rule needs between 1 and 16 members")
    if any(not rule.axes[a] for a in AXES) or not rule.actions or not rule.rows:
        raise ValueError("every axis, the actions and the rows need entries")
    # The front ends name these values; a renamed or dropped one must fail here.
    for what, names, present in (("member", REQUIRED_MEMBERS, rule.members),
                                 ("action", REQUIRED_ACTIONS, rule.actions),
                                 *(((a.lower(), REQUIRED_AXES[a], rule.axes[a]) for a in AXES))):
        missing = [n for n in names if n not in present]
        if missing:
            raise ValueError(f"the front ends need {what} values {', '.join(missing)}")
    # A hand-off releases nothing on either side (decision 2026-11-15), so it
    # is sound only for a named local's one storage that is not read again.
    origin_local = frozenset({rule.axes["ORIGIN"].index("LOCAL")})
    later_once = frozenset({rule.axes["LATER"].index("ONCE")})
    hand_off = rule.actions.index("HAND_OFF")
    for row in rule.rows + rule.divergence_rows:
        if row.action == hand_off and (row.axes[0] != origin_local or row.axes[1] != later_once):
            raise ValueError("a HAND_OFF row must take only a LOCAL origin used ONCE")
    kinds_seen = {rule.kind_of(m) for m in range(rule.all_members + 1)}
    for index, kind in enumerate(rule.kinds):
        if index not in kinds_seen:
            raise ValueError(f"kind {kind.name} matches no class set")
    covered: set[tuple[int, ...]] = set()
    for row in rule.rows:
        reached = [c for c in rule.cells() if row_matches(row, c) and c not in covered]
        if not reached:
            raise ValueError(f"a {rule.actions[row.action]} row is shadowed by earlier rows")
        covered.update(reached)
    missing = [c for c in rule.cells() if c not in covered]
    if missing:
        raise ValueError(f"the decision table leaves {len(missing)} cells without an action")
    forms = {d.name: d for d in rule.divergences}
    for row in rule.divergence_rows:
        if forms[row.divergence].form != "ROW":
            raise ValueError(f"divergence {row.divergence} is not a ROW divergence")
        if not any(row_matches(row, c) and rule.lookup(c, set()) != row.action for c in rule.cells()):
            raise ValueError(f"divergence row of {row.divergence} changes no cell")
    for name, code, members in rule.effect_divergences:
        if forms.get(name) is None or forms[name].form != "PROJECTION":
            raise ValueError(f"effect divergence {name} is not a PROJECTION divergence")
        if rule.effects[code] == members:
            raise ValueError(f"effect divergence {name} does not change code {code}")
    for d in rule.divergences:
        rows = [r for r in rule.divergence_rows if r.divergence == d.name]
        effects = [e for e in rule.effect_divergences if e[0] == d.name]
        if d.form == "ROW" and not rows or d.form == "PROJECTION" and not effects:
            raise ValueError(f"divergence {d.name} has no rows")
        if d.form == "PROJECTION" and d.front_end != "SELF_HOST":
            raise ValueError(f"projection divergence {d.name} must be a self-host one")
    if sorted(rule.effects) != list(range(len(rule.effects))):
        raise ValueError("self-host effect codes must run from 0 without gaps")
    if {b for b, _ in rule.native_bits} != set(NATIVE_SUMMARY_BITS):
        raise ValueError("every native summary bit needs a projection")


def flag_divergences(rule: Rule) -> list[Divergence]:
    return [d for d in rule.divergences if d.form != "INPUT"]


def any_use_table(rule: Rule) -> dict[tuple[int, int, int], int | None]:
    """The action every origin and later use gets, without divergences."""
    table: dict[tuple[int, int, int], int | None] = {}
    origins, laters = len(rule.axes["ORIGIN"]), len(rule.axes["LATER"])
    for kind, context, elements in product(range(len(rule.kinds)),
                                           range(len(rule.axes["CONTEXT"])),
                                           range(len(rule.axes["ELEMENTS"]))):
        actions = {rule.lookup((kind, o, l, context, elements), set())
                   for o in range(origins) for l in range(laters)}
        table[(kind, context, elements)] = actions.pop() if len(actions) == 1 else None
    return table


def bits(indices) -> int:
    return sum(1 << i for i in indices)


def camel(name: str) -> str:
    return "".join(part.capitalize() for part in name.split("_"))


HEADER = "Generated by scripts/render_call_handoff_rule.py."
SOURCE = "Source: src/semantic/call_handoff_rule.def. Do not edit by hand."


def divergence_comment(rule: Rule, prefix: str) -> list[str]:
    lines = [f"{prefix}Divergences from the rule kept by S1 (front end, form, removing slice):"]
    for d in rule.divergences:
        lines.append(f"{prefix}- {d.name}: {d.front_end}, {d.form}, {d.slice}")
    return lines


def render_c(rule: Rule) -> str:
    out = [f"/* {HEADER}", f" * {SOURCE}", " *"]
    out += divergence_comment(rule, " * ")
    out += [" */", "#ifndef PGY_CALL_HANDOFF_RULE_H", "#define PGY_CALL_HANDOFF_RULE_H", "",
            "#include <stddef.h>", "", '#include "slot_summary.h"', "", "enum {"]
    for i, m in enumerate(rule.members):
        out.append(f"    PGY_CALL_HANDOFF_MEMBER_{m} = 1u << {i},")
    moving = bits(i for i, flag in enumerate(rule.moving) if flag)
    out += [f"    PGY_CALL_HANDOFF_MEMBER_ALL = {rule.all_members:#x}u,",
            f"    PGY_CALL_HANDOFF_MEMBER_MOVING = {moving:#x}u", "};", ""]

    def enum(type_name: str, prefix: str, names: list[str], last: str) -> None:
        out.append("typedef enum {")
        for i, n in enumerate(names):
            out.append(f"    {prefix}{n} = {i},")
        out.append(f"    {prefix}{last}")
        out.append(f"}} {type_name};")
        out.append("")

    enum("PgyCallHandoffKind", "PGY_CALL_HANDOFF_KIND_", [k.name for k in rule.kinds], "COUNT")
    for axis, type_name in zip(AXES, ("Origin", "Later", "Context", "Elements")):
        enum(f"PgyCallHandoff{type_name}", f"PGY_CALL_HANDOFF_{axis}_", rule.axes[axis], "COUNT")
    enum("PgyCallHandoffAction", "PGY_CALL_HANDOFF_ACTION_", rule.actions, "UNDECIDED")
    flagged = flag_divergences(rule)
    out.append("enum {")
    for i, d in enumerate(flagged):
        out.append(f"    PGY_CALL_HANDOFF_DIVERGENCE_{d.name} = 1u << {i},")
    native = " | ".join(f"PGY_CALL_HANDOFF_DIVERGENCE_{d.name}"
                        for d in flagged if d.front_end == "NATIVE" and d.form == "ROW") or "0u"
    out += ["    PGY_CALL_HANDOFF_DIVERGENCE_NONE = 0u,",
            f"    PGY_CALL_HANDOFF_DIVERGENCE_FLAG_COUNT = {len(flagged)}", "};", "",
            "/* The row divergences the native front end selects (see the .def). */",
            f"#define PGY_CALL_HANDOFF_NATIVE_ROW_DIVERGENCES ({native})", "",
            "/* The kind of a class set: the first kind it matches. A bit outside",
            " * the members fails closed to the kind of an unproved set. */",
            "static inline PgyCallHandoffKind",
            "pgy_call_handoff_kind(unsigned members)", "{"]
    unproved_kind = rule.kinds[rule.kind_of(1 << rule.members.index("UNPROVED"))].name \
        if "UNPROVED" in rule.members else rule.kinds[-1].name
    out.append("    if ((members & ~(unsigned)PGY_CALL_HANDOFF_MEMBER_ALL) != 0u)")
    out.append(f"        return PGY_CALL_HANDOFF_KIND_{unproved_kind};")
    for kind in rule.kinds:
        tests = []
        if kind.any_of:
            tests.append(f"(members & {kind.any_of:#x}u) != 0u")
        if kind.only != rule.all_members:
            tests.append(f"(members & ~{kind.only:#x}u) == 0u")
        if not tests:
            out.append(f"    return PGY_CALL_HANDOFF_KIND_{kind.name};")
            break
        out.append(f"    if ({' && '.join(tests)}) return PGY_CALL_HANDOFF_KIND_{kind.name};")
    else:
        out.append(f"    return PGY_CALL_HANDOFF_KIND_{unproved_kind};")
    out += ["}", "",
            "/* Study 2.1, native column: a parameter-flow summary mask as a class",
            " * set. A bit the projection does not name is UNPROVED. */",
            "static inline unsigned",
            "pgy_call_handoff_members_from_native_summary(unsigned mask)", "{",
            "    unsigned members = 0u;"]
    for bit, members in rule.native_bits:
        out.append(f"    if ((mask & (unsigned){bit}) != 0u) members |= {members:#x}u;")
    named = " | ".join(f"(unsigned){b}" for b, _ in rule.native_bits)
    out += [f"    if ((mask & ~({named})) != 0u)",
            "        members |= PGY_CALL_HANDOFF_MEMBER_UNPROVED;", "    return members;", "}", "",
            "/* The decision table: divergence rows the caller selects first, then",
            " * the rule. The renderer proves every cell has exactly one rule action. */",
            "static inline PgyCallHandoffAction",
            "pgy_call_handoff_action_for_kind(PgyCallHandoffKind kind,",
            "    PgyCallHandoffOrigin origin, PgyCallHandoffLater later,",
            "    PgyCallHandoffContext context, PgyCallHandoffElements elements,",
            "    unsigned divergences)", "{",
            "    static const struct {",
            "        unsigned divergence, kinds, origins, later, contexts, elements;",
            "        PgyCallHandoffAction action;",
            "    } rows[] = {"]
    flag_index = {d.name: i for i, d in enumerate(flagged)}
    for row in rule.divergence_rows + rule.rows:
        flag = f"1u << {flag_index[row.divergence]}" if row.divergence else "0u"
        sets = ", ".join(f"{bits(s):#x}u" for s in (row.kinds, *row.axes))
        out.append(f"        {{{flag}, {sets}, PGY_CALL_HANDOFF_ACTION_{rule.actions[row.action]}}},")
    out += ["    };",
            "    if ((unsigned)kind >= PGY_CALL_HANDOFF_KIND_COUNT ||",
            "        (unsigned)origin >= PGY_CALL_HANDOFF_ORIGIN_COUNT ||",
            "        (unsigned)later >= PGY_CALL_HANDOFF_LATER_COUNT ||",
            "        (unsigned)context >= PGY_CALL_HANDOFF_CONTEXT_COUNT ||",
            "        (unsigned)elements >= PGY_CALL_HANDOFF_ELEMENTS_COUNT)",
            "        return PGY_CALL_HANDOFF_ACTION_UNDECIDED;",
            "    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {",
            "        if (rows[i].divergence != 0u && (divergences & rows[i].divergence) == 0u)",
            "            continue;",
            "        if (((rows[i].kinds >> (unsigned)kind) & 1u) != 0u &&",
            "            ((rows[i].origins >> (unsigned)origin) & 1u) != 0u &&",
            "            ((rows[i].later >> (unsigned)later) & 1u) != 0u &&",
            "            ((rows[i].contexts >> (unsigned)context) & 1u) != 0u &&",
            "            ((rows[i].elements >> (unsigned)elements) & 1u) != 0u)",
            "            return rows[i].action;",
            "    }",
            "    return PGY_CALL_HANDOFF_ACTION_UNDECIDED;", "}", "",
            "static inline PgyCallHandoffAction",
            "pgy_call_handoff_action(unsigned members, PgyCallHandoffOrigin origin,",
            "    PgyCallHandoffLater later, PgyCallHandoffContext context,",
            "    PgyCallHandoffElements elements, unsigned divergences)", "{",
            "    return pgy_call_handoff_action_for_kind(pgy_call_handoff_kind(members),",
            "        origin, later, context, elements, divergences);", "}", "",
            "/* The rule action every origin and later use of a kind gets, or",
            " * UNDECIDED when they differ: for a site that knows neither. */",
            "static inline PgyCallHandoffAction",
            "pgy_call_handoff_action_any_use(PgyCallHandoffKind kind,",
            "    PgyCallHandoffContext context, PgyCallHandoffElements elements)", "{"]
    table = any_use_table(rule)
    contexts, elements = len(rule.axes["CONTEXT"]), len(rule.axes["ELEMENTS"])
    out.append(f"    static const PgyCallHandoffAction table[{len(rule.kinds)}][{contexts}][{elements}] = {{")
    for kind in range(len(rule.kinds)):
        cells = []
        for context in range(contexts):
            row = [table[(kind, context, e)] for e in range(elements)]
            names = [f"PGY_CALL_HANDOFF_ACTION_{rule.actions[a]}" if a is not None
                     else "PGY_CALL_HANDOFF_ACTION_UNDECIDED" for a in row]
            cells.append("{" + ", ".join(names) + "}")
        out.append(f"        /* {rule.kinds[kind].name} */ {{" + ", ".join(cells) + "},")
    out += ["    };",
            "    if ((unsigned)kind >= PGY_CALL_HANDOFF_KIND_COUNT ||",
            "        (unsigned)context >= PGY_CALL_HANDOFF_CONTEXT_COUNT ||",
            "        (unsigned)elements >= PGY_CALL_HANDOFF_ELEMENTS_COUNT)",
            "        return PGY_CALL_HANDOFF_ACTION_UNDECIDED;",
            "    return table[kind][context][elements];", "}", ""]
    out += render_c_names(rule)
    out += ["#endif", ""]
    return "\n".join(out)


def render_c_names(rule: Rule) -> list[str]:
    """The MIR JSON spellings of the vocabulary (slice S2 rows)."""
    out = ["/* MIR JSON spellings of the vocabulary (call_handoff rows, slice S2):",
           " * a member by its bit index, each axis value, each action and each",
           " * divergence by its declaration index. NULL outside the vocabulary. */"]

    def name_function(fn: str, param: str, names: list[str]) -> None:
        out.extend(["static inline const char *", f"{fn}({param})", "{",
                    "    switch ((unsigned)value) {"])
        for i, n in enumerate(names):
            out.append(f"    case {i}: return \"{n}\";")
        out.extend(["    default: return NULL;", "    }", "}", ""])

    name_function("pgy_call_handoff_member_name", "unsigned value", rule.members)
    for axis, type_name in zip(AXES, ("Origin", "Later", "Context", "Elements")):
        name_function(f"pgy_call_handoff_{axis.lower()}_name",
                      f"PgyCallHandoff{type_name} value", rule.axes[axis])
    name_function("pgy_call_handoff_action_name", "PgyCallHandoffAction value", rule.actions)
    out.append("typedef enum {")
    for i, d in enumerate(rule.divergences):
        out.append(f"    PGY_CALL_HANDOFF_DIVERGENCE_ID_{d.name} = {i},")
    out += ["    PGY_CALL_HANDOFF_DIVERGENCE_ID_COUNT", "} PgyCallHandoffDivergenceId;", ""]
    name_function("pgy_call_handoff_divergence_name", "PgyCallHandoffDivergenceId value",
                  [d.name for d in rule.divergences])
    return out


def render_pgy_names(rule: Rule) -> list[str]:
    """The MIR JSON spellings of the vocabulary (slice S2 rows)."""
    out = ["// MIR JSON spellings of the vocabulary (call_handoff rows, slice S2):",
           "// a member by its bit index, each axis value, each action and each",
           "// divergence by its declaration index, with the divergence's form and",
           "// row flag (0 for an input divergence). \"\" outside the vocabulary."]

    def name_function(fn: str, names: list[str]) -> None:
        out.append(f"func SemanticCallHandoff{fn}(value: Int) -> String {{")
        for i, n in enumerate(names):
            out.append(f"    if value == {i} {{ return \"{n}\"; }}")
        out.extend(["    return \"\";", "}"])

    out.append(f"func SemanticCallHandoffMemberCount() -> Int {{ return {len(rule.members)}; }}")
    name_function("MemberName", rule.members)
    for axis in AXES:
        out.append(f"func SemanticCallHandoff{camel(axis)}Count() -> Int "
                   f"{{ return {len(rule.axes[axis])}; }}")
        name_function(f"{camel(axis)}Name", rule.axes[axis])
    out.append(f"func SemanticCallHandoffActionCount() -> Int {{ return {len(rule.actions)}; }}")
    name_function("ActionName", rule.actions)
    out.append(f"func SemanticCallHandoffDivergenceCount() -> Int "
               f"{{ return {len(rule.divergences)}; }}")
    for i, d in enumerate(rule.divergences):
        out.append(f"func SemanticCallHandoffDivergenceId{camel(d.name)}() -> Int {{ return {i}; }}")
    name_function("DivergenceName", [d.name for d in rule.divergences])
    name_function("DivergenceForm", [d.form for d in rule.divergences])
    flag_index = {d.name: i for i, d in enumerate(flag_divergences(rule))}
    out.append("func SemanticCallHandoffDivergenceFlag(value: Int) -> Int {")
    for i, d in enumerate(rule.divergences):
        if d.name in flag_index:
            out.append(f"    if value == {i} {{ return {1 << flag_index[d.name]}; }}")
    out.extend(["    return 0;", "}", ""])
    return out


def render_pgy(rule: Rule) -> str:
    out = [f"// {HEADER}", f"// {SOURCE}", "//"]
    out += divergence_comment(rule, "// ")
    out.append("")

    def constant(name: str, value: int) -> None:
        out.append(f"func SemanticCallHandoff{name}() -> Int {{ return {value}; }}")

    for i, m in enumerate(rule.members):
        constant(f"Member{camel(m)}", 1 << i)
    for i, k in enumerate(rule.kinds):
        constant(f"Kind{camel(k.name)}", i)
    for axis in AXES:
        for i, v in enumerate(rule.axes[axis]):
            constant(f"{camel(axis)}{camel(v)}", i)
    for i, a in enumerate(rule.actions):
        constant(f"Action{camel(a)}", i)
    constant("ActionUndecided", len(rule.actions))
    flagged = flag_divergences(rule)
    for i, d in enumerate(flagged):
        constant(f"Divergence{camel(d.name)}", 1 << i)
    constant("DivergenceFlagCount", len(flagged))
    unproved_kind = rule.kind_of(1 << rule.members.index("UNPROVED")) \
        if "UNPROVED" in rule.members else len(rule.kinds) - 1
    out += ["",
            "// Whether bit `index` is set in the non-negative set `set_bits`.",
            "func SemanticCallHandoffSetHas(set_bits: Int, index: Int) -> Bool {",
            "    if set_bits < 0 || index < 0 { return false; }",
            "    var bit: Int = 1;",
            "    var shift: Int = 0;",
            "    while shift < index { bit = bit * 2; shift = shift + 1; }",
            "    return (set_bits / bit) % 2 == 1;",
            "}",
            "",
            "// The join of two class sets (study 2.1): their union.",
            "func SemanticCallHandoffJoin(left: Int, right: Int) -> Int {",
            "    var joined: Int = 0;",
            "    var bit: Int = 1;",
            "    var index: Int = 0;",
            f"    while index < {len(rule.members)} {{",
            "        if SemanticCallHandoffSetHas(left, index) || SemanticCallHandoffSetHas(right, index) {",
            "            joined = joined + bit;",
            "        }",
            "        bit = bit * 2;",
            "        index = index + 1;",
            "    }",
            "    return joined;",
            "}",
            "",
            "// The kind of a class set: the first kind it matches. A set outside",
            "// the members fails closed to the kind of an unproved set.",
            "func SemanticCallHandoffKind(members: Int) -> Int {",
            f"    if members < 0 || members > {rule.all_members} {{ return {unproved_kind}; }}"]
    for index, kind in enumerate(rule.kinds):
        tests = []
        if kind.any_of:
            tests.append("(" + " || ".join(f"SemanticCallHandoffSetHas(members, {i})"
                                           for i in range(len(rule.members))
                                           if kind.any_of >> i & 1) + ")")
        tests += [f"!SemanticCallHandoffSetHas(members, {i})"
                  for i in range(len(rule.members)) if not kind.only >> i & 1]
        if not tests:
            out.append(f"    return {index};")
            break
        out.append(f"    if {' && '.join(tests)} {{ return {index}; }}")
    else:
        out.append(f"    return {unproved_kind};")
    out += ["}", "",
            "// Study 2.1, self-host column: a formal effect code as a class set.",
            "// A code the projection does not name is UNPROVED.",
            "func SemanticCallHandoffMembersForSelfHostEffect(effect_code: Int, divergences: Int) -> Int {"]
    flag_index = {d.name: i for i, d in enumerate(flagged)}
    for name, code, members in rule.effect_divergences:
        out.append(f"    if SemanticCallHandoffSetHas(divergences, {flag_index[name]}) && "
                   f"effect_code == {code} {{ return {members}; }}")
    for code in sorted(rule.effects):
        out.append(f"    if effect_code == {code} {{ return {rule.effects[code]}; }}")
    unproved = 1 << rule.members.index("UNPROVED") if "UNPROVED" in rule.members else rule.all_members
    out += [f"    return {unproved};", "}", "",
            "// The decision table: divergence rows the caller selects first, then",
            "// the rule. The renderer proves every cell has exactly one rule action.",
            "func SemanticCallHandoffActionForKind(",
            "    kind: Int, origin: Int, later: Int, site_context: Int, elements: Int, divergences: Int",
            ") -> Int {"]
    names = ("kind", "origin", "later", "site_context", "elements")
    for row in rule.divergence_rows + rule.rows:
        tests = []
        if row.divergence:
            tests.append(f"SemanticCallHandoffSetHas(divergences, {flag_index[row.divergence]})")
        sizes = [len(rule.kinds)] + [len(rule.axes[a]) for a in AXES]
        for name, values, size in zip(names, (row.kinds, *row.axes), sizes):
            if len(values) < size:
                tests.append(f"SemanticCallHandoffSetHas({bits(values)}, {name})")
        cond = " &&\n        ".join(tests) if tests else "true"
        out.append(f"    if {cond} {{ return {row.action}; }}")
    out += [f"    return {len(rule.actions)};", "}", "",
            "func SemanticCallHandoffAction(",
            "    members: Int, origin: Int, later: Int, site_context: Int, elements: Int, divergences: Int",
            ") -> Int {",
            "    return SemanticCallHandoffActionForKind(SemanticCallHandoffKind(members), origin, later,",
            "        site_context, elements, divergences);",
            "}", "",
            "// The action every named owner (any origin but FRESH) gets, or Undecided",
            "// when they differ: for a site that knows the place is named, not whose.",
            "func SemanticCallHandoffActionForNamedOwner(",
            "    members: Int, later: Int, site_context: Int, elements: Int, divergences: Int",
            ") -> Int {",
            "    var common_answer: Int = -1;",
            "    var origin: Int = 0;",
            f"    while origin < {len(rule.axes['ORIGIN'])} {{",
            f"        if origin != {rule.axes['ORIGIN'].index('FRESH')} {{",
            "            let answer: Int = SemanticCallHandoffAction(members, origin, later, site_context,",
            "                elements, divergences);",
            "            if common_answer >= 0 && common_answer != answer { return SemanticCallHandoffActionUndecided(); }",
            "            common_answer = answer;",
            "        }",
            "        origin = origin + 1;",
            "    }",
            "    return common_answer;",
            "}", ""]
    out += render_pgy_names(rule)
    return "\n".join(out)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("rule", type=Path)
    parser.add_argument("header", type=Path)
    parser.add_argument("owner", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    rule = load(args.rule)
    outputs = ((args.header, render_c(rule)), (args.owner, render_pgy(rule)))
    if args.check:
        stale = [str(path) for path, text in outputs
                 if not path.is_file() or path.read_text(encoding="utf-8") != text]
        if stale:
            raise SystemExit("call hand-off rule outputs are stale: " + ", ".join(stale))
        return 0
    for path, text in outputs:
        path.write_text(text, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

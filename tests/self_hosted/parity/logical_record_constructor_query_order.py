"""Structural residue/order guard; executable parity owns query behavior."""
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text(encoding="utf-8")
name = "DirectMirScalarProgramLogicalRecordConstructorFromGraph"
match = re.search(r"^func " + name + r"\(.*?^}", source, re.M | re.S)
if match is None:
    raise SystemExit("[constructor-order] missing constructor query")
body = " ".join(match.group().split())
if body.count("DirectMirScalarProgramLogicalRecordRow(") != 1:
    raise SystemExit("[constructor-order] constructor must acquire its validated row once")
if "DirectMirScalarProgramLogicalRecordFieldType(" in body:
    raise SystemExit("[constructor-order] per-argument full-table query reopened")
position = 0
for token in (
    "DirectMirScalarProgramLogicalRecordRow(",
    "sequence.arena.identities.call_target_syntax_ids[chain.call_node] != 0",
    "SemanticCallTargetDirect()",
    "record_row < 0",
    "record_type != record.names[record_row]",
    "if count > record.field_counts[record_row]",
    "let field_start: Int = record.field_starts[record_row];",
    "while ordinal < count",
    "record.field_types[field_start + ordinal]",
    "count == record.field_counts[record_row]",
):
    found = body.find(token, position)
    if found < 0:
        raise SystemExit(f"[constructor-order] missing/out-of-order {token}")
    position = found + len(token)
print("[constructor-order] one validated row, bounded local field reads, no repeated query: PASS")

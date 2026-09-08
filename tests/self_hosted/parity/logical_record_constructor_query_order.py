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
for query in ("DirectMirScalarProgramLogicalRecordRow(", "DirectMirIdentityCellRow("):
    if body.count(query) != 1:
        raise SystemExit(f"[constructor-order] constructor must acquire {query} once")
for query in ("DirectMirScalarProgramLogicalRecordFieldType(", "DirectMirIdentityCellFieldType("):
    if query in body:
        raise SystemExit("[constructor-order] per-argument full-table query reopened")
position = 0
for token in (
    "DirectMirScalarProgramLogicalRecordRow(",
    "DirectMirIdentityCellRow(",
    "sequence.arena.identities.call_target_syntax_ids[chain.call_node] != 0",
    "SemanticCallTargetDirect()",
    "(record_row < 0 && cell_row < 0) || (record_row >= 0 && cell_row >= 0)",
    "if cell_row >= 0",
    "field_count = record.identity_cells.field_counts[cell_row]",
    "field_start = record.identity_cells.field_starts[cell_row]",
    "field_count = record.field_counts[record_row]",
    "field_start = record.field_starts[record_row]",
    "if count > field_count",
    "while ordinal < count",
    "record.identity_cells.field_types[field_start + ordinal]",
    "record.field_types[field_start + ordinal]",
    "count == field_count",
):
    found = body.find(token, position)
    if found < 0:
        raise SystemExit(f"[constructor-order] missing/out-of-order {token}")
    position = found + len(token)
print("[constructor-order] one validated row, bounded local field reads, no repeated query: PASS")

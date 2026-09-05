"""Assert the single-step fixture's published v3 plan, not full MIR validity."""

import json
import sys
from pathlib import Path

document = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8-sig"))
assert document["schema"] == "pgy.mir.v1", "wrong MIR schema"
plan = document.get("intent_execution")
assert isinstance(plan, dict), "source MIR omitted intent_execution"
assert plan["schema"] == "pgy.selfhost.mir-intent-execution-plan.v3"
assert isinstance(plan["plan_digest"], int) and plan["plan_digest"] != 0
assert len(plan["steps"]) == 1 and len(plan["terminals"]) == 2
step = plan["steps"][0]
assert step["step_name"] == "Read" and not step["has_predecessor"]
assert step["where_zone_name"] == "AuditZone" and step["where_zone_syntax_id"] > 0
assert step["success_payload_decl_syntax_id"] > 0
assert step["failure_payload_decl_syntax_id"] > 0
for terminal in plan["terminals"]:
    assert terminal["source_payload_decl_syntax_id"] > 0
    assert terminal["result_payload_decl_syntax_id"] == terminal["source_payload_decl_syntax_id"]

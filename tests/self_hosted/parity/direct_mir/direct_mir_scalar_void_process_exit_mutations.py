import json
import pathlib
import sys


source = pathlib.Path(sys.argv[1])
mutation = sys.argv[2]
target = pathlib.Path(sys.argv[3])
document = json.loads(source.read_text(encoding="utf-8"))
routine = next(row for row in document["routines"] if row["name"] == "FailClosed")
zero_void = next(row for row in document["routines"] if row["name"] == "RequireProbe")
nonvoid_exit = next(row for row in document["routines"] if row["name"] == "RequiredName")

if mutation == "scalar-carriage":
    routine["params"][0]["carriage"] = "readonly-ref"
elif mutation == "exit-value-type":
    instruction = next(
        row
        for block in routine["blocks"]
        for row in block["instructions"]
        if row.get("arg0") == "Exit"
    )
    instruction["expr0"] = '"bad"'
    instruction["expr0_graph"]["nodes"][0]["kind"] = "string_literal"
    instruction["expr0_graph"]["nodes"][0]["text"] = '"bad"'
elif mutation == "zero-void-return-type":
    zero_void["return"] = "Array<String>"
elif mutation == "zero-void-phantom-param":
    parameter = dict(routine["params"][0])
    parameter["name"] = "phantom"
    parameter["carriage"] = "readonly-ref"
    zero_void["params"].append(parameter)
elif mutation == "nonvoid-terminal-not-exit":
    instruction = next(
        row for block in nonvoid_exit["blocks"]
        for row in block["instructions"] if row.get("arg0") == "Exit"
    )
    instruction["arg0"] = "Log"
    instruction["expr0"] = "Log(9)"
else:
    raise SystemExit(f"unknown mutation: {mutation}")

target.write_text(
    json.dumps(document, separators=(",", ":"), ensure_ascii=False),
    encoding="utf-8",
)

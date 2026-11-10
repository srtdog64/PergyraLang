import json
import pathlib
import sys


source, output = map(pathlib.Path, sys.argv[1:])
document = json.loads(source.read_text(encoding="utf-8"))
routine = next(row for row in document["routines"]
               if row.get("name") == "RelayBoolRows")
parameter = routine["params"][0]
assert parameter["type"] == "Array<Bool>"
assert parameter["carriage"] == "owner-handle"
parameter["carriage"] = "readonly-ref"
output.write_text(json.dumps(document, separators=(",", ":")) + "\n",
                  encoding="utf-8")

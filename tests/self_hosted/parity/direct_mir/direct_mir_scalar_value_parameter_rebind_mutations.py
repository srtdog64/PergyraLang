#!/usr/bin/env python3
"""Break the formal identity that owns one rebound scalar working local."""

import json
import sys


source_path, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source_file:
    document = json.load(source_file)

routine = next(row for row in document["routines"] if row["name"] == "Advance")
assert routine["params"][0]["name"] == "start"
routine["params"][0]["name"] = "unrelated"

with open(output_path, "w", encoding="utf-8", newline="\n") as output_file:
    json.dump(document, output_file, separators=(",", ":"))
    output_file.write("\n")

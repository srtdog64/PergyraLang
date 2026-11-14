#!/usr/bin/env python3
import copy
import json
import sys

source_path, mode, output_path = sys.argv[1:4]
with open(source_path, "r", encoding="utf-8") as stream:
    document = json.load(stream)

routine = next(row for row in document["routines"] if row["name"] == "AdvanceProbe")

if mode == "copyout-pass":
    routine["params"][6]["pass"] = "indirect"
elif mode == "record-carriage":
    routine["params"][1]["carriage"] = "owner-handle"
    routine["params"][1]["pass"] = "direct"
elif mode == "return-identity":
    routine["return"] = "SignatureProbe"
else:
    raise SystemExit(f"unknown mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(document, stream, separators=(",", ":"))
    stream.write("\n")

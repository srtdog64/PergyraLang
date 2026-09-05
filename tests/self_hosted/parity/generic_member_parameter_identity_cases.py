"""Cross-epoch parity and receiver-ID cases shared by generic member gates."""

import copy
import json
from pathlib import Path


def routine(document, name):
    rows = [row for row in document["routines"] if row["name"] == name]
    if len(rows) != 1:
        raise RuntimeError(f"expected one routine named {name}")
    return rows[0]


def assert_parameter_identity(self_path, native_path, names):
    # Producer SyntaxNodeIds differ; compare complete rows after owned rebinding.
    documents = [json.loads(Path(path).read_text(encoding="utf-8"))
                 for path in (self_path, native_path)]
    for name in names:
        if routine(documents[0], name)["params"] != routine(documents[1], name)["params"]:
            raise RuntimeError(f"canonical {name} parameter identity drifted")


def emit_receiver_identity_cases(baseline, output_dir, method):
    matches = [index for index, row in enumerate(baseline["routines"])
               if row["name"] == method]
    if len(matches) != 1:
        raise RuntimeError(f"expected one receiver owner named {method}")
    row = matches[0]
    parameters = baseline["routines"][row]["params"]
    for name, value in (
            ("renumber", 100000), ("missing", None), ("zero", 0),
            ("negative", -1), ("fractional", 8.5), ("string", "8"),
            ("null", None), ("collision", parameters[1]["source_syntax_id"])):
        document = copy.deepcopy(baseline)
        receiver = document["routines"][row]["params"][0]
        if name == "missing":
            receiver.pop("source_syntax_id")
        else:
            receiver["source_syntax_id"] = value
        (output_dir / f"receiver-id-{name}.json").write_text(
            json.dumps(document, separators=(",", ":")), encoding="utf-8")
    # Serialize the duplicate key literally; a dict would silently remove it.
    receiver_json = json.dumps(parameters[0], separators=(",", ":"))
    duplicate = receiver_json[:-1] + ",\"source_syntax_id\":" + str(
        parameters[0]["source_syntax_id"]) + "}"
    document_json = json.dumps(baseline, separators=(",", ":"))
    if document_json.count(receiver_json) != 1:
        raise RuntimeError("receiver duplicate-ID mutation has an ambiguous target")
    (output_dir / "receiver-id-duplicate.json").write_text(
        document_json.replace(receiver_json, duplicate), encoding="utf-8")

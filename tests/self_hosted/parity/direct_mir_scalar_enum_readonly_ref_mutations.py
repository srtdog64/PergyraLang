import json
import sys


def routine(document, name):
    row = next(
        (item for item in document.get("routines", [])
         if item.get("name") == name),
        None,
    )
    if row is None or len(row.get("params", [])) != 1:
        raise SystemExit(f"fixture has no exact {name} signature")
    return row


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)

    observation = routine(document, "EnumRefObservationReady")["params"][0]
    machine = routine(document, "EnumRefMachineReady")["params"][0]
    targets = {
        "payload-free-pass-indirect": (observation, "pass", "indirect"),
        "payload-free-resource": (observation, "resource", "region"),
        "payload-free-abi-required": (
            observation, "abi_layout_required", True),
        "payload-free-layout-id": (observation, "abi_layout_id", 1),
        "payload-enum-pass-indirect": (machine, "pass", "indirect"),
        "payload-enum-resource": (machine, "resource", "region"),
        "payload-enum-abi-required": (machine, "abi_layout_required", True),
        "payload-enum-layout-id": (machine, "abi_layout_id", 1),
    }
    if kind not in targets:
        raise SystemExit(f"unknown mutation: {kind}")
    target, field, value = targets[kind]
    target[field] = value

    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()

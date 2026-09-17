#!/usr/bin/env python3
"""Fail-closed admission for the vendored PgyMath registry projection."""

import argparse
import hashlib
import json
import pathlib
import re
import sys


MAX_RECEIPT_BYTES = 256 * 1024
MAX_PROJECTION_BYTES = 256 * 1024
MAX_JSON_DEPTH = 32
EXPECTED_SOURCE_COMMIT = "74185a3ad8c54711f3c2e30597ab0a05603a79a6"
EXPECTED_REGISTRY_DIGEST = (
    "sha256:1bc6e88600e1fa66fd7ee13eb6ec5478a3e9d1513d97ea25c7ac2f63dc7a0275"
)
EXPECTED_PROJECTION_DIGEST = (
    "sha256:fd0f35c348caa4524350f764ef1e57836821528245fc627f6d4fc677088ef5ac"
)
GENERATED_MARKER = (
    "// Generated from PgyMath.Templates.Registry. Do not edit by hand."
)
REQUIRED_KEYS = {
    "artifactSchema",
    "sourceRepository",
    "sourceCommit",
    "registryDigest",
    "schemaVersion",
    "templateCount",
    "templateIds",
    "lookupKeys",
    "computeOperations",
    "verificationContract",
}
REQUIRED_VERIFICATION_CONTRACT = {
    "id": "verification.function_contract",
    "owner": "PgyMath.Verify.Contract",
    "proofRefs": [
        {
            "lawId": "validity_definition",
            "theorem": "PgyMath.Verify.valid_iff",
        },
        {
            "lawId": "counterexample_completeness",
            "theorem": "PgyMath.Verify.invalid_iff_exists_counterexample",
        },
    ],
}


class AdmissionError(ValueError):
    pass


def reject_duplicate_keys(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise AdmissionError("duplicate JSON key: " + key)
        value[key] = item
    return value


def check_depth(value, depth=0):
    if depth > MAX_JSON_DEPTH:
        raise AdmissionError("receipt JSON nesting exceeds the admission limit")
    if isinstance(value, dict):
        for item in value.values():
            check_depth(item, depth + 1)
    elif isinstance(value, list):
        for item in value:
            check_depth(item, depth + 1)


def read_bounded(path, limit, label):
    # Read at most one byte beyond the contract limit.  Checking the length
    # after Path.read_bytes() would reject an oversized artifact eventually,
    # but would not bound the memory consumed while admitting it.
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise AdmissionError(
            "{} exceeds {} byte admission limit".format(label, limit)
        )
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise AdmissionError(label + " is not valid UTF-8") from exc


def require_string_list(receipt, key):
    values = receipt.get(key)
    if not isinstance(values, list) or not values:
        raise AdmissionError(key + " must be a non-empty array")
    if not all(isinstance(value, str) and value for value in values):
        raise AdmissionError(key + " must contain only non-empty strings")
    if len(values) != len(set(values)):
        raise AdmissionError(key + " contains duplicate values")
    return values


def load_receipt(path):
    text = read_bounded(path, MAX_RECEIPT_BYTES, "registry receipt")
    try:
        receipt = json.loads(text, object_pairs_hook=reject_duplicate_keys)
    except (json.JSONDecodeError, AdmissionError) as exc:
        raise AdmissionError("invalid registry receipt: " + str(exc)) from exc
    check_depth(receipt)
    if not isinstance(receipt, dict):
        raise AdmissionError("registry receipt root must be an object")
    if set(receipt) != REQUIRED_KEYS:
        missing = sorted(REQUIRED_KEYS - set(receipt))
        extra = sorted(set(receipt) - REQUIRED_KEYS)
        raise AdmissionError(
            "registry receipt fields drifted; missing={} extra={}".format(
                missing, extra
            )
        )
    if receipt["artifactSchema"] != "pgy.math.pergyra-registry-receipt.v1":
        raise AdmissionError("unsupported registry receipt schema")
    if receipt["sourceRepository"] != "https://github.com/srtdog64/pgy_math.git":
        raise AdmissionError("unexpected PgyMath source repository")
    if not isinstance(receipt["sourceCommit"], str) or not re.fullmatch(
        r"[0-9a-f]{40}", receipt["sourceCommit"]
    ):
        raise AdmissionError("sourceCommit must be an exact lowercase Git object id")
    if receipt["sourceCommit"] != EXPECTED_SOURCE_COMMIT:
        raise AdmissionError("PgyMath source commit is not admitted")
    if not isinstance(receipt["registryDigest"], str) or not re.fullmatch(
        r"sha256:[0-9a-f]{64}", receipt["registryDigest"]
    ):
        raise AdmissionError("registryDigest must be a lowercase SHA-256 digest")
    if receipt["registryDigest"] != EXPECTED_REGISTRY_DIGEST:
        raise AdmissionError("PgyMath registry digest is not admitted")
    if receipt["schemaVersion"] != 2:
        raise AdmissionError("PgyMath registry schema version drifted")

    template_ids = require_string_list(receipt, "templateIds")
    lookup_keys = require_string_list(receipt, "lookupKeys")
    compute_operations = require_string_list(receipt, "computeOperations")
    if receipt["templateCount"] != len(template_ids):
        raise AdmissionError("templateCount does not match templateIds")
    if receipt["verificationContract"] != REQUIRED_VERIFICATION_CONTRACT:
        raise AdmissionError("verification.function_contract proof identity drifted")
    if REQUIRED_VERIFICATION_CONTRACT["id"] not in template_ids:
        raise AdmissionError("verification.function_contract is missing from templateIds")
    if REQUIRED_VERIFICATION_CONTRACT["id"] not in lookup_keys:
        raise AdmissionError("verification.function_contract is missing from lookupKeys")
    if "function_contract" not in lookup_keys:
        raise AdmissionError("function_contract lookup keyword is missing")
    return receipt, template_ids, lookup_keys, compute_operations


def render_membership(name, values):
    lines = [
        "    export func {}(value: String) -> Bool".format(name),
        "    {",
    ]
    for value in values:
        encoded = json.dumps(value, ensure_ascii=False)
        lines.append("        if value == {} {{ return true; }}".format(encoded))
    lines.extend(["        return false;", "    }"])
    return "\n".join(lines)


def render_projection(receipt):
    return "\n".join(
        [
            GENERATED_MARKER,
            "namespace PgyMathRegistry",
            "{",
            "    export func SchemaVersion() -> Int",
            "    {",
            "        return {};".format(receipt["schemaVersion"]),
            "    }",
            "",
            "    export func TemplateCount() -> Int",
            "    {",
            "        return {};".format(receipt["templateCount"]),
            "    }",
            "",
            "    export func RegistryDigest() -> String",
            "    {",
            "        return {};".format(
                json.dumps(receipt["registryDigest"], ensure_ascii=False)
            ),
            "    }",
            "",
            render_membership("HasTemplateId", receipt["templateIds"]),
            "",
            render_membership("HasLookupKey", receipt["lookupKeys"]),
            "",
            render_membership(
                "HasComputeOperation", receipt["computeOperations"]
            ),
            "}",
            "",
        ]
    )


def admit(receipt_path, projection_path):
    receipt, _, _, _ = load_receipt(receipt_path)
    observed = read_bounded(
        projection_path, MAX_PROJECTION_BYTES, "Pergyra registry projection"
    ).replace("\r\n", "\n")
    expected = render_projection(receipt)
    expected_digest = "sha256:" + hashlib.sha256(
        expected.encode("utf-8")
    ).hexdigest()
    if expected_digest != EXPECTED_PROJECTION_DIGEST:
        raise AdmissionError(
            "receipt does not derive the admitted Pergyra projection identity"
        )
    if observed != expected:
        raise AdmissionError(
            "Pergyra registry projection does not exactly match its receipt"
        )
    return receipt


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Admit an exact PgyMath registry projection and receipt"
    )
    parser.add_argument("--receipt", required=True, type=pathlib.Path)
    parser.add_argument("--projection", required=True, type=pathlib.Path)
    args = parser.parse_args(argv)
    try:
        receipt = admit(args.receipt, args.projection)
    except (AdmissionError, OSError) as exc:
        print("[pgy-math-admission] reject: {}".format(exc), file=sys.stderr)
        return 1
    print(
        "[pgy-math-admission] admitted commit={} digest={} templates={}".format(
            receipt["sourceCommit"],
            receipt["registryDigest"],
            receipt["templateCount"],
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

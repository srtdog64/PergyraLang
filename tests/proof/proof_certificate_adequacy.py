"""Finite checker equivalence plus benign envelope admission/owner controls."""
import copy
import itertools
import json
import pathlib
import subprocess
import sys
import tempfile
from unittest.mock import patch

import proof_certificate_admission as admission


def compare_finite_checker(checker):
    inputs = []
    expected = []
    for bits in itertools.product((False, True), repeat=admission.CORE_WIDTH):
        inputs.append("".join("1" if b else "0" for b in bits) + "\n")
        expected.append("1\n" if admission.certificate_core_admits(bits) else "0\n")
    # Shape refusals also exercise the model's length precondition.
    for bits in ((), (True,), (True,) * 17, (True,) * 19):
        inputs.append("".join("1" if b else "0" for b in bits) + "\n")
        expected.append("0\n")
    result = subprocess.run([checker], input="".join(inputs), capture_output=True,
                            text=True, timeout=60, check=True)
    if result.stdout != "".join(expected):
        raise AssertionError("extracted and executable finite checker decisions differ")
    for bits in ((1,) * 18, (None,) * 18, ("true",) * 18):
        if admission.certificate_core_admits(bits):
            raise AssertionError("finite admission accepted a non-boolean vector")
    print("[proof-carrying-adequacy] 262144 finite valuations + 4 length controls agree")


def envelope_controls(work):
    source, air_path, mir_path = (work / name for name in ("source.pgy", "air.json", "mir.json"))
    source.write_text("func Main() -> Void {}\n", encoding="utf-8")
    air = {"schema": "pgy.air.graph.v1", "summary": {"strict_evidence": True, "drift_count": 0},
           "evidence": [{"kind": kind, "fallback_count": 0} for kind in admission.AIR_REQUIRED]}
    mir = {"schema": "pgy.mir.v1", "routines": [{"kind": "intent", "blocks": [
        {"instructions": [{"kind": "cleanup", "source_type": "Int", "expr0": "0"}]}]}]}

    def bind(cert, a=air, m=mir):
        air_path.write_text(json.dumps(a), encoding="utf-8")
        mir_path.write_text(json.dumps(m), encoding="utf-8")
        cert["source_digest_sha256"] = admission.digest(source)
        cert["binding_digest_sha256"] = admission.binding_digest(source, air_path, mir_path)
        for layer in cert["layers"]:
            if layer.get("id") in ("air", "mir"):
                layer["digest_sha256"] = admission.digest(air_path if layer["id"] == "air" else mir_path)
        return cert

    certificate = bind({
        "schema": "pgy.proof-carrying-ir.v1", "source": source.as_posix(),
        "policy": {"semantic_fallback": "forbidden", "backend_consumption": "fact-or-fail-closed",
                   "negative_check": "delete-required-fact+mutate-bound-input"},
        "layers": [
            {"id": "air", "payload_schema": "pgy.air.graph.v1", "required_evidence": list(admission.AIR_REQUIRED)},
            {"id": "dag", "payload_schema": "type-resolution-metadata", "status": "manifest-only"},
            {"id": "mir", "payload_schema": "pgy.mir.v1", "required_facts": list(admission.MIR_REQUIRED)},
            {"id": "abi", "payload_schema": "mir-runtime-abi-facts", "status": "manifest-only"},
            {"id": "backend", "payload_schema": "backend-consumption-trace", "status": "manifest-only",
             "consumption": "fact-or-fail-closed"},
        ],
    })
    checks = 0

    def check(cert, accepted, reason=None):
        nonlocal checks
        errors = []
        admission.validate_certificate(cert, source, air_path, mir_path, errors)
        if (not errors) != accepted or (reason and not any(reason in e for e in errors)):
            raise AssertionError("envelope admission control disagreed: " + repr(errors))
        checks += 1

    check(certificate, True)
    # The last consumer must really use the core, with no optimistic fallback.
    with patch.object(admission, "certificate_core_admits", return_value=False) as owner:
        check(certificate, False, "finite certificate facts rejected")
        owner.assert_called_once()
    for kind in admission.REQUIRED_LAYERS:
        bad = copy.deepcopy(certificate)
        bad["layers"] = [layer for layer in bad["layers"] if layer["id"] != kind]
        check(bad, False, "layer set drifted")
    for kind in admission.AIR_REQUIRED:
        a = copy.deepcopy(air)
        a["evidence"] = [row for row in a["evidence"] if row["kind"] != kind]
        check(bind(copy.deepcopy(certificate), a=a), False, "finite certificate facts rejected")
    for name, value in (("strict_evidence", False), ("drift_count", 1)):
        a = copy.deepcopy(air)
        a["summary"][name] = value
        check(bind(copy.deepcopy(certificate), a=a), False, "finite certificate facts rejected")
    for kind in admission.MIR_REQUIRED:
        m = copy.deepcopy(mir)
        if kind == "cfg_blocks":
            m["routines"][0]["blocks"] = []
        else:
            inst = m["routines"][0]["blocks"][0]["instructions"][0]
            if kind == "source_shape":
                del inst["source_type"]
            elif kind == "cleanup":
                inst["kind"] = "value"
            else:
                del inst[kind]
        check(bind(copy.deepcopy(certificate), m=m), False, "finite certificate facts rejected")
    bind(copy.deepcopy(certificate))
    for location in ("policy", "layer"):
        bad = copy.deepcopy(certificate)
        if location == "policy":
            bad["policy"]["backend_consumption"] = "compat-may-succeed"
        else:
            bad["layers"][-1]["consumption"] = "compat-may-succeed"
        check(bad, False, "finite certificate facts rejected")
    for bad in (None, [], {**certificate, "layers": [1]},
                {**certificate, "policy": []},
                {**certificate, "layers": certificate["layers"] + [certificate["layers"][0]]}):
        check(bad, False)
    for key, value in (("semantic_fallback", "permitted"), ("negative_check", "none")):
        bad = copy.deepcopy(certificate)
        bad["policy"][key] = value
        check(bad, False)
    for kind in ("dag", "abi", "backend"):
        bad = copy.deepcopy(certificate)
        next(layer for layer in bad["layers"] if layer["id"] == kind)["status"] = "verified"
        check(bad, False, "manifest-only")
    for field in ("required_evidence", "payload_schema"):
        bad = copy.deepcopy(certificate)
        bad["layers"][0][field] = [1] if field == "required_evidence" else "unknown"
        check(bad, False)
    for name, value in (("strict_evidence", 1), ("drift_count", False)):
        a = copy.deepcopy(air)
        a["summary"][name] = value
        check(bind(copy.deepcopy(certificate), a=a), False, "finite certificate facts rejected")
    a = copy.deepcopy(air)
    del a["evidence"][0]["fallback_count"]
    check(bind(copy.deepcopy(certificate), a=a), False, "finite certificate facts rejected")
    m = copy.deepcopy(mir)
    m["routines"][0]["blocks"] = None
    check(bind(copy.deepcopy(certificate), m=m), False, "MIR blocks must be an object array")
    for field in ("source_type", "expr0"):
        m = copy.deepcopy(mir)
        m["routines"][0]["blocks"][0]["instructions"][0][field] = 1
        check(bind(copy.deepcopy(certificate), m=m), False, "must be text or null")
    bind(copy.deepcopy(certificate))
    # Byte binding and input read failures remain distinct from the finite rule.
    air_path.write_text("[]", encoding="utf-8")
    check(certificate, False, "binding digest drifted")
    bind(copy.deepcopy(certificate))
    missing = work / "missing.json"
    errors = []
    admission.validate_certificate(certificate, source, missing, mir_path, errors)
    if not any("bound input read failed" in error for error in errors):
        raise AssertionError("input read failure lost its admission diagnosis")
    checks += 1
    print(f"[proof-carrying-adequacy] {checks} envelope projection/owner controls passed")


if __name__ == "__main__":
    compare_finite_checker(sys.argv[1])
    with tempfile.TemporaryDirectory(prefix="pgy-certificate-admission-") as temp:
        envelope_controls(pathlib.Path(temp))

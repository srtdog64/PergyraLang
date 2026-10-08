"""G1 receipt: extracted elab cost, not a compiler or runtime cleanup claim."""
import argparse
import hashlib
import json
import platform
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    work = args.work_dir.resolve()
    bindings = {
        "model": root / "docs/semantics/proofs/OwnershipCleanCore.v",
        "composition": root / "docs/semantics/proofs/OwnershipCleanComposition.v",
        "readonly_elision": root / "docs/semantics/proofs/OwnershipCleanReadOnly.v",
        "extraction": root / "tests/coq/OwnershipCleanupExtraction.v",
        "driver": root / "tests/ocaml/ownership_clean_driver.ml",
        "toolchain_owner": root / "scripts/rocq_toolchain_owner.sh",
        "kernel_gate": root / "tests/coq_kernel_check.sh",
        "receipt_generator": Path(__file__).resolve(),
    }
    for name in bindings:
        if sha(bindings[name]) != sha(work / bindings[name].name):
            raise SystemExit(f"source changed after snapshot: {name}")
    version = subprocess.check_output(["rocq", "--version"], text=True).strip()
    checker = subprocess.check_output(["rocqchk", "--version"], text=True).strip()
    stdlib = subprocess.check_output(["opam", "var", "rocq-stdlib:version"], text=True).strip()
    if "version 9.3.0\n" not in version + "\n" or not checker.endswith("version 9.3.0") or stdlib != "9.2.0":
        raise SystemExit("receipt toolchain does not match admitted stable pair")
    run = subprocess.run([str(work / "ownership_clean_driver"), "--bench"],
                         check=True, text=True, capture_output=True, timeout=180)
    rows = [json.loads(line) for line in run.stdout.splitlines()]
    if len(rows) != 30 or any(r["cpu_seconds_median"] < 0 or r["allocated_words_median"] <= 0 for r in rows):
        raise SystemExit("fixed workload receipt is incomplete")
    elision_run = subprocess.run([str(work / "ownership_clean_driver"), "--readonly-cost"],
                                check=True, text=True, capture_output=True, timeout=20)
    elision_rows = [json.loads(line) for line in elision_run.stdout.splitlines()]
    if [r["flag"] for r in elision_rows] != [0, 1] or any(
            (r["copies_before"], r["copies_after"], r["allocations_before"], r["allocations_after"])
            != (1, 0, 3, 2) for r in elision_rows):
        raise SystemExit("readonly allocation witness is incomplete")
    for row in rows:
        # Hash the complete deterministic recipe, not an OCaml closure address
        # or a timing sample. The driver hash binds the AST constructors/fns.
        recipe = {key: row[key] for key in ("name", "statements", "live_width", "borrowed_width", "loop_depth")}
        recipe["driver_sha256"] = sha(bindings["driver"])
        row["input_recipe_sha256"] = hashlib.sha256(
            json.dumps(recipe, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    for name in bindings:
        if sha(bindings[name]) != sha(work / bindings[name].name):
            raise SystemExit(f"source changed during measurement: {name}")
    # This receipt admits these inputs, not every unrelated compiler checkout
    # edit. A whole-tree status on a Windows-mounted worktree can consume the
    # entire focused budget after all proof/behavior/cost checks have finished.
    status_paths = [path.relative_to(root).as_posix() for path in bindings.values()]
    source_status = subprocess.check_output(
        ["git", "--no-optional-locks", "status", "--porcelain", "--", *status_paths],
        cwd=root, text=True, timeout=20).splitlines()
    receipt = {
        "schema": "pergyra.ownership-clean.elab-cost.v4",
        "scope": "canonical elab cost and certified local read-only rewrite; no physical compiler/runtime refinement",
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "bound_source_dirty_entries": len(source_status),
        "git_status_scope": status_paths,
        "toolchain": {"prover": version, "checker": checker, "stdlib": stdlib,
                      "opam": subprocess.check_output(["opam", "--version"], text=True).strip(),
                      "ocaml": subprocess.check_output(["ocamlopt", "-version"], text=True).strip()},
        "host": {"platform": platform.platform(), "machine": platform.machine()},
        "source_sha256": {name: sha(path) for name, path in bindings.items()},
        "artifact_sha256": {name: sha(work / name) for name in
                            ("ownership_clean.ml", "ownership_clean.mli", "ownership_clean_driver", "kernel.log", "controls.log")},
        "method": "prebuilt fixed AST; borrow_all modes; one warmup; 5 repeats; fixed batch; median per elab call; input building, normalization and observers excluded",
        "limits": ["OCaml int extraction with bounded nonnegative inputs", "no GC/RC runtime added to Pergyra",
                   "OCaml allocation counts are analyzer costs, not generated-program memory",
                   "normalization removes syntax nodes only; resource sites and observable traces are preserved",
                   "readonly allocation counts use fixed_allocations_sound and ro_demo_one_fewer_allocation, not host RSS",
                   "no performance threshold or baseline speedup is claimed", "D1(B) type policy is not enforced by this core"],
        "cases": rows,
        "readonly_witnesses": elision_rows,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(f"[ownership-clean-cost] {len(rows)} fixed cases; receipt {args.output}")
    for family in ("statements", "live-width", "borrowed-width", "loops", "wide-live-program"):
        members = [r for r in rows if r["name"] == family]
        slowest = max(members, key=lambda r: r["cpu_seconds_median"])
        print(f"  {family}: max {slowest['cpu_seconds_median'] * 1000:.3f} ms per elaboration")


if __name__ == "__main__":
    main()

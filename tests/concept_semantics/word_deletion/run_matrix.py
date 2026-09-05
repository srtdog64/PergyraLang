"""Language-word deletion matrix: compile and run every case on one pipeline.

Each directory under cases/ holds one bounded experiment:
  orig.pgy       the program written with the word under test
  subst.pgy      the same program written without it
  neg_orig.pgy   the same mistake written with the word     (expected: rejected)
  neg_subst.pgy  the same mistake written without the word  (question: still rejected?)
  other names    extra probes recorded but not paired

Every source is compiled with the C backend and, when it compiles, executed.
The record keeps the compile exit code, the first diagnostic line, the full
diagnostic tail, stdout and the run exit code. Nothing here owns language
semantics; equal stdout is evidence for one program pair on one pipeline,
not proof that a word is removable.

Usage (from the repository root):

    python3 tests/concept_semantics/word_deletion/run_matrix.py            # public path
    PGY_EXTRA_FLAGS=--native-pipeline PGY_RESULTS=native.json \
        python3 tests/concept_semantics/word_deletion/run_matrix.py        # native front end
    python3 tests/concept_semantics/word_deletion/run_matrix.py 07_intent  # one case

Environment:
    PGY_BIN          compiler (default bin/pgy or bin/pgy.exe)
    PGY_EXTRA_FLAGS  flags placed before the source path, e.g. --native-pipeline
    PGY_RESULTS      results file name inside PGY_WORK (default results.json)
    PGY_WORK         scratch directory for executables and results
                     (default .tmp/self_hosted/word_deletion)
"""
import io, json, os, re, shutil, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
CASES = os.path.join(HERE, "cases")
WORK = os.environ.get("PGY_WORK", os.path.join(ROOT, ".tmp", "self_hosted", "word_deletion"))
EXTRA = os.environ.get("PGY_EXTRA_FLAGS", "").split()
OUT = os.environ.get("PGY_RESULTS", "results.json")
ONLY = sys.argv[1:]


def default_pgy():
    env = os.environ.get("PGY_BIN")
    if env:
        return env
    for name in ("pgy.exe", "pgy"):
        p = os.path.join(ROOT, "bin", name)
        if os.path.exists(p):
            return p
    return os.path.join(ROOT, "bin", "pgy")


PGY = default_pgy()


def loc(path):
    n = 0
    for ln in io.open(path, encoding="utf-8"):
        t = ln.strip()
        if t and not t.startswith("//"):
            n += 1
    return n


def first_diag(text):
    for ln in text.splitlines():
        t = ln.strip()
        if t and re.search(r"error|ERROR|Code:|Reason:|rejected|must|cannot|invalid", t):
            return t[:160]
    lines = [l for l in text.splitlines() if l.strip()]
    return lines[0][:160] if lines else ""


def run_variant(case, name):
    src = os.path.join(CASES, case, name + ".pgy")
    wdir = os.path.join(WORK, case)
    os.makedirs(wdir, exist_ok=True)
    exe = os.path.join(wdir, name + (".exe" if os.name == "nt" else ""))
    if os.path.exists(exe):
        os.remove(exe)
    t0 = time.time()
    p = subprocess.run([PGY] + EXTRA + [src, "--backend=c", "-o", exe], cwd=wdir,
                       capture_output=True, text=True, encoding="utf-8",
                       errors="replace", timeout=300)
    rec = {"loc": loc(src), "compile_rc": p.returncode,
           "compile_s": round(time.time() - t0, 1)}
    combined = (p.stdout or "") + "\n" + (p.stderr or "")
    if p.returncode != 0 or not os.path.exists(exe):
        rec["diag"] = first_diag(combined)
        rec["diag_full"] = combined[-1500:]
        return rec
    r = subprocess.run([exe], cwd=wdir, capture_output=True, text=True,
                       encoding="utf-8", errors="replace", timeout=60)
    rec["run_rc"] = r.returncode
    rec["stdout"] = (r.stdout or "").replace("\r\n", "\n").strip()
    rec["stderr"] = (r.stderr or "").replace("\r\n", "\n").strip()[:400]
    return rec


def main():
    if not os.path.exists(PGY):
        print(f"[word-deletion] compiler not found: {PGY}", file=sys.stderr)
        return 2
    os.makedirs(WORK, exist_ok=True)
    results = {}
    names = sorted(n for n in os.listdir(CASES) if os.path.isdir(os.path.join(CASES, n)))
    if ONLY:
        names = [n for n in names if any(f in n for f in ONLY)]
    for case in names:
        variants = sorted(f[:-4] for f in os.listdir(os.path.join(CASES, case)) if f.endswith(".pgy"))
        res = {}
        for v in variants:
            try:
                res[v] = run_variant(case, v)
            except subprocess.TimeoutExpired:
                res[v] = {"compile_rc": -1, "diag": "TIMEOUT"}
        results[case] = res
        o, s = res.get("orig"), res.get("subst")
        line = f"{case}:"
        if o and s:
            if o.get("compile_rc") == 0 and s.get("compile_rc") == 0:
                same = o.get("stdout") == s.get("stdout") and o.get("run_rc") == s.get("run_rc")
                line += f" output {'SAME' if same else 'DIFF'} (loc {o['loc']} -> {s['loc']})"
            else:
                line += f" orig_rc={o.get('compile_rc')} subst_rc={s.get('compile_rc')}"
        for tag in ("neg_orig", "neg_subst"):
            if tag in res:
                line += f" | {tag} {'REJECT' if res[tag].get('compile_rc') else 'ACCEPT'}"
        for v in variants:
            if v not in ("orig", "subst", "neg_orig", "neg_subst"):
                line += f" | {v} {'rc0' if res[v].get('compile_rc') == 0 else 'REJECT'}"
        print(line, flush=True)
    with io.open(os.path.join(WORK, OUT), "w", encoding="utf-8") as fh:
        json.dump(results, fh, indent=1, ensure_ascii=False)
    print(f"[word-deletion] {len(results)} cases -> {os.path.join(WORK, OUT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

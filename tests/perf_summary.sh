#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: tests/perf_summary.sh <test-abi-perf.log>" >&2
  exit 1
fi

LOG="$1"
if [[ ! -f "$LOG" ]]; then
  echo "perf-summary: log not found: $LOG" >&2
  exit 1
fi

awk '
function flush_case() {
    if (backend == "" || name == "")
        return
    count[backend]++
    compile_sum[backend] += compile
    run_sum[backend] += run
    if (compile > compile_max[backend] || compile_max_name[backend] == "") {
        compile_max[backend] = compile
        compile_max_name[backend] = name
    }
    if (run > run_max[backend] || run_max_name[backend] == "") {
        run_max[backend] = run
        run_max_name[backend] = name
    }
}

/^  (c|llvm)\// && /: wrote test source/ {
    split($1, parts, "/")
    backend = parts[1]
    name = parts[2]
    sub(/:$/, "", name)
    next
}

/metrics: compile=/ {
    compile = $2
    run = $3
    sub(/^compile=/, "", compile)
    sub(/s$/, "", compile)
    sub(/^run=/, "", run)
    sub(/s$/, "", run)
    compile += 0
    run += 0
    flush_case()
    backend = ""
    name = ""
}

END {
    printf("backend cases compile_avg_s compile_max_s compile_max_case run_avg_s run_max_s run_max_case\n")
    for (b in count) {
        printf("%s %d %.3f %.3f %s %.3f %.3f %s\n",
               b,
               count[b],
               compile_sum[b] / count[b],
               compile_max[b],
               compile_max_name[b],
               run_sum[b] / count[b],
               run_max[b],
               run_max_name[b])
    }
}
' "$LOG" | sort

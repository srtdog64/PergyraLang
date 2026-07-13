#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <pgy --ast output>" >&2
    exit 2
fi

AST_DUMP="$1"
if [ ! -f "$AST_DUMP" ]; then
    echo "parser-tree-census: missing AST dump: $AST_DUMP" >&2
    exit 2
fi

# The AST printer is an ordered, two-space-indented tree projection. Intern
# bottom-up (label, ordered child ids) tuples so identical projected subtrees
# receive one canonical id. This measures dump-tree compression only; it does
# not claim that the in-memory AST is hash-consed or that printer-only rows are
# physical ASTNode allocations.
awk '
function fail(message) {
    print "parser-tree-census: " message > "/dev/stderr"
    failed = 1
    exit 2
}

/^[[:space:]]*$/ { next }

{
    match($0, /^ */)
    indent = RLENGTH
    if (indent % 2 != 0)
        fail("indentation is not a multiple of two at input line " NR)

    depth = indent / 2
    n++
    label[n] = substr($0, indent + 1)
    if (depth == 0) {
        parent[n] = 0
        roots++
    } else {
        if (!(depth - 1 in stack))
            fail("missing parent at input line " NR)
        parent[n] = stack[depth - 1]
    }
    stack[depth] = n
    for (d = depth + 1; d <= max_depth; d++)
        delete stack[d]
    if (depth > max_depth)
        max_depth = depth
}

END {
    if (failed)
        exit 2
    if (n == 0)
        fail("AST dump is empty")

    for (i = n; i >= 1; i--) {
        key = label[i] SUBSEP child_ids[i]
        if (key in canonical) {
            id = canonical[key]
        } else {
            unique++
            id = unique
            canonical[key] = id
        }
        frequency[id]++

        p = parent[i]
        if (p != 0) {
            if (child_ids[p] == "")
                child_ids[p] = id
            else
                child_ids[p] = id "," child_ids[p]
        }
    }

    removed = n - unique
    pct = n > 0 ? (100.0 * removed / n) : 0.0
    printf "nodes=%d edges=%d roots=%d unique_subtrees=%d " \
           "hash_cons_removed=%d hash_cons_removed_pct=%.1f\n", \
           n, n - roots, roots, unique, removed, pct
}
' "$AST_DUMP"

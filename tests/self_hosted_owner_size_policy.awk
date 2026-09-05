# Size-policy owner shared by the component and production-size gates.
# lookup requires one exact registered path; scan applies defaults only to
# unregistered paths. Registered caps retain component record-count semantics;
# other paths retain the production gate's batch wc newline counts.
function fail(message) {
    print "[self-host-owner-size] " message > "/dev/stderr"
    failed = 1
    exit 1
}
function line_count(rel,    path, count, status, line) {
    path = root "/" rel
    count = 0
    while ((status = (getline line < path)) > 0) count++
    close(path)
    if (status < 0) fail("unreadable source: " rel)
    return count
}
BEGIN {
    if (root == "") fail("missing repository root")
    if (mode != "lookup" && mode != "scan") fail("invalid policy mode")
    if (manifest == "")
        manifest = root "/tests/fixtures/self_hosted_responsibility_caps.tsv"
    while ((status = (getline row < manifest)) > 0) {
        sub(/\r$/, "", row)
        if (row == "" || row ~ /^#/) continue
        fields = split(row, cell, "|")
        rel = cell[1]
        cap = cell[2]
        if (fields != 2 || rel !~ /^src\/self_hosted\/[A-Za-z0-9_\/]+[.]pgy$/ ||
            cap !~ /^(0|[1-9][0-9]*)$/)
            fail("invalid responsibility cap row: " row)
        if (rel in caps) fail("duplicate responsibility cap: " rel)
        caps[rel] = cap + 0
        counts[rel] = line_count(rel)
        registered++
    }
    close(manifest)
    if (status < 0) fail("unreadable responsibility caps: " manifest)
    if (!registered) fail("empty responsibility caps")
    if (mode == "lookup") {
        if (!(owner in caps)) fail("missing responsibility cap: " owner)
        print caps[owner]
        exit 0
    }
    if (general_limit !~ /^(0|[1-9][0-9]*)$/ ||
        semantic_limit !~ /^(0|[1-9][0-9]*)$/ ||
        general_explicit !~ /^[01]$/)
        fail("invalid default size limits")
}
mode == "scan" {
    if ($1 ~ /^[0-9]+$/ && $2 == "total" && NF == 2) next
    count = $1
    rel = $0
    sub(/^[ \t]*[0-9]+[ \t]+/, "", rel)
    if (count !~ /^[0-9]+$/) fail("invalid newline-count row: " $0)
    if (rel == "" || rel !~ /^src\/self_hosted\// || seen[rel]++)
        fail("invalid or duplicate scan path: " rel)
    if (rel in counts) count = counts[rel]
    limit = general_limit + 0
    if (rel in caps) {
        limit = caps[rel]
    } else if (rel ~ /^src\/self_hosted\/semantic\/[^\/]+[.]pgy$/ &&
               rel != "src/self_hosted/semantic/diagnostic_owner.pgy" &&
               semantic_limit + 0 < limit) {
        limit = semantic_limit + 0
    }
    # An explicit caller ceiling may tighten a registered owner, never silently
    # disappear merely because that owner has a responsibility-specific cap.
    if (general_explicit == 1 && general_limit + 0 < limit)
        limit = general_limit + 0
    if (count > limit) {
        print "[self-host-owner-size] " rel " has " count " lines; cap is " limit > "/dev/stderr"
        exceeded = 1
    }
    scanned++
}
END {
    if (failed) exit 1
    if (mode == "scan") {
        if (!scanned) {
            print "[self-host-owner-size] no source paths scanned" > "/dev/stderr"
            exit 1
        }
        for (rel in caps) {
            if (!(rel in seen)) {
                print "[self-host-owner-size] registered source missing from scan: " rel > "/dev/stderr"
                exit 1
            }
        }
        if (exceeded) exit 1
    }
}

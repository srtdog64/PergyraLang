# Size-policy owner shared by the component and production-size gates.
# lookup requires one exact registered path; scan applies defaults only to
# unregistered paths. Registered caps retain component record-count semantics;
# other paths retain the production gate's batch wc newline counts.
function fail(message, error_status) {
    print "[self-host-owner-size] " message > "/dev/stderr"
    if (error_status == "") error_status = 1
    failed = error_status
    exit failed
}
function require_regular_file(path, diagnostic,    quoted_path, path_index, character, preflight_status) {
    # BWK awk can abort while reading a directory instead of returning -1.
    # Quote the whole path: the repository root need not be a shell word.
    quoted_path = "'"
    for (path_index = 1; path_index <= length(path); path_index++) {
        character = substr(path, path_index, 1)
        if (character == "'") quoted_path = quoted_path "'\\''"
        else quoted_path = quoted_path character
    }
    quoted_path = quoted_path "'"
    preflight_status = system("test -f " quoted_path)
    if (preflight_status == 1) fail(diagnostic)
    if (preflight_status != 0)
        fail("regular-file preflight failed with status " preflight_status ": " path, 2)
}
function line_count(rel,    path, count, status, line) {
    path = root "/" rel
    require_regular_file(path, "unreadable source: " rel)
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
    require_regular_file(manifest, "unreadable responsibility caps: " manifest)
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
    if (failed) exit failed
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

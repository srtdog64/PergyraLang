#!/usr/bin/env bash

# Replace exactly the first literal occurrence and fail if the fixture no
# longer contains it.  This keeps negative mutations identical on BSD/GNU awk.
pgy_replace_first_literal()
{
    local input="$1"
    local output="$2"
    local needle="$3"
    local replacement="$4"

    awk -v needle="$needle" -v replacement="$replacement" '
        BEGIN { replaced = 0 }
        {
            if (!replaced) {
                pos = index($0, needle)
                if (pos > 0) {
                    $0 = substr($0, 1, pos - 1) replacement \
                        substr($0, pos + length(needle))
                    replaced = 1
                }
            }
            print
        }
        END { if (!replaced) exit 42 }
    ' "$input" >"$output"
}

pgy_replace_first_regex()
{
    local input="$1"
    local output="$2"
    local pattern="$3"
    local replacement="$4"

    awk -v pattern="$pattern" -v replacement="$replacement" '
        BEGIN { replaced = 0 }
        {
            if (!replaced && match($0, pattern)) {
                $0 = substr($0, 1, RSTART - 1) replacement \
                    substr($0, RSTART + RLENGTH)
                replaced = 1
            }
            print
        }
        END { if (!replaced) exit 42 }
    ' "$input" >"$output"
}

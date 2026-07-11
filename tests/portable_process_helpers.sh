#!/usr/bin/env bash
# Portable bounded subprocess execution for test harnesses.

pgy_run_with_timeout() {
    local seconds="$1"
    local stdout_file="$2"
    local stderr_file="$3"
    shift 3

    if command -v timeout >/dev/null 2>&1 &&
        timeout --version 2>/dev/null | grep -Fq 'GNU coreutils'; then
        timeout "${seconds}s" "$@" >"$stdout_file" 2>"$stderr_file"
        return $?
    fi
    if command -v gtimeout >/dev/null 2>&1; then
        gtimeout "${seconds}s" "$@" >"$stdout_file" 2>"$stderr_file"
        return $?
    fi
    if command -v perl >/dev/null 2>&1; then
        perl -e '
            use strict;
            use warnings;
            my $seconds = shift @ARGV;
            my $pid = fork();
            exit 127 unless defined $pid;
            if ($pid == 0) { exec @ARGV; exit 127; }
            local $SIG{ALRM} = sub {
                kill "TERM", $pid;
                select undef, undef, undef, 0.25;
                kill "KILL", $pid;
                waitpid $pid, 0;
                exit 124;
            };
            alarm $seconds;
            waitpid $pid, 0;
            alarm 0;
            my $status = $?;
            exit(($status & 127) ? 128 + ($status & 127) : $status >> 8);
        ' "$seconds" "$@" >"$stdout_file" 2>"$stderr_file"
        return $?
    fi
    printf '%s\n' 'portable timeout runner unavailable (need coreutils or perl)' \
        >"$stderr_file"
    : >"$stdout_file"
    return 127
}

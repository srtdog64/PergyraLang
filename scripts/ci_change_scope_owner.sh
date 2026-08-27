#!/usr/bin/env bash
set -euo pipefail

base_revision="${1:-}"
head_revision="${2:-}"
output_path="${3:-${GITHUB_OUTPUT:-}}"

emit_output() {
    local run_full="$1"
    local markdown_only="$2"
    local reason="$3"
    local changed_paths="$4"

    printf 'run_full=%s\n' "$run_full"
    printf 'markdown_only=%s\n' "$markdown_only"
    printf 'reason=%s\n' "$reason"
    printf 'changed_paths=%s\n' "$changed_paths"

    if [[ -n "$output_path" ]]; then
        {
            printf 'run_full=%s\n' "$run_full"
            printf 'markdown_only=%s\n' "$markdown_only"
            printf 'reason=%s\n' "$reason"
            printf 'changed_paths=%s\n' "$changed_paths"
        } >>"$output_path"
    fi
}

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "[ci-change-scope] current directory is not a Git worktree" >&2
    exit 1
fi

if [[ -z "$head_revision" ]] ||
    ! git rev-parse --verify "${head_revision}^{commit}" >/dev/null 2>&1; then
    echo "[ci-change-scope] head revision is missing or invalid" >&2
    exit 1
fi

if [[ -z "$base_revision" ]] ||
    [[ "$base_revision" =~ ^0+$ ]] ||
    ! git rev-parse --verify "${base_revision}^{commit}" >/dev/null 2>&1; then
    emit_output true false base-unavailable 0
    exit 0
fi

changed_paths=0
markdown_only=true
shopt -s nocasematch

while IFS= read -r -d '' status; do
    paths=()
    if [[ "$status" == R* ]] || [[ "$status" == C* ]]; then
        IFS= read -r -d '' old_path
        IFS= read -r -d '' new_path
        paths+=("$old_path" "$new_path")
    else
        IFS= read -r -d '' path
        paths+=("$path")
    fi

    for path in "${paths[@]}"; do
        changed_paths=$((changed_paths + 1))
        if [[ "$path" != *.md ]]; then
            markdown_only=false
        fi
    done
done < <(git diff --name-status -z --find-renames "$base_revision" "$head_revision")

shopt -u nocasematch

if [[ "$changed_paths" -eq 0 ]]; then
    emit_output true false empty-diff 0
elif [[ "$markdown_only" == true ]]; then
    emit_output false true markdown-only "$changed_paths"
else
    emit_output true false non-markdown-change "$changed_paths"
fi

#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
selector="${repo_dir}/extras/scripts/selectBlackSTAR.sh"
test_root="$(mktemp -d "${TMPDIR:-/tmp}/blackstar-selector-test.XXXXXX")"
trap 'rm -rf "${test_root}"' EXIT

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

make_star_stub() {
    local path="$1"
    local version="$2"
    mkdir -p "$(dirname "${path}")"
    printf '#!/usr/bin/env bash\nprintf '\''%%s\\n'\'' %q\n' "${version}" > "${path}"
    chmod 0755 "${path}"
}

sha256() {
    sha256sum -- "$1" | awk '{print $1}'
}

decision() {
    awk -F '\t' '$1 == "decision" {print $2}' "$1/status.tsv"
}

run_selector() {
    local state_dir="$1"
    local candidate="$2"
    local candidate_sha="$3"
    local fallback="$4"
    local fallback_sha="$5"
    local canary="$6"
    local policy="${7:-candidate-first}"
    local canary_sha="${8:-$(sha256 "${canary}")}"
    "${selector}" \
        --candidate "${candidate}" \
        --candidate-sha256 "${candidate_sha}" \
        --candidate-version candidate-1 \
        --fallback "${fallback}" \
        --fallback-sha256 "${fallback_sha}" \
        --fallback-version fallback-1 \
        --state-dir "${state_dir}" \
        --canary-script "${canary}" \
        --canary-sha256 "${canary_sha}" \
        --require-candidate-openmp 0 \
        --selection-policy "${policy}"
}

candidate="${test_root}/binaries with spaces/candidate STAR"
fallback="${test_root}/binaries with spaces/fallback STAR"
make_star_stub "${candidate}" candidate-1
make_star_stub "${fallback}" fallback-1
candidate_sha="$(sha256 "${candidate}")"
fallback_sha="$(sha256 "${fallback}")"

canary_pass="${test_root}/canary-pass.sh"
cat > "${canary_pass}" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
"$1" --version >/dev/null
printf 'canary passed for %s\n' "$1"
EOF
chmod 0755 "${canary_pass}"

canary_reject_candidate="${test_root}/canary-reject-candidate.sh"
cat > "${canary_reject_candidate}" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
if [[ "$1" == *"candidate STAR" ]]; then
    echo "candidate rejected by fixture" >&2
    exit 17
fi
"$1" --version >/dev/null
EOF
chmod 0755 "${canary_reject_candidate}"

state_dir="${test_root}/state with spaces"
run_selector "${state_dir}" "${candidate}" "${candidate_sha}" \
    "${fallback}" "${fallback_sha}" "${canary_pass}"
[[ "$(decision "${state_dir}")" == candidate ]] || fail "healthy candidate was not selected"
[[ "$(sha256 "${state_dir}/selected-star")" == "${candidate_sha}" ]] || \
    fail "selected-star does not contain the candidate"

run_selector "${state_dir}" "${candidate}" "${candidate_sha}" \
    "${fallback}" "${fallback_sha}" "${canary_pass}" fallback-only
[[ "$(decision "${state_dir}")" == fallback ]] || fail "fallback-only policy did not select fallback"
grep -F $'candidate_status\tskipped' "${state_dir}/status.tsv" >/dev/null || \
    fail "fallback-only policy did not record the candidate as skipped"

bad_sha="${candidate_sha%?}0"
if [[ "${bad_sha}" == "${candidate_sha}" ]]; then
    bad_sha="${candidate_sha%?}1"
fi
run_selector "${state_dir}" "${candidate}" "${bad_sha}" \
    "${fallback}" "${fallback_sha}" "${canary_pass}"
[[ "$(decision "${state_dir}")" == fallback ]] || fail "checksum failure did not select fallback"
[[ "$(sha256 "${state_dir}/selected-star")" == "${fallback_sha}" ]] || \
    fail "selected-star does not contain the fallback"
grep -F $'candidate_reason\tSHA-256 mismatch' "${state_dir}/status.tsv" >/dev/null || \
    fail "checksum failure reason was not recorded"

missing_candidate="${test_root}/binaries with spaces/missing STAR"
run_selector "${state_dir}" "${missing_candidate}" "${candidate_sha}" \
    "${fallback}" "${fallback_sha}" "${canary_pass}"
[[ "$(decision "${state_dir}")" == fallback ]] || fail "missing candidate did not select fallback"
grep -F $'candidate_reason\tnot an executable regular file' "${state_dir}/status.tsv" >/dev/null || \
    fail "missing candidate reason was not recorded"

run_selector "${state_dir}" "${candidate}" "${candidate_sha}" \
    "${fallback}" "${fallback_sha}" "${canary_reject_candidate}"
[[ "$(decision "${state_dir}")" == fallback ]] || fail "canary failure did not select fallback"
grep -F $'candidate_reason\tcanary failed with exit 17:' "${state_dir}/status.tsv" >/dev/null || \
    fail "canary failure reason was not recorded"

run_selector "${state_dir}" "${candidate}" "${candidate_sha}" \
    "${fallback}" "${fallback_sha}" "${canary_pass}"

current_before="$(readlink "${state_dir}/current")"
status_before="$(sha256 "${state_dir}/status.tsv")"
if run_selector "${state_dir}" "${candidate}" "${candidate_sha}" \
        "${fallback}" "${fallback_sha}" "${canary_pass}" candidate-first "${candidate_sha}"; then
    fail "selector accepted an invalid canary checksum"
fi
[[ "$(readlink "${state_dir}/current")" == "${current_before}" ]] || \
    fail "invalid canary checksum changed the current generation"
[[ "$(sha256 "${state_dir}/status.tsv")" == "${status_before}" ]] || \
    fail "invalid canary checksum changed the published status"

fragile_candidate="${test_root}/fragile candidate/candidate STAR"
mkdir -p "$(dirname "${fragile_candidate}")"
cat > "${fragile_candidate}" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
[[ -f "$(dirname "$0")/required-marker" ]] || exit 23
printf 'candidate-1\n'
EOF
touch "$(dirname "${fragile_candidate}")/required-marker"
chmod 0755 "${fragile_candidate}"
fragile_sha="$(sha256 "${fragile_candidate}")"
current_before="$(readlink "${state_dir}/current")"
status_before="$(sha256 "${state_dir}/status.tsv")"
generation_count_before="$(find "${state_dir}/generations" -mindepth 1 -maxdepth 1 -type d | wc -l)"
if run_selector "${state_dir}" "${fragile_candidate}" "${fragile_sha}" \
        "${fallback}" "${fallback_sha}" "${canary_pass}"; then
    fail "selector published an executable whose generation-local copy failed"
fi
[[ "$(readlink "${state_dir}/current")" == "${current_before}" ]] || \
    fail "failed generation-copy validation changed the current generation"
[[ "$(sha256 "${state_dir}/status.tsv")" == "${status_before}" ]] || \
    fail "failed generation-copy validation changed the published status"
[[ "$(find "${state_dir}/generations" -mindepth 1 -maxdepth 1 -type d | wc -l)" == "${generation_count_before}" ]] || \
    fail "failed generation-copy validation left a partial generation"

blocked_state="${test_root}/blocked-state"
mkdir -p "${blocked_state}"
printf 'do-not-replace\n' > "${blocked_state}/current"
if run_selector "${blocked_state}" "${candidate}" "${candidate_sha}" \
        "${fallback}" "${fallback_sha}" "${canary_pass}"; then
    fail "selector replaced a non-symlink current path"
fi
[[ "$(cat "${blocked_state}/current")" == "do-not-replace" ]] || \
    fail "selector modified a non-symlink current path"

run_selector "${state_dir}" "${candidate}" "${candidate_sha}" \
    "${fallback}" "${fallback_sha}" "${canary_pass}" \
    > "${test_root}/concurrent-candidate.log" 2>&1 &
candidate_pid=$!
run_selector "${state_dir}" "${candidate}" "${candidate_sha}" \
    "${fallback}" "${fallback_sha}" "${canary_pass}" fallback-only \
    > "${test_root}/concurrent-fallback.log" 2>&1 &
fallback_pid=$!
wait "${candidate_pid}"
wait "${fallback_pid}"
concurrent_decision="$(decision "${state_dir}")"
concurrent_sha="$(sha256 "${state_dir}/selected-star")"
if [[ "${concurrent_decision}" == candidate ]]; then
    [[ "${concurrent_sha}" == "${candidate_sha}" ]] || fail "concurrent candidate status and binary disagree"
elif [[ "${concurrent_decision}" == fallback ]]; then
    [[ "${concurrent_sha}" == "${fallback_sha}" ]] || fail "concurrent fallback status and binary disagree"
else
    fail "concurrent selectors published an invalid decision: ${concurrent_decision}"
fi

current_before="$(readlink "${state_dir}/current")"
status_before="$(sha256 "${state_dir}/status.tsv")"
selected_before="$(readlink -f "${state_dir}/selected-star")"
if run_selector "${state_dir}" "${candidate}" "${bad_sha}" \
        "${fallback}" "${bad_sha}" "${canary_pass}"; then
    fail "selector succeeded when candidate and fallback were invalid"
fi
[[ "$(readlink "${state_dir}/current")" == "${current_before}" ]] || \
    fail "both-invalid attempt changed the current generation"
[[ "$(sha256 "${state_dir}/status.tsv")" == "${status_before}" ]] || \
    fail "both-invalid attempt changed the published status"
[[ "$(readlink -f "${state_dir}/selected-star")" == "${selected_before}" ]] || \
    fail "both-invalid attempt changed the selected binary"

generation_count="$(find "${state_dir}/generations" -mindepth 1 -maxdepth 1 -type d | wc -l)"
[[ "${generation_count}" == 8 ]] || fail "expected eight successful immutable generations, found ${generation_count}"

echo "BlackSTAR selector tests passed"

#!/usr/bin/env bash
set -euo pipefail
umask 022
export LC_ALL=C
export TZ=UTC

usage() {
    cat <<'EOF'
Usage: selectBlackSTAR.sh \
    --candidate PATH --candidate-sha256 HEX --candidate-version VERSION \
    --fallback PATH --fallback-sha256 HEX --fallback-version VERSION \
    --state-dir DIR [--canary-script PATH] [--canary-sha256 HEX] \
    [--require-candidate-openmp 0|1] \
    [--selection-policy candidate-first|fallback-only]

Validate a pinned BlackSTAR candidate and atomically select it or a pinned
fallback. The optional canary executable is called with the binary path as its
only argument. Consumers should execute STATE_DIR/selected-star.
EOF
}

candidate=""
candidate_sha256=""
candidate_version_expected=""
fallback=""
fallback_sha256=""
fallback_version_expected=""
state_dir=""
canary_script=""
canary_sha256_expected=""
require_candidate_openmp=1
selection_policy="candidate-first"
healthcheck_timeout_seconds="${HEALTHCHECK_TIMEOUT_SECONDS:-60}"

while (($#)); do
    case "$1" in
        --candidate)
            candidate="${2:-}"
            shift 2
            ;;
        --candidate-sha256)
            candidate_sha256="${2:-}"
            shift 2
            ;;
        --candidate-version)
            candidate_version_expected="${2:-}"
            shift 2
            ;;
        --fallback)
            fallback="${2:-}"
            shift 2
            ;;
        --fallback-sha256)
            fallback_sha256="${2:-}"
            shift 2
            ;;
        --fallback-version)
            fallback_version_expected="${2:-}"
            shift 2
            ;;
        --state-dir)
            state_dir="${2:-}"
            shift 2
            ;;
        --canary-script)
            canary_script="${2:-}"
            shift 2
            ;;
        --canary-sha256)
            canary_sha256_expected="${2:-}"
            shift 2
            ;;
        --require-candidate-openmp)
            require_candidate_openmp="${2:-}"
            shift 2
            ;;
        --selection-policy)
            selection_policy="${2:-}"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: unknown or incomplete argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

for value_name in \
    candidate candidate_sha256 candidate_version_expected \
    fallback fallback_sha256 fallback_version_expected state_dir; do
    if [[ -z "${!value_name}" ]]; then
        echo "ERROR: ${value_name} is required" >&2
        usage >&2
        exit 2
    fi
done

if [[ ! "${candidate_sha256}" =~ ^[[:xdigit:]]{64}$ ]]; then
    echo "ERROR: candidate SHA-256 must contain exactly 64 hexadecimal characters" >&2
    exit 2
fi
if [[ ! "${fallback_sha256}" =~ ^[[:xdigit:]]{64}$ ]]; then
    echo "ERROR: fallback SHA-256 must contain exactly 64 hexadecimal characters" >&2
    exit 2
fi
candidate_sha256="${candidate_sha256,,}"
fallback_sha256="${fallback_sha256,,}"
if [[ -n "${canary_sha256_expected}" && ! "${canary_sha256_expected}" =~ ^[[:xdigit:]]{64}$ ]]; then
    echo "ERROR: canary SHA-256 must contain exactly 64 hexadecimal characters" >&2
    exit 2
fi
canary_sha256_expected="${canary_sha256_expected,,}"

if [[ "${require_candidate_openmp}" != "0" && "${require_candidate_openmp}" != "1" ]]; then
    echo "ERROR: --require-candidate-openmp must be 0 or 1" >&2
    exit 2
fi
if [[ "${selection_policy}" != "candidate-first" && "${selection_policy}" != "fallback-only" ]]; then
    echo "ERROR: --selection-policy must be candidate-first or fallback-only" >&2
    exit 2
fi
if [[ ! "${healthcheck_timeout_seconds}" =~ ^[1-9][0-9]*$ ]]; then
    echo "ERROR: HEALTHCHECK_TIMEOUT_SECONDS must be a positive integer" >&2
    exit 2
fi

reject_control_characters() {
    local label="$1"
    local value="$2"
    if [[ "${value}" == *$'\n'* || "${value}" == *$'\r'* || "${value}" == *$'\t'* ]]; then
        echo "ERROR: ${label} must not contain tabs or newlines" >&2
        exit 2
    fi
}

reject_control_characters "candidate path" "${candidate}"
reject_control_characters "candidate version" "${candidate_version_expected}"
reject_control_characters "fallback path" "${fallback}"
reject_control_characters "fallback version" "${fallback_version_expected}"
reject_control_characters "state directory" "${state_dir}"
if [[ -n "${canary_script}" ]]; then
    reject_control_characters "canary path" "${canary_script}"
fi

candidate_resolved="$(readlink -m -- "${candidate}")"
fallback_resolved="$(readlink -m -- "${fallback}")"
if [[ "${candidate_resolved}" == "${fallback_resolved}" ]]; then
    echo "ERROR: candidate and fallback resolve to the same file" >&2
    exit 2
fi
if [[ -n "${canary_script}" ]]; then
    if [[ ! -f "${canary_script}" || ! -x "${canary_script}" ]]; then
        echo "ERROR: canary is not an executable regular file: ${canary_script}" >&2
        exit 2
    fi
    canary_resolved="$(readlink -f -- "${canary_script}")"
    canary_sha256_actual="$(sha256sum -- "${canary_resolved}" | awk '{print $1}')"
    if [[ -n "${canary_sha256_expected}" && "${canary_sha256_actual}" != "${canary_sha256_expected}" ]]; then
        echo "ERROR: canary SHA-256 mismatch" >&2
        exit 2
    fi
    if [[ -z "${canary_sha256_expected}" ]]; then
        canary_sha256_expected="${canary_sha256_actual}"
    fi
else
    canary_resolved=""
    canary_sha256_actual=""
    if [[ -n "${canary_sha256_expected}" ]]; then
        echo "ERROR: --canary-sha256 requires --canary-script" >&2
        exit 2
    fi
fi

mkdir -p -- "${state_dir}/generations"
state_dir="$(readlink -f -- "${state_dir}")"
exec 9>"${state_dir}/selector.lock"
flock 9

for published_path in current selected-star status.tsv; do
    if [[ -e "${state_dir}/${published_path}" && ! -L "${state_dir}/${published_path}" ]]; then
        echo "ERROR: ${state_dir}/${published_path} exists and is not a symbolic link" >&2
        exit 1
    fi
done

check_reason=""
check_actual_sha256=""
check_actual_version=""
check_canary_output=""

sanitize_one_line() {
    printf '%s' "$1" | tr '\t\r\n' '   ' | sed -E 's/[[:space:]]+/ /g; s/^ //; s/ $//'
}

check_binary() {
    local role="$1"
    local binary="$2"
    local expected_sha256="$3"
    local expected_version="$4"
    local require_openmp="$5"
    local output rc ldd_output

    : "${role}"
    check_reason=""
    check_actual_sha256=""
    check_actual_version=""
    check_canary_output=""

    if [[ ! -f "${binary}" || ! -x "${binary}" ]]; then
        check_actual_sha256=""
        check_reason="not an executable regular file"
        return 1
    fi

    set +e
    output="$(sha256sum -- "${binary}" 2>&1)"
    rc=$?
    set -e
    if ((rc != 0)); then
        check_actual_sha256=""
        check_reason="SHA-256 calculation failed: $(sanitize_one_line "${output}")"
        return 1
    fi
    check_actual_sha256="${output%%[[:space:]]*}"
    if [[ "${check_actual_sha256}" != "${expected_sha256}" ]]; then
        check_reason="SHA-256 mismatch"
        return 1
    fi

    set +e
    output="$(timeout "${healthcheck_timeout_seconds}" "${binary}" --version 2>&1)"
    rc=$?
    set -e
    if ((rc != 0)); then
        check_reason="--version failed with exit ${rc}: $(sanitize_one_line "${output}")"
        return 1
    fi
    check_actual_version="$(sanitize_one_line "${output}")"
    if [[ "${output}" != "${expected_version}" ]]; then
        check_reason="version mismatch: $(sanitize_one_line "${check_actual_version}")"
        return 1
    fi

    if [[ "${require_openmp}" == "1" ]]; then
        set +e
        ldd_output="$(ldd "${binary}" 2>&1)"
        rc=$?
        set -e
        if ((rc != 0)) || [[ ! "${ldd_output}" =~ lib(gomp|omp) ]]; then
            check_reason="candidate is not linked to libgomp or libomp"
            return 1
        fi
    fi

    if [[ -n "${canary_resolved}" ]]; then
        if [[ "$(sha256sum -- "${canary_resolved}" | awk '{print $1}')" != "${canary_sha256_expected}" ]]; then
            check_reason="canary SHA-256 changed during selection"
            return 1
        fi
        set +e
        output="$(timeout "${healthcheck_timeout_seconds}" "${canary_resolved}" "${binary}" 2>&1)"
        rc=$?
        set -e
        check_canary_output="$(sanitize_one_line "${output}")"
        if ((rc != 0)); then
            check_reason="canary failed with exit ${rc}: ${check_canary_output}"
            return 1
        fi
    fi

    check_reason="healthy"
    return 0
}

candidate_status="fail"
candidate_reason=""
candidate_actual_sha256=""
candidate_actual_version=""
candidate_canary_output=""
fallback_status="not_checked"
fallback_reason="not checked because candidate passed"
fallback_actual_sha256=""
fallback_actual_version=""
fallback_canary_output=""

if [[ "${selection_policy}" == "fallback-only" ]]; then
    candidate_status="skipped"
    candidate_reason="operator requested fallback-only policy"
else
    if check_binary candidate "${candidate_resolved}" "${candidate_sha256}" \
            "${candidate_version_expected}" "${require_candidate_openmp}"; then
        candidate_status="pass"
        candidate_reason="${check_reason}"
        selected_role="candidate"
        selected_binary="${candidate_resolved}"
        selected_sha256="${check_actual_sha256}"
        selected_version="${check_actual_version}"
    else
        candidate_reason="${check_reason}"
    fi
fi
candidate_actual_sha256="${check_actual_sha256}"
candidate_actual_version="${check_actual_version}"
candidate_canary_output="${check_canary_output}"

if [[ "${candidate_status}" != "pass" ]]; then
    if check_binary fallback "${fallback_resolved}" "${fallback_sha256}" \
            "${fallback_version_expected}" 0; then
        fallback_status="pass"
        fallback_reason="${check_reason}"
        selected_role="fallback"
        selected_binary="${fallback_resolved}"
        selected_sha256="${check_actual_sha256}"
        selected_version="${check_actual_version}"
    else
        fallback_status="fail"
        fallback_reason="${check_reason}"
    fi
    fallback_actual_sha256="${check_actual_sha256}"
    fallback_actual_version="${check_actual_version}"
    fallback_canary_output="${check_canary_output}"
fi

if [[ "${candidate_status}" != "pass" && "${fallback_status}" != "pass" ]]; then
    echo "ERROR: candidate failed: ${candidate_reason}" >&2
    echo "ERROR: fallback failed: ${fallback_reason}" >&2
    echo "ERROR: the previously selected generation was not changed" >&2
    exit 1
fi

selected_utc="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
generation="$(date -u '+%Y%m%dT%H%M%SZ').$$"
generation_dir="${state_dir}/generations/${generation}"
mkdir -- "${generation_dir}"
generation_published=0
cleanup_generation() {
    if [[ "${generation_published}" == "0" && -d "${generation_dir}" ]]; then
        rm -rf -- "${generation_dir}"
    fi
}
trap cleanup_generation EXIT
install -m 0755 -- "${selected_binary}" "${generation_dir}/STAR"
installed_sha256="$(sha256sum -- "${generation_dir}/STAR" | awk '{print $1}')"
if [[ "${installed_sha256}" != "${selected_sha256}" ]]; then
    echo "ERROR: checksum changed while publishing ${selected_role}" >&2
    rm -rf -- "${generation_dir}"
    exit 1
fi
generation_require_openmp=0
if [[ "${selected_role}" == "candidate" ]]; then
    generation_require_openmp="${require_candidate_openmp}"
fi
if ! check_binary published-generation "${generation_dir}/STAR" \
        "${selected_sha256}" "${selected_version}" "${generation_require_openmp}"; then
    echo "ERROR: published ${selected_role} copy failed validation: ${check_reason}" >&2
    echo "ERROR: the previously selected generation was not changed" >&2
    rm -rf -- "${generation_dir}"
    exit 1
fi
generation_canary_output="${check_canary_output}"
ln -s -- STAR "${generation_dir}/selected-star"
{
    printf 'key\tvalue\n'
    printf 'schema_version\t1\n'
    printf 'selected_utc\t%s\n' "${selected_utc}"
    printf 'selection_policy\t%s\n' "${selection_policy}"
    printf 'decision\t%s\n' "${selected_role}"
    printf 'selected_source_binary\t%s\n' "${selected_binary}"
    printf 'selected_generation_binary\t%s\n' "${generation_dir}/STAR"
    printf 'selected_sha256\t%s\n' "${selected_sha256}"
    printf 'selected_version\t%s\n' "${selected_version}"
    printf 'generation_status\tpass\n'
    printf 'generation_canary_output\t%s\n' "$(sanitize_one_line "${generation_canary_output}")"
    printf 'candidate_binary\t%s\n' "${candidate_resolved}"
    printf 'candidate_expected_sha256\t%s\n' "${candidate_sha256}"
    printf 'candidate_actual_sha256\t%s\n' "${candidate_actual_sha256}"
    printf 'candidate_expected_version\t%s\n' "${candidate_version_expected}"
    printf 'candidate_actual_version\t%s\n' "${candidate_actual_version}"
    printf 'candidate_status\t%s\n' "${candidate_status}"
    printf 'candidate_reason\t%s\n' "$(sanitize_one_line "${candidate_reason}")"
    printf 'candidate_canary_output\t%s\n' "$(sanitize_one_line "${candidate_canary_output}")"
    printf 'fallback_binary\t%s\n' "${fallback_resolved}"
    printf 'fallback_expected_sha256\t%s\n' "${fallback_sha256}"
    printf 'fallback_actual_sha256\t%s\n' "${fallback_actual_sha256}"
    printf 'fallback_expected_version\t%s\n' "${fallback_version_expected}"
    printf 'fallback_actual_version\t%s\n' "${fallback_actual_version}"
    printf 'fallback_status\t%s\n' "${fallback_status}"
    printf 'fallback_reason\t%s\n' "$(sanitize_one_line "${fallback_reason}")"
    printf 'fallback_canary_output\t%s\n' "$(sanitize_one_line "${fallback_canary_output}")"
    printf 'canary_script\t%s\n' "${canary_resolved}"
    printf 'canary_sha256\t%s\n' "${canary_sha256_expected}"
    printf 'require_candidate_openmp\t%s\n' "${require_candidate_openmp}"
} > "${generation_dir}/status.tsv"

ln -sfn -- current/selected-star "${state_dir}/selected-star"
ln -sfn -- current/status.tsv "${state_dir}/status.tsv"

current_tmp="${state_dir}/.current.${generation}.tmp"
ln -s -- "generations/${generation}" "${current_tmp}"
mv -Tf -- "${current_tmp}" "${state_dir}/current"
generation_published=1
trap - EXIT

printf 'Selected %s: %s (%s)\n' "${selected_role}" "${selected_binary}" "${selected_version}"
printf 'Status: %s\n' "${state_dir}/status.tsv"

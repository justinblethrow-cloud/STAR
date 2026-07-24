#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
star="${STAR_BIN:-${repo_root}/source/STAR}"

if [[ ! -x "${star}" ]]; then
    echo "ERROR: STAR binary is not executable: ${star}" >&2
    exit 1
fi

expected_executable="$(sed -n 's/^#define STAR_VERSION "\(.*\)"$/\1/p' "${repo_root}/source/VERSION")"
expected_blackstar="$(sed -n 's/^#define BLACKSTAR_VERSION "\(.*\)"$/\1/p' "${repo_root}/source/VERSION")"
expected_compatibility="$(sed -n 's/^#define STAR_COMPATIBILITY_VERSION "\(.*\)"$/\1/p' "${repo_root}/source/VERSION")"
expected_genome="$(sed -n 's/^#define BLACKSTAR_GENOME_FORMAT_VERSION "\(.*\)"$/\1/p' "${repo_root}/source/VERSION")"

[[ "$("${star}" --version)" == "${expected_executable}" ]]

version_json="$("${star}" --version-json)"
python3 - "${version_json}" \
    "${expected_blackstar}" \
    "${expected_compatibility}" \
    "${expected_executable}" \
    "${expected_genome}" <<'PY'
import json
import sys

actual = json.loads(sys.argv[1])
expected = {
    "blackstar_version": sys.argv[2],
    "star_compatibility_version": sys.argv[3],
    "executable_version": sys.argv[4],
    "genome_format_version": sys.argv[5],
}
if actual != expected:
    raise SystemExit(f"version metadata differs: {actual!r} != {expected!r}")
PY

default_genome="$(awk '$1 == "versionGenome" { print $2; exit }' "${repo_root}/source/parametersDefault")"
[[ "${default_genome}" == "${expected_genome}" ]]

printf 'BlackSTAR version metadata: PASS\n'

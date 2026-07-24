#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 2 ]]; then
    echo "Usage: $0 BUILD_DIRECTORY_1 BUILD_DIRECTORY_2" >&2
    exit 2
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
version="$(sed -n 's/^#define BLACKSTAR_VERSION "\(.*\)"$/\1/p' "${repo_root}/source/VERSION")"
package="blackstar-${version}-linux-x86_64"
first="$1"
second="$2"
status=0

products=(
    "${package}.tar.gz"
    "${package}.tar.gz.sha256"
    "${package}.spdx.json"
    "${package}/STAR"
    "${package}/build-info.tsv"
    "${package}/ldd.txt"
    "${package}/LICENSE"
    "${package}/ATTRIBUTION.md"
    "${package}/sbom.spdx.json"
)

for product in "${products[@]}"; do
    first_path="${first}/${product}"
    second_path="${second}/${product}"
    if [[ ! -f "${first_path}" || ! -f "${second_path}" ]]; then
        echo "MISSING: ${product}" >&2
        status=1
    elif ! cmp -s "${first_path}" "${second_path}"; then
        echo "MISMATCH: ${product}" >&2
        sha256sum "${first_path}" "${second_path}" >&2
        status=1
    fi
done

if [[ "${status}" -ne 0 ]]; then
    exit "${status}"
fi

echo "BlackSTAR release products are byte-identical across independent source paths"

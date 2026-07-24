#!/usr/bin/env bash
set -euo pipefail
umask 022
export LC_ALL=C
export TZ=UTC

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
cxx="${CXX:-g++}"
jobs="${JOBS:-$(nproc 2>/dev/null || echo 1)}"
allow_dirty="${ALLOW_DIRTY:-0}"

cd "${repo_root}"
if [[ "${allow_dirty}" != "1" && -n "$(git status --porcelain)" ]]; then
    echo "ERROR: release builds require a clean source tree; set ALLOW_DIRTY=1 only for local diagnostics" >&2
    exit 1
fi

commit="$(git rev-parse HEAD)"
source_date_epoch="${SOURCE_DATE_EPOCH:-$(git show -s --format=%ct HEAD)}"
executable_version="$(sed -n 's/^#define STAR_VERSION "\(.*\)"$/\1/p' source/VERSION)"
version="$(sed -n 's/^#define BLACKSTAR_VERSION "\(.*\)"$/\1/p' source/VERSION)"
compatibility_version="$(sed -n 's/^#define STAR_COMPATIBILITY_VERSION "\(.*\)"$/\1/p' source/VERSION)"
genome_format_version="$(sed -n 's/^#define BLACKSTAR_GENOME_FORMAT_VERSION "\(.*\)"$/\1/p' source/VERSION)"
if [[ -z "${version}" || -z "${executable_version}" || "${executable_version}" != *blackstar* ]]; then
    echo "ERROR: source/VERSION does not identify a BlackSTAR release" >&2
    exit 1
fi
if [[ -z "${compatibility_version}" || -z "${genome_format_version}" ]]; then
    echo "ERROR: source/VERSION does not identify the compatibility boundary" >&2
    exit 1
fi

dist_root="${DIST_DIR:-${repo_root}/dist}"
package_name="blackstar-${version}-linux-x86_64"
package_final="${dist_root}/${package_name}"
archive="${dist_root}/${package_name}.tar.gz"
sbom="${dist_root}/${package_name}.spdx.json"
if [[ -e "${package_final}" || -e "${archive}" || -e "${archive}.sha256" || -e "${sbom}" ]]; then
    echo "ERROR: release destination already exists for ${package_name}" >&2
    exit 1
fi
mkdir -p "${dist_root}"
stage_root="$(mktemp -d "${dist_root}/.${package_name}.tmp.XXXXXX")"
package_dir="${stage_root}/${package_name}"
archive_staged="${stage_root}/${package_name}.tar.gz"
checksum_staged="${stage_root}/${package_name}.tar.gz.sha256"
cleanup() {
    if [[ -n "${stage_root:-}" && -d "${stage_root}" ]]; then
        rm -rf "${stage_root}"
    fi
}
trap cleanup EXIT
mkdir "${package_dir}"

provenance="commit=${commit};tree=$([[ -z "$(git status --porcelain)" ]] && echo clean || echo dirty);release=${version};executable=${executable_version}"
export SOURCE_DATE_EPOCH="${source_date_epoch}"
make -C source clean
make -C source -j"${jobs}" STAR \
    CXX="${cxx}" \
    BUILD_PLACE="blackstar-reproducible-build" \
    GIT_PROVENANCE="${provenance}" \
    CXXFLAGSextra="${CXXFLAGSEXTRA:-}" \
    LDFLAGSextra="${LDFLAGSEXTRA:-}"

if [[ "$(source/STAR --version)" != "${executable_version}" ]]; then
    echo "ERROR: built binary reports an unexpected version" >&2
    exit 1
fi
if ! ldd source/STAR | grep -Eq 'libgomp|libomp'; then
    echo "ERROR: built binary is not linked to an OpenMP runtime" >&2
    exit 1
fi

install -m 0755 source/STAR "${package_dir}/STAR"
install -m 0644 LICENSE "${package_dir}/LICENSE"
install -m 0644 ATTRIBUTION.md "${package_dir}/ATTRIBUTION.md"
binary_sha256="$(sha256sum "${package_dir}/STAR" | awk '{print $1}')"
license_sha256="$(sha256sum "${package_dir}/LICENSE" | awk '{print $1}')"
attribution_sha256="$(sha256sum "${package_dir}/ATTRIBUTION.md" | awk '{print $1}')"
compiler_version="$("${cxx}" --version | sed -n '1p')"
build_utc="$(date -u -d "@${source_date_epoch}" '+%Y-%m-%dT%H:%M:%SZ')"
python3 extras/scripts/generateBlackSTARSbom.py \
    --repo-root "${repo_root}" \
    --binary "${package_dir}/STAR" \
    --commit "${commit}" \
    --source-date-epoch "${source_date_epoch}" \
    --output "${package_dir}/sbom.spdx.json"
sbom_sha256="$(sha256sum "${package_dir}/sbom.spdx.json" | awk '{print $1}')"
{
    printf 'key\tvalue\n'
    printf 'blackstar_version\t%s\n' "${version}"
    printf 'star_executable_version\t%s\n' "${executable_version}"
    printf 'star_compatibility_version\t%s\n' "${compatibility_version}"
    printf 'genome_format_version\t%s\n' "${genome_format_version}"
    printf 'git_commit\t%s\n' "${commit}"
    printf 'source_tree\t%s\n' "$([[ -z "$(git status --porcelain)" ]] && echo clean || echo dirty)"
    printf 'source_date_epoch\t%s\n' "${source_date_epoch}"
    printf 'build_utc\t%s\n' "${build_utc}"
    printf 'build_place\tblackstar-reproducible-build\n'
    printf 'compiler\t%s\n' "${compiler_version}"
    printf 'cxxflags_extra\t%s\n' "${CXXFLAGSEXTRA:-}"
    printf 'ldflags_extra\t%s\n' "${LDFLAGSEXTRA:-}"
    printf 'binary_sha256\t%s\n' "${binary_sha256}"
    printf 'license_sha256\t%s\n' "${license_sha256}"
    printf 'attribution_sha256\t%s\n' "${attribution_sha256}"
    printf 'sbom_sha256\t%s\n' "${sbom_sha256}"
} > "${package_dir}/build-info.tsv"
ldd "${package_dir}/STAR" | sed -E 's/ \(0x[0-9a-f]+\)$//' > "${package_dir}/ldd.txt"

tar \
    --sort=name \
    --mtime="@${source_date_epoch}" \
    --owner=0 --group=0 --numeric-owner \
    -C "${stage_root}" -cf - "${package_name}" \
    | gzip -n > "${archive_staged}"
archive_sha256="$(sha256sum "${archive_staged}" | awk '{print $1}')"
printf '%s  %s\n' "${archive_sha256}" "${package_name}.tar.gz" > "${checksum_staged}"

mv "${package_dir}" "${package_final}"
mv "${archive_staged}" "${archive}"
mv "${checksum_staged}" "${archive}.sha256"
cp -p "${package_final}/sbom.spdx.json" "${sbom}"
rmdir "${stage_root}"
stage_root=""
trap - EXIT

printf 'Built %s\n' "${archive}"
printf 'Binary SHA-256: %s\n' "${binary_sha256}"

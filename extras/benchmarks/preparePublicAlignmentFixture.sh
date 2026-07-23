#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
manifest="${FIXTURE_MANIFEST:-${repo_root}/extras/benchmarks/public_alignment_fixture.tsv}"
out_dir="${1:?usage: preparePublicAlignmentFixture.sh OUT_DIR}"
subset_reads="${SUBSET_READS:-0}"

mkdir -p "${out_dir}"
receipt_tmp="${out_dir}/fixture.receipt.tsv.tmp.$$"
receipt="${out_dir}/fixture.receipt.tsv"
trap 'rm -f "${receipt_tmp}"' EXIT
printf 'accession\tmate\tpath\tbytes\tmd5\tsha256\tread_count\tread_length\tsource_page\n' > "${receipt_tmp}"

tail -n +2 "${manifest}" | while IFS=$'\t' read -r accession mate url expected_md5 expected_size read_count read_length source_page; do
    [[ -n "${accession}" ]] || continue
    output="${out_dir}/${accession}.fastq.gz"
    partial="${output}.partial"
    if [[ ! -f "${output}" ]]; then
        curl --fail --location --retry 5 --retry-all-errors \
            --continue-at - --output "${partial}" "${url}"
        actual_size="$(stat --format='%s' "${partial}")"
        actual_md5="$(md5sum "${partial}" | awk '{print $1}')"
        if [[ "${actual_size}" != "${expected_size}" || "${actual_md5}" != "${expected_md5}" ]]; then
            printf 'fixture validation failed for %s: bytes=%s md5=%s\n' \
                "${accession}" "${actual_size}" "${actual_md5}" >&2
            exit 1
        fi
        mv "${partial}" "${output}"
    fi

    actual_size="$(stat --format='%s' "${output}")"
    actual_md5="$(md5sum "${output}" | awk '{print $1}')"
    if [[ "${actual_size}" != "${expected_size}" || "${actual_md5}" != "${expected_md5}" ]]; then
        printf 'existing fixture validation failed for %s: bytes=%s md5=%s\n' \
            "${accession}" "${actual_size}" "${actual_md5}" >&2
        exit 1
    fi
    actual_sha256="$(sha256sum "${output}" | awk '{print $1}')"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "${accession}" "${mate}" "${output}" "${actual_size}" "${actual_md5}" \
        "${actual_sha256}" "${read_count}" "${read_length}" "${source_page}" \
        >> "${receipt_tmp}"
done

mv "${receipt_tmp}" "${receipt}"

if (( subset_reads > 0 )); then
    python3 "${repo_root}/extras/benchmarks/makePairedFastqSubset.py" \
        --read1 "${out_dir}/ENCFF000CXW.fastq.gz" \
        --read2 "${out_dir}/ENCFF000CYM.fastq.gz" \
        --records "${subset_reads}" \
        --output1 "${out_dir}/ENCFF000CXW.first-${subset_reads}.fastq.gz" \
        --output2 "${out_dir}/ENCFF000CYM.first-${subset_reads}.fastq.gz"
fi

printf 'public alignment fixture ready: %s\n' "${receipt}"

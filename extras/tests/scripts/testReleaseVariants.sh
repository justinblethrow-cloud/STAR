#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"
version="$(sed -n 's/^#define BLACKSTAR_VERSION "\(.*\)"$/\1/p' "${repo_root}/source/VERSION")"
dist_dir="${DIST_DIR:-${repo_root}/dist}"
baseline_star="${BASELINE_STAR_BIN:-${dist_dir}/blackstar-${version}-linux-x86_64-baseline/STAR}"
avx2_star="${AVX2_STAR_BIN:-${dist_dir}/blackstar-${version}-linux-x86_64-avx2/STAR}"
out_root="${OUT_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/blackstar-release-variants.XXXXXX")}"
keep_output="${KEEP_TEST_OUTPUT:-0}"

for binary in "${baseline_star}" "${avx2_star}"; do
    if [[ ! -x "${binary}" ]]; then
        echo "ERROR: release variant is not executable: ${binary}" >&2
        exit 1
    fi
done

for target in baseline avx2; do
    package="${dist_dir}/blackstar-${version}-linux-x86_64-${target}"
    binary="${package}/STAR"
    reported_target="$(
        "${binary}" --version-json |
            python3 -c 'import json, sys; print(json.load(sys.stdin)["cpu_target"])'
    )"
    if [[ "${reported_target}" != "${target}" ]]; then
        echo "ERROR: ${target} binary reports CPU target ${reported_target}" >&2
        exit 1
    fi
    grep -Fxq "cpu_target	${target}" "${package}/compatibility.tsv"
done

cleanup() {
    if [[ "${keep_output}" == "1" ]]; then
        echo "Keeping release variant output: ${out_root}" >&2
    else
        rm -rf "${out_root}"
    fi
}
trap cleanup EXIT

if python3 "${repo_root}/extras/scripts/inspectBlackSTARBinary.py" \
        --binary "${avx2_star}" \
        --cpu-target baseline \
        --output "${out_root}/mislabeled-avx2.tsv" \
        > /dev/null 2>&1; then
    echo "ERROR: AVX2 binary passed the baseline ISA check" >&2
    exit 1
fi
if python3 "${repo_root}/extras/scripts/inspectBlackSTARBinary.py" \
        --binary "${baseline_star}" \
        --cpu-target avx2 \
        --output "${out_root}/mislabeled-baseline.tsv" \
        > /dev/null 2>&1; then
    echo "ERROR: baseline binary passed the AVX2 ISA check" >&2
    exit 1
fi

mkdir -p \
    "${out_root}/index" \
    "${out_root}/baseline" \
    "${out_root}/avx2"
sequence_file="${out_root}/sequence.txt"
genome_fasta="${out_root}/genome.fa"
genome_gtf="${out_root}/genes.gtf"
reads_fastq="${out_root}/reads.fq"

awk '
    BEGIN {
        state=29
        bases="ACGT"
        for (ii=0; ii<120000; ii++) {
            state=(state*48271)%2147483647
            printf "%s", substr(bases, state%4+1, 1)
        }
        printf "\n"
    }
' > "${sequence_file}"
{
    printf '>chrVariant\n'
    fold -w 80 "${sequence_file}"
} > "${genome_fasta}"
printf 'chrVariant\ttest\texon\t1\t120000\t.\t+\t.\tgene_id "variantGene"; transcript_id "variantTranscript";\n' \
    > "${genome_gtf}"
read_sequence="$(cut -c 50001-50100 "${sequence_file}")"
{
    printf '@variant_read\n%s\n+\n' "${read_sequence}"
    head -c 100 /dev/zero | tr '\0' I
    printf '\n'
} > "${reads_fastq}"

"${baseline_star}" \
    --runMode genomeGenerate \
    --runThreadN 4 \
    --genomeDir "${out_root}/index" \
    --genomeFastaFiles "${genome_fasta}" \
    --sjdbGTFfile "${genome_gtf}" \
    --sjdbOverhang 99 \
    --genomeSAindexNbases 5 \
    --genomeChrBinNbits 10 \
    --limitGenomeGenerateRAM 200000000 \
    --outFileNamePrefix "${out_root}/build_" \
    > "${out_root}/build.log" 2>&1

for target in baseline avx2; do
    if [[ "${target}" == "baseline" ]]; then
        binary="${baseline_star}"
    else
        binary="${avx2_star}"
    fi
    "${binary}" \
        --runThreadN 4 \
        --genomeDir "${out_root}/index" \
        --readFilesIn "${reads_fastq}" \
        --quantMode GeneCounts \
        --outSAMtype SAM \
        --outFileNamePrefix "${out_root}/${target}/" \
        > "${out_root}/${target}.log" 2>&1
    grep -v '^@' "${out_root}/${target}/Aligned.out.sam" \
        > "${out_root}/${target}/Aligned.body.sam"
done

diff -u \
    "${out_root}/baseline/Aligned.body.sam" \
    "${out_root}/avx2/Aligned.body.sam"
diff -u \
    "${out_root}/baseline/SJ.out.tab" \
    "${out_root}/avx2/SJ.out.tab"
diff -u \
    "${out_root}/baseline/ReadsPerGene.out.tab" \
    "${out_root}/avx2/ReadsPerGene.out.tab"
if ! grep -Eq 'Number of input reads[[:space:]]*\|[[:space:]]*1$' \
        "${out_root}/baseline/Log.final.out"; then
    echo "ERROR: baseline release did not process the fixture read" >&2
    exit 1
fi

printf 'check\tstatus\n'
printf 'baseline_alignment\tpass\n'
printf 'avx2_alignment\tpass\n'
printf 'variant_alignment_equivalence\tpass\n'
printf 'variant_gene_count_equivalence\tpass\n'

#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"
star_long="${STARLONG_BIN:-${repo_root}/source/STARlong}"
upstream_star_long="${UPSTREAM_STARLONG_BIN:-}"
require_upstream="${REQUIRE_UPSTREAM_STARLONG:-0}"
threads="${THREADS:-96}"
out_root="${OUT_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/blackstar-starlong-smoke.XXXXXX")}"
keep_output="${KEEP_TEST_OUTPUT:-0}"

if [[ ! -x "${star_long}" ]]; then
    echo "ERROR: STARLONG_BIN is not executable: ${star_long}" >&2
    exit 1
fi
if [[ -n "${upstream_star_long}" && ! -x "${upstream_star_long}" ]]; then
    echo "ERROR: UPSTREAM_STARLONG_BIN is not executable: ${upstream_star_long}" >&2
    exit 1
fi
if [[ -n "${upstream_star_long}" ]] &&
        [[ "$("${upstream_star_long}" --version)" != "2.7.11b" ]]; then
    echo "ERROR: UPSTREAM_STARLONG_BIN does not report official STAR 2.7.11b" >&2
    exit 1
fi
if [[ "${require_upstream}" == "1" && -z "${upstream_star_long}" ]]; then
    echo "ERROR: REQUIRE_UPSTREAM_STARLONG=1 requires UPSTREAM_STARLONG_BIN" >&2
    exit 1
fi

cleanup() {
    if [[ "${keep_output}" == "1" ]]; then
        echo "Keeping STARlong smoke output: ${out_root}" >&2
    else
        rm -rf "${out_root}"
    fi
}
trap cleanup EXIT

mkdir -p \
    "${out_root}/index" \
    "${out_root}/alignment" \
    "${out_root}/upstream-alignment"
genome_fasta="${out_root}/genome.fa"
reads_fastq="${out_root}/reads.fq"
sequence_file="${out_root}/sequence.txt"

awk '
    BEGIN {
        state=17
        bases="ACGT"
        for (ii=0; ii<120000; ii++) {
            state=(state*48271)%2147483647
            printf "%s", substr(bases, state%4+1, 1)
        }
        printf "\n"
    }
' > "${sequence_file}"
{
    printf '>chrLongSmoke\n'
    fold -w 80 "${sequence_file}"
} > "${genome_fasta}"
long_read="$(cut -c 20001-22000 "${sequence_file}")"
{
    printf '@long_read_2000nt\n%s\n+\n' "${long_read}"
    head -c 2000 /dev/zero | tr '\0' I
    printf '\n'
} > "${reads_fastq}"

"${star_long}" \
    --runMode genomeGenerate \
    --runThreadN 4 \
    --genomeDir "${out_root}/index" \
    --genomeFastaFiles "${genome_fasta}" \
    --genomeSAindexNbases 5 \
    --genomeChrBinNbits 10 \
    --limitGenomeGenerateRAM 200000000 \
    --outFileNamePrefix "${out_root}/build_" \
    > "${out_root}/build.log" 2>&1

"${star_long}" \
    --runThreadN "${threads}" \
    --genomeDir "${out_root}/index" \
    --readFilesIn "${reads_fastq}" \
    --outSAMtype SAM \
    --outSAMattributes Standard \
    --outSJtype None \
    --outFileNamePrefix "${out_root}/alignment/" \
    > "${out_root}/alignment.log" 2>&1

chunk_bytes="$(
    awk '
        /Read input chunk buffer:/ {
            print $5
            exit
        }
    ' "${out_root}/alignment/Log.out"
)"
if [[ -z "${chunk_bytes}" ]] || (( chunk_bytes < 8800000 )); then
    echo "ERROR: STARlong adaptive chunk buffer did not retain eight long-read slots" >&2
    exit 1
fi
if ! grep -Fq "mode=adaptive" "${out_root}/alignment/Log.out"; then
    echo "ERROR: STARlong high-thread smoke did not use adaptive chunk sizing" >&2
    exit 1
fi
if ! grep -Eq 'Number of input reads[[:space:]]*\|[[:space:]]*1$' \
        "${out_root}/alignment/Log.final.out"; then
    echo "ERROR: STARlong did not process the 2000 nt smoke read" >&2
    exit 1
fi
if ! awk '$0 !~ /^@/ && $6=="2000M" {found=1} END {exit found ? 0 : 1}' \
        "${out_root}/alignment/Aligned.out.sam"; then
    echo "ERROR: STARlong did not emit the expected full-length alignment" >&2
    exit 1
fi

upstream_status="not_run"
if [[ -n "${upstream_star_long}" ]]; then
    "${upstream_star_long}" \
        --runThreadN 1 \
        --genomeDir "${out_root}/index" \
        --readFilesIn "${reads_fastq}" \
        --outSAMtype SAM \
        --outSAMattributes Standard \
        --outSJtype None \
        --outFileNamePrefix "${out_root}/upstream-alignment/" \
        > "${out_root}/upstream-alignment.log" 2>&1
    awk '$0 !~ /^@/' "${out_root}/alignment/Aligned.out.sam" |
        LC_ALL=C sort > "${out_root}/alignment.body.sorted.sam"
    awk '$0 !~ /^@/' "${out_root}/upstream-alignment/Aligned.out.sam" |
        LC_ALL=C sort > "${out_root}/upstream-alignment.body.sorted.sam"
    if ! cmp -s \
            "${out_root}/upstream-alignment.body.sorted.sam" \
            "${out_root}/alignment.body.sorted.sam"; then
        echo "ERROR: STARlong alignment differs from official STARlong" >&2
        diff -u \
            "${out_root}/upstream-alignment.body.sorted.sam" \
            "${out_root}/alignment.body.sorted.sam" |
            sed -n '1,160p' >&2 || true
        exit 1
    fi
    upstream_status="pass"
fi

printf 'check\tstatus\n'
printf 'clean_long_read_build\tpass\n'
printf 'long_read_2000nt_input\tpass\n'
printf 'high_thread_chunk_floor\tpass\n'
printf 'upstream_long_read_equivalence\t%s\n' "${upstream_status}"

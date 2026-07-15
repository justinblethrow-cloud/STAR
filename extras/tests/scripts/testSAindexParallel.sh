#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"
star_bin="${STAR_BIN:-${repo_root}/source/STAR}"
parallel_threads="${PARALLEL_THREADS:-16}"
keep_output="${KEEP_TEST_OUTPUT:-0}"
out_root="${OUT_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/star-saindex-parallel-test.XXXXXX")}"

if [[ ! -x "${star_bin}" ]]; then
    echo "ERROR: STAR_BIN is not executable: ${star_bin}" >&2
    exit 1
fi
if (( parallel_threads < 16 )); then
    echo "ERROR: PARALLEL_THREADS must be at least 16 to exercise bounded parallel SAindex events" >&2
    exit 1
fi

cleanup() {
    if [[ "${keep_output}" == "1" ]]; then
        echo "Keeping SAindex test output: ${out_root}" >&2
    else
        rm -rf "${out_root}"
    fi
}
trap cleanup EXIT

mkdir -p "${out_root}"
fasta="${out_root}/synthetic.fa"
gtf="${out_root}/synthetic.gtf"
awk '
    BEGIN {
        state=1729
        print ">chrSynthetic"
        for (lineN=0; lineN<4096; ++lineN) {
            line=""
            for (baseN=0; baseN<85; ++baseN) {
                state=(1664525*state+1013904223)%4294967296
                line=line substr("ACGT", int(state/65536)%4+1, 1)
            }
            print line
        }
    }
' > "${fasta}"
{
    printf 'chrSynthetic\ttest\texon\t1001\t50000\t.\t+\t.\tgene_id "geneSynthetic"; transcript_id "txSynthetic";\n'
    printf 'chrSynthetic\ttest\texon\t60001\t110000\t.\t+\t.\tgene_id "geneSynthetic"; transcript_id "txSynthetic";\n'
} > "${gtf}"

build_index() {
    local label="$1"
    local threads="$2"
    local ram="$3"
    local index="${out_root}/${label}"
    "${star_bin}" \
        --runMode genomeGenerate \
        --runThreadN "${threads}" \
        --limitGenomeGenerateRAM "${ram}" \
        --genomeSAindexNbases 6 \
        --genomeChrBinNbits 10 \
        --sjdbOverhang 49 \
        --sjdbGTFfile "${gtf}" \
        --genomeDir "${index}" \
        --genomeFastaFiles "${fasta}" \
        --outFileNamePrefix "${out_root}/${label}_" \
        > "${out_root}/${label}.stdout" 2>&1
}

build_index serial 1 450000000
build_index parallel "${parallel_threads}" 450000000
build_index constrained "${parallel_threads}" 100000000

if ! grep -Fq 'SAindex traversal strategy: bounded-parallel-events' "${out_root}/parallel/Log.out"; then
    echo "ERROR: parallel build did not exercise bounded parallel SAindex events" >&2
    exit 1
fi
if ! grep -Fq 'SAindex traversal strategy: skip-search' "${out_root}/constrained/Log.out"; then
    echo "ERROR: constrained build did not fall back to skip-search" >&2
    exit 1
fi

equivalent_files=(
    Genome
    SA
    SAindex
    chrName.txt
    chrStart.txt
    chrLength.txt
    chrNameLength.txt
    sjdbInfo.txt
    sjdbList.out.tab
    sjdbList.fromGTF.out.tab
    exonInfo.tab
    exonGeTrInfo.tab
    geneInfo.tab
    transcriptInfo.tab
)
for candidate in parallel constrained; do
    for file in "${equivalent_files[@]}"; do
        if ! cmp -s "${out_root}/serial/${file}" "${out_root}/${candidate}/${file}"; then
            echo "ERROR: ${candidate} SAindex strategy changed ${file}" >&2
            exit 1
        fi
    done
done

cat <<'EOF_RESULTS'
check	status
bounded_parallel_strategy_exercised	pass
low_memory_fallback_exercised	pass
parallel_vs_serial_index_identity	pass
constrained_vs_serial_index_identity	pass
EOF_RESULTS

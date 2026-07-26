#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"
out_root="$(mktemp -d "${TMPDIR:-/tmp}/blackstar-generalization-harness.XXXXXX")"

cleanup() {
    rm -rf "${out_root}"
}
trap cleanup EXIT

mkdir -p "${out_root}/index"
for name in Genome SA SAindex genomeParameters.txt; do
    printf '%s\n' "${name}" > "${out_root}/index/${name}"
done
printf '@read1\nACGT\n+\nIIII\n' > "${out_root}/read1.fq"
printf '@read1\nTGCA\n+\nIIII\n' > "${out_root}/read2.fq"
printf 'ACGTACGTACGTACGT\n' > "${out_root}/whitelist.txt"
printf '#!/usr/bin/env bash\nexit 0\n' > "${out_root}/baseline"
printf '#!/usr/bin/env bash\nexit 0\n' > "${out_root}/candidate"
chmod +x "${out_root}/baseline" "${out_root}/candidate"

cat > "${out_root}/fake-runner" <<'EOF_RUNNER'
#!/usr/bin/env bash
set -euo pipefail
mode="$1"
binary="$2"
out="$6"
mkdir -p "${out}"
if [[ "$(basename "${binary}")" == "baseline" ]]; then
    wall="0:10.00"
    rss="1000"
else
    wall="0:09.00"
    rss="990"
fi
cat > "${out}/time.txt" <<EOF_TIME
Elapsed (wall clock) time (h:mm:ss or m:ss): ${wall}
Maximum resident set size (kbytes): ${rss}
EOF_TIME
cat > "${out}/star.Log.final.out" <<'EOF_LOG'
Number of input reads | 1
Uniquely mapped reads number | 1
EOF_LOG
printf 'chrSynthetic\t1\t2\t1\t1\t1\t0\t0\t1\n' > "${out}/star.SJ.out.tab"
printf 'N_unmapped\t0\t0\t0\n' > "${out}/star.ReadsPerGene.out.tab"
printf 'mode\t%s\n' "${mode}" > "${out}/provenance.tsv"
if [[ "${mode}" == "starsolo" ]]; then
    mkdir -p "${out}/star.Solo.out/Gene/raw"
    printf 'cell\n' > "${out}/star.Solo.out/Gene/raw/barcodes.tsv"
    printf 'gene\tgene\tGene Expression\n' \
        > "${out}/star.Solo.out/Gene/raw/features.tsv"
    printf '%%%%MatrixMarket matrix coordinate integer general\n1 1 1\n1 1 1\n' \
        > "${out}/star.Solo.out/Gene/raw/matrix.mtx"
fi
if [[ "${mode}" == "transcriptome-bam" ]]; then
    cat > "${out}/main.sam" <<'EOF_MAIN'
@HD	VN:1.6	SO:unsorted
read1	4	*	0	0	*	*	0	0	ACGT	IIII
EOF_MAIN
    if [[ "$(basename "${binary}")" == "baseline" ]]; then
        transcriptome_flag=260
    else
        transcriptome_flag=4
    fi
    cat > "${out}/transcriptome.sam" <<EOF_TRANSCRIPTOME
@HD	VN:1.6	SO:unsorted
read1	${transcriptome_flag}	*	0	0	*	*	0	0	ACGT	IIII
EOF_TRANSCRIPTOME
    samtools view -bS "${out}/main.sam" \
        > "${out}/star.Aligned.out.bam"
    samtools view -bS "${out}/transcriptome.sam" \
        > "${out}/star.Aligned.toTranscriptome.out.bam"
fi
EOF_RUNNER
chmod +x "${out_root}/fake-runner"

python3 "${repo_root}/extras/benchmarks/runGeneralizationPairs.py" \
    --mode paired-mapping \
    --baseline-bin "${out_root}/baseline" \
    --candidate-bin "${out_root}/candidate" \
    --genome-dir "${out_root}/index" \
    --read1 "${out_root}/read1.fq" \
    --read2 "${out_root}/read2.fq" \
    --output "${out_root}/result" \
    --runner "${out_root}/fake-runner" \
    --threads 4 \
    --pairs 3 \
    --settle-seconds 0 \
    --skip-quiet-gate \
    > "${out_root}/driver.log"

python3 - "${out_root}/result/result.json" <<'EOF_CHECK'
import json
import sys

result = json.load(open(sys.argv[1], encoding="utf-8"))
assert result["accepted"]
assert result["gates"]["correctness"]
assert result["gates"]["wall_time_noninferiority"]
assert result["median_improvement_percent"] == 10.0
assert result["median_gain_at_least_2_percent"]
assert result["superiority_2_percent"]
EOF_CHECK
jq -e '
    .warmup_position == "after_quiet_gate"
' "${out_root}/result/contract.json" > /dev/null

STARSOLO_WHITELIST="${out_root}/whitelist.txt" \
python3 "${repo_root}/extras/benchmarks/runGeneralizationPairs.py" \
    --mode starsolo \
    --baseline-bin "${out_root}/baseline" \
    --candidate-bin "${out_root}/candidate" \
    --genome-dir "${out_root}/index" \
    --read1 "${out_root}/read1.fq" \
    --read2 "${out_root}/read2.fq" \
    --output "${out_root}/solo-result" \
    --runner "${out_root}/fake-runner" \
    --threads 4 \
    --pairs 3 \
    --settle-seconds 0 \
    --skip-quiet-gate \
    > "${out_root}/solo-driver.log"
jq -e '
    .starsolo_whitelist_sha256 | length == 64
' "${out_root}/solo-result/contract.json" > /dev/null

python3 "${repo_root}/extras/benchmarks/runGeneralizationPairs.py" \
    --mode transcriptome-bam \
    --baseline-bin "${out_root}/baseline" \
    --candidate-bin "${out_root}/candidate" \
    --genome-dir "${out_root}/index" \
    --read1 "${out_root}/read1.fq" \
    --read2 "${out_root}/read2.fq" \
    --output "${out_root}/transcriptome-result" \
    --runner "${out_root}/fake-runner" \
    --threads 4 \
    --pairs 3 \
    --settle-seconds 0 \
    --skip-quiet-gate \
    > "${out_root}/transcriptome-driver.log"
jq -e '
    .accepted and
    .gates.correctness and
    .gates.candidate_transcriptome_primary_determinism and
    ([.candidate_transcriptome_primary_digests[].sha256] | unique | length) == 1
' "${out_root}/transcriptome-result/result.json" > /dev/null
jq -e '
    .passed and
    ([.checks[] |
      select(.name | contains("ignoring primary/secondary choice"))] |
      length) == 1
' "${out_root}/transcriptome-result/pair-01-comparison.json" > /dev/null

if env -u STARSOLO_WHITELIST \
    "${repo_root}/extras/benchmarks/runGeneralizationMode.sh" \
    starsolo /bin/true "${out_root}/index" \
    "${out_root}/read1.fq" "${out_root}/read2.fq" \
    "${out_root}/missing-whitelist" > "${out_root}/missing.log" 2>&1; then
    printf 'STARsolo runner accepted a missing whitelist\n' >&2
    exit 1
fi
[[ ! -e "${out_root}/missing-whitelist" ]]

STARSOLO_WHITELIST="${out_root}/whitelist.txt" THREADS=1 \
    "${repo_root}/extras/benchmarks/runGeneralizationMode.sh" \
    starsolo /bin/true "${out_root}/index" \
    "${out_root}/read1.fq" "${out_root}/read2.fq" \
    "${out_root}/solo-command" > "${out_root}/solo-command.log"
grep -Fq -- '--soloUMIlen 12' "${out_root}/solo-command/command.sh"
grep -Eq $'starsolo_whitelist_sha256\t[0-9a-f]{64}' \
    "${out_root}/solo-command/provenance.tsv"

mkdir -p \
    "${out_root}/solo-a/star.Solo.out/Gene/raw" \
    "${out_root}/solo-b/star.Solo.out/Gene/raw"
for run in solo-a solo-b; do
    cat > "${out_root}/${run}/star.Log.final.out" <<'EOF_LOG'
Number of input reads | 1
Uniquely mapped reads number | 1
EOF_LOG
    printf 'cell\n' > "${out_root}/${run}/star.Solo.out/Gene/raw/barcodes.tsv"
    printf 'gene\tgene\tGene Expression\n' \
        > "${out_root}/${run}/star.Solo.out/Gene/raw/features.tsv"
    printf '%%%%MatrixMarket matrix coordinate integer general\n1 1 1\n1 1 1\n' \
        > "${out_root}/${run}/star.Solo.out/Gene/raw/matrix.mtx"
done
python3 "${repo_root}/extras/benchmarks/compareGeneralizationRuns.py" \
    starsolo "${out_root}/solo-a" "${out_root}/solo-b" \
    --output "${out_root}/solo-comparison.json"

printf 'generalization benchmark harness tests passed\n'

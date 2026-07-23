#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/blackstar-benchmark-test.XXXXXX")"
trap 'rm -rf "${tmp_dir}"' EXIT

gzip -n -c "${repo_root}/extras/tests/fixtures/alignment/read1.fastq" > "${tmp_dir}/read1.fastq.gz"
gzip -n -c "${repo_root}/extras/tests/fixtures/alignment/read2.fastq" > "${tmp_dir}/read2.fastq.gz"

{
    printf 'accession\tmate\turl\tmd5\tfile_size\tread_count\tread_length\tsource_page\n'
    printf 'LOCAL_R1\t1\tfile://%s\t%s\t%s\t3\t8\tlocal-test\n' \
        "${tmp_dir}/read1.fastq.gz" \
        "$(md5sum "${tmp_dir}/read1.fastq.gz" | awk '{print $1}')" \
        "$(stat --format='%s' "${tmp_dir}/read1.fastq.gz")"
    printf 'LOCAL_R2\t2\tfile://%s\t%s\t%s\t3\t8\tlocal-test\n' \
        "${tmp_dir}/read2.fastq.gz" \
        "$(md5sum "${tmp_dir}/read2.fastq.gz" | awk '{print $1}')" \
        "$(stat --format='%s' "${tmp_dir}/read2.fastq.gz")"
    printf '\n'
} > "${tmp_dir}/local-fixture.tsv"
FIXTURE_MANIFEST="${tmp_dir}/local-fixture.tsv" SUBSET_READS=0 \
    "${repo_root}/extras/benchmarks/preparePublicAlignmentFixture.sh" \
    "${tmp_dir}/prepared"
[[ "$(wc -l < "${tmp_dir}/prepared/fixture.receipt.tsv")" -eq 3 ]]

for iteration in 1 2; do
    python3 "${repo_root}/extras/benchmarks/makePairedFastqSubset.py" \
        --read1 "${tmp_dir}/read1.fastq.gz" \
        --read2 "${tmp_dir}/read2.fastq.gz" \
        --records 2 \
        --output1 "${tmp_dir}/subset-r1-${iteration}.fastq.gz" \
        --output2 "${tmp_dir}/subset-r2-${iteration}.fastq.gz"
done
cmp "${tmp_dir}/subset-r1-1.fastq.gz" "${tmp_dir}/subset-r1-2.fastq.gz"
cmp "${tmp_dir}/subset-r2-1.fastq.gz" "${tmp_dir}/subset-r2-2.fastq.gz"
[[ "$(gzip -cd "${tmp_dir}/subset-r1-1.fastq.gz" | wc -l)" -eq 8 ]]
[[ "$(gzip -cd "${tmp_dir}/subset-r2-1.fastq.gz" | wc -l)" -eq 8 ]]

python3 "${repo_root}/extras/benchmarks/quietSystemGate.py" \
    --duration 0.1 --interval 0.05 \
    --min-idle 0 --max-iowait 100 --max-storage-util 100 \
    --path "${tmp_dir}" --output "${tmp_dir}/quiet.tsv"
[[ "$(wc -l < "${tmp_dir}/quiet.tsv")" -ge 2 ]]

for run in baseline candidate; do
    mkdir -p "${tmp_dir}/${run}"
    {
        printf 'field\tvalue\n'
        printf 'mode\tmapping-only\n'
        printf 'threads\t4\n'
        printf 'star_version\tblackstar-test\n'
        printf 'star_sha256\tdeadbeef\n'
        printf 'start_utc\t2026-01-01T00:00:00Z\n'
        printf 'finish_utc\t2026-01-01T00:00:01Z\n'
    } > "${tmp_dir}/${run}/provenance.tsv"
    {
        printf 'Elapsed (wall clock) time (h:mm:ss or m:ss): 0:01.25\n'
        printf 'User time (seconds): 2.00\n'
        printf 'System time (seconds): 0.10\n'
        printf 'Maximum resident set size (kbytes): 1000\n'
        printf 'File system inputs: 0\n'
        printf 'File system outputs: 8\n'
    } > "${tmp_dir}/${run}/time.txt"
    {
        printf 'Started job on | Jan 01 00:00:00\n'
        printf 'Number of input reads | 2\n'
        printf 'Uniquely mapped reads number | 2\n'
        printf 'Uniquely mapped reads %% | 100.00%%\n'
    } > "${tmp_dir}/${run}/star.Log.final.out"
    printf 'chr1\t1\t2\t1\t0\t0\t1\t0\t1\n' > "${tmp_dir}/${run}/star.SJ.out.tab"
    printf 'gene1\t2\t2\t2\n' > "${tmp_dir}/${run}/star.ReadsPerGene.out.tab"
    {
        printf '@HD\tVN:1.4\tSO:unsorted\n'
        printf '@SQ\tSN:chr1\tLN:8\n'
        printf '@PG\tID:STAR\tCL:/different/%s/path\n' "${run}"
        printf '@CO\tuser command line: STAR --outFileNamePrefix /different/%s/path\n' "${run}"
        printf 'read1\t0\tchr1\t1\t255\t8M\t*\t0\t0\tACGTACGT\tIIIIIIII\n'
    } > "${tmp_dir}/${run}/star.Aligned.out.sam"
done

python3 "${repo_root}/extras/benchmarks/summarizeAlignmentA00.py" \
    "${tmp_dir}/baseline" "${tmp_dir}/candidate" \
    --output "${tmp_dir}/summary.tsv"
[[ "$(wc -l < "${tmp_dir}/summary.tsv")" -eq 3 ]]
rg -q $'\t1.25\t' "${tmp_dir}/summary.tsv"

python3 "${repo_root}/extras/benchmarks/compareAlignmentRuns.py" \
    "${tmp_dir}/baseline" "${tmp_dir}/candidate" \
    --output "${tmp_dir}/comparison.json"
jq -e '.passed == true and (.checks | length) == 4' "${tmp_dir}/comparison.json" > /dev/null

mkdir -p "${tmp_dir}/pair-genome"
for file in Genome SA SAindex genomeParameters.txt; do
    printf '%s\n' "${file}" > "${tmp_dir}/pair-genome/${file}"
done
printf 'baseline\n' > "${tmp_dir}/STAR-baseline"
printf 'candidate\n' > "${tmp_dir}/STAR-candidate"
chmod 0755 "${tmp_dir}/STAR-baseline" "${tmp_dir}/STAR-candidate"
fake_runner="${tmp_dir}/fake-runner.sh"
cat > "${fake_runner}" <<'EOF_FAKE_RUNNER'
#!/usr/bin/env bash
set -euo pipefail
binary="$2"
output="$6"
mkdir -p "${output}"
if [[ "$(basename "${binary}")" == "STAR-baseline" ]]; then
    wall="0:01.00"
    rss="1000"
else
    wall="0:00.80"
    rss="1005"
fi
{
    printf 'Elapsed (wall clock) time (h:mm:ss or m:ss): %s\n' "${wall}"
    printf 'Maximum resident set size (kbytes): %s\n' "${rss}"
} > "${output}/time.txt"
{
    printf 'Number of input reads | 2\n'
    printf 'Uniquely mapped reads number | 2\n'
    printf 'Uniquely mapped reads %% | 100.00%%\n'
} > "${output}/star.Log.final.out"
printf 'chr1\t1\t2\t1\t0\t0\t1\t0\t1\n' > "${output}/star.SJ.out.tab"
printf 'gene1\t2\t2\t2\n' > "${output}/star.ReadsPerGene.out.tab"
EOF_FAKE_RUNNER
chmod 0755 "${fake_runner}"

if OMP_PROC_BIND=close OMP_PLACES=cores \
    "${repo_root}/extras/benchmarks/runAlignmentA00.sh" \
    mapping-only "${tmp_dir}/STAR-baseline" "${tmp_dir}/pair-genome" \
    "${repo_root}/extras/tests/fixtures/alignment/read1.fastq" \
    "${repo_root}/extras/tests/fixtures/alignment/read2.fastq" \
    "${tmp_dir}/unsafe-single" > "${tmp_dir}/unsafe-single.log" 2>&1; then
    printf 'single-run harness accepted unsafe OpenMP binding\n' >&2
    exit 1
fi
rg -q 'pthread workers may inherit one OpenMP place' "${tmp_dir}/unsafe-single.log"
[[ ! -e "${tmp_dir}/unsafe-single" ]]

mkdir -p "${tmp_dir}/mock-bin"
cat > "${tmp_dir}/mock-bin/gprofng" <<'EOF_GPROFNG'
#!/usr/bin/env bash
set -euo pipefail
if [[ "${1:-}" == "--version" ]]; then
    printf 'GNU gprofng mock 1.0\n'
    exit 0
fi
[[ "${1:-}" == "collect" && "${2:-}" == "app" ]]
shift 2
while (( $# > 0 )); do
    case "$1" in
        -p|-a|-F|-S)
            shift 2
            ;;
        -O)
            mkdir -p "$2"
            shift 2
            ;;
        *)
            break
            ;;
    esac
done
"$@"
EOF_GPROFNG
chmod 0755 "${tmp_dir}/mock-bin/gprofng"

env -u OMP_PROC_BIND -u OMP_PLACES \
    PATH="${tmp_dir}/mock-bin:${PATH}" \
    THREADS=1 QUANT_MODE=None PERF_MODE=gprofng \
    GPROFNG_CLOCK_PROFILE=lo GPROFNG_ARCHIVE=usedldobjects \
    "${repo_root}/extras/benchmarks/runAlignmentA00.sh" \
    mapping-only /bin/true "${tmp_dir}/pair-genome" \
    "${repo_root}/extras/tests/fixtures/alignment/read1.fastq" \
    "${repo_root}/extras/tests/fixtures/alignment/read2.fastq" \
    "${tmp_dir}/gprofng-run"
[[ -d "${tmp_dir}/gprofng-run/gprofng.er" ]]
rg -q $'perf_mode\tgprofng' "${tmp_dir}/gprofng-run/provenance.tsv"
rg -q $'gprofng_clock_profile\tlo' "${tmp_dir}/gprofng-run/provenance.tsv"
rg -q $'gprofng_version\tGNU gprofng mock 1.0' "${tmp_dir}/gprofng-run/provenance.tsv"

if OMP_PROC_BIND=close OMP_PLACES=cores \
    python3 "${repo_root}/extras/benchmarks/runAlignmentPairs.py" \
    --baseline-bin "${tmp_dir}/STAR-baseline" \
    --candidate-bin "${tmp_dir}/STAR-candidate" \
    --genome-dir "${tmp_dir}/pair-genome" \
    --read1 "${repo_root}/extras/tests/fixtures/alignment/read1.fastq" \
    --read2 "${repo_root}/extras/tests/fixtures/alignment/read2.fastq" \
    --runner "${fake_runner}" --pairs 1 --threads 4 \
    --settle-seconds 0 --skip-quiet-gate \
    --output "${tmp_dir}/unsafe-paired" \
    > "${tmp_dir}/unsafe-paired.log" 2>&1; then
    printf 'pair driver accepted unsafe OpenMP binding\n' >&2
    exit 1
fi
rg -q 'pthread workers may inherit one OpenMP place' "${tmp_dir}/unsafe-paired.log"
[[ ! -e "${tmp_dir}/unsafe-paired" ]]

ALLOW_OMP_THREAD_BINDING=1 OMP_PROC_BIND=close OMP_PLACES=cores \
    python3 "${repo_root}/extras/benchmarks/runAlignmentPairs.py" \
    --baseline-bin "${tmp_dir}/STAR-baseline" \
    --candidate-bin "${tmp_dir}/STAR-candidate" \
    --genome-dir "${tmp_dir}/pair-genome" \
    --read1 "${repo_root}/extras/tests/fixtures/alignment/read1.fastq" \
    --read2 "${repo_root}/extras/tests/fixtures/alignment/read2.fastq" \
    --runner "${fake_runner}" --pairs 1 --threads 4 \
    --settle-seconds 0 --skip-quiet-gate \
    --output "${tmp_dir}/paired-affinity-override" \
    > "${tmp_dir}/paired-affinity-override.stdout"
jq -e '
    .environment.ALLOW_OMP_THREAD_BINDING == "1" and
    .environment.OMP_PROC_BIND == "close" and
    .environment.OMP_PLACES == "cores"
' "${tmp_dir}/paired-affinity-override/contract.json" > /dev/null

env -u OMP_PROC_BIND -u OMP_PLACES \
    python3 "${repo_root}/extras/benchmarks/runAlignmentPairs.py" \
    --baseline-bin "${tmp_dir}/STAR-baseline" \
    --candidate-bin "${tmp_dir}/STAR-candidate" \
    --genome-dir "${tmp_dir}/pair-genome" \
    --read1 "${repo_root}/extras/tests/fixtures/alignment/read1.fastq" \
    --read2 "${repo_root}/extras/tests/fixtures/alignment/read2.fastq" \
    --warmup-read1 "${repo_root}/extras/tests/fixtures/alignment/read1.fastq" \
    --warmup-read2 "${repo_root}/extras/tests/fixtures/alignment/read2.fastq" \
    --runner "${fake_runner}" \
    --pairs 3 --threads 4 --settle-seconds 0 --skip-quiet-gate \
    --output "${tmp_dir}/paired" > "${tmp_dir}/paired.stdout"
jq -e '
    .accepted == true and
    .correctness_passed == true and
    .median_improvement_percent > 19.99 and
    .median_improvement_percent < 20.01 and
    .median_rss_increase_percent > 0.49 and
    .median_rss_increase_percent < 0.51
' "${tmp_dir}/paired/result.json" > /dev/null
[[ "$(wc -l < "${tmp_dir}/paired/schedule.tsv")" -eq 7 ]]
jq -e '.tools.pair_driver_sha256 | length == 64' \
    "${tmp_dir}/paired/contract.json" > /dev/null
jq -e '.passed == true' "${tmp_dir}/paired/warmup-comparison.json" > /dev/null

python3 "${repo_root}/extras/benchmarks/evaluateAlignmentNoninferiority.py" \
    "${tmp_dir}/paired/pairs.tsv" \
    --minimum-pairs 3 \
    --output "${tmp_dir}/paired/noninferiority.json" \
    > "${tmp_dir}/paired/noninferiority.stdout"
jq -e '
    .accepted == true and
    .gates.correctness == true and
    .gates.wall_time_noninferiority == true and
    .pair_count == 3
' "${tmp_dir}/paired/noninferiority.json" > /dev/null

printf 'benchmark harness tests passed\n'

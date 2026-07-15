#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"
star_bin="${STAR_BIN:-${repo_root}/source/STAR}"
threads="${THREADS:-4}"
keep_output="${KEEP_TEST_OUTPUT:-0}"
out_root="${OUT_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/star-genome-insert-hardening.XXXXXX")}"

if [[ ! -x "${star_bin}" ]]; then
    echo "ERROR: STAR_BIN is not executable: ${star_bin}" >&2
    exit 1
fi

cleanup() {
    if [[ "${keep_output}" == "1" ]]; then
        echo "Keeping hardening output: ${out_root}" >&2
    else
        rm -rf "${out_root}"
    fi
}
trap cleanup EXIT

mkdir -p "${out_root}"
fixture="${out_root}/fixture"
KEEP_TEST_OUTPUT=1 OUT_DIR="${fixture}" THREADS="${threads}" STAR_BIN="${star_bin}" \
    "${script_dir}/testGenomeInsert.sh" > "${out_root}/fixture-regression.log" 2>&1

digest_bin="${out_root}/blackstar-file-identity"
"${CXX:-g++}" \
    -std=c++11 -O2 -fopenmp -I"${repo_root}/source" \
    "${repo_root}/extras/tests/testBlackstarSha256.cpp" \
    "${repo_root}/source/BlackstarSha256.cpp" \
    -o "${digest_bin}"

base_index="${fixture}/base_index"
insert_fasta="${fixture}/inputs/insert.fa"
insert_gtf="${fixture}/inputs/insert.gtf"
base_fasta="${fixture}/inputs/base.fa"
base_gtf="${fixture}/inputs/base.gtf"
reads_fastq="${fixture}/inputs/reads.fq"

expect_failure() {
    local label="$1"
    local expected="$2"
    shift 2
    local log="${out_root}/${label}.log"
    local status
    set +e
    "$@" > "${log}" 2>&1
    status="$?"
    set -e
    if [[ "${status}" -eq 0 ]]; then
        echo "ERROR: ${label} unexpectedly succeeded" >&2
        exit 1
    fi
    if ! grep -Fq -- "${expected}" "${log}"; then
        echo "ERROR: ${label} did not report: ${expected}" >&2
        sed -n '1,160p' "${log}" >&2
        exit 1
    fi
}

align_expect_failure() {
    local label="$1"
    local genome_dir="$2"
    local expected="$3"
    expect_failure "${label}" "${expected}" \
        "${star_bin}" \
        --runThreadN "${threads}" \
        --genomeDir "${genome_dir}" \
        --readFilesIn "${reads_fastq}" \
        --outSAMtype None \
        --outFileNamePrefix "${out_root}/${label}_"
}

refresh_completion_entry() {
    local artifact_dir="$1"
    local file_name="$2"
    local file_path="${artifact_dir}/${file_name}"
    local encoded_name
    local digest
    local size
    local temporary
    encoded_name="$(printf '%s' "${file_name}" | od -An -tx1 | tr -d ' \n')"
    digest="$("${digest_bin}" --file "${file_path}")"
    size="$(stat -c '%s' "${file_path}")"
    temporary="${artifact_dir}/blackstar.complete.tsv.new"
    awk -F '\t' -v OFS='\t' -v name="${encoded_name}" -v size="${size}" -v digest="${digest}" '
        $1=="file" && $2==name {$3=size; $4=digest; found=1}
        {print}
        END {if (!found) exit 1}
    ' "${artifact_dir}/blackstar.complete.tsv" > "${temporary}"
    mv "${temporary}" "${artifact_dir}/blackstar.complete.tsv"
}

for artifact_spec in \
    "incremental_index Full" \
    "incremental_repeat_index Full" \
    "overlay_index Overlay" \
    "delta_index Delta" \
    "delta_nojunction_index Delta"; do
    read -r artifact mode <<< "${artifact_spec}"
    completion="${fixture}/${artifact}/blackstar.complete.tsv"
    if [[ ! -s "${completion}" ]]; then
        echo "ERROR: ${artifact} has no completion manifest" >&2
        exit 1
    fi
    if ! grep -Fxq $'artifactMode\t'"${mode}" "${completion}"; then
        echo "ERROR: ${artifact} completion manifest has the wrong mode" >&2
        exit 1
    fi
done

cmp "${fixture}/incremental_index/blackstar.complete.tsv" \
    "${fixture}/incremental_repeat_index/blackstar.complete.tsv"

full_repeat="${out_root}/full-repeat-one-thread"
"${star_bin}" \
    --runMode genomeInsert \
    --runThreadN 1 \
    --genomeDir "${base_index}" \
    --genomeFastaFiles "${insert_fasta}" \
    --sjdbGTFfile "${insert_gtf}" \
    --genomeInsertOutDir "${full_repeat}" \
    --outFileNamePrefix "${out_root}/full-repeat_" \
    > "${out_root}/full-repeat.log" 2>&1
for file in "${fixture}/incremental_index"/*; do
    name="$(basename "${file}")"
    if ! cmp -s "${file}" "${full_repeat}/${name}"; then
        echo "ERROR: Full artifact is not deterministic across thread counts: ${name}" >&2
        exit 1
    fi
done

delta_repeat="${out_root}/delta-repeat-one-thread"
"${star_bin}" \
    --runMode genomeInsert \
    --runThreadN 1 \
    --genomeDir "${base_index}" \
    --genomeFastaFiles "${insert_fasta}" \
    --sjdbGTFfile "${insert_gtf}" \
    --genomeInsertOutMode Delta \
    --genomeInsertOutDir "${delta_repeat}" \
    --outFileNamePrefix "${out_root}/delta-repeat_" \
    > "${out_root}/delta-repeat.log" 2>&1

for file in inserted.fa inserted.gtf genomeInsertOverlay.tsv genomeInsertDelta.bin blackstar.complete.tsv; do
    if ! cmp -s "${fixture}/delta_index/${file}" "${delta_repeat}/${file}"; then
        echo "ERROR: Delta artifact is not deterministic across thread counts: ${file}" >&2
        exit 1
    fi
done

collision_dir="${out_root}/collision-inputs"
mkdir -p "${collision_dir}"
cat > "${collision_dir}/duplicate.fa" <<'EOF_DUPLICATE'
>duplicate
ACGTACGTACGT
>duplicate
TGCATGCATGCA
EOF_DUPLICATE
expect_failure "duplicate-reference" "duplicate inserted reference name duplicate" \
    "${star_bin}" --runMode genomeInsert --runThreadN "${threads}" \
    --genomeDir "${base_index}" --genomeFastaFiles "${collision_dir}/duplicate.fa" \
    --genomeInsertOutMode Overlay --genomeInsertOutDir "${out_root}/duplicate-output" \
    --outFileNamePrefix "${out_root}/duplicate_"

cat > "${collision_dir}/base-name.fa" <<'EOF_BASE_NAME'
>chrA
ACGTACGTACGT
EOF_BASE_NAME
expect_failure "base-reference-collision" "inserted reference name collides with base index: chrA" \
    "${star_bin}" --runMode genomeInsert --runThreadN "${threads}" \
    --genomeDir "${base_index}" --genomeFastaFiles "${collision_dir}/base-name.fa" \
    --genomeInsertOutMode Overlay --genomeInsertOutDir "${out_root}/base-name-output" \
    --outFileNamePrefix "${out_root}/base-name_"

cat > "${collision_dir}/empty.fa" <<'EOF_EMPTY'
>emptySequence
EOF_EMPTY
expect_failure "empty-reference" "inserted FASTA sequence emptySequence is empty" \
    "${star_bin}" --runMode genomeInsert --runThreadN "${threads}" \
    --genomeDir "${base_index}" --genomeFastaFiles "${collision_dir}/empty.fa" \
    --genomeInsertOutMode Overlay --genomeInsertOutDir "${out_root}/empty-output" \
    --outFileNamePrefix "${out_root}/empty_"

printf 'addGFP\ttest\texon\t1\t40\t.\t+\t.\tgene_id "geneA"; transcript_id "txNew";\n' \
    > "${collision_dir}/gene-collision.gtf"
expect_failure "gene-id-collision" "inserted gene_id collides with base annotation: geneA" \
    "${star_bin}" --runMode genomeInsert --runThreadN "${threads}" \
    --genomeDir "${base_index}" --genomeFastaFiles "${insert_fasta}" \
    --sjdbGTFfile "${collision_dir}/gene-collision.gtf" \
    --genomeInsertOutMode Overlay --genomeInsertOutDir "${out_root}/gene-collision-output" \
    --outFileNamePrefix "${out_root}/gene-collision_"

printf 'addGFP\ttest\texon\t1\t40\t.\t+\t.\tgene_id "geneNew"; transcript_id "txA";\n' \
    > "${collision_dir}/transcript-collision.gtf"
expect_failure "transcript-id-collision" "inserted transcript_id collides with base annotation: txA" \
    "${star_bin}" --runMode genomeInsert --runThreadN "${threads}" \
    --genomeDir "${base_index}" --genomeFastaFiles "${insert_fasta}" \
    --sjdbGTFfile "${collision_dir}/transcript-collision.gtf" \
    --genomeInsertOutMode Overlay --genomeInsertOutDir "${out_root}/transcript-collision-output" \
    --outFileNamePrefix "${out_root}/transcript-collision_"

cat > "${collision_dir}/transcript-context-collision.gtf" <<'EOF_TRANSCRIPT_CONTEXT'
addGFP	test	exon	1	20	.	+	.	gene_id "geneNew"; transcript_id "txNew";
addGFP	test	exon	21	40	.	-	.	gene_id "geneNew"; transcript_id "txNew";
EOF_TRANSCRIPT_CONTEXT
expect_failure "transcript-context-collision" "inserted transcript_id is reused across incompatible chromosome, strand, or gene contexts: txNew" \
    "${star_bin}" --runMode genomeInsert --runThreadN "${threads}" \
    --genomeDir "${base_index}" --genomeFastaFiles "${insert_fasta}" \
    --sjdbGTFfile "${collision_dir}/transcript-context-collision.gtf" \
    --genomeInsertOutMode Overlay --genomeInsertOutDir "${out_root}/transcript-context-output" \
    --outFileNamePrefix "${out_root}/transcript-context_"

printf 'addGFP\ttest\texon\t1\t40\t.\t+\tgene_id "geneNew"; transcript_id "txNew";\n' \
    > "${collision_dir}/malformed.gtf"
expect_failure "malformed-gtf" "expected 9 tab-separated GTF fields" \
    "${star_bin}" --runMode genomeInsert --runThreadN "${threads}" \
    --genomeDir "${base_index}" --genomeFastaFiles "${insert_fasta}" \
    --sjdbGTFfile "${collision_dir}/malformed.gtf" \
    --genomeInsertOutMode Overlay --genomeInsertOutDir "${out_root}/malformed-output" \
    --outFileNamePrefix "${out_root}/malformed_"

inherited_overhang_output="${out_root}/inherited-overhang-output"
"${star_bin}" --runMode genomeInsert --runThreadN "${threads}" \
    --genomeDir "${base_index}" --genomeFastaFiles "${insert_fasta}" \
    --sjdbGTFfile "${insert_gtf}" \
    --genomeInsertOutMode Overlay --genomeInsertOutDir "${inherited_overhang_output}" \
    --outFileNamePrefix "${out_root}/inherited-overhang_" \
    > "${out_root}/inherited-overhang.log" 2>&1
if ! grep -Fxq $'sjdbOverhang\t9' "${inherited_overhang_output}/genomeInsertOverlay.tsv"; then
    echo "ERROR: Overlay did not inherit the base index sjdbOverhang" >&2
    exit 1
fi
expect_failure "mismatched-overhang" "does not match the base index value 9" \
    "${star_bin}" --runMode genomeInsert --runThreadN "${threads}" \
    --genomeDir "${base_index}" --genomeFastaFiles "${insert_fasta}" \
    --sjdbGTFfile "${insert_gtf}" --sjdbOverhang 8 \
    --genomeInsertOutMode Overlay --genomeInsertOutDir "${out_root}/mismatched-overhang-output" \
    --outFileNamePrefix "${out_root}/mismatched-overhang_"

nonempty_output="${out_root}/nonempty-output"
mkdir -p "${nonempty_output}"
printf 'preserve-me\n' > "${nonempty_output}/marker.txt"
expect_failure "nonempty-destination" "already exists and is not an empty removable directory" \
    "${star_bin}" --runMode genomeInsert --runThreadN "${threads}" \
    --genomeDir "${base_index}" --genomeFastaFiles "${insert_fasta}" \
    --genomeInsertOutMode Overlay --genomeInsertOutDir "${nonempty_output}" \
    --outFileNamePrefix "${out_root}/nonempty_"
if [[ "$(cat "${nonempty_output}/marker.txt")" != "preserve-me" ]]; then
    echo "ERROR: nonempty destination was modified" >&2
    exit 1
fi

broken_base="${out_root}/broken-base"
cp -a "${base_index}" "${broken_base}"
rm "${broken_base}/SAindex"
failed_output="${out_root}/failed-after-staging"
expect_failure "failed-stage-cleanup" "required base index file is missing or not regular" \
    "${star_bin}" --runMode genomeInsert --runThreadN "${threads}" \
    --genomeDir "${broken_base}" --genomeFastaFiles "${insert_fasta}" \
    --genomeInsertOutMode Overlay --genomeInsertOutDir "${failed_output}" \
    --outFileNamePrefix "${out_root}/failed-stage_"
if [[ -e "${failed_output}" ]] || compgen -G "${failed_output}.tmp.*" > /dev/null; then
    echo "ERROR: failed genomeInsert left a published or staging directory" >&2
    exit 1
fi

space_dir="${out_root}/paths with spaces"
mkdir -p "${space_dir}"
space_fasta="${space_dir}/insert records.fa"
space_gtf="${space_dir}/insert annotations.gtf"
space_delta="${space_dir}/delta artifact"
cp "${insert_fasta}" "${space_fasta}"
cp "${insert_gtf}" "${space_gtf}"
"${star_bin}" \
    --runMode genomeInsert --runThreadN "${threads}" \
    --genomeDir "${base_index}" \
    --genomeFastaFiles "${space_fasta}" \
    --sjdbGTFfile "${space_gtf}" \
    --genomeInsertOutMode Delta \
    --genomeInsertOutDir "${space_delta}" \
    --outFileNamePrefix "${space_dir}/build " \
    > "${out_root}/space-build.log" 2>&1
printf '>changed-after-publication\nAAAA\n' > "${space_fasta}"
printf 'changed\n' > "${space_gtf}"
mkdir -p "${space_dir}/alignment"
"${star_bin}" \
    --runThreadN "${threads}" \
    --genomeDir "${space_delta}" \
    --readFilesIn "${reads_fastq}" \
    --quantMode GeneCounts \
    --outSAMtype SAM \
    --outFileNamePrefix "${space_dir}/alignment/" \
    > "${out_root}/space-align.log" 2>&1
grep -v '^@' "${space_dir}/alignment/Aligned.out.sam" > "${space_dir}/alignment/Aligned.body.sam"
diff -u "${fixture}/align_delta/Aligned.body.sam" "${space_dir}/alignment/Aligned.body.sam"
diff -u "${fixture}/align_delta/ReadsPerGene.out.tab" "${space_dir}/alignment/ReadsPerGene.out.tab"

relocated_delta="${out_root}/relocated-delta"
cp -a "${space_delta}" "${relocated_delta}"
mkdir -p "${out_root}/relocated-alignment"
"${star_bin}" \
    --runThreadN "${threads}" \
    --genomeDir "${relocated_delta}" \
    --readFilesIn "${reads_fastq}" \
    --quantMode GeneCounts \
    --outSAMtype SAM \
    --outFileNamePrefix "${out_root}/relocated-alignment/" \
    > "${out_root}/relocated-align.log" 2>&1
grep -v '^@' "${out_root}/relocated-alignment/Aligned.out.sam" > "${out_root}/relocated-alignment/Aligned.body.sam"
diff -u "${fixture}/align_delta/Aligned.body.sam" "${out_root}/relocated-alignment/Aligned.body.sam"
diff -u "${fixture}/align_delta/ReadsPerGene.out.tab" "${out_root}/relocated-alignment/ReadsPerGene.out.tab"

missing_completion="${out_root}/delta-missing-completion"
cp -a "${fixture}/delta_index" "${missing_completion}"
rm "${missing_completion}/blackstar.complete.tsv"
align_expect_failure "missing-completion" "${missing_completion}" "incomplete BlackSTAR artifact"

corrupt_package="${out_root}/delta-corrupt-package"
cp -a "${fixture}/delta_index" "${corrupt_package}"
printf 'X' | dd of="${corrupt_package}/inserted.fa" bs=1 seek=0 count=1 conv=notrunc status=none
align_expect_failure "corrupt-package" "${corrupt_package}" "artifact checksum mismatch"

extra_file="${out_root}/delta-extra-file"
cp -a "${fixture}/delta_index" "${extra_file}"
printf 'unexpected\n' > "${extra_file}/unexpected.txt"
align_expect_failure "extra-artifact-file" "${extra_file}" "completion manifest file set does not match"

inner_delta_corruption="${out_root}/delta-inner-corruption"
cp -a "${fixture}/delta_index" "${inner_delta_corruption}"
printf 'X' | dd of="${inner_delta_corruption}/genomeInsertDelta.bin" bs=1 seek=240 count=1 conv=notrunc status=none
refresh_completion_entry "${inner_delta_corruption}" genomeInsertDelta.bin
align_expect_failure "inner-delta-corruption" "${inner_delta_corruption}" "Delta payload checksum mismatch"

strict_manifest="${out_root}/delta-unknown-manifest-key"
cp -a "${fixture}/delta_index" "${strict_manifest}"
printf 'unknownKey\tvalue\n' >> "${strict_manifest}/genomeInsertOverlay.tsv"
refresh_completion_entry "${strict_manifest}" genomeInsertOverlay.tsv
align_expect_failure "unknown-manifest-key" "${strict_manifest}" "unknown key unknownKey"

alternate_fasta="${out_root}/alternate-base.fa"
awk '
    /^>/ {print; next}
    !changed {print (substr($0,1,1)=="A" ? "C" : "A") substr($0,2); changed=1; next}
    {print}
' "${base_fasta}" > "${alternate_fasta}"
alternate_index="${out_root}/alternate-index"
"${star_bin}" \
    --runMode genomeGenerate --runThreadN "${threads}" \
    --limitGenomeGenerateRAM 10000000 \
    --genomeSAindexNbases 2 --genomeChrBinNbits 4 --sjdbOverhang 9 \
    --sjdbGTFfile "${base_gtf}" \
    --genomeDir "${alternate_index}" \
    --genomeFastaFiles "${alternate_fasta}" \
    --outFileNamePrefix "${out_root}/alternate_" \
    > "${out_root}/alternate-build.log" 2>&1

mutable_base="${out_root}/mutable-base"
cp -a "${base_index}" "${mutable_base}"
stale_delta="${out_root}/stale-base-delta"
"${star_bin}" \
    --runMode genomeInsert --runThreadN "${threads}" \
    --genomeDir "${mutable_base}" \
    --genomeFastaFiles "${insert_fasta}" \
    --sjdbGTFfile "${insert_gtf}" \
    --genomeInsertOutMode Delta \
    --genomeInsertOutDir "${stale_delta}" \
    --outFileNamePrefix "${out_root}/stale-build_" \
    > "${out_root}/stale-build.log" 2>&1
mv "${mutable_base}" "${out_root}/original-mutable-base"
mv "${alternate_index}" "${mutable_base}"
for file in Genome SA SAindex; do
    if [[ "$(stat -c '%s' "${out_root}/original-mutable-base/${file}")" != \
          "$(stat -c '%s' "${mutable_base}/${file}")" ]]; then
        echo "ERROR: stale-base fixture changed ${file} dimensions" >&2
        exit 1
    fi
done
align_expect_failure "stale-base" "${stale_delta}" "genome insert artifact does not match the loaded base genome index"

expect_failure "benchmark-requires-gtf" "INSERT_GTF_FILE is required by default" \
    env BASE_GENOME_DIR="${base_index}" INSERT_FASTA_FILES="${insert_fasta}" RUN_FULL_REBUILD=0 \
    "${script_dir}/benchmarkGenomeInsert.sh"

benchmark_insert_only="${out_root}/benchmark-insert-only"
env \
    STAR_BIN="${star_bin}" \
    OUT_DIR="${benchmark_insert_only}" \
    THREADS=2 \
    IOSTAT_BIN=/bin/false \
    BASE_GENOME_DIR="${base_index}" \
    INSERT_FASTA_FILES="${insert_fasta}" \
    INSERT_GTF_FILE="${insert_gtf}" \
    RUN_FULL_REBUILD=0 \
    "${script_dir}/benchmarkGenomeInsert.sh" \
    > "${out_root}/benchmark-insert-only.log" 2>&1
if [[ -e "${benchmark_insert_only}/validation.tsv" ]] \
        || ! grep -Fq $'genomeInsert\t' "${benchmark_insert_only}/summary.tsv"; then
    echo "ERROR: insert-only benchmark did not complete cleanly" >&2
    exit 1
fi

benchmark_mismatch="${out_root}/benchmark-mismatch"
set +e
env \
    STAR_BIN="${star_bin}" \
    OUT_DIR="${benchmark_mismatch}" \
    THREADS=2 \
    BASE_GENOME_DIR="${base_index}" \
    BASE_FASTA_FILES="${alternate_fasta}" \
    INSERT_FASTA_FILES="${insert_fasta}" \
    INSERT_GTF_FILE="${insert_gtf}" \
    SJDB_GTF_FILE="${fixture}/inputs/combined.gtf" \
    SJDB_OVERHANG=9 \
    GENOME_SAINDEX_NBASES=2 \
    GENOME_CHRBIN_NBITS=4 \
    LIMIT_GENOME_GENERATE_RAM=10000000 \
    RUN_FULL_REBUILD=1 \
    "${script_dir}/benchmarkGenomeInsert.sh" \
    > "${out_root}/benchmark-mismatch.log" 2>&1
benchmark_status="$?"
set -e
if [[ "${benchmark_status}" -eq 0 ]] || ! grep -Fq $'overall\tfail' "${benchmark_mismatch}/validation.tsv"; then
    echo "ERROR: benchmark harness did not fail a known-mismatched comparison" >&2
    exit 1
fi

sa_mutating_star="${out_root}/sa-mutating-star"
cat > "${sa_mutating_star}" <<'EOF_SA_MUTATING_STAR'
#!/usr/bin/env bash
set -euo pipefail
"${REAL_STAR}" "$@"
mode=""
output=""
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --runMode)
            mode="$2"
            shift 2
            ;;
        --genomeInsertOutDir)
            output="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done
if [[ "${mode}" == "genomeInsert" && -n "${output}" ]]; then
    printf '\0' >> "${output%/}/SA"
fi
EOF_SA_MUTATING_STAR
chmod +x "${sa_mutating_star}"
benchmark_inconclusive="${out_root}/benchmark-inconclusive"
set +e
env \
    REAL_STAR="${star_bin}" \
    STAR_BIN="${sa_mutating_star}" \
    OUT_DIR="${benchmark_inconclusive}" \
    THREADS=2 \
    IOSTAT_BIN=/bin/false \
    BASE_GENOME_DIR="${base_index}" \
    BASE_FASTA_FILES="${base_fasta}" \
    INSERT_FASTA_FILES="${insert_fasta}" \
    INSERT_GTF_FILE="${insert_gtf}" \
    SJDB_GTF_FILE="${fixture}/inputs/combined.gtf" \
    SJDB_OVERHANG=9 \
    GENOME_SAINDEX_NBASES=2 \
    GENOME_CHRBIN_NBITS=4 \
    LIMIT_GENOME_GENERATE_RAM=10000000 \
    RUN_FULL_REBUILD=1 \
    "${script_dir}/benchmarkGenomeInsert.sh" \
    > "${out_root}/benchmark-inconclusive.log" 2>&1
benchmark_inconclusive_status="$?"
set -e
if [[ "${benchmark_inconclusive_status}" -ne 2 ]] \
        || ! grep -Fq $'overall\tinconclusive-SA-requires-alignment-validation' \
            "${benchmark_inconclusive}/validation.tsv"; then
    echo "ERROR: benchmark harness did not fail closed on an unvalidated SA difference" >&2
    exit 1
fi

stock_compatibility="skipped"
if [[ -n "${UPSTREAM_STAR_BIN:-}" ]]; then
    if [[ ! -x "${UPSTREAM_STAR_BIN}" ]]; then
        echo "ERROR: UPSTREAM_STAR_BIN is not executable: ${UPSTREAM_STAR_BIN}" >&2
        exit 1
    fi
    mkdir -p "${out_root}/stock-alignment"
    "${UPSTREAM_STAR_BIN}" \
        --runThreadN "${threads}" \
        --genomeDir "${full_repeat}" \
        --readFilesIn "${reads_fastq}" \
        --quantMode GeneCounts \
        --outSAMtype SAM \
        --outFileNamePrefix "${out_root}/stock-alignment/" \
        > "${out_root}/stock-align.log" 2>&1
    grep -v '^@' "${out_root}/stock-alignment/Aligned.out.sam" > "${out_root}/stock-alignment/Aligned.body.sam"
    diff -u "${fixture}/align_incremental/Aligned.body.sam" "${out_root}/stock-alignment/Aligned.body.sam"
    diff -u "${fixture}/align_incremental/SJ.out.tab" "${out_root}/stock-alignment/SJ.out.tab"
    diff -u "${fixture}/align_incremental/ReadsPerGene.out.tab" "${out_root}/stock-alignment/ReadsPerGene.out.tab"
    stock_compatibility="pass"
fi

cat <<'EOF_RESULTS'
check	status
completion_manifests	pass
full_artifact_idempotence	pass
full_cross_thread_idempotence	pass
delta_cross_thread_idempotence	pass
reference_namespace_collisions	pass
annotation_namespace_collisions	pass
transcript_context_validation	pass
strict_gtf_shape	pass
base_sjdb_overhang_contract	pass
nonempty_destination_preserved	pass
failed_stage_cleanup	pass
space_paths_and_packaged_inputs	pass
relocated_package	pass
missing_completion_rejected	pass
corrupt_package_rejected	pass
unexpected_artifact_file_rejected	pass
inner_delta_checksum_rejected	pass
strict_manifest_schema	pass
stale_same_dimension_base_rejected	pass
benchmark_gtf_contract	pass
benchmark_insert_only_exit_status	pass
benchmark_failure_exit_status	pass
benchmark_inconclusive_exit_status	pass
EOF_RESULTS
printf 'stock_STAR_full_index_compatibility\t%s\n' "${stock_compatibility}"

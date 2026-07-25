#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"
star_bin="${STAR_BIN:-${repo_root}/source/STAR}"
upstream_star="${UPSTREAM_STAR_BIN:-}"
threads="${THREADS:-4}"
keep_output="${KEEP_TEST_OUTPUT:-0}"
out_root="${OUT_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/blackstar-specialized-modes.XXXXXX")}"

if [[ ! -x "${star_bin}" ]]; then
    echo "ERROR: STAR_BIN is not executable: ${star_bin}" >&2
    exit 1
fi
if [[ -z "${upstream_star}" || ! -x "${upstream_star}" ]]; then
    echo "ERROR: UPSTREAM_STAR_BIN must name an executable official STAR oracle" >&2
    exit 1
fi
if [[ "$("${upstream_star}" --version)" != "2.7.11b" ]]; then
    echo "ERROR: UPSTREAM_STAR_BIN does not report official STAR 2.7.11b" >&2
    exit 1
fi
if ! command -v samtools > /dev/null 2>&1; then
    echo "ERROR: samtools is required to compare transcriptome BAM output" >&2
    exit 1
fi

base_index="${out_root}/base-index"
shared_cleanup_prefix="${out_root}/shared-cleanup_"

remove_shared_genome() {
    if [[ -d "${base_index}" ]]; then
        "${star_bin}" \
            --genomeDir "${base_index}" \
            --genomeLoad Remove \
            --outFileNamePrefix "${shared_cleanup_prefix}" \
            > /dev/null 2>&1 || true
    fi
}

cleanup() {
    remove_shared_genome
    if [[ "${keep_output}" == "1" ]]; then
        echo "Keeping specialized-mode output: ${out_root}" >&2
    else
        rm -rf "${out_root}"
    fi
}
trap cleanup EXIT

mkdir -p "${out_root}/inputs" "${base_index}"
python3 - "${out_root}/inputs" <<'PY'
from __future__ import annotations

import hashlib
from pathlib import Path
import sys


root = Path(sys.argv[1])


def make_sequence(length: int, seed: int) -> list[str]:
    bases = "ACGT"
    sequence: list[str] = []
    counter = 0
    seed_bytes = seed.to_bytes(8, byteorder="little", signed=False)
    while len(sequence) < length:
        digest = hashlib.sha256(
            seed_bytes + counter.to_bytes(8, byteorder="little", signed=False)
        ).digest()
        for value in digest:
            for shift in (0, 2, 4, 6):
                sequence.append(bases[(value >> shift) & 3])
                if len(sequence) == length:
                    return sequence
        counter += 1
    return sequence


def subseq(sequence: list[str], start: int, length: int) -> str:
    return "".join(sequence[start - 1 : start - 1 + length])


def reverse_complement(sequence: str) -> str:
    return sequence.translate(str.maketrans("ACGT", "TGCA"))[::-1]


def write_fastq(path: Path, reads: list[tuple[str, str]]) -> None:
    with path.open("w", encoding="ascii", newline="\n") as handle:
        for name, sequence in reads:
            handle.write(f"@{name}\n{sequence}\n+\n{'I' * len(sequence)}\n")


chr_a = make_sequence(160000, 1729)
chr_b = make_sequence(160000, 8675309)

# Canonical GT/AG introns for one annotated and one unannotated splice.
for sequence, exon_end, next_exon_start in (
    (chr_a, 10300, 11001),
    (chr_a, 50200, 51001),
):
    sequence[exon_end : exon_end + 2] = list("GT")
    sequence[next_exon_start - 3 : next_exon_start - 1] = list("AG")

with (root / "genome.fa").open("w", encoding="ascii", newline="\n") as handle:
    for name, sequence in (("chrA", chr_a), ("chrB", chr_b)):
        handle.write(f">{name}\n")
        joined = "".join(sequence)
        for offset in range(0, len(joined), 80):
            handle.write(joined[offset : offset + 80] + "\n")

(root / "genes.gtf").write_text(
    "\n".join(
        (
            'chrA\tfixture\texon\t10001\t10300\t.\t+\t.\tgene_id "geneA"; transcript_id "txA";',
            'chrA\tfixture\texon\t11001\t11300\t.\t+\t.\tgene_id "geneA"; transcript_id "txA";',
            'chrA\tfixture\texon\t90001\t90600\t.\t+\t.\tgene_id "geneW"; transcript_id "txW";',
            'chrB\tfixture\texon\t30001\t30600\t.\t+\t.\tgene_id "geneB"; transcript_id "txB";',
        )
    )
    + "\n",
    encoding="ascii",
)

paired_r1 = [
    ("pair_exonic", subseq(chr_a, 10050, 100)),
    (
        "pair_spliced",
        subseq(chr_a, 10241, 60) + subseq(chr_a, 11001, 40),
    ),
    ("pair_gene_w", subseq(chr_a, 90101, 100)),
]
paired_r2 = [
    ("pair_exonic", reverse_complement(subseq(chr_a, 11150, 100))),
    ("pair_spliced", reverse_complement(subseq(chr_a, 11121, 100))),
    ("pair_gene_w", reverse_complement(subseq(chr_a, 90301, 100))),
]
write_fastq(root / "paired_R1.fq", paired_r1)
write_fastq(root / "paired_R2.fq", paired_r2)

novel_reads: list[tuple[str, str]] = []
for index in range(8):
    left_length = 46 + index
    right_length = 100 - left_length
    novel_reads.append(
        (
            f"novel_splice_{index + 1}",
            subseq(chr_a, 50201 - left_length, left_length)
            + subseq(chr_a, 51001, right_length),
        )
    )
write_fastq(root / "novel_splice.fq", novel_reads)

chimeric_sequence = subseq(chr_a, 70001, 70) + subseq(chr_b, 80001, 70)
write_fastq(root / "chimeric.fq", [("chrA_chrB_fusion", chimeric_sequence)])

variant_position = 90250
reference = chr_a[variant_position - 1]
alternate = next(base for base in "ACGT" if base != reference)
variant_start = 90201
reference_read = subseq(chr_a, variant_start, 100)
alternate_read = list(reference_read)
alternate_read[variant_position - variant_start] = alternate
alternate_read_text = "".join(alternate_read)
write_fastq(
    root / "wasp.fq",
    (("wasp_reference", reference_read), ("wasp_alternate", alternate_read_text)),
)
write_fastq(root / "transform.fq", [("haploid_alternate", alternate_read_text)])

(root / "variants.vcf").write_text(
    "##fileformat=VCFv4.2\n"
    "##FORMAT=<ID=GT,Number=1,Type=String,Description=\"Genotype\">\n"
    "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT\tfixture\n"
    f"chrA\t{variant_position}\t.\t{reference}\t{alternate}\t.\tPASS\t.\tGT\t0/1\n",
    encoding="ascii",
)

barcode = "ACGTACGTACGTACGT"
umis = ("AACCGGTTAA", "AACCGGTTAC", "TTGGAACCTT")
solo_cdna = [
    (f"solo_{index + 1}", subseq(chr_a, 90101 + index * 30, 100))
    for index in range(len(umis))
]
solo_barcode = [
    (f"solo_{index + 1}", barcode + umi)
    for index, umi in enumerate(umis)
]
write_fastq(root / "solo_cdna.fq", solo_cdna)
write_fastq(root / "solo_barcode.fq", solo_barcode)
(root / "solo_whitelist.txt").write_text(barcode + "\n", encoding="ascii")

(root / "input.sam").write_text(
    "@HD\tVN:1.6\tSO:unsorted\n"
    "@SQ\tSN:chrA\tLN:160000\n"
    f"sam_input\t4\t*\t0\t0\t*\t*\t0\t0\t{reference_read}\t"
    f"{'I' * len(reference_read)}\tRG:Z:fixture\tZZ:Z:preserved\n",
    encoding="ascii",
)
PY

genome_fasta="${out_root}/inputs/genome.fa"
genome_gtf="${out_root}/inputs/genes.gtf"
variants_vcf="${out_root}/inputs/variants.vcf"

"${star_bin}" \
    --runMode genomeGenerate \
    --runThreadN "${threads}" \
    --genomeDir "${base_index}" \
    --genomeFastaFiles "${genome_fasta}" \
    --sjdbGTFfile "${genome_gtf}" \
    --sjdbOverhang 99 \
    --genomeSAindexNbases 6 \
    --genomeChrBinNbits 10 \
    --limitGenomeGenerateRAM 500000000 \
    --outFileNamePrefix "${out_root}/base-build_" \
    > "${out_root}/base-build.log" 2>&1

normalize_sam() {
    local input="$1"
    local output="$2"
    awk '$0 !~ /^@/' "${input}" | LC_ALL=C sort > "${output}"
}

normalize_bam() {
    local input="$1"
    local output="$2"
    samtools view "${input}" | LC_ALL=C sort > "${output}"
}

normalize_text() {
    local input="$1"
    local output="$2"
    LC_ALL=C sort "${input}" > "${output}"
}

compare_exact() {
    local label="$1"
    local first="$2"
    local second="$3"
    if ! cmp -s "${first}" "${second}"; then
        echo "ERROR: ${label} differs between official STAR and BlackSTAR" >&2
        diff -u "${first}" "${second}" | sed -n '1,160p' >&2 || true
        exit 1
    fi
}

prepare_mode() {
    local mode="$1"
    mkdir -p "${out_root}/${mode}/upstream" "${out_root}/${mode}/blackstar"
}

compare_sam_outputs() {
    local mode="$1"
    normalize_sam \
        "${out_root}/${mode}/upstream/Aligned.out.sam" \
        "${out_root}/${mode}/upstream/Aligned.body.sorted.sam"
    normalize_sam \
        "${out_root}/${mode}/blackstar/Aligned.out.sam" \
        "${out_root}/${mode}/blackstar/Aligned.body.sorted.sam"
    compare_exact \
        "${mode} SAM records" \
        "${out_root}/${mode}/upstream/Aligned.body.sorted.sam" \
        "${out_root}/${mode}/blackstar/Aligned.body.sorted.sam"
}

prepare_mode paired
"${upstream_star}" \
    --runThreadN 1 \
    --genomeDir "${base_index}" \
    --readFilesIn "${out_root}/inputs/paired_R1.fq" "${out_root}/inputs/paired_R2.fq" \
    --outSAMtype SAM \
    --outSAMattributes Standard \
    --quantMode TranscriptomeSAM GeneCounts \
    --outFileNamePrefix "${out_root}/paired/upstream/" \
    > "${out_root}/paired/upstream.stdout" 2>&1
"${star_bin}" \
    --runThreadN "${threads}" \
    --genomeDir "${base_index}" \
    --readFilesIn "${out_root}/inputs/paired_R1.fq" "${out_root}/inputs/paired_R2.fq" \
    --outSAMtype SAM \
    --outSAMattributes Standard \
    --quantMode TranscriptomeSAM GeneCounts \
    --outFileNamePrefix "${out_root}/paired/blackstar/" \
    > "${out_root}/paired/blackstar.stdout" 2>&1
compare_sam_outputs paired
normalize_bam \
    "${out_root}/paired/upstream/Aligned.toTranscriptome.out.bam" \
    "${out_root}/paired/upstream/transcriptome.sorted.sam"
normalize_bam \
    "${out_root}/paired/blackstar/Aligned.toTranscriptome.out.bam" \
    "${out_root}/paired/blackstar/transcriptome.sorted.sam"
compare_exact \
    "paired transcriptome BAM records" \
    "${out_root}/paired/upstream/transcriptome.sorted.sam" \
    "${out_root}/paired/blackstar/transcriptome.sorted.sam"
compare_exact \
    "paired gene counts" \
    "${out_root}/paired/upstream/ReadsPerGene.out.tab" \
    "${out_root}/paired/blackstar/ReadsPerGene.out.tab"
if ! grep -Eq $'\t[0-9]+M[0-9]+N[0-9]+M\t' \
        "${out_root}/paired/blackstar/Aligned.body.sorted.sam"; then
    echo "ERROR: paired whole-transcript fixture produced no spliced alignment" >&2
    exit 1
fi

for mode in twopass bysjout; do
    prepare_mode "${mode}"
    extra_args=()
    if [[ "${mode}" == "twopass" ]]; then
        extra_args+=(--twopassMode Basic)
    else
        extra_args+=(--outFilterType BySJout)
    fi
    "${upstream_star}" \
        --runThreadN 1 \
        --genomeDir "${base_index}" \
        --readFilesIn "${out_root}/inputs/novel_splice.fq" \
        --outSAMtype SAM \
        --outSAMattributes Standard \
        --outFileNamePrefix "${out_root}/${mode}/upstream/" \
        "${extra_args[@]}" \
        > "${out_root}/${mode}/upstream.stdout" 2>&1
    "${star_bin}" \
        --runThreadN "${threads}" \
        --genomeDir "${base_index}" \
        --readFilesIn "${out_root}/inputs/novel_splice.fq" \
        --outSAMtype SAM \
        --outSAMattributes Standard \
        --outFileNamePrefix "${out_root}/${mode}/blackstar/" \
        "${extra_args[@]}" \
        > "${out_root}/${mode}/blackstar.stdout" 2>&1
    compare_sam_outputs "${mode}"
    normalize_text \
        "${out_root}/${mode}/upstream/SJ.out.tab" \
        "${out_root}/${mode}/upstream/SJ.sorted.tab"
    normalize_text \
        "${out_root}/${mode}/blackstar/SJ.out.tab" \
        "${out_root}/${mode}/blackstar/SJ.sorted.tab"
    compare_exact \
        "${mode} splice junctions" \
        "${out_root}/${mode}/upstream/SJ.sorted.tab" \
        "${out_root}/${mode}/blackstar/SJ.sorted.tab"
    if [[ ! -s "${out_root}/${mode}/blackstar/SJ.sorted.tab" ]]; then
        echo "ERROR: ${mode} fixture produced no splice junction" >&2
        exit 1
    fi
done

prepare_mode chimeric
for implementation in upstream blackstar; do
    if [[ "${implementation}" == "upstream" ]]; then
        binary="${upstream_star}"
        mode_threads=1
    else
        binary="${star_bin}"
        mode_threads="${threads}"
    fi
    "${binary}" \
        --runThreadN "${mode_threads}" \
        --genomeDir "${base_index}" \
        --readFilesIn "${out_root}/inputs/chimeric.fq" \
        --outSAMtype SAM \
        --outSAMattributes Standard \
        --chimSegmentMin 30 \
        --chimJunctionOverhangMin 20 \
        --chimScoreMin 30 \
        --chimScoreDropMax 100 \
        --chimScoreSeparation 0 \
        --chimFilter None \
        --chimOutType Junctions SeparateSAMold \
        --outFileNamePrefix "${out_root}/chimeric/${implementation}/" \
        > "${out_root}/chimeric/${implementation}.stdout" 2>&1
done
compare_sam_outputs chimeric
normalize_sam \
    "${out_root}/chimeric/upstream/Chimeric.out.sam" \
    "${out_root}/chimeric/upstream/Chimeric.body.sorted.sam"
normalize_sam \
    "${out_root}/chimeric/blackstar/Chimeric.out.sam" \
    "${out_root}/chimeric/blackstar/Chimeric.body.sorted.sam"
compare_exact \
    "chimeric SAM records" \
    "${out_root}/chimeric/upstream/Chimeric.body.sorted.sam" \
    "${out_root}/chimeric/blackstar/Chimeric.body.sorted.sam"
normalize_text \
    "${out_root}/chimeric/upstream/Chimeric.out.junction" \
    "${out_root}/chimeric/upstream/Chimeric.sorted.junction"
normalize_text \
    "${out_root}/chimeric/blackstar/Chimeric.out.junction" \
    "${out_root}/chimeric/blackstar/Chimeric.sorted.junction"
compare_exact \
    "chimeric junctions" \
    "${out_root}/chimeric/upstream/Chimeric.sorted.junction" \
    "${out_root}/chimeric/blackstar/Chimeric.sorted.junction"
if [[ ! -s "${out_root}/chimeric/blackstar/Chimeric.sorted.junction" ]]; then
    echo "ERROR: chimeric fixture produced no fusion junction" >&2
    exit 1
fi

prepare_mode wasp
for implementation in upstream blackstar; do
    if [[ "${implementation}" == "upstream" ]]; then
        binary="${upstream_star}"
        mode_threads=1
    else
        binary="${star_bin}"
        mode_threads="${threads}"
    fi
    "${binary}" \
        --runThreadN "${mode_threads}" \
        --genomeDir "${base_index}" \
        --readFilesIn "${out_root}/inputs/wasp.fq" \
        --varVCFfile "${variants_vcf}" \
        --waspOutputMode SAMtag \
        --outSAMtype BAM Unsorted \
        --outSAMattributes NH HI AS nM vW \
        --outFileNamePrefix "${out_root}/wasp/${implementation}/" \
        > "${out_root}/wasp/${implementation}.stdout" 2>&1
done
normalize_bam \
    "${out_root}/wasp/upstream/Aligned.out.bam" \
    "${out_root}/wasp/upstream/Aligned.body.sorted.sam"
normalize_bam \
    "${out_root}/wasp/blackstar/Aligned.out.bam" \
    "${out_root}/wasp/blackstar/Aligned.body.sorted.sam"
compare_exact \
    "WASP BAM records" \
    "${out_root}/wasp/upstream/Aligned.body.sorted.sam" \
    "${out_root}/wasp/blackstar/Aligned.body.sorted.sam"
if ! grep -Fq $'\tvW:i:' "${out_root}/wasp/blackstar/Aligned.body.sorted.sam"; then
    echo "ERROR: WASP fixture produced no vW tag" >&2
    exit 1
fi

prepare_mode solo
for implementation in upstream blackstar; do
    if [[ "${implementation}" == "upstream" ]]; then
        binary="${upstream_star}"
        mode_threads=1
    else
        binary="${star_bin}"
        mode_threads="${threads}"
    fi
    "${binary}" \
        --runThreadN "${mode_threads}" \
        --genomeDir "${base_index}" \
        --readFilesIn \
            "${out_root}/inputs/solo_cdna.fq" \
            "${out_root}/inputs/solo_barcode.fq" \
        --soloType CB_UMI_Simple \
        --soloCBwhitelist "${out_root}/inputs/solo_whitelist.txt" \
        --soloCBmatchWLtype Exact \
        --soloUMIdedup Exact \
        --soloCellFilter None \
        --soloStrand Forward \
        --soloFeatures Gene \
        --outSAMtype SAM \
        --outSAMattributes Standard CR UR CB UB GX GN \
        --outFileNamePrefix "${out_root}/solo/${implementation}/" \
        > "${out_root}/solo/${implementation}.stdout" 2>&1
done
compare_sam_outputs solo
for file in barcodes.tsv features.tsv matrix.mtx; do
    compare_exact \
        "STARsolo raw ${file}" \
        "${out_root}/solo/upstream/Solo.out/Gene/raw/${file}" \
        "${out_root}/solo/blackstar/Solo.out/Gene/raw/${file}"
done
if ! awk '
    /^%/ {next}
    {line++}
    line>1 && $3>0 {found=1}
    END {exit found ? 0 : 1}
' "${out_root}/solo/blackstar/Solo.out/Gene/raw/matrix.mtx"; then
    echo "ERROR: STARsolo fixture produced no gene/UMI count" >&2
    exit 1
fi

prepare_mode sam-input
for implementation in upstream blackstar; do
    if [[ "${implementation}" == "upstream" ]]; then
        binary="${upstream_star}"
        mode_threads=1
    else
        binary="${star_bin}"
        mode_threads="${threads}"
    fi
    "${binary}" \
        --runThreadN "${mode_threads}" \
        --genomeDir "${base_index}" \
        --readFilesIn "${out_root}/inputs/input.sam" \
        --readFilesType SAM SE \
        --readFilesSAMattrKeep All \
        --outSAMtype SAM \
        --outSAMattributes Standard \
        --outFileNamePrefix "${out_root}/sam-input/${implementation}/" \
        > "${out_root}/sam-input/${implementation}.stdout" 2>&1
done
compare_sam_outputs sam-input
if ! grep -Fq $'\tRG:Z:fixture\tZZ:Z:preserved' \
        "${out_root}/sam-input/blackstar/Aligned.body.sorted.sam"; then
    echo "ERROR: SAM-input fixture did not preserve optional attributes" >&2
    exit 1
fi

prepare_mode shared
"${star_bin}" \
    --runThreadN "${threads}" \
    --genomeDir "${base_index}" \
    --readFilesIn "${out_root}/inputs/novel_splice.fq" \
    --outSAMtype SAM \
    --outSAMattributes Standard \
    --outFileNamePrefix "${out_root}/shared/blackstar-private_" \
    > "${out_root}/shared/blackstar-private.stdout" 2>&1
"${upstream_star}" \
    --runThreadN 1 \
    --genomeDir "${base_index}" \
    --genomeLoad LoadAndKeep \
    --readFilesIn "${out_root}/inputs/novel_splice.fq" \
    --outSAMtype SAM \
    --outSAMattributes Standard \
    --outFileNamePrefix "${out_root}/shared/upstream-keep_" \
    > "${out_root}/shared/upstream-keep.stdout" 2>&1
"${star_bin}" \
    --runThreadN "${threads}" \
    --genomeDir "${base_index}" \
    --genomeLoad LoadAndRemove \
    --readFilesIn "${out_root}/inputs/novel_splice.fq" \
    --outSAMtype SAM \
    --outSAMattributes Standard \
    --outFileNamePrefix "${out_root}/shared/blackstar-remove_" \
    > "${out_root}/shared/blackstar-remove.stdout" 2>&1
"${star_bin}" \
    --runThreadN "${threads}" \
    --genomeDir "${base_index}" \
    --genomeLoad LoadAndKeep \
    --readFilesIn "${out_root}/inputs/novel_splice.fq" \
    --outSAMtype SAM \
    --outSAMattributes Standard \
    --outFileNamePrefix "${out_root}/shared/blackstar-keep_" \
    > "${out_root}/shared/blackstar-keep.stdout" 2>&1
"${upstream_star}" \
    --runThreadN 1 \
    --genomeDir "${base_index}" \
    --genomeLoad LoadAndRemove \
    --readFilesIn "${out_root}/inputs/novel_splice.fq" \
    --outSAMtype SAM \
    --outSAMattributes Standard \
    --outFileNamePrefix "${out_root}/shared/upstream-remove_" \
    > "${out_root}/shared/upstream-remove.stdout" 2>&1
for label in \
    blackstar-private \
    upstream-keep \
    blackstar-remove \
    blackstar-keep \
    upstream-remove; do
    normalize_sam \
        "${out_root}/shared/${label}_Aligned.out.sam" \
        "${out_root}/shared/${label}.body.sorted.sam"
done
for label in upstream-keep blackstar-remove blackstar-keep upstream-remove; do
    compare_exact \
        "shared-memory ${label} records" \
        "${out_root}/shared/blackstar-private.body.sorted.sam" \
        "${out_root}/shared/${label}.body.sorted.sam"
done

mkdir -p \
    "${out_root}/transform/upstream-index" \
    "${out_root}/transform/blackstar-index" \
    "${out_root}/transform/upstream" \
    "${out_root}/transform/blackstar"
for implementation in upstream blackstar; do
    if [[ "${implementation}" == "upstream" ]]; then
        binary="${upstream_star}"
    else
        binary="${star_bin}"
    fi
    "${binary}" \
        --runMode genomeGenerate \
        --runThreadN 1 \
        --genomeDir "${out_root}/transform/${implementation}-index" \
        --genomeFastaFiles "${genome_fasta}" \
        --sjdbGTFfile "${genome_gtf}" \
        --sjdbOverhang 99 \
        --genomeTransformType Haploid \
        --genomeTransformVCF "${variants_vcf}" \
        --genomeSAindexNbases 6 \
        --genomeChrBinNbits 10 \
        --limitGenomeGenerateRAM 500000000 \
        --outFileNamePrefix "${out_root}/transform/${implementation}-build_" \
        > "${out_root}/transform/${implementation}-build.stdout" 2>&1
    "${binary}" \
        --runThreadN 1 \
        --genomeDir "${out_root}/transform/${implementation}-index" \
        --readFilesIn "${out_root}/inputs/transform.fq" \
        --genomeTransformOutput SAM SJ Quant \
        --quantMode GeneCounts \
        --outSAMtype SAM \
        --outSAMattributes Standard \
        --outFileNamePrefix "${out_root}/transform/${implementation}/" \
        > "${out_root}/transform/${implementation}.stdout" 2>&1
done
compare_sam_outputs transform
compare_exact \
    "transformed-genome gene counts" \
    "${out_root}/transform/upstream/ReadsPerGene.out.tab" \
    "${out_root}/transform/blackstar/ReadsPerGene.out.tab"
normalize_text \
    "${out_root}/transform/upstream/SJ.out.tab" \
    "${out_root}/transform/upstream/SJ.sorted.tab"
normalize_text \
    "${out_root}/transform/blackstar/SJ.out.tab" \
    "${out_root}/transform/blackstar/SJ.sorted.tab"
compare_exact \
    "transformed-genome splice junctions" \
    "${out_root}/transform/upstream/SJ.sorted.tab" \
    "${out_root}/transform/blackstar/SJ.sorted.tab"

cat <<'EOF_RESULTS'
check	status
paired_fragmented_alignment	pass
transcriptome_bam	pass
gene_counts	pass
two_pass_mapping	pass
bysjout_filtering	pass
chimeric_detection	pass
wasp_filtering	pass
starsolo_gene_umi_counts	pass
sam_input	pass
shared_memory_cross_binary_lifecycle	pass
haploid_genome_transform	pass
EOF_RESULTS

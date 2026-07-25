#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat >&2 <<'EOF'
usage: runGeneralizationMode.sh MODE STAR_BIN GENOME_DIR READ1 READ2_OR_NONE OUT_DIR

MODE is one of:
  paired-mapping
  single-mapping
  two-pass
  bysjout
  chimeric
  sorted-bam
  transcriptome-bam
  starsolo
  starlong

Environment:
  THREADS=96
  GENOME_LOAD_MODE=NoSharedMemory
  READ_FILES_COMMAND=auto
  BAM_SORT_RAM=30000000000
  STAR_EXTRA_ARGS=""
  STARSOLO_WHITELIST=/path/to/3M-february-2018.txt

For starsolo, READ1 is the cDNA read and READ2 is the barcode/UMI read.
Use READ2_OR_NONE=none for single-mapping and starlong.
EOF
    exit 2
}

[[ $# -eq 6 ]] || usage
mode="$1"
star_bin="$(realpath "$2")"
genome_dir="$(realpath "$3")"
read1="$(realpath "$4")"
read2_arg="$5"
out_dir="$6"
threads="${THREADS:-96}"
genome_load_mode="${GENOME_LOAD_MODE:-NoSharedMemory}"
read_files_command="${READ_FILES_COMMAND:-auto}"
allow_omp_thread_binding="${ALLOW_OMP_THREAD_BINDING:-0}"

case "${mode}" in
    paired-mapping|single-mapping|two-pass|bysjout|chimeric|sorted-bam|transcriptome-bam|starsolo|starlong) ;;
    *) usage ;;
esac

case "${mode}" in
    single-mapping|starlong)
        [[ "${read2_arg}" == "none" ]] || {
            printf '%s requires READ2_OR_NONE=none\n' "${mode}" >&2
            exit 2
        }
        read2="none"
        ;;
    *)
        [[ "${read2_arg}" != "none" ]] || {
            printf '%s requires two input files\n' "${mode}" >&2
            exit 2
        }
        read2="$(realpath "${read2_arg}")"
        ;;
esac

[[ -x "${star_bin}" ]] || {
    printf 'STAR binary is not executable: %s\n' "${star_bin}" >&2
    exit 2
}
[[ -f "${genome_dir}/Genome" && -f "${genome_dir}/SA" &&
   -f "${genome_dir}/SAindex" && -f "${genome_dir}/genomeParameters.txt" ]] || {
    printf 'incomplete genome directory: %s\n' "${genome_dir}" >&2
    exit 2
}
[[ -f "${read1}" ]] || {
    printf 'input file is absent: %s\n' "${read1}" >&2
    exit 2
}
if [[ "${read2}" != "none" && ! -f "${read2}" ]]; then
    printf 'input file is absent: %s\n' "${read2}" >&2
    exit 2
fi
[[ "${threads}" =~ ^[1-9][0-9]*$ ]] || {
    printf 'THREADS must be positive\n' >&2
    exit 2
}
[[ ! -e "${out_dir}" ]] || {
    printf 'output path already exists: %s\n' "${out_dir}" >&2
    exit 2
}
[[ "${allow_omp_thread_binding}" == "0" ||
   "${allow_omp_thread_binding}" == "1" ]] || {
    printf 'ALLOW_OMP_THREAD_BINDING must be 0 or 1\n' >&2
    exit 2
}

omp_proc_bind_normalized="${OMP_PROC_BIND:-}"
omp_proc_bind_normalized="${omp_proc_bind_normalized,,}"
if [[ "${allow_omp_thread_binding}" != "1" ]] &&
   { [[ -n "${OMP_PLACES:-}" ]] ||
     [[ -n "${omp_proc_bind_normalized}" &&
        "${omp_proc_bind_normalized}" != "false" ]]; }; then
    printf '%s\n' \
        'OpenMP processor binding is unsafe for alignment comparisons.' \
        'Unset OMP_PROC_BIND and OMP_PLACES, or explicitly allow a controlled affinity test.' >&2
    exit 2
fi

solo_whitelist="none"
if [[ "${mode}" == "starsolo" ]]; then
    [[ -n "${STARSOLO_WHITELIST:-}" &&
       -f "${STARSOLO_WHITELIST}" ]] || {
        printf 'STARSOLO_WHITELIST must name an existing file\n' >&2
        exit 2
    }
    solo_whitelist="$(realpath "${STARSOLO_WHITELIST}")"
fi

case "${genome_load_mode}" in
    NoSharedMemory|LoadAndKeep|LoadAndRemove) ;;
    *)
        printf 'unsupported GENOME_LOAD_MODE: %s\n' "${genome_load_mode}" >&2
        exit 2
        ;;
esac

inputs=("${read1}")
if [[ "${read2}" != "none" ]]; then
    inputs+=("${read2}")
fi

case "${read_files_command}" in
    auto)
        all_gzip=1
        any_gzip=0
        for input in "${inputs[@]}"; do
            if [[ "${input}" == *.gz ]]; then
                any_gzip=1
            else
                all_gzip=0
            fi
        done
        if (( all_gzip )); then
            read_command=(zcat)
        elif (( any_gzip )); then
            printf 'all inputs must use the same compression mode\n' >&2
            exit 2
        else
            read_command=()
        fi
        ;;
    None|none)
        read_command=()
        ;;
    *)
        read -r -a read_command <<< "${read_files_command}"
        ;;
esac

mkdir -p "${out_dir}"
export OMP_DYNAMIC=FALSE

command=(
    "${star_bin}"
    --runMode alignReads
    --runThreadN "${threads}"
    --genomeDir "${genome_dir}"
    --genomeLoad "${genome_load_mode}"
    --readFilesIn "${inputs[@]}"
    --outFileNamePrefix "${out_dir}/star."
)
if (( ${#read_command[@]} > 0 )); then
    command+=(--readFilesCommand "${read_command[@]}")
fi

case "${mode}" in
    paired-mapping|single-mapping)
        command+=(--outSAMtype None --quantMode GeneCounts)
        ;;
    two-pass)
        command+=(
            --twopassMode Basic
            --outSAMtype None
            --quantMode GeneCounts
        )
        ;;
    bysjout)
        command+=(
            --outFilterType BySJout
            --outSAMtype None
            --quantMode GeneCounts
        )
        ;;
    chimeric)
        command+=(
            --outSAMtype None
            --quantMode GeneCounts
            --chimSegmentMin 30
            --chimJunctionOverhangMin 20
            --chimScoreMin 30
            --chimScoreDropMax 100
            --chimScoreSeparation 0
            --chimFilter None
            --chimOutType Junctions SeparateSAMold
        )
        ;;
    sorted-bam)
        command+=(
            --outSAMtype BAM SortedByCoordinate
            --quantMode GeneCounts
            --limitBAMsortRAM "${BAM_SORT_RAM:-30000000000}"
        )
        ;;
    transcriptome-bam)
        command+=(
            --outSAMtype BAM Unsorted
            --quantMode TranscriptomeSAM GeneCounts
        )
        ;;
    starsolo)
        command+=(
            --soloType CB_UMI_Simple
            --soloCBstart 1
            --soloCBlen 16
            --soloUMIstart 17
            --soloUMIlen 12
            --soloCBwhitelist "${solo_whitelist}"
            --soloCBmatchWLtype 1MM_multi_Nbase_pseudocounts
            --soloUMIdedup 1MM_CR
            --soloCellFilter None
            --soloStrand Forward
            --soloFeatures Gene
            --outSAMtype None
        )
        ;;
    starlong)
        command+=(
            --outSAMtype SAM
            --outSAMattributes Standard
            --outSJtype None
        )
        ;;
esac

if [[ -n "${STAR_EXTRA_ARGS:-}" ]]; then
    read -r -a extra_args <<< "${STAR_EXTRA_ARGS}"
    command+=("${extra_args[@]}")
fi

{
    printf 'field\tvalue\n'
    printf 'schema\tblackstar-generalization-run-v1\n'
    printf 'mode\t%s\n' "${mode}"
    printf 'start_utc\t%s\n' "$(date --utc --iso-8601=seconds)"
    printf 'host\t%s\n' "$(hostname)"
    printf 'threads\t%s\n' "${threads}"
    printf 'star_version\t%s\n' "$("${star_bin}" --version)"
    printf 'star_sha256\t%s\n' "$(sha256sum "${star_bin}" | awk '{print $1}')"
    printf 'genome_dir\t%s\n' "${genome_dir}"
    printf 'genome_parameters_sha256\t%s\n' \
        "$(sha256sum "${genome_dir}/genomeParameters.txt" | awk '{print $1}')"
    printf 'read1\t%s\n' "${read1}"
    printf 'read1_bytes\t%s\n' "$(stat --format='%s' "${read1}")"
    printf 'read1_sha256\t%s\n' "$(sha256sum "${read1}" | awk '{print $1}')"
    printf 'read2\t%s\n' "${read2}"
    if [[ "${read2}" != "none" ]]; then
        printf 'read2_bytes\t%s\n' "$(stat --format='%s' "${read2}")"
        printf 'read2_sha256\t%s\n' "$(sha256sum "${read2}" | awk '{print $1}')"
    fi
    printf 'read_files_command\t%s\n' "${read_command[*]:-None}"
    printf 'starsolo_whitelist\t%s\n' "${solo_whitelist}"
    if [[ "${solo_whitelist}" != "none" ]]; then
        printf 'starsolo_whitelist_sha256\t%s\n' \
            "$(sha256sum "${solo_whitelist}" | awk '{print $1}')"
    fi
    printf 'genome_load_mode\t%s\n' "${genome_load_mode}"
    printf 'omp_dynamic\t%s\n' "${OMP_DYNAMIC}"
    printf 'omp_proc_bind\t%s\n' "${OMP_PROC_BIND:-unset}"
    printf 'omp_places\t%s\n' "${OMP_PLACES:-unset}"
    printf 'cpu_allowed_list\t%s\n' \
        "$(awk '/Cpus_allowed_list:/ {print $2}' /proc/self/status)"
    printf 'kernel\t%s\n' "$(uname -sr)"
} > "${out_dir}/provenance.tsv"

printf '%q ' "${command[@]}" > "${out_dir}/command.sh"
printf '\n' >> "${out_dir}/command.sh"
lscpu > "${out_dir}/lscpu.txt"
numactl --hardware > "${out_dir}/numa.txt" 2>&1 || true

/usr/bin/time -v -o "${out_dir}/time.txt" \
    "${command[@]}" > "${out_dir}/stdout.log" 2> "${out_dir}/stderr.log"

printf 'finish_utc\t%s\n' "$(date --utc --iso-8601=seconds)" \
    >> "${out_dir}/provenance.tsv"
find "${out_dir}" -maxdepth 1 -type f -printf '%f\n' |
    LC_ALL=C sort > "${out_dir}/files.txt"
printf 'generalization run complete: %s\n' "${out_dir}"

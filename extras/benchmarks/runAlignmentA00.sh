#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat >&2 <<'EOF'
usage: runAlignmentA00.sh MODE STAR_BIN GENOME_DIR READ1 READ2 OUT_DIR

MODE is mapping-only, unsorted-bam, or sorted-bam.
Environment: THREADS=16 QUANT_MODE=GeneCounts PERF_MODE=none|stat|record|gprofng.
GENOME_LOAD_MODE defaults to NoSharedMemory; LoadAndKeep and LoadAndRemove are allowed.
GPROFNG_CLOCK_PROFILE defaults to hi; GPROFNG_ARCHIVE defaults to usedldobjects.
READ_FILES_COMMAND defaults to auto: zcat for two .gz inputs, otherwise none.
OpenMP processor binding is rejected unless ALLOW_OMP_THREAD_BINDING=1.
EOF
    exit 2
}

[[ $# -eq 6 ]] || usage
mode="$1"
star_bin="$(realpath "$2")"
genome_dir="$(realpath "$3")"
read1="$(realpath "$4")"
read2="$(realpath "$5")"
out_dir="$6"
threads="${THREADS:-16}"
quant_mode="${QUANT_MODE:-GeneCounts}"
perf_mode="${PERF_MODE:-none}"
gprofng_clock_profile="${GPROFNG_CLOCK_PROFILE:-hi}"
gprofng_archive="${GPROFNG_ARCHIVE:-usedldobjects}"
read_files_command="${READ_FILES_COMMAND:-auto}"
genome_load_mode="${GENOME_LOAD_MODE:-NoSharedMemory}"
allow_omp_thread_binding="${ALLOW_OMP_THREAD_BINDING:-0}"
omp_dynamic_inherited="${OMP_DYNAMIC:-unset}"
omp_places_inherited="${OMP_PLACES:-unset}"
omp_proc_bind_inherited="${OMP_PROC_BIND:-unset}"
omp_proc_bind_normalized="${OMP_PROC_BIND:-}"
omp_proc_bind_normalized="${omp_proc_bind_normalized,,}"

[[ "${allow_omp_thread_binding}" == "0" || "${allow_omp_thread_binding}" == "1" ]] || {
    printf 'ALLOW_OMP_THREAD_BINDING must be 0 or 1\n' >&2
    exit 2
}
if [[ "${allow_omp_thread_binding}" != "1" ]] &&
   { [[ -n "${OMP_PLACES:-}" ]] ||
     [[ -n "${omp_proc_bind_normalized}" && "${omp_proc_bind_normalized}" != "false" ]]; }; then
    printf '%s\n' \
        'OpenMP processor binding is unsafe for alignment comparisons: pthread workers may inherit one OpenMP place.' \
        'Unset OMP_PROC_BIND and OMP_PLACES, or set ALLOW_OMP_THREAD_BINDING=1 only for a deliberate affinity test.' >&2
    exit 2
fi

case "${genome_load_mode}" in
    NoSharedMemory|LoadAndKeep|LoadAndRemove) ;;
    *)
        printf 'unsupported GENOME_LOAD_MODE for alignment benchmark: %s\n' \
            "${genome_load_mode}" >&2
        exit 2
        ;;
esac

export OMP_DYNAMIC=FALSE

case "${perf_mode}" in
    none|stat|record) ;;
    gprofng)
        command -v gprofng > /dev/null || {
            printf 'PERF_MODE=gprofng requires gprofng in PATH\n' >&2
            exit 2
        }
        [[ "${gprofng_clock_profile}" =~ ^(off|on|lo|hi|[1-9][0-9]*)$ ]] || {
            printf 'invalid GPROFNG_CLOCK_PROFILE: %s\n' "${gprofng_clock_profile}" >&2
            exit 2
        }
        case "${gprofng_archive}" in
            off|on|ldobjects|src|usedldobjects|usedsrc) ;;
            *) printf 'invalid GPROFNG_ARCHIVE: %s\n' "${gprofng_archive}" >&2; exit 2 ;;
        esac
        ;;
    *) printf 'invalid PERF_MODE: %s\n' "${perf_mode}" >&2; exit 2 ;;
esac

[[ -x "${star_bin}" ]] || { printf 'STAR binary is not executable: %s\n' "${star_bin}" >&2; exit 2; }
[[ -f "${genome_dir}/Genome" && -f "${genome_dir}/SA" && -f "${genome_dir}/SAindex" ]] || {
    printf 'incomplete genome directory: %s\n' "${genome_dir}" >&2
    exit 2
}
[[ -f "${read1}" && -f "${read2}" ]] || { printf 'FASTQ input missing\n' >&2; exit 2; }
[[ "${threads}" =~ ^[1-9][0-9]*$ ]] || { printf 'THREADS must be positive\n' >&2; exit 2; }
[[ ! -e "${out_dir}" ]] || { printf 'output path already exists: %s\n' "${out_dir}" >&2; exit 2; }
mkdir -p "${out_dir}"

case "${mode}" in
    mapping-only) out_sam_type=(None) ;;
    unsorted-bam) out_sam_type=(BAM Unsorted) ;;
    sorted-bam) out_sam_type=(BAM SortedByCoordinate) ;;
    *) usage ;;
esac

case "${read_files_command}" in
    auto)
        if [[ "${read1}" == *.gz && "${read2}" == *.gz ]]; then
            read_command=(zcat)
        elif [[ "${read1}" != *.gz && "${read2}" != *.gz ]]; then
            read_command=()
        else
            printf 'both FASTQ inputs must use the same compression mode\n' >&2
            exit 2
        fi
        ;;
    None|none)
        read_command=()
        ;;
    *)
        read -r -a read_command <<< "${read_files_command}"
        ;;
esac

command=(
    "${star_bin}"
    --runMode alignReads
    --runThreadN "${threads}"
    --genomeDir "${genome_dir}"
    --genomeLoad "${genome_load_mode}"
    --readFilesIn "${read1}" "${read2}"
    --outSAMtype "${out_sam_type[@]}"
    --outFileNamePrefix "${out_dir}/star."
)
if (( ${#read_command[@]} > 0 )); then
    command+=(--readFilesCommand "${read_command[@]}")
fi
if [[ "${quant_mode}" != "None" ]]; then
    command+=(--quantMode "${quant_mode}")
fi
if [[ "${mode}" == "sorted-bam" ]]; then
    command+=(--limitBAMsortRAM "${BAM_SORT_RAM:-30000000000}")
fi
if [[ -n "${STAR_EXTRA_ARGS:-}" ]]; then
    read -r -a extra_args <<< "${STAR_EXTRA_ARGS}"
    command+=("${extra_args[@]}")
fi

{
    printf 'field\tvalue\n'
    printf 'schema\tblackstar-a00-v1\n'
    printf 'mode\t%s\n' "${mode}"
    printf 'start_utc\t%s\n' "$(date --utc --iso-8601=seconds)"
    printf 'host\t%s\n' "$(hostname)"
    printf 'threads\t%s\n' "${threads}"
    printf 'star_version\t%s\n' "$("${star_bin}" --version)"
    printf 'star_sha256\t%s\n' "$(sha256sum "${star_bin}" | awk '{print $1}')"
    printf 'genome_dir\t%s\n' "${genome_dir}"
    printf 'genome_parameters_sha256\t%s\n' "$(sha256sum "${genome_dir}/genomeParameters.txt" | awk '{print $1}')"
    printf 'read1\t%s\n' "${read1}"
    printf 'read1_bytes\t%s\n' "$(stat --format='%s' "${read1}")"
    printf 'read1_sha256\t%s\n' "$(sha256sum "${read1}" | awk '{print $1}')"
    printf 'read2\t%s\n' "${read2}"
    printf 'read2_bytes\t%s\n' "$(stat --format='%s' "${read2}")"
    printf 'read2_sha256\t%s\n' "$(sha256sum "${read2}" | awk '{print $1}')"
    printf 'compiler\t%s\n' "$(g++ --version | head -1)"
    printf 'kernel\t%s\n' "$(uname -sr)"
    printf 'perf_mode\t%s\n' "${perf_mode}"
    printf 'gprofng_clock_profile\t%s\n' "${gprofng_clock_profile}"
    printf 'gprofng_archive\t%s\n' "${gprofng_archive}"
    if [[ "${perf_mode}" == "gprofng" ]]; then
        printf 'gprofng_version\t%s\n' "$(gprofng --version | head -1)"
    fi
    printf 'read_files_command\t%s\n' "${read_command[*]:-None}"
    printf 'genome_load_mode\t%s\n' "${genome_load_mode}"
    printf 'omp_dynamic_inherited\t%s\n' "${omp_dynamic_inherited}"
    printf 'omp_dynamic_effective\t%s\n' "${OMP_DYNAMIC}"
    printf 'omp_proc_bind\t%s\n' "${omp_proc_bind_inherited}"
    printf 'omp_places\t%s\n' "${omp_places_inherited}"
    printf 'allow_omp_thread_binding\t%s\n' "${allow_omp_thread_binding}"
    printf 'slurm_job_id\t%s\n' "${SLURM_JOB_ID:-none}"
    printf 'cpu_allowed_list\t%s\n' "$(awk '/Cpus_allowed_list:/ {print $2}' /proc/self/status)"
} > "${out_dir}/provenance.tsv"

printf '%q ' "${command[@]}" > "${out_dir}/command.sh"
printf '\n' >> "${out_dir}/command.sh"
lscpu > "${out_dir}/lscpu.txt"
numactl --hardware > "${out_dir}/numa.txt" 2>&1 || true

case "${perf_mode}" in
    none)
        /usr/bin/time -v -o "${out_dir}/time.txt" \
            "${command[@]}" > "${out_dir}/stdout.log" 2> "${out_dir}/stderr.log"
        ;;
    stat)
        /usr/bin/time -v -o "${out_dir}/time.txt" \
            perf stat -x $'\t' -o "${out_dir}/perf-stat.tsv" -- \
            "${command[@]}" > "${out_dir}/stdout.log" 2> "${out_dir}/stderr.log"
        ;;
    record)
        /usr/bin/time -v -o "${out_dir}/time.txt" \
            perf record --call-graph dwarf -o "${out_dir}/perf.data" -- \
            "${command[@]}" > "${out_dir}/stdout.log" 2> "${out_dir}/stderr.log"
        ;;
    gprofng)
        /usr/bin/time -v -o "${out_dir}/time.txt" \
            gprofng collect app \
                -p "${gprofng_clock_profile}" \
                -a "${gprofng_archive}" \
                -F off \
                -S 1 \
                -O "${out_dir}/gprofng.er" \
                "${command[@]}" > "${out_dir}/stdout.log" 2> "${out_dir}/stderr.log"
        ;;
esac

printf 'finish_utc\t%s\n' "$(date --utc --iso-8601=seconds)" >> "${out_dir}/provenance.tsv"
find "${out_dir}" -maxdepth 1 -type f -printf '%f\n' | LC_ALL=C sort > "${out_dir}/files.txt"
printf 'A00 run complete: %s\n' "${out_dir}"

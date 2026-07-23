#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/blackstar-affinity-test.XXXXXX")"
trap 'rm -rf "${tmp_dir}"' EXIT

g++ -std=c++11 -O2 -Wall -Wextra -fopenmp -pthread \
    -I"${repo_root}/source" \
    "${repo_root}/extras/tests/testAlignmentThreadAffinity.cpp" \
    "${repo_root}/source/AlignmentThreadAffinity.cpp" \
    -o "${tmp_dir}/testAlignmentThreadAffinity"

env -u OMP_PROC_BIND -u OMP_PLACES OMP_DYNAMIC=FALSE \
    "${tmp_dir}/testAlignmentThreadAffinity" > "${tmp_dir}/unbound.tsv"
awk -F '\t' '
    $1 == "binding_requested" { requested=$2 }
    $1 == "binding_active" { binding=$2 }
    $1 == "before_cpu_count" { before=$2 }
    $1 == "after_cpu_count" { after=$2 }
    $1 == "child_cpu_count" { child=$2 }
    END { exit !(requested == 0 && binding == 0 && before == after && after == child) }
' "${tmp_dir}/unbound.tsv"

OMP_PROC_BIND=FALSE OMP_PLACES=cores OMP_DYNAMIC=FALSE \
    "${tmp_dir}/testAlignmentThreadAffinity" > "${tmp_dir}/disabled.tsv"
awk -F '\t' '
    $1 == "binding_requested" { requested=$2 }
    $1 == "binding_active" { binding=$2 }
    $1 == "before_cpu_count" { before=$2 }
    $1 == "after_cpu_count" { after=$2 }
    END { exit !(requested == 0 && binding == 0 && before == after) }
' "${tmp_dir}/disabled.tsv"

OMP_PROC_BIND=close OMP_PLACES=cores OMP_DYNAMIC=FALSE \
    "${tmp_dir}/testAlignmentThreadAffinity" > "${tmp_dir}/bound.tsv"
awk -F '\t' '
    $1 == "binding_requested" { requested=$2 }
    $1 == "binding_active" { binding=$2 }
    $1 == "place_count" { places=$2 }
    $1 == "reported_cpu_count" { reported=$2 }
    $1 == "status" { status=$2 }
    $1 == "before_cpu_count" { before=$2 }
    $1 == "after_cpu_count" { after=$2 }
    $1 == "child_cpu_count" { child=$2 }
    END {
        exit !(requested == 1 && binding == 1 && places > 0 && status == 0 &&
               after == reported && child == after && after >= before)
    }
' "${tmp_dir}/bound.tsv"

printf 'alignment thread affinity tests passed\n'

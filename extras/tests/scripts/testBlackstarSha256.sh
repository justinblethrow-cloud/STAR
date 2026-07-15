#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
build_dir="$(mktemp -d "${TMPDIR:-/tmp}/blackstar-sha256-build.XXXXXX")"
trap 'rm -rf "${build_dir}"' EXIT

"${CXX:-g++}" \
    -std=c++11 -Wall -Wextra -fsanitize=address,undefined -fopenmp \
    -I"${repo_dir}/source" \
    "${repo_dir}/extras/tests/testBlackstarSha256.cpp" \
    "${repo_dir}/source/BlackstarSha256.cpp" \
    -o "${build_dir}/testBlackstarSha256"

ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 OMP_DYNAMIC=FALSE \
    "${build_dir}/testBlackstarSha256"

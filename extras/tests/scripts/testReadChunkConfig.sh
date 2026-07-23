#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
build_dir="$(mktemp -d "${TMPDIR:-/tmp}/blackstar-read-chunk-test.XXXXXX")"
trap 'rm -rf "${build_dir}"' EXIT

"${CXX:-g++}" \
    -std=c++11 -Wall -Wextra -fsanitize=address,undefined \
    -I"${repo_root}/source" \
    "${repo_root}/extras/tests/testReadChunkConfig.cpp" \
    "${repo_root}/source/ReadChunkConfig.cpp" \
    -o "${build_dir}/testReadChunkConfig"

ASAN_OPTIONS=detect_leaks=0 "${build_dir}/testReadChunkConfig"

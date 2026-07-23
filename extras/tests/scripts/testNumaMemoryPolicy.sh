#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/blackstar-numa-test.XXXXXX")"
trap 'rm -rf "${tmp_dir}"' EXIT

g++ -std=c++11 -O2 -Wall -Wextra \
    -I"${repo_root}/source" \
    "${repo_root}/extras/tests/testNumaMemoryPolicy.cpp" \
    "${repo_root}/source/NumaMemoryPolicy.cpp" \
    -o "${tmp_dir}/testNumaMemoryPolicy"

"${tmp_dir}/testNumaMemoryPolicy"
printf 'NUMA memory policy tests passed\n'

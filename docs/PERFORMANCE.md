# BlackSTAR Performance

Performance claims apply only to the stated workload, host, binaries, thread
count, storage placement, and cache definition. They are not universal
guarantees.

## Direct Official STAR Comparison

The cumulative qualification compared official STAR `2.7.11b` at
`b1edc1208d91a53bf40ebae8669f71d50b994851` with released BlackSTAR
`2.7.11b-blackstar.2` at
`d6fbf932ae2b155ce4f689bce106429ab2bc07f6`.

The benchmark ran on dedicated Slurm node `ca2`, using Linux x86-64 GCC 13
binaries, 96 logical CPUs for the primary high-thread measurements, and
node-local SSD. Every timing used seeded, order-balanced pairs.

### Full CHM13 Index

CHM13v2 plus ERCC and the production CHM13/Ensembl/HPRC/ERCC GTF used three
pairs:

| Metric | Official STAR | BlackSTAR |
| --- | ---: | ---: |
| Median wall time | 1,256.03 s | 607.18 s |
| Paired reduction | | 51.44% |
| Speedup | | 2.06x |
| Median peak RSS | 52.78 GiB | 77.15 GiB |

All 42 substantive index-file comparisons passed byte-for-byte. The speedup
therefore carries a material 46.18% memory cost that must remain visible in
capacity planning.

### Alignment Scaling

The public corpus contained 12,768,316 paired 76-base ENCODE reads and used a
common GRCh38/Ensembl 114 index with `GeneCounts`, `NoSharedMemory`, and no
alignment file.

| Input | Threads | Official STAR | BlackSTAR | Reduction |
| --- | ---: | ---: | ---: | ---: |
| Uncompressed | 1 | 1,613.41 s | 1,524.61 s | 6.17% |
| Uncompressed | 32 | 101.50 s | 99.22 s | 2.38% |
| Uncompressed | 64 | 83.35 s | 61.07 s | 26.73% |
| Uncompressed | 96 | 82.49 s | 55.20 s | 33.11% |
| `zcat` | 1 | 1,633.61 s | 1,522.24 s | 6.68% |
| `zcat` | 32 | 101.14 s | 98.89 s | 2.20% |
| `zcat` | 64 | 84.96 s | 64.88 s | 23.30% |
| `zcat` | 96 | 90.22 s | 63.15 s | 30.75% |

The 1-thread control shows a modest serial gain. The larger 64- and 96-thread
differences establish improved multicore efficiency. The approximately 2%
32-thread result prevents a general claim that BlackSTAR is 30% faster at all
thread counts.

All 24 mapping-pair comparisons passed. A separate 2-million-pair unsorted-BAM
check passed canonical record identity. Median peak RSS at 96 threads was
approximately 5.7% lower for BlackSTAR.

### Named-Sequence Addition

The test added public GFP and glutathione S-transferase FASTA records plus
insert-only GTF annotations to GRCh38/Ensembl 114.

| Input-file cache | Official full rebuild | BlackSTAR Delta | Speedup |
| --- | ---: | ---: | ---: |
| Warm | 1,172.64 s | 23.21 s | 50.52x |
| Verified cold | 1,225.62 s | 39.04 s | 31.50x |

All six cold gates measured zero resident input-file pages before timing. This
controls file-page residency, not powered-off hardware; filesystem metadata,
libraries, and device caches can remain warm.

All 57 cold-versus-warm artifact checks passed. Full and Delta alignment
matched timing-independent metrics, junctions, gene counts, and canonical BAM
records, with exactly 100 GFP and 100 GST fragments counted in each mode.

## Evidence

Bounded machine-readable receipts are committed under
`docs/benchmarks/official-star-2.7.11b-vs-blackstar.2/`. Large raw outputs remain
outside Git. The receipts identify both binaries and preserve every published
aggregate needed to audit the claims.

## Claim Rules

- Do not add incremental experiment percentages to cumulative results.
- Do not generalize high-thread results to all thread counts.
- Report memory changes alongside wall-time changes.
- State local, network, cold, and warm storage conditions explicitly.
- Require output-equivalence gates before accepting a speed result.
- Requalify after performance-sensitive code, compiler, dependency, or host
  topology changes.

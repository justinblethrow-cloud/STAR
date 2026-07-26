# Q02: Cross-Workload Generalization

## Status

- State: complete; mixed qualification result
- Parent commit:
  `0978542f0957c47548f5f00fc706e1e08c021375`
- Final experiment commit:
  `ef2a2560013293a3cd93403d876f50a3d5ec759c`
- Opened: 2026-07-25
- Decided: 2026-07-26
- Release disposition: retained as an unreleased Labs hardening candidate

Q02 asked whether BlackSTAR's improvements and compatibility controls extend
beyond the paired short-read, gene-count-only workload that originally drove
alignment optimization. It covers fragmented paired and single-end RNA-seq,
two-pass alignment, splice-junction filtering, chimeric detection, coordinate
sorting, transcriptome BAM output, STARsolo, STARlong, SAM input, WASP,
shared-memory lifecycle, and transformed-genome output.

The result is deliberately mixed. Nine public performance series passed their
predeclared compatibility and noninferiority gates. The exclusive-node
single-end series preserved every measured output and passed noninferiority,
but failed its variability gate and does not support a speed claim. This record
retains that failure.

## Hypothesis

The released high-thread scheduling and NUMA changes should preserve inherited
STAR behavior across common workflows and produce either:

1. a positive, replicated direct improvement over official STAR; or
2. a bounded noninferiority result with exact mode-specific outputs.

The hardening added during Q02 should also close concrete liabilities found by
the audit without creating a new format or deployment dependency.

## Scope

Runtime and operational changes evaluated in this phase include:

- record-safe fallback for SAM input when automatic FASTQ chunk sizing is
  active;
- cgroup-aware host-memory assessment for parallel index strategies;
- restoration of inherited NUMA policy after private genome loading;
- isolated `STAR` and `STARlong` build state;
- explicit baseline x86-64 and AVX2 release variants;
- additional genome-insert annotation and identity checks; and
- deterministic `TranscriptomeSAM` primary-alignment selection based on the
  run seed and stable input-read ordinal.

The paired, two-pass, BySJout, chimeric, sorted-BAM, STARsolo, and STARlong
performance rows used the phase candidate binary identified by SHA-256
`fb82a5cf2fb0fdd285f9d211045d3b11ad0d5a91d360bda3874c639dc6848489`.
The final TranscriptomeSAM and exclusive single-end rows used the clean
archive-derived `ef2a256` binary identified by SHA-256
`297db5482236d970b2b19fed6016c1f9981d973fdbd6784a1700f1a7bba40635`.
Changes between those binaries affect TranscriptomeSAM primary selection and
benchmark infrastructure, not the earlier measured FASTQ runtime paths.
Performance values nevertheless remain claims about the exact measured binary,
not an unmeasured future release artifact.

This phase does not authorize a version bump, tag, GitHub update, release, or
external pipeline deployment.

## Correctness Contract

Each mode compares every artifact it can make biologically visible:

| Mode | Required oracle |
| --- | --- |
| Paired, single, two-pass, BySJout | Timing-independent final metrics, sorted junctions, and gene counts |
| Chimeric | Core outputs plus canonical chimeric SAM records and junctions |
| Sorted BAM | Core outputs plus canonical coordinate-sorted BAM records |
| TranscriptomeSAM | Genomic BAM exact; transcript alignment set exact after clearing only flag `0x100`; BlackSTAR primary flags exact across repeated and cross-thread runs |
| STARsolo | Exact output inventory and exact file contents |
| STARlong | Canonical SAM records and timing-independent metrics |

The specialized synthetic matrix additionally requires exact or
upstream-equivalent behavior for WASP, SAM input, cross-binary shared-memory
lifecycle, and haploid transformed-genome output.

## Benchmark Contract

- Official control: STAR 2.7.11b at
  `b1edc1208d91a53bf40ebae8669f71d50b994851`.
- Index: GRCh38 with Ensembl 114 annotations.
- Primary paired corpus: 12,768,316 public paired 76-base ENCODE reads.
- Additional corpora: public paired and single-end 150-base fragmented RNA-seq,
  public 10x Genomics v3, and public direct-RNA long reads.
- Hardware: hardware-matched dual-socket AMD EPYC 7742 systems; every series
  stayed on one host.
- Storage: node-local SSD or NVMe; no measured path used network storage.
- Threads: 96 for primary public series; 1 and 96 for the transcriptome
  determinism control.
- Design: seeded order-balanced pairs, excluded warmups immediately before
  measurement, and three or five pairs per series.
- Compatibility gate: all correctness checks, at least three pairs, both arm
  CV no greater than 5 percent, lower paired-bootstrap interval no worse than
  -2 percent, and no more than 5 percent median peak-RSS growth.
- Speed reporting: noninferiority alone is never called a speedup. The interval
  and variability remain visible beside every point estimate.

Dedicated-node runs record an explicit quiet-gate waiver. Shared-host runs used
the quiet-system gate. A waiver does not relax correctness, order balancing,
replication, resource, or statistical requirements.

## Public Performance Results

Positive paired improvement means less BlackSTAR wall time.

| Workload | Pairs | Official STAR | BlackSTAR | Median paired improvement | 95% interval | Peak-RSS change | Verdict |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Paired 76-base fragmented | 5 | 72.02 s | 58.02 s | 20.05% | 16.32 to 23.96% | -5.68% | Compatibility pass; positive speed interval |
| Paired 150-base fragmented | 3 | 89.77 s | 78.66 s | 12.67% | 11.38 to 13.46% | -5.65% | All gates pass |
| Single-end 150-base | 5 | 61.52 s | 58.95 s | 1.67% | -1.87 to 13.42% | -5.66% | **Timing series fails CV; no speed claim** |
| Two-pass Basic | 3 | 223.91 s | 177.05 s | 20.25% | 15.44 to 20.93% | -8.76% | Compatibility pass; positive speed interval |
| BySJout | 3 | 91.36 s | 62.16 s | 31.45% | 31.22 to 35.64% | -4.21% | All gates pass |
| Chimeric detection | 3 | 89.55 s | 60.09 s | 31.76% | 30.98 to 34.00% | -5.68% | All gates pass |
| Coordinate-sorted BAM | 5 | 106.56 s | 85.31 s | 18.42% | 16.93 to 24.26% | -4.47% | Compatibility pass; canonical BAM exact |
| Genomic plus transcriptome BAM | 3 | 217.33 s | 213.58 s | 1.55% | -0.45 to 2.63% | -3.82% | Accepted noninferiority; no speed claim |
| STARsolo 10x v3 Gene | 3 | 153.45 s | 149.23 s | 3.78% | -1.43 to 10.49% | -4.83% | Accepted noninferiority; no speed claim |
| STARlong direct RNA | 3 | 72.79 s | 58.61 s | 19.48% | 17.11 to 22.38% | -2.22% | All gates pass with symmetric seed-limit override |

The paired 76-base, two-pass, and sorted-BAM control arms had CV values between
3 and 5 percent. They pass the predeclared generalization compatibility gate,
but their measured gains should not be promoted as new release claims under the
separate Labs preference for less than 3 percent arm CV. Paired 150-base,
BySJout, chimeric, and STARlong satisfy that stricter variability preference.

Across all ten series, including the nonaccepted single-end timing aggregate,
all 36/36 mode-specific pair comparisons passed. The single-end result therefore
identifies a performance-evidence limitation, not a correctness regression.

## TranscriptomeSAM Liability and Fix

Official STAR chooses one transcriptome primary alignment using an RNG owned by
the worker. Changing worker assignment can therefore change SAM flag `0x100`
without changing the alignment set.

BlackSTAR now:

1. retains one inherited RNG draw so later inherited random choices keep the
   same stream position;
2. hashes `runRNGseed` and the stable `iReadAll` ordinal with SplitMix64;
3. selects the primary transcript modulo the number of transcript alignments;
4. leaves every transcript alignment and downstream count unchanged.

The full public corpus produced raw candidate digest
`a993252b865e62ce17a6f2bb594187e69e1e869e1f5f679ba62dd00f37ca61dc`
in the warmup and all three measured runs.

A controlled 250,000-pair test then used the same input at 1 and 96 threads:

| Implementation | 1-thread primary digest | 96-thread primary digest | Result |
| --- | --- | --- | --- |
| Official STAR | `903a7e8b900a...` | `b6b772c7b04b...` | Different |
| BlackSTAR | `f9f1417857e5...` | `f9f1417857e5...` | Exact |

All four transcript alignment sets had digest
`5672c7e1447d4e4900cef79dc063e74caac1c90e72140583d1661e5df993dd86`
after clearing only `0x100`. Genomic BAM records, gene counts, junctions, and
timing-independent metrics were also exact across all four runs.

This is an intentional compatibility-visible correction: BlackSTAR preserves
the complete upstream transcript alignment set but may choose a different
primary transcript than one particular official STAR run.

## Functional Gates

The current clean candidate passed:

- 11/11 focused ASan and UBSan scripts;
- the generalization benchmark-harness regression;
- 12/12 specialized-mode compatibility checks;
- a clean OpenMP-linked short-read build; and
- a clean isolated STARlong build and compatibility oracle.

The specialized matrix includes fragmented paired alignment, TranscriptomeSAM
and deterministic primary flags, gene counts, two-pass, BySJout, chimeric,
WASP, STARsolo, SAM input, shared-memory cross-binary lifecycle, and
transformed-genome output.

## Negative and Limiting Evidence

- The first three-pair sorted-BAM series had 6.59 percent upstream CV and was
  rejected. The retained five-pair rerun reduced maximum arm CV to 3.80 percent.
- A shared-host single-end series appeared 22.55 percent faster but had
  7.61/6.41 percent arm CV. It is superseded and must not be cited.
- The exclusive-node single-end rerun reduced that point estimate to 1.67
  percent and still narrowly failed the 5 percent variability gate.
- At 32 threads, the single-end point estimate was -1.99 percent with an
  interval of -4.01 to +6.02 percent; even 2 percent noninferiority was not
  established.
- Both official STARlong and BlackSTAR STARlong abort on the real direct-RNA
  fixture under the inherited default `seedPerReadNmax`. The accepted
  comparison used `--seedPerReadNmax 100000` in both arms.
- WASP, SAM-input, shared-memory, and transformed-genome paths have differential
  functional coverage but no replicated performance claim.
- Results cover one CPU family, one GCC toolchain, local storage, one primary
  high thread count, and the stated public fixtures.

## Decision

- Outcome: retain the hardening source and evidence as a Labs candidate.
- Compatibility: no measured biological-output regression across the tested
  short-read, single-cell, specialized, and long-read modes.
- Performance: strong cumulative gains generalize to several fragmented and
  specialized modes; STARsolo and TranscriptomeSAM are noninferior only.
- Unresolved: single-end performance remains variable and does not qualify as
  an improvement.
- Release: not promoted, versioned, tagged, pushed, or deployed by Q02.
- Follow-up: any release candidate must rebuild from the eventual protected
  commit and rerun release reproducibility plus the selected cumulative
  workload matrix.

## Plain-Language Takeaway

BlackSTAR's multicore gains are not confined to one 3-prime gene-count
workflow. They remain substantial for paired fragmented reads, 150-base paired
reads, two-pass alignment, splice-junction filtering, chimeric detection,
sorted BAM output, and a real long-read fixture. STARsolo and transcriptome BAM
output remain compatible without a proven speed gain. Single-end output is
correct and noninferior in the exclusive-node test, but its timing is too
variable to claim improvement. BlackSTAR also removes an upstream
thread-dependent TranscriptomeSAM primary-flag behavior while preserving the
complete alignment set.

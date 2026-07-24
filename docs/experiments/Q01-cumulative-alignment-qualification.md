# Q01: Cumulative Alignment Qualification

## Status

- State: complete; cumulative Labs candidate technically qualified
- Release disposition: not promoted, tagged, or released
- Qualified release remains:
  `821457378fa38bfb23b061b8f11ee0a09431dda7`
- Cumulative source revision used for paired timing:
  `7b31a5fe5cb966c9146b99d5ca1ad81ea9c81cdb`
- Final runtime implementation commit:
  `6032393155317b17fef1750672f1da6770ae6042`
- Qualification harness commit:
  `9d37e0f8009c28fd72e84c3c42fa272ed53b1f71`
- Decided: 2026-07-24

Q01 qualifies the cumulative source stack containing H01 affinity recovery,
A02 adaptive input chunks, A05 NUMA-aware private genome placement, and A06
transcript-recursion copy elision. It does not alter the qualified
`blackstar.1` release boundary. Promotion remains a separate deliberate
operation.

## Candidate Stack

| Change | Runtime implementation | Role |
| --- | --- | --- |
| H01 | `64cd86a`, `496ee8a` | Restore the allowed CPU-place union before pthread worker creation when inherited OpenMP binding narrowed the initial thread. |
| A02 | `a438a4f` | Use adaptive record-safe input chunks at high thread counts. |
| A05 | `eb54b0b`, `91de892` | Interleave eligible high-thread private genome memory while preserving inherited NUMA policy and shared-memory behavior. |
| A06 | `6032393` | Forward unchanged transcript recursion state by const reference and copy only mutating or terminal branches. |

A09 LTO and PGO were evaluated independently and rejected below the practical
performance gate. Neither toolchain variant is present in the Q01 candidate.

## Identity Contract

| Artifact | SHA-256 |
| --- | --- |
| Qualified-release benchmark binary | `115e4964990b91292f16e163c1a5fd694d5575cbe1b9ef3be342852eb91bed6a` |
| Cumulative benchmark binary | `6b08a7925022c6a03813660c69d0b0bd1ed98a377ac9c1136dd987e3d4c8f85e` |
| Genome parameters | `102d4859552f2e8b4eff643995aee73879c03d9ab6e0ea5ad35499d399c1f00b` |
| Upstream compatibility oracle | STAR 2.7.11b at `b1edc1208d91a53bf40ebae8669f71d50b994851` |

The benchmark binary was built before the harness-only shared-load change.
The package reproducibility gate was built at the harness commit. The runtime
source is unchanged between those revisions.

## Primary Benchmark Contract

- Public corpus: 12,768,316 paired 76-base ENCODE reads.
- Index: public GRCh38 with Ensembl 114 annotations.
- Host: administratively dedicated compute node.
- Storage: compute-node local SSD.
- Threads: 96 logical CPUs requested.
- Mode: mapping only with gene counts; no alignment file in the primary timing.
- Input paths: uncompressed FASTQ and the same corpus through `zcat`.
- Design: three seeded order-balanced pairs per input mode.
- Acceptance: at least 2 percent median paired improvement, positive paired
  bootstrap interval, no more than 5 percent peak-RSS increase, less than
  3 percent arm CV, and exact timing-independent outputs.
- Quiet gate: explicitly waived because the host was administratively
  dedicated; the waiver is recorded in each contract.

## Replicated Performance

| Input | Qualified release | Cumulative candidate | Paired improvement | 95% interval | Release/Candidate CV | Peak-RSS change | Correctness |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Uncompressed | 84.28 s | 56.17 s | **33.2025%** | 33.0802 to 35.8089% | 1.8120 / 0.5874% | -6.8126% | 3/3 |
| `zcat` | 90.61 s | 64.03 s | **28.5399%** | 28.4661 to 29.6105% | 0.8034 / 0.6756% | -6.7724% | 3/3 |

Both modes passed every predeclared performance, variability, resource, and
correctness gate. Correctness compared timing-independent final metrics,
splice junctions, and gene counts in every pair.

These are cumulative comparisons with the qualified release. They must not be
added arithmetically to the separately measured effects of H01, A02, A05, or
A06.

## Shared Index and BAM Safety

The candidate preloaded a 29,940,711,542-byte shared genome segment and
reported:

```text
BLACKSTAR_NUMA_POLICY requested Auto effective Default allowed_nodes -1
inherited NotQueried inherited_nodes -1 status 0 reason shared-genome
```

A matched `LoadAndKeep` pair using 2,000,000 public paired reads and unsorted
BAM output passed all five checks:

- timing-independent final metrics;
- splice junctions;
- gene counts;
- BAM presence; and
- canonical BAM records.

Both canonical BAM streams had SHA-256
`e3f67bccb149277f6f9673e204071b80afab5021c8182028ec17678cdbc750ac`.
`LoadAndRemove` then removed the test segment. The single ordered pair is a
safety check, not a shared-index performance claim.

## Bound-Affinity Positive Control

One matched 2,000,000-pair control deliberately inherited:

```text
OMP_PROC_BIND=close
OMP_PLACES=cores
```

with 96 requested STAR threads. The cumulative candidate reported:

```text
BLACKSTAR_ALIGN_AFFINITY_RECOVERY places 128 cpus 256 status 0
```

| Binary | Wall time | Mean CPU reported by `time` |
| --- | ---: | ---: |
| Qualified release | 226.13 s | 186% |
| Cumulative candidate | 37.69 s | 952% |

This is an **83.3326 percent reduction**, or a descriptive **6.00-fold
recovery**, with all three timing-independent comparisons passing. It is a
matched failure-mode positive control with one pair, not an ordinary-path
performance estimate. The recovered place count reflects the host's complete
allowed 256-logical-CPU set, not the 96 threads requested for this STAR run.

## Functional and Compatibility Gates

All eight focused ASan/UBSan scripts passed:

1. alignment-thread affinity;
2. adaptive read-chunk configuration;
3. packed arrays;
4. suffix comparison;
5. transcript initialization and copying;
6. genome-parameter initialization;
7. junction alignment; and
8. BlackSTAR SHA-256 support.

Additional gates passed:

| Gate | Result |
| --- | ---: |
| Deployment selector matrix, including expected negative cases | Pass |
| Benchmark harness regression, including shared-load contracts | Pass |
| Genome-insert hardening | 24/24 |
| SAindex bounded/low-memory strategy and identity | 4/4 |
| Full-index alignment under official upstream STAR 2.7.11b | 1/1 |

The genome-insert suite covers Full, Overlay, and Delta outputs, optional and
required GTF handling, namespace collisions, idempotence across thread counts,
manifest and payload corruption, stale-base rejection, atomic cleanup, paths
containing spaces, benchmark failure propagation, and upstream compatibility.

## Reproducible Package Gate

Two independent clean package builds at the qualification harness commit
produced exact identities:

| Artifact | Build A | Build B |
| --- | --- | --- |
| Binary | `b8f5aeb4b0202013d62740b606b418564a9972a0a1df531d6c62fdb30d67de00` | same |
| Archive | `c7b9f875e735935818233bd81441a06bfe0ca4bfbe1ee7972f0f4f5c93e266f9` | same |
| `build-info.tsv` | `2e4afa3f7cd3392a4d9ac08616f98385b35322587f87aad8d7215c89a4505e18` | same |

The binary was OpenMP-linked. The package still carries the current
`2.7.11b-blackstar.1` source version string because Q01 does not perform a
release-version bump. These packages are qualification artifacts and must not
be distributed as a replacement `blackstar.1` release.

## Decision

- Outcome: cumulative Labs candidate technically qualified.
- Performance: primary uncompressed and compressed gates passed.
- Correctness: every paired output comparison passed.
- Safety: shared-index lifecycle, canonical BAM, affinity failure recovery,
  sanitizers, insertion, SAindex, deployment, and upstream compatibility
  gates passed.
- Reproducibility: two clean package builds were byte-identical.
- Promotion: deliberately deferred. The qualified release worktree and tag
  remain unchanged.
- Next experimental stage: return to the index roadmap beginning with I01,
  unless a deliberate release-promotion decision is made first.

## Limitations

- Performance was measured on one Linux host, one GCC version, one public
  paired short-read corpus, one index, one thread count, and local SSD.
- The shared-index BAM and bound-affinity controls used one pair each and
  support safety or failure recovery only.
- No Q01 result establishes performance for long reads, single-end reads,
  alternate CPUs, network storage, BAM sorting, or other STAR modes.
- Technical qualification does not itself define a release version, support
  policy, or deployment approval.

## Plain-Language Takeaway

The cumulative unreleased BlackSTAR alignment changes reduced full-corpus
runtime by about one-third for uncompressed input and by about 29 percent for
compressed input versus the qualified release. Outputs remained exact, memory
use fell, inherited OpenMP binding was recovered, shared-index and BAM paths
passed, and clean packages rebuilt byte-for-byte. The code is technically
qualified in Labs, but it has not been promoted or released.

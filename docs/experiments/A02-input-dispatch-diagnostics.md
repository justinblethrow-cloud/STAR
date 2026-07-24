# A02: Adaptive Input-Chunk Dispatch

## Status

- State: accepted in `2.7.11b-blackstar.2`
- Qualified-release ancestor: `821457378fa38bfb23b061b8f11ee0a09431dda7`
- Runtime parent: H01 commit `496ee8a5687c21520b91603b6e04141fd16a4fa0`
- Diagnostic commit: `544237f25af23623346d8c305aebd96d0d798786`
- Candidate commit: `a438a4f55b855c42c04c89cc52bbc7f082547620`
- Opened: 2026-07-23
- Decided: 2026-07-23
- Cumulative qualification: Q01 passed 2026-07-24

## Hypothesis

STAR's shared input mutex, serial FASTQ parsing inside that critical section,
and coarse input chunks prevent high-thread alignment workers from receiving
work evenly enough to use the requested CPUs. Reducing chunk granularity at
high thread counts should shorten the completion tail without changing record
boundaries, input order, alignment logic, or output semantics.

## Scope

The diagnostic phase added compile-time-only per-thread measurements to the
clean qualified-release source. It did not include A01 or alter runtime
behavior. Those measurements localized material lock occupancy and tail
imbalance.

The accepted candidate changes only input-buffer sizing:

- `ReadChunkConfig` validates and calculates the effective allocation.
- `readChunkSizeBytes=0` selects a 1,000,000-byte total input chunk at 64 or
  more mapping threads.
- Fewer than 64 threads retain STAR's `limitIObufferSize` input value.
- An explicit positive `readChunkSizeBytes` overrides the automatic target but
  cannot exceed the configured `limitIObufferSize` maximum.
- Long-read reserve requirements can raise the automatic target above 1 MB.
- Invalid small or 32-bit-overflowing allocations fail before allocation.

Parsing, locking, chunk ownership, output order, mapping kernels, index formats,
and the H01 affinity behavior are unchanged. No producer/consumer queue was
introduced.

## Correctness Contract

- FASTQ records and paired ends may never be split across chunks.
- Input-read order and chunk identifiers remain deterministic.
- Timing-independent final metrics, splice junctions, and gene counts must
  match the H01 runtime parent in every measured pair.
- An unsorted-BAM differential check must match after canonical record sorting.
- The 63-to-64-thread boundary, explicit override, long-read reserve, invalid
  minimum, zero-end, and 32-bit-overflow cases require sanitizer-backed tests.
- H01 affinity tests and the complete BlackSTAR index and insertion gates must
  continue to pass.

## Benchmark Contract

- Public corpus: 12,768,316 paired 76-base ENCODE reads pinned by
  `extras/benchmarks/public_alignment_fixture.tsv`.
- Index: public GRCh38 with Ensembl 114 annotations.
- Storage: compute-node local NVMe; OpenMP binding variables unset.
- Primary mode: uncompressed, mapping-only, gene counts enabled, 96 logical
  CPUs from 48 physical cores.
- Primary decision: three seeded order-balanced H01/A02 pairs after a recorded
  five-minute quiet-system gate.
- Generalization: three order-balanced full-corpus gzip-input pairs under the
  same gate and one 2,000,000-pair unsorted-BAM canonical differential check.
- Threshold support: one 64-thread candidate run bracketed by two H01 controls.
- Acceptance: at least 2 percent median end-to-end improvement, no more than
  5 percent peak-RSS increase, arm CV no greater than 3 percent, and exact
  correctness in every pair.

## Diagnostic Results

The diagnostic build processed the full public corpus from uncompressed FASTQ
with the same timing-independent outputs as the qualified release.

| Metric | Result |
| --- | ---: |
| End-to-end wall time | 70.92 s |
| Measured mapping interval | 39.8175 s |
| Active workers | 96/96 |
| Chunks per active worker | 1-2 |
| Input-lock hold | 19.7189 s |
| Input-lock occupied fraction | 49.52% |
| Aggregate mapping concurrency | 71.28 equivalent CPUs |
| Active-worker finish spread | 12.93 s |
| Per-worker read-count CV | 18.28% |

The lock fraction and completion tail justified testing finer scheduling units,
but did not themselves imply an achievable speedup because the stages overlap.

## Candidate Results

The exact candidate binary embedded commit
`a438a4f55b855c42c04c89cc52bbc7f082547620` and had SHA-256
`9d589256ed3421e67c6aa1788641466c20099ca607d62fba5895a8476a837028`.

### Primary Uncompressed Gate

| Metric | H01 | A02 |
| --- | ---: | ---: |
| Median wall time | 70.88 s | 61.47 s |
| Wall-time CV | 0.7880% | 0.2522% |
| Median peak RSS | 40,402,944 KiB | 37,666,816 KiB |

- Median paired improvement: **13.4143 percent**.
- Paired bootstrap 95 percent interval: **13.0502 to 14.4110 percent**.
- Median peak-RSS change: **-6.7721 percent**.
- Correctness: **3 of 3 pairs passed**.

The 64-thread support bracket measured 71.04 and 72.06 seconds for the legacy
controls and 64.98 seconds for the 1 MB candidate, a 9.1824 percent reduction
from the control midpoint. This is threshold support, not a replicated speed
claim.

### Compressed-Input Gate

| Metric | H01 | A02 |
| --- | ---: | ---: |
| Median wall time | 71.68 s | 65.37 s |
| Wall-time CV | 0.7270% | 0.3710% |

- Median paired improvement: **8.7394 percent**.
- Paired bootstrap 95 percent interval: **8.7054 to 10.4327 percent**.
- Median peak-RSS change: **-6.7634 percent**.
- Correctness: **3 of 3 pairs passed**.

### BAM Safety Gate

The 2,000,000-pair unsorted-BAM comparison passed all five checks: final
timing-independent metrics, junctions, gene counts, BAM presence, and canonical
BAM records. Both canonical BAM streams had SHA-256
`e3f67bccb149277f6f9673e204071b80afab5021c8182028ec17678cdbc750ac`.
The observed 51.28 versus 46.62 second wall times are descriptive only because
this safety check used one fixed-order comparison.

Compact public evidence is recorded in
`docs/architecture/evidence/alignment-A02-adaptive-chunks-20260723.tsv`.

## Decision

- Outcome: accepted and promoted in `blackstar.2`
- Reason: the smallest measured dispatch change passed uncompressed and
  compressed end-to-end performance gates, reduced memory use, preserved all
  measured outputs, and avoided a new queue or parser implementation.
- Accepted commit: `a438a4f55b855c42c04c89cc52bbc7f082547620`
- Supersedes: the A02 state of "diagnostics complete; implementation next"
- Follow-up: A02b profiled and rejected the producer/consumer redesign; A05,
  A06, and Q01 then completed the cumulative path. Additional thread counts and
  long-read workloads remain release-promotion caveats.

## Limitations

- Replicated performance evidence covers 96 threads; 64 threads has one
  bracketed support measurement. Lower thread counts intentionally keep legacy
  behavior.
- The primary corpus is paired short-read bulk RNA-seq on one dual-socket host.
- The BAM run is a correctness gate, not a BAM performance claim.
- A02 improves scheduling granularity but does not remove serialized parsing.
- This change remains absent from `blackstar.1` and is accepted in
  `blackstar.2` after Q01 cumulative qualification.

## Plain-Language Takeaway

High-thread STAR gave each worker too much input at once, so the last workers
finished unevenly. BlackSTAR now uses smaller, still record-safe chunks only at
64 or more threads. On the full public workload this reduced ordinary alignment
time by 13.4 percent for uncompressed input and 8.7 percent for gzip input,
while using less memory and producing the same measured biological outputs.

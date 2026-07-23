# A05: NUMA-Aware Private Genome Placement

## Status

- State: accepted Labs candidate; not released
- Qualified-release ancestor: `821457378fa38bfb23b061b8f11ee0a09431dda7`
- Runtime parent: A02 commit `a438a4f55b855c42c04c89cc52bbc7f082547620`
- Implementation commit: `eb54b0b94e488b95f7e0adaa6a21961e7b9f5306`
- Accepted commit: `91de89210ed936d162ef6f7ad69984b0752e1641`
- Opened: 2026-07-23
- Decided: 2026-07-23

## Hypothesis

On a multi-node NUMA host, loading a private STAR genome under the inherited
default memory policy can concentrate pages near the loading thread. Linux
automatic NUMA balancing then migrates pages while many mapping workers access
the index. Applying an interleave policy before private index allocation should
distribute pages predictably, reduce migration and fault work, and improve
high-thread alignment without changing mapping semantics.

## Scope

A05 adds `--genomeLoadNumaPolicy Auto|Default|Interleave`.

`Auto`, the default, applies interleave only when all of these conditions hold:

- `--runMode alignReads`;
- `--genomeLoad NoSharedMemory`;
- at least 64 mapping threads;
- Linux exposes at least two process-allowed memory nodes; and
- the caller has not supplied a bind, preferred, local, or interleave policy.

An inherited bind, preferred, or local policy is retained. An inherited
interleave policy, including its node subset, is retained. `Default` always
leaves the inherited policy unchanged. `Interleave` explicitly requires
interleaving across the process-allowed nodes and fails clearly if it cannot be
activated.

The implementation uses Linux `get_mempolicy` and `set_mempolicy` syscalls
directly. It adds no `libnuma` build or runtime dependency. Unsupported systems,
blocked policy queries, lower thread counts, and shared-genome modes fall back
without changing the process policy under `Auto`.

Every alignment run records the requested and effective policy, process-allowed
node count, inherited policy and node count, status, and decision reason in
`Log.out`.

## Correctness Contract

- The genome, suffix array, mapping kernels, scoring, and output ordering are
  unchanged.
- Timing-independent final metrics, splice junctions, and gene counts must
  match A02 in every performance pair.
- A 2,000,000-pair unsorted-BAM comparison must match after canonical record
  sorting.
- Below-threshold `Auto` must match explicit `Default`.
- Caller-supplied bind and interleave subsets must be retained.
- Shared loading must retain default behavior under `Auto`; explicit
  `Interleave` with shared loading must fail before index allocation.
- The selector must compile and run without Linux NUMA APIs.

## Benchmark Contract

- Public corpus: 12,768,316 paired 76-base ENCODE reads.
- Index: public GRCh38 with Ensembl 114 annotations.
- Storage: compute-node local SSD.
- Runtime parent: exact accepted A02 binary.
- Candidate: exact commit
  `91de89210ed936d162ef6f7ad69984b0752e1641`, SHA-256
  `34cd427eb9942adc9c188fef417d02840edfca8e2761861e7ef6e53863d40be2`.
- Primary mode: uncompressed, mapping only, gene counts enabled, 96 logical
  CPUs.
- Primary decision: three seeded order-balanced A02/A05 pairs after a recorded
  five-minute quiet-system gate.
- Generalization: five full-corpus gzip-input pairs, a replicated 64-thread
  boundary test, and a 2,000,000-pair canonical BAM differential check.
- Acceptance: at least 2 percent median wall-time improvement, no more than
  5 percent peak-RSS increase, arm CV no greater than 3 percent for the primary
  gate, and exact correctness in every pair.

## Diagnostic Attribution

The post-A02 diagnostic run already reached 94.16 equivalent mapping CPUs and
lost only 0.75 worker-equivalents to input-lock wait. This rejected a
producer/consumer queue as the next change.

A single instrumented external-interleave screen reduced mapping interval from
34.6260 to 23.2992 seconds, instrumented wall time from 69.93 to 60.76 seconds,
and minor faults from 18.02 million to 9.46 million. That one-run screen
supported the mechanism but was not used as an accepted speed claim.

## Primary Uncompressed Gate

| Metric | A02 | A05 |
| --- | ---: | ---: |
| Median wall time | 74.01 s | 57.35 s |
| Wall-time CV | 0.6274% | 0.8765% |
| Median peak RSS | 37,695,488 KiB | 37,646,336 KiB |

- Median paired improvement: **22.5105 percent**.
- Paired bootstrap 95 percent interval: **21.3818 to 23.5588 percent**.
- Median peak-RSS change: **-0.1304 percent**.
- Correctness: **3 of 3 pairs passed**.
- Policy: all candidate runs recorded default inheritance followed by
  interleave across eight allowed nodes.

This is the accepted A05 performance result.

## 64-Thread Activation Boundary

| Metric | A02 | A05 |
| --- | ---: | ---: |
| Median wall time | 74.62 s | 63.97 s |
| Wall-time CV | 2.9608% | 0.2280% |

- Median paired improvement: **14.2723 percent**.
- Paired bootstrap 95 percent interval: **11.2874 to 15.9863 percent**.
- Median peak-RSS change: **-0.0402 percent**.
- Correctness: **3 of 3 pairs passed**.
- All candidate runs activated interleave across eight allowed nodes.

The exact accepted binary therefore passes at the automatic 64-thread
activation boundary as well as at 96 threads.

## Compressed-Input Generalization

| Metric | A02 | A05 |
| --- | ---: | ---: |
| Median wall time | 78.31 s | 62.25 s |
| Wall-time CV | 4.2820% | 1.2459% |

- Median paired improvement: **20.6697 percent**.
- Paired bootstrap 95 percent interval: **12.6284 to 22.6516 percent**.
- Correctness: **5 of 5 pairs passed**.
- All five paired improvements were positive.

The compressed result is supportive, not a formal accepted performance gate.
The A05 arm passed the 3 percent variability threshold, but the A02 control did
not. The gate was not changed after observing that control variance.

## Safety And Fallback Gates

- The 2,000,000-pair unsorted-BAM comparison passed timing-independent final
  metrics, junctions, gene counts, BAM presence, and canonical BAM records.
  Both canonical streams had SHA-256
  `e3f67bccb149277f6f9673e204071b80afab5021c8182028ec17678cdbc750ac`.
  Its single-pair timing is not a performance claim.
- At 48 threads, `Auto` selected `Default` with reason
  `below-thread-threshold` and matched explicit `Default`.
- A one-node inherited bind remained `Bind`; a two-node inherited interleave
  remained limited to those two nodes. Both produced the same measured outputs
  as the ordinary policy run.
- `LoadAndRemove` selected `Default`, matched private-load outputs, and removed
  its shared segment. Explicit `Interleave` with shared loading exited before
  index allocation with reason `shared-genome`.
- The selector unit passed when compiled with Linux platform macros disabled.
- The linked binary has no `libnuma` dependency.

## Decision

- Outcome: accepted as a Labs candidate; not promoted to a release
- Reason: the primary full-corpus gate improved median wall time by 22.51
  percent with stable replicates, unchanged memory use, and exact measured
  outputs. Threshold, inherited-policy, shared-mode, portability, and BAM
  safety checks passed.
- Accepted commit: `91de89210ed936d162ef6f7ad69984b0752e1641`
- Runtime parent: A02
- Follow-up: profile the cumulative H01+A02+A05 stack before selecting any
  additional CPU kernel. Do not infer a remaining parser opportunity from lock
  occupancy alone.

## Limitations

- Accepted performance evidence is from one Linux host with two sockets, eight
  exposed NUMA memory domains, local SSD input, and one paired short-read bulk
  RNA-seq corpus.
- A05 intentionally does not alter shared-genome placement.
- Interleave remains active for subsequent process allocations, matching the
  externally screened policy; index-only scoping is a separate experiment.
- The compressed-input effect is positive in all five pairs but fails the
  predeclared arm-CV gate because the A02 control is unstable.
- A05 is absent from `2.7.11b-blackstar.1` until cumulative qualification and a
  deliberate promotion.

## Plain-Language Takeaway

STAR's workers were fast, but on a large NUMA machine the genome pages started
in an uneven place and Linux spent substantial work moving them. A05 spreads
private genome allocations across the memory nodes before mapping starts. On
the accepted public workload, this made the already improved A02 build another
22.5 percent faster without changing the measured biological outputs.

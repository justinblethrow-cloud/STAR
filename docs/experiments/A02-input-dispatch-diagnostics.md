# A02: Input Dispatch Diagnostics

## Status

- State: diagnostics complete; bounded implementation next
- Parent commit: `821457378fa38bfb23b061b8f11ee0a09431dda7`
- Diagnostic commit: `544237f25af23623346d8c305aebd96d0d798786`
- Opened: 2026-07-23
- Diagnostic decision: 2026-07-23

## Hypothesis

STAR's shared input mutex, serial FASTQ parsing inside that critical section,
and coarse input chunks may prevent the alignment workers from receiving work
quickly and evenly enough to use the requested CPUs. This is a hypothesis to
measure, not a conclusion drawn from aggregate CPU utilization.

## Scope

The first phase adds compile-time-only diagnostics to the qualified release
source. It does not include A01 and does not change parsing, scheduling,
alignment, output, or index behavior. A diagnostic build records private
per-thread counters and emits them after all workers join. A normal build has
no diagnostic fields, clock reads, or log output.

The second phase will implement only the mechanism supported by those
measurements: a faster streaming parser, finer scheduling units, a bounded
producer/consumer queue, or no dispatch change if the hypothesis is rejected.

## Correctness Contract

- The diagnostic macro must be absent from normal release builds.
- Diagnostic and qualified-release binaries must produce identical canonical
  alignments, final metrics, junctions, and counts on the public fixture.
- Per-thread read totals must sum to STAR's final input-read count.
- Chunk identifiers remain unique and input order semantics remain unchanged.
- No A01 source may be present in the A02 parent.

## Measurement Contract

- Public fixture: first 2,000,000 pairs from the A00 ENCODE corpus, then the
  full public corpus for any candidate that passes the diagnostic gate.
- Mode: uncompressed mapping-only input on local NVMe.
- Threads: 96 logical CPUs from 48 physical cores plus SMT siblings.
- Per-thread metrics: input-lock wait, input-lock hold, chunks, reads, bytes,
  mapping time, completion time, and final idle tail.
- Global checks: wall time, CPU time, peak RSS, allocation mask, and exact
  output comparison.
- Profiling overhead is acceptable for attribution but never for a release
  performance claim.

## Decision Tree

1. If serialized lock hold dominates wall time, optimize or partition parsing.
2. If workers receive few coarse chunks or the final tail is large, reduce
   scheduling granularity without changing record boundaries.
3. If parsing and mapping cannot overlap, test a bounded producer/consumer
   queue with explicit ownership and backpressure.
4. If dispatch is not material, archive A02 and resume profile-led kernel work.

See [F16](../architecture/diagrams/svg/F16-input-dispatch-diagnostics.svg) for
the observed source structure, current evidence, and attribution gate.

## Results

The compile-time diagnostic build processed the full 12,768,316-pair public
corpus from uncompressed FASTQ on compute-node local NVMe with OpenMP binding
unset.
It produced the same timing-independent metrics, junctions, and counts as the
qualified release.

| Metric | Result |
| --- | ---: |
| End-to-end wall time | 70.92 s |
| Measured mapping interval | 39.8175 s |
| Active workers | 96/96 |
| Chunks per active worker | 1-2 |
| Input-lock hold | 19.7189 s |
| Input-lock occupied fraction | 49.52% |
| Sum of input-lock wait | 462.37 thread-s |
| Sum of mapping work | 2838.04 thread-s |
| Aggregate mapping concurrency | 71.28 equivalent CPUs |
| Active worker finish spread | 12.93 s |
| Per-worker read-count CV | 18.28% |

The lock fraction and completion tail are both material relative to the
39.82-second mapping interval. They support testing finer scheduling units and
a bounded producer/consumer path. They do not imply that 19.72 or 12.93
seconds can be subtracted from wall time because input preparation overlaps
mapping and the two measurements overlap each other.

## Decision

- Outcome: diagnostic gate passed; implementation experiment authorized
- Reason: corrected per-thread measurements identify material serialized input
  occupancy and tail imbalance without relying on aggregate CPU inference.
- Accepted commit or archived branch: diagnostic commit retained on
  `labs/a02-input-dispatch`; no runtime change accepted yet
- Supersedes: none
- Follow-up: implement the smallest bounded dispatch change from the clean H01
  parent, then require paired performance and exact-output gates.

## Plain-Language Takeaway

STAR activates every requested alignment thread, but they take turns preparing
large chunks behind one lock and finish unevenly. The full-corpus diagnostic
shows enough serialized dispatch and tail imbalance to justify a measured
redesign, while making no speed claim before that candidate exists.

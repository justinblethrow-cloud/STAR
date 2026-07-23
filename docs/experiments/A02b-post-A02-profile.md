# A02b: Post-A02 Input Pipeline Profile

## Status

- State: rejected after profiling; no runtime source stacked
- Runtime parent: A02 commit `a438a4f55b855c42c04c89cc52bbc7f082547620`
- Diagnostic commit: `b26311f242b5eb42108ce0286eeb52900f919590`
- Opened: 2026-07-23
- Decided: 2026-07-23

## Hypothesis

After A02 reduced high-thread input chunks to 1 MB, the remaining serialized
FASTQ parser and input mutex might still leave enough workers idle to justify a
producer/consumer queue.

## Scope

The diagnostic build added compile-time-only per-thread measurements. It
recorded thread CPU time, input-mutex wait and hold time, parser and mapping
windows, chunk and read counts, context switches, affinity, and completion
spread. The runtime algorithm, chunk boundaries, mapping kernels, and output
semantics were unchanged.

No queue implementation was added before profiling. This avoided expanding
ownership, cancellation, backpressure, paired-record, and error-propagation
semantics without evidence that the scheduling headroom was material.

## Benchmark Contract

- Public corpus: 12,768,316 paired 76-base ENCODE reads.
- Index: public GRCh38 with Ensembl 114 annotations.
- Storage: compute-node local SSD.
- Input: uncompressed FASTQ.
- Threads: 96 logical CPUs with OpenMP binding variables unset.
- Output: mapping only with gene counts.
- Correctness: timing-independent final metrics, splice junctions, and gene
  counts must match the accepted A02 binary.

## Results

The diagnostic run used 6,689 record-safe chunks and all 96 workers performed
mapping work.

| Metric | Result |
| --- | ---: |
| End-to-end instrumented wall time | 69.93 s |
| Measured mapping interval | 34.6260 s |
| Aggregate mapping concurrency | 94.1601 equivalent CPUs |
| Aggregate thread-CPU concurrency | 94.5899 equivalent CPUs |
| Input-lock hold time | 15.9050 s |
| Input-lock occupied fraction | 45.93% |
| Aggregate input-lock wait | 25.8576 worker-seconds |
| Input-lock wait as interval capacity | 0.7468 workers |
| Active-worker finish spread | 0.3593 s |

The occupied fraction is not the removable wall-time fraction because parsing
overlaps mapping. The decisive measurements are 94-way mapping concurrency,
less than one worker-equivalent lost to lock wait, and a 0.36-second completion
tail. They leave roughly one percent of scheduling capacity available to a
queue, before accounting for queue overhead.

Timing-independent final metrics, junctions, and gene counts matched A02.

## Decision

- Outcome: rejected
- Reason: A02 already removed the material worker-starvation and completion-tail
  problem. A producer/consumer queue would add substantial lifecycle and
  correctness surface for less than approximately one percent of observed
  scheduling headroom.
- Source disposition: retain the compile-time diagnostic branch and evidence;
  do not stack its instrumentation or a queue into the runtime candidate.
- Follow-up: investigate memory placement, which the same profiling session
  identified as a materially larger opportunity.

## NUMA Screen

A single external `interleave=all` diagnostic screen reduced the measured
mapping interval from 34.6260 to 23.2992 seconds and instrumented wall time from
69.93 to 60.76 seconds. Minor faults fell from 18.02 million to 9.46 million,
and memory was distributed across eight nodes.

This screen is mechanism evidence, not an accepted speed claim: it used one
instrumented run per policy. It justified A05's clean, paired implementation
test.

## Plain-Language Takeaway

The parser is still serial, but it is no longer keeping the mapping workers
meaningfully idle. Building a queue would add risk for little expected benefit.
The larger remaining problem was where the genome pages lived in NUMA memory.

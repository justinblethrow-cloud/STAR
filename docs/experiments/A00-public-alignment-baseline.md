# A00: Public Alignment Baseline

## Status

- State: corrected baseline and dispatch diagnostics complete
- Parent commit: `821457378fa38bfb23b061b8f11ee0a09431dda7`
- Harness lineage begins at: `67052a1a5ea98969c775c13c859d72f174812860`
- Opened: 2026-07-22
- Baseline measured: 2026-07-23

## Purpose

Historical profiling identified candidate alignment hot paths, but it used an
internal workload and an excluded BAM-threading prototype. A00 replaces that
evidence with a reproducible public baseline before any alignment optimization
is accepted.

## Public Corpus

- Reference: public GRCh38 FASTA and matching Ensembl release 114 GTF.
- Reads: ENCODE GM12878 paired-end public FASTQ pair, recorded by accession and
  checksum in the fixture manifest.
- Delta control: public GFP and glutathione S-transferase records plus
  deterministic synthetic reads.

The harness supports a bounded read subset for iteration and the full public
files for qualification.

## Modes

The completed baseline uses a mapping-only SAM sink to isolate input dispatch
and alignment kernels. Unsorted BAM, coordinate-sorted BAM, and Base versus
Delta comparisons remain secondary modes for experiments that target those
surfaces; they are not part of the A00 timing claim.

## Evidence

Each run records executable identity, commit, compiler, CPU and NUMA topology,
input SHA-256 values, command line, UTC interval, `/usr/bin/time -v`, STAR
logs, perf stat, optional perf record, I/O telemetry, and output checksums.

The primary kernel benchmark uses new STAR processes with a documented warm
filesystem cache. The pair driver hashes measured reads, runs both binaries on
an untimed small subset to warm and validate the index path, and then enforces
five quiet minutes before measured AB/BA pairs. Cold-cache end-to-end timing is
reported separately and explicitly evicts cache state between runs.

## Correctness

The initial oracle compares the qualified release binary with itself across
repeated runs and validates mapping metrics, normalized SAM/BAM records,
junctions, and counts. Experiments add immediate-parent differential checks.

## Historical Directional Evidence

An older internal profile attributed substantial self time to seed comparison,
full window-bin clearing, transcript copying and stitching, and suffix-array
access. This is prioritization evidence only and is not an A00 performance
baseline.

## Corrected Baseline

The public run used the qualified release executable, a full GRCh38 index,
12,768,316 public paired reads, 96 logical CPUs from 48 physical cores and
their SMT siblings, 500 GB RAM, and compute-node local NVMe storage.

The first A00/A01 series inherited `OMP_PROC_BIND=close` and
`OMP_PLACES=cores`. The linked OpenMP runtime bound the initial process thread
to one two-CPU SMT place before STAR spawned its pthread workers. All 96
workers inherited that same place. Those compressed and uncompressed release
medians, 1225.54 and 1199.38 seconds, are retained as historical diagnostics
but invalidated as alignment baselines. Their A01 performance differences are
invalidated for the same reason.

The corrected release-only series explicitly removed OpenMP binding and used
new processes with a documented warm filesystem cache:

| Input path | Release wall times (s) | Median (s) | Wall CV | Mean CPU use |
| --- | --- | ---: | ---: | ---: |
| gzip through `zcat` | 71.67, 72.36, 72.15 | 72.15 | 0.491% | 3493% |
| uncompressed FASTQ | 71.31, 71.53, 71.35 | 71.35 | 0.164% | 4141% |

All six runs produced the same gene-count SHA-256
`de5f085b7ab1d90b418063c64a0191d1c4370d0946c460e2941fca5198b892ac`
and splice-junction SHA-256
`d84efe0c839d0e883bbcf1dbc3341fb35cec648c61f35315fa48abc7ef6e5481`.
The 1.12% compressed-versus-uncompressed median difference is descriptive,
not an optimization claim.

## Directional Profile

A compile-time-instrumented `gprof` build of the exact release commit processed
2,000,000 public read pairs from uncompressed FASTQ. This historical run also
inherited the affinity-pinned environment. Its 268-second mapping interval,
289-second end-to-end time, and 188% CPU use are invalid for operational
performance or occupancy claims. The sampled symbol shares remain limited
directional evidence about work executed in that pathological configuration:

| Symbol | Sampled CPU time |
| --- | ---: |
| `compareSeqToGenome` | 17.44% |
| `Transcript` copy constructor | 14.00% |
| `Genome::SAvalue` | 11.62% |
| `ReadAlign::stitchPieces` | 10.39% |
| `extendAlign` | 9.72% |
| `ReadAlign::assignAlignToWindow` | 9.09% |

`ReadAlign::createExtendWindowsWithAlign`, the narrow path initially used to
motivate A01, accounted for only 0.98% of sampled self time. The profile cannot
establish ordinary multi-core cost: `gprof` call counts race between pthreads,
mutex wait time is not represented as active CPU samples, and the affinity
confound changed the workload's scheduling and cache behavior.

## Dispatch Observation

Source inspection shows that every mapping thread acquires the same
`mutexInRead`, then keeps it while parsing and copying an entire input chunk.
Only after releasing that lock does the worker map its chunk. The default
30 MB input buffer is split between mates, producing coarse scheduling units.

A corrected full-corpus A02 diagnostic activated all 96 workers. Its measured
mapping interval was 39.8175 seconds; the input lock was occupied for 19.7189
seconds, or 49.52% of that interval. Summed worker mapping time corresponded
to 71.28 effective CPUs, and active worker completion spanned 12.93 seconds.

These measurements establish input dispatch and tail balance as material
candidates. They do not establish an achievable speedup: lock hold can overlap
mapping, and reducing it can move the bottleneck elsewhere. A02 therefore
proceeds with bounded implementation experiments and immediate rollback.

## Decision

- Outcome: corrected baseline accepted for Labs experiment decisions
- Release effect: none
- Historical effect: the affinity-pinned A00 and A01 performance measurements
  are invalidated; A01 correctness evidence remains valid.
- Follow-up: H01 affinity recovery is accepted in Labs but not yet promoted;
  A02 diagnostics are complete and dispatch implementation is next.

# A01: Touched Window-Bin Reset

## Status

- State: excluded; historical performance effect invalidated
- Parent commit: `821457378fa38bfb23b061b8f11ee0a09431dda7`
- Experiment commit: `67052a1a5ea98969c775c13c859d72f174812860`
- Opened: 2026-07-22
- Decided: 2026-07-23

## Hypothesis

For each read, upstream STAR resets both genome-wide window-bin arrays with two
full `memset` calls. Most reads populate only a sparse set of those bins.
Remembering bins on their first transition away from the empty sentinel and
resetting only that set should reduce memory traffic in `stitchPieces` without
changing window construction or alignment selection.

## Scope

The experiment changes only the lifecycle of `ReadAlign::winBin`. All reads,
writes, window identifiers, merge order, scoring, output formats, and public
parameters remain unchanged. `TouchedWinBins` is private per `ReadAlign`, so it
adds no shared mutable state or synchronization.

Rollback consists of restoring the two per-read `memset` calls and direct bin
writes. The qualified release is unchanged in its separate worktree.

## Correctness Contract

- Unit oracle: first-touch recording, duplicate writes, strand separation,
  reset, and tracker reuse under address and undefined-behavior sanitizers.
- Differential oracle: canonical SAM records, timing-independent final-log
  fields, and splice-junction output against the qualified release binary at
  one and four threads.
- Determinism: two candidate runs must produce the same canonical output.
- Insertion compatibility: the complete Full, Overlay, and Delta hardening
  suite must pass because all three modes reach the common alignment path.
- Mutation invariant: every write to a live `winBin` slot passes through
  `TouchedWinBins::set`; read-only accesses remain direct.

## Benchmark Contract

- Fixture: the A00 public paired-end corpus and deterministic 250,000-pair
  subset recorded in `public_alignment_fixture.tsv`.
- Primary mode: mapping-only output to isolate the alignment path.
- Secondary modes: unsorted BAM, coordinate-sorted BAM, and Base versus Delta.
- Comparison: randomized paired runs against the qualified release executable.
- Cache state: new processes with read files hashed and both binaries run on an
  untimed subset before the five-minute quiet gate.
- Acceptance: at least 2 percent median end-to-end improvement, or at least
  15 percent improvement in the targeted stage without end-to-end regression.
- Resources: peak RSS may not increase by more than 5 percent.
- Environment: the five-minute quiet-system gate must pass before each series.

## Implementation

`TouchedWinBins` stores an encoded `(bin, strand)` only when a slot changes from
`uintWinBinMax`. Subsequent writes to the same slot preserve normal overwrite
semantics but do not append duplicates. At the start of the next read, the
tracker restores exactly those slots to `uintWinBinMax` and clears its private
list. The two arrays are still fully initialized once in the constructor.

See [F15](../architecture/diagrams/svg/F15-touched-window-bins.svg) for the
state transition and preserved invariants.

## Results

Correctness gates passed on 2026-07-22:

- sanitizer unit test;
- qualified-release versus candidate differential mapping at one and four
  threads over 512 sparse, multimapping, reverse-strand, and unmapped reads;
- candidate repeated-run equivalence at both thread counts;
- focused junction test; and
- genome-insert hardening, including cross-thread idempotence, namespace and
  manifest validation, package relocation, and corruption rejection.

Two public three-pair series passed every differential correctness comparison.
Their performance results are retained for auditability but are not valid
ordinary multi-core effect estimates:

| Input path | Baseline median (s) | Candidate median (s) | Median change | Paired bootstrap 95% CI |
| --- | ---: | ---: | ---: | ---: |
| gzip through `zcat` | 1225.54 | 1231.49 | -0.86% | -0.95% to -0.41% |
| uncompressed FASTQ | 1199.38 | 1206.12 | -0.56% | -1.05% to -0.17% |

Negative improvement means the candidate was slower in those runs. Baseline
and candidate coefficients of variation were below 0.33%, and peak RSS changed
by less than 0.02%.

However, both binaries inherited `OMP_PROC_BIND=close` and
`OMP_PLACES=cores`. All 96 pthread mapping workers were consequently pinned to
the same one-core SMT place. This materially changed scheduling, cache
behavior, and the relative cost of per-read bookkeeping. The table cannot
support an operational A01 speed or regression claim on a correctly unbound
96-CPU run. A clean rerun would be required to estimate that effect.

The affinity-pinned A00 directional profile also found that
`ReadAlign::createExtendWindowsWithAlign`, the narrow path initially used to
motivate this experiment, represented only 0.98% of sampled self time. That
supports deprioritization, but it does not quantify ordinary-path impact.

## Decision

- Outcome: excluded from all later candidates
- Reason: correctness passed, but no valid performance benefit was
  demonstrated, the targeted path was small in the directional profile, and
  later experiments must remain based on the qualified release rather than
  stack unqualified source.
- Accepted commit or archived branch: no accepted commit; retained on
  `labs/visual-baseline` as an archived experiment
- Supersedes: none
- Follow-up: do not stack A01 into later candidates. Revisit only with a clean
  unbound paired benchmark if later profiling makes this path material.

## Plain-Language Takeaway

The candidate correctly remembered and reset only the scratch-table entries a
read used. Its performance series ran under a severe CPU-affinity confound, so
we cannot say whether it helps or hurts normal multi-core alignment. It remains
excluded because it has no demonstrated benefit and later work needs a clean
parent.

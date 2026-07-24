# A06: Transcript Recursion Copy Elision

## Status

- State: accepted in `2.7.11b-blackstar.2`
- Qualified-release ancestor: `821457378fa38bfb23b061b8f11ee0a09431dda7`
- Runtime source parent: A05 commit
  `91de89210ed936d162ef6f7ad69984b0752e1641`
- Benchmark-harness parent:
  `abaedd1477711010881d4729eb14dba1b42e1615`
- Implementation and accepted source commit:
  `6032393155317b17fef1750672f1da6770ae6042`
- Opened: 2026-07-23
- Decided: 2026-07-23
- Cumulative qualification: Q01 passed 2026-07-24

## Hypothesis

`stitchWindowAligns` previously accepted its recursive `Transcript` state by
value. Every include and exclude edge therefore invoked the copy constructor,
even though the exclude edge only observes and forwards the existing state.
Passing the state by const reference, then copying only for a mutating include
edge and terminal finalization, should remove redundant copies without changing
the recursive search tree or alignment semantics.

This is narrower than the roadmap's original compact rollback-state proposal.
The corrected cumulative profile showed that a conservative ownership change
could test the same bottleneck with substantially less implementation risk.

## Scope

A06 changes only `stitchWindowAligns.cpp` and its declaration:

- recursive input state is a `const Transcript&`;
- an include edge creates one private mutable `Transcript` copy before
  stitching the candidate alignment;
- an exclude edge forwards the unchanged input state by reference; and
- terminal finalization creates one private copy before extension, annotation,
  scoring, and output selection.

The recursion order, branch conditions, score calculation, `WAincl` updates,
transcript finalization, duplicate filtering, and output ordering are
unchanged. A06 adds no parameter, file format, dependency, persistent state, or
runtime mode.

## Correctness Contract

- Include and exclude edges must visit the same search states in the same
  order.
- A branch may mutate only its private include or finalization copy.
- Timing-independent final metrics, splice junctions, and gene counts must
  match A05 in every performance pair.
- A 2,000,000-pair unsorted-BAM comparison must match after canonical record
  sorting.
- Existing transcript-initialization and junction-alignment sanitizer tests
  must pass.

## Benchmark Contract

- Public corpus: 12,768,316 paired 76-base ENCODE reads.
- Index: public GRCh38 with Ensembl 114 annotations.
- Storage: compute-node local SSD.
- Runtime parent: exact accepted A05 binary, SHA-256
  `34cd427eb9942adc9c188fef417d02840edfca8e2761861e7ef6e53863d40be2`.
- Candidate: exact commit
  `6032393155317b17fef1750672f1da6770ae6042`, SHA-256
  `f1206614f6f4984353f96224f7e414a4a803703af448eb2e4d7296704e5fe632`.
- Primary mode: uncompressed, mapping only, gene counts enabled, 96 logical
  CPUs.
- Initial gate: three seeded order-balanced pairs after a recorded five-minute
  quiet-system qualification.
- Small-effect confirmation: five new seeded order-balanced pairs on an
  administratively dedicated host.
- Generalization: three full-corpus gzip-input pairs and a 2,000,000-pair
  canonical BAM differential check.
- Acceptance: at least 2 percent median wall-time improvement, no more than
  5 percent peak-RSS increase, a positive paired bootstrap interval over at
  least five pairs for the observed small effect, and exact correctness.

The initial run was planned around a 5 percent effect and therefore used three
pairs. Its observed 2.47 percent gain remained above the practical gate, but
the experiment was extended with five fresh pairs to satisfy the stricter
small-effect rule. This confirmation was not used to relax any threshold.

## Profile Attribution

The cumulative A05 profile identified `Transcript::Transcript(const
Transcript&)` as the second-largest exclusive CPU consumer:

| Metric | A05 | A06 |
| --- | ---: | ---: |
| Total sampled CPU | 3229.911 s | 2904.594 s |
| Transcript copy-constructor CPU | 351.957 s | 225.353 s |
| Transcript copy-constructor share | 10.90% | 7.76% |

- Exclusive copy-constructor CPU fell **35.97 percent**.
- Total sampled CPU fell 10.07 percent in the two diagnostic runs.
- The post-change function signature in the profile confirms that recursive
  calls received `const Transcript&`.

These are single symbolized-profile runs. They support the intended mechanism
but are not accepted wall-time claims; the paired optimized-binary results
below establish performance.

## Initial Quiet-Qualified Gate

| Metric | A05 | A06 |
| --- | ---: | ---: |
| Median wall time | 57.39 s | 55.97 s |
| Wall-time CV | 0.3789% | 0.1342% |

- Median paired improvement: **2.4743 percent**.
- Paired bootstrap 95 percent interval: **1.8587 to 2.7517 percent**.
- Median peak-RSS change: **-0.0054 percent**.
- Correctness: **3 of 3 pairs passed**.

This run passed the predeclared practical, variability, resource, and
correctness gates after the recorded five-minute quiet-system qualification.

## Five-Pair Small-Effect Confirmation

| Metric | A05 | A06 |
| --- | ---: | ---: |
| Median wall time | 57.39 s | 56.11 s |
| Wall-time CV | 0.7380% | 0.4275% |

- Median paired improvement: **2.2914 percent**.
- Paired bootstrap 95 percent interval: **1.5159 to 3.7927 percent**.
- Median peak-RSS change: **-0.0109 percent**.
- Correctness: **5 of 5 pairs passed**.

The host was administratively dedicated to this benchmark, so the redundant
quiet telemetry wait was explicitly waived. Run order, warmups, settle time,
replicate count, output comparison, and all acceptance thresholds were
retained.

## Compressed-Input Generalization

| Metric | A05 | A06 |
| --- | ---: | ---: |
| Median wall time | 64.28 s | 60.77 s |
| Wall-time CV | 1.9561% | 0.5402% |

- Median paired improvement: **4.5893 percent**.
- Paired bootstrap 95 percent interval: **2.7680 to 6.4089 percent**.
- Median peak-RSS change: **-0.0054 percent**.
- Correctness: **3 of 3 pairs passed**.

This is supportive generalization rather than the primary accepted claim. It
shows that decompression overhead did not erase the benefit.

## BAM Safety Gate

The 2,000,000-pair unsorted-BAM comparison passed timing-independent final
metrics, junctions, gene counts, BAM presence, and canonical BAM records. Both
canonical streams had SHA-256
`e3f67bccb149277f6f9673e204071b80afab5021c8182028ec17678cdbc750ac`.
Its single-pair timing is not a performance claim.

## Sanitizer Gate

An end-to-end ASan and UBSan build mapped 250,000 public read pairs against the
full GRCh38 index with 16 threads. It completed 68,700 splice events with
abort-on-error enabled, exit status zero, and empty sanitizer stderr. The
focused transcript-initialization and junction-alignment sanitizer tests also
passed.

## Decision

- Outcome: accepted and promoted in `blackstar.2`
- Reason: the five-pair confirmation improved median end-to-end wall time by
  2.29 percent, its paired interval excluded zero, memory use was unchanged,
  and all measured outputs matched. The independent compressed-input and BAM
  gates and the end-to-end sanitizer run passed, while profiling confirmed
  that the intended copy cost fell.
- Accepted source commit:
  `6032393155317b17fef1750672f1da6770ae6042`
- Runtime parent: A05
- Follow-up: A09 evaluated and rejected cumulative LTO and PGO; Q01 then
  qualified the source stack for promotion in `blackstar.2`.

## Limitations

- Accepted evidence is from one Linux host with two sockets, eight exposed
  NUMA memory domains, local SSD input, and one paired short-read bulk RNA-seq
  corpus.
- The benefit depends on how often a workload explores exclude edges and on
  the size and contents of `Transcript` state.
- The profile comparison is diagnostic single-run evidence.
- The compressed-input result has three pairs and remains supportive.
- A06 remains absent from `blackstar.1` and is accepted in `blackstar.2` after
  Q01 cumulative qualification.

## Plain-Language Takeaway

STAR was copying a large in-progress alignment record on recursive branches
that did not modify it. A06 keeps the unchanged branch as a reference and
copies only where mutation is required. On the public full-corpus workload,
that conservative two-file change removed about 36 percent of the measured
copy-constructor work and reduced end-to-end alignment time by about 2.3
percent without changing the measured outputs.

# A09: LTO and Profile-Guided Optimization

## Status

- State: rejected; no toolchain variant stacked
- Qualified-release ancestor: `821457378fa38bfb23b061b8f11ee0a09431dda7`
- Cumulative Labs source:
  `7b31a5fe5cb966c9146b99d5ca1ad81ea9c81cdb`
- Runtime source implementation: A06 commit
  `6032393155317b17fef1750672f1da6770ae6042`
- Opened: 2026-07-23
- Decided: 2026-07-23

## Hypothesis

GCC link-time optimization (LTO) or profile-guided optimization (PGO) may
improve the cumulative H01+A02+A05+A06 alignment stack by enabling
cross-translation-unit inlining, profile-informed layout, and better branch
decisions without changing STAR source or runtime semantics.

## Scope

A09 made no source change. It built three alternatives from the same clean
cumulative source with GCC 13.3.0:

- the normal optimized BlackSTAR binary;
- an LTO binary using `-flto=auto -fuse-linker-plugin`; and
- a PGO binary trained with one complete public alignment, then rebuilt with
  the resulting profile.

LTO and PGO were evaluated independently. Because each failed the practical
performance gate, they were not compounded.

## Correctness Contract

- Every candidate must use the exact same source and normal runtime command as
  its control.
- Timing-independent final metrics, splice junctions, and gene counts must
  match in every measured pair.
- PGO profile use must fail the build on missing profiles or profile/source
  mismatch.
- No candidate may change the index, output format, runtime parameters, or
  accepted source stack.

## Build Contract

The normal binary used the existing optimized build. LTO added:

```text
CXXFLAGSextra=-flto=auto -fuse-linker-plugin
LDFLAGSextra=-flto=auto -fuse-linker-plugin
```

PGO generation added:

```text
CXXFLAGSextra=-fprofile-generate=<profile-dir> -fprofile-update=atomic \
  -fprofile-reproducible=parallel-runs
LDFLAGSextra=-fprofile-generate=<profile-dir>
```

PGO use added:

```text
CXXFLAGSextra=-fprofile-use=<profile-dir> -Werror=missing-profile \
  -Werror=coverage-mismatch
LDFLAGSextra=-fprofile-use=<profile-dir>
```

The PGO-use build completed with the missing-profile and coverage-mismatch
warnings promoted to errors, establishing that GCC accepted the training data
for the exact source build.

## Binary Identity

| Build | SHA-256 | Bytes | Change from normal |
| --- | --- | ---: | ---: |
| Normal | `6b08a7925022c6a03813660c69d0b0bd1ed98a377ac9c1136dd987e3d4c8f85e` | 3,572,528 | - |
| LTO | `59f6a4b2f8733f15203f8b2dd5cfeeac956b53cf78e8fa57a3ed78c44b7a884f` | 2,948,544 | -17.47% |
| PGO generate | `e33ed686d601713428421d8a12bf518c7526ebdd7b51279ddcbce37146a16499` | 8,144,904 | training only |
| PGO use | `ba0298802a667ff02f2f07965045aa116ea67b920b588b6ff2652d70e4c56b87` | 2,781,080 | -22.15% |

The smaller production candidates confirm that both toolchain modes materially
changed code generation. Binary size is not an acceptance metric.

## Benchmark Contract

- Public corpus: 12,768,316 paired 76-base ENCODE reads.
- Index: public GRCh38 with Ensembl 114 annotations.
- Storage: compute-node local SSD.
- Mode: uncompressed, mapping only, gene counts enabled, 96 logical CPUs.
- Host: administratively dedicated benchmark node; the redundant quiet wait
  was explicitly waived in each contract.
- Design: two warmups followed by five seeded order-balanced pairs.
- Acceptance: at least 2 percent median wall-time improvement, positive paired
  bootstrap interval, no more than 5 percent peak-RSS increase, and exact
  measured outputs.

## LTO Result

| Metric | Normal | LTO |
| --- | ---: | ---: |
| Median wall time | 55.75 s | 55.32 s |
| Wall-time CV | 1.1876% | 1.2132% |

- Median paired improvement: **1.2143 percent**.
- Paired bootstrap 95 percent interval: **0.6462 to 3.2878 percent**.
- Median peak-RSS change: **+0.0054 percent**.
- Correctness: **5 of 5 pairs passed**.

LTO produced a repeatable positive effect and exact outputs, but the median
gain was below the predeclared 2 percent practical gate.

## PGO Training

The instrumented binary trained on the complete public alignment workload:

| Metric | Value |
| --- | ---: |
| Training wall time | 1:20:21 |
| Training user CPU | 457,535.72 s |
| Mean CPU utilization | 9,489% |
| Peak RSS | 37,676,568 KiB |
| Profile files | 163 |
| Profile footprint | 1.1 MiB |

The training run exited successfully. It intentionally optimized the same
mapping and gene-count path used by the acceptance benchmark.

## PGO Result

| Metric | Normal | PGO |
| --- | ---: | ---: |
| Median wall time | 55.89 s | 55.20 s |
| Wall-time CV | 1.1480% | 0.5499% |

- Median paired improvement: **1.1988 percent**.
- Paired bootstrap 95 percent interval: **0.5712 to 4.2735 percent**.
- Median peak-RSS change: **-0.0054 percent**.
- Correctness: **5 of 5 pairs passed**.

PGO also produced a repeatable positive effect and exact outputs, but its
1.20 percent median gain was below the practical gate and required an
80-minute training run.

## Reproducibility and Adoption

Other builders can reproduce PGO by compiling an instrumented binary, running
a representative training workload, and rebuilding with the profile. A
prebuilt PGO release binary would carry its optimized code without requiring
end users to train it.

The profile itself is not a durable portable artifact. It is coupled to the
exact source layout, compiler family and version, build flags, and training
workload. A maintainable public implementation would therefore provide a
pinned training recipe and let release builders regenerate the profile. It
should also avoid `-march=native` so a build is not silently specialized for
one host.

This experiment does not add such a public build target because the measured
gain failed the project's practical threshold.

## Decision

- Outcome: rejected; neither LTO nor PGO is stacked
- Reason: both candidates preserved exact outputs and had positive paired
  intervals, but their five-pair median gains were only 1.21 and 1.20 percent.
  Neither met the 2 percent practical performance gate.
- Cumulative source remains: H01+A02+A05+A06
- Follow-up: Q01 completed cumulative release-style technical qualification
  and the source stack was promoted in `blackstar.2`. Return to
  profile-directed source or index experiments.

## Limitations

- The benchmark used one GCC version, one Linux host, one public paired
  short-read corpus, one index, one thread count, and uncompressed input.
- Rejected candidates were not subjected to compressed-input or BAM
  generalization because they failed the primary gate.
- A different compiler, CPU, training mix, or STAR mode could produce a
  different PGO result.
- The observed positive intervals establish a small effect on this workload;
  rejection means the effect is too small to justify added build complexity,
  not that LTO or PGO is universally ineffective.

## Plain-Language Takeaway

Both advanced compiler modes made BlackSTAR slightly faster and substantially
smaller without changing measured results. The gain was about 1.2 percent,
below the project's 2 percent threshold. PGO is reproducible by other
builders, but its profile must be regenerated for the exact source and
compiler, and the measured benefit here does not justify making that process
part of the release.

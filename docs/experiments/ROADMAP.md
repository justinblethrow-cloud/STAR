# BlackSTAR Labs Experimental Roadmap

The qualified starting point is tag `2.7.11b-blackstar.1` at commit
`821457378fa38bfb23b061b8f11ee0a09431dda7`. No Labs result changes that
release until it passes cumulative qualification and is deliberately promoted.

| ID | Area | Hypothesis | State | Dependency |
| --- | --- | --- | --- | --- |
| A00 | Alignment baseline | A public corpus can reproduce and localize the release hot paths. | Corrected baseline complete | Release tag |
| H01 | Alignment affinity | Restore the complete requested CPU place set before pthread worker creation when an OpenMP runtime binds the initial thread. | Accepted Labs candidate; not released | Corrected A00 |
| A01 | Window-bin clearing | Clearing only bins touched by the previous read removes repeated full-array memset work. | Excluded; historical effect size invalidated | A00 |
| A02 | Input dispatch | Adaptive high-thread chunk granularity reduces tail imbalance without changing record boundaries or mapping semantics. | Accepted Labs candidate; not released | H01 |
| A02b | Input producer/consumer queue | Removing the remaining parser mutex materially increases post-A02 worker utilization. | Rejected; post-A02 wait capacity was below one worker-equivalent | A02 |
| A03 | SA accessor specialization | One per-read Base or Delta dispatch and sparse rank checkpoints reduce virtual-SA lookup overhead. | Proposed | Cumulative A06 profile |
| A04 | SIMD seed comparison | Runtime-dispatched AVX2 comparison accelerates bounded nucleotide matching without changing sentinel semantics. | Proposed | A03 |
| A05 | NUMA placement | Measured interleave placement reduces migration and remote-memory work for high-thread private genome loading. | Accepted Labs candidate; not released | A02b profile |
| A06 | Transcript search state | Const-reference recursion copies transcript state only for mutating and terminal branches. | Accepted Labs candidate; not released | Cumulative A05 profile |
| A07 | Modern BAM output | Pinned modern HTSlib with an ordered bounded queue removes global compression serialization. | Proposed | A00 |
| A08 | Multi-sample scheduler | Shared immutable index memory plus explicit resource tokens improves node throughput with complete isolation. | Proposed | A07 |
| A09 | Toolchain | LTO and profile-guided optimization improve the accepted cumulative alignment stack without semantic changes. | Proposed | A06 |
| I01 | Genome preparation | Parallel reverse-complement and bounded private prefix histograms reduce serial setup. | Proposed | A09 |
| I02 | SA packing | Record-block partitioning permits deterministic disjoint-byte parallel packing. | Proposed | I01 |
| I03 | Junction merge | Partitioned merge and rank calculation reduce the remaining serial junction stage. | Proposed | I02 |
| I04 | Suffix sorting | Comparator correction plus inlined multikey radix sorting reduces dominant bin-sort work. | Proposed | I03 |
| I05 | Index v2 | Pinned libsais64 may justify an opt-in incompatible format only if I04 is insufficient. | Conditional | I04 decision |

## Stop Conditions

- Reject a change that fails any correctness oracle.
- Reject a change below the practical performance gate.
- Stop local kernel work when profiling shows no remaining material local hot
  path; reassess architecture rather than accumulating complexity.
- Do not promote A02 until its adaptive default passes cumulative release
  qualification; its Labs decision is supported by paired uncompressed and
  compressed measurements plus a canonical BAM differential check.
- Do not revive A02b without a new profile showing materially more than the
  observed 0.75 worker-equivalents of input-lock wait.
- Do not promote A05 until H01+A02+A05 passes cumulative release qualification
  on private and shared genome-loading modes. Its gzip result remains
  supportive because the A02 control exceeded the predeclared variability
  gate.
- Do not promote A06 until the cumulative H01+A02+A05+A06 stack passes release
  qualification. Its five-pair 2.29 percent result is the primary small-effect
  confirmation; the three-pair gzip result remains supportive.
- I05 begins only after a documented I04 decision and retains the default v1
  index format.

# BlackSTAR Labs Experimental Roadmap

The qualified starting point is tag `2.7.11b-blackstar.1` at commit
`821457378fa38bfb23b061b8f11ee0a09431dda7`. No Labs result changes that
release until it passes cumulative qualification and is deliberately promoted.

| ID | Area | Hypothesis | State | Dependency |
| --- | --- | --- | --- | --- |
| A00 | Alignment baseline | A public corpus can reproduce and localize the release hot paths. | Corrected baseline complete | Release tag |
| H01 | Alignment affinity | Restore the complete requested CPU place set before pthread worker creation when an OpenMP runtime binds the initial thread. | Accepted Labs candidate; not released | Corrected A00 |
| A01 | Window-bin clearing | Clearing only bins touched by the previous read removes repeated full-array memset work. | Excluded; historical effect size invalidated | A00 |
| A02 | Input dispatch | Per-thread timing can determine whether serialized parsing, coarse chunks, or tail imbalance suppress CPU occupancy. | Diagnostics complete; implementation next | H01 |
| A03 | SA accessor specialization | One per-read Base or Delta dispatch and sparse rank checkpoints reduce virtual-SA lookup overhead. | Proposed | A02 decision |
| A04 | SIMD seed comparison | Runtime-dispatched AVX2 comparison accelerates bounded nucleotide matching without changing sentinel semantics. | Proposed | A03 |
| A05 | NUMA placement | Parallel first-touch, huge-page advice, and measured placement reduce remote-memory stalls. | Proposed | A02 decision |
| A06 | Transcript search state | A compact rollback state avoids copying cold transcript containers during recursive stitching. | Proposed | A03-A05 |
| A07 | Modern BAM output | Pinned modern HTSlib with an ordered bounded queue removes global compression serialization. | Proposed | A00 |
| A08 | Multi-sample scheduler | Shared immutable index memory plus explicit resource tokens improves node throughput with complete isolation. | Proposed | A07 |
| A09 | Toolchain | LTO and profile-guided optimization improve cumulative kernels after their interfaces stabilize. | Proposed | A03-A08 |
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
- Do not accept an input-dispatch redesign until it improves corrected paired
  measurements and preserves exact output; A02 has completed the prerequisite
  timing decomposition.
- I05 begins only after a documented I04 decision and retains the default v1
  index format.

# BlackSTAR Labs Experimental Roadmap

The current qualified release is `2.7.11b-blackstar.2`. It inherits
`2.7.11b-blackstar.1` at
`821457378fa38bfb23b061b8f11ee0a09431dda7` and promotes the Q01-qualified
H01+A02+A05+A06 alignment stack. Later Labs work remains outside the release
until it passes cumulative qualification and is deliberately promoted.

| ID | Area | Hypothesis | State | Dependency |
| --- | --- | --- | --- | --- |
| A00 | Alignment baseline | A public corpus can reproduce and localize the release hot paths. | Corrected baseline complete | Release tag |
| H01 | Alignment affinity | Restore the complete requested CPU place set before pthread worker creation when an OpenMP runtime binds the initial thread. | Accepted in blackstar.2 | Corrected A00 |
| A01 | Window-bin clearing | Clearing only bins touched by the previous read removes repeated full-array memset work. | Excluded; historical effect size invalidated | A00 |
| A02 | Input dispatch | Adaptive high-thread chunk granularity reduces tail imbalance without changing record boundaries or mapping semantics. | Accepted in blackstar.2 | H01 |
| A02b | Input producer/consumer queue | Removing the remaining parser mutex materially increases post-A02 worker utilization. | Rejected; post-A02 wait capacity was below one worker-equivalent | A02 |
| A03 | SA accessor specialization | One per-read Base or Delta dispatch and sparse rank checkpoints reduce virtual-SA lookup overhead. | Proposed | Cumulative A06 profile |
| A04 | SIMD seed comparison | Runtime-dispatched AVX2 comparison accelerates bounded nucleotide matching without changing sentinel semantics. | Proposed | A03 |
| A05 | NUMA placement | Measured interleave placement reduces migration and remote-memory work for high-thread private genome loading. | Accepted in blackstar.2 | A02b profile |
| A06 | Transcript search state | Const-reference recursion copies transcript state only for mutating and terminal branches. | Accepted in blackstar.2 | Cumulative A05 profile |
| A07 | Modern BAM output | Pinned modern HTSlib with an ordered bounded queue removes global compression serialization. | Proposed | A00 |
| A08 | Multi-sample scheduler | Shared immutable index memory plus explicit resource tokens improves node throughput with complete isolation. | Proposed | A07 |
| A09 | Toolchain | LTO and profile-guided optimization improve the accepted cumulative alignment stack without semantic changes. | Rejected; LTO and PGO each gained about 1.2%, below the 2% practical gate | A06 |
| Q01 | Cumulative alignment qualification | The complete H01+A02+A05+A06 stack preserves release behavior and generalizes across private, shared, compressed, BAM, affinity, sanitizer, compatibility, and package gates. | Complete; promoted in blackstar.2 | A06 and A09 decision |
| Q02 | Cross-workload generalization | The released high-thread stack and new compatibility hardening preserve behavior beyond paired gene-count-only RNA-seq. | Complete; nine public series passed, single-end timing remains unresolved, no release promotion | Q01 and successor transition |
| I01 | Genome preparation | Parallel reverse-complement and bounded private prefix histograms reduce serial setup. | Proposed | Cumulative alignment qualification |
| I02 | SA packing | Record-block partitioning permits deterministic disjoint-byte parallel packing. | Proposed | I01 |
| I03 | Junction merge | Partitioned merge and rank calculation reduce the remaining serial junction stage. | Proposed | I02 |
| I04 | Suffix sorting | Comparator correction plus inlined multikey radix sorting reduces dominant bin-sort work. | Proposed | I03 |
| I05 | Index v2 | Pinned libsais64 may justify an opt-in incompatible format only if I04 is insufficient. | Conditional | I04 decision |

## Stop Conditions

- Reject a change that fails any correctness oracle.
- Reject a change below the practical performance gate.
- Stop local kernel work when profiling shows no remaining material local hot
  path; reassess architecture rather than accumulating complexity.
- Q01 satisfied the cumulative technical qualification dependency for H01,
  A02, A05, and A06 and supports their `blackstar.2` release boundary.
- Do not revive A02b without a new profile showing materially more than the
  observed 0.75 worker-equivalents of input-lock wait.
- Do not stack A09. LTO and PGO preserved exact measured outputs and had
  positive paired intervals, but their five-pair median gains of 1.21 and 1.20
  percent did not meet the 2 percent practical gate.
- Q01 pre-promotion package artifacts retain the old `.1` source version and
  are not release assets. Only clean `.2` packages from the protected release
  commit are distributable.
- Q02 does not establish a single-end speed improvement. Its exclusive-node
  five-pair series passed correctness, RSS, and noninferiority but exceeded the
  5 percent compatibility CV threshold.
- Q02 TranscriptomeSAM output intentionally stabilizes the primary transcript
  flag across worker schedules. The complete upstream alignment set remains the
  compatibility oracle after clearing only flag `0x100`.
- I05 begins only after a documented I04 decision and retains the default v1
  index format.

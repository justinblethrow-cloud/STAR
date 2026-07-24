# BlackSTAR Status History

This registry is the public boundary between release capability and historical
experimentation. A result remains useful evidence after rejection, but it must
not be described as shipped behavior.

| Area | Status | Release | Decision |
| --- | --- | --- | --- |
| Memory-adaptive parallel suffix-array construction | Accepted | blackstar.1, blackstar.2 | Qualified by paired full-CHM13 timing and byte identity. |
| Bounded parallel SAindex construction | Accepted | blackstar.1, blackstar.2 | Deterministic reduction and low-memory fallback qualified. |
| Parallel junction index construction | Accepted | blackstar.1, blackstar.2 | Retained under byte-identity tests. |
| Persistent Full, Overlay, and Delta sequence insertion | Accepted | blackstar.1, blackstar.2 | Qualified with GTF, corruption rejection, mapping, and counts. |
| Atomic package publication and strong base identity | Accepted | blackstar.1, blackstar.2 | Required release integrity control. |
| Persistent prefork workers | Rejected | None | Slower in measured configurations and lifecycle semantics were incomplete. |
| libdeflate replacement | Rejected | None | Slower than bundled zlib in the measured workload. |
| Threaded BAM compression prototype | Archived experiment | None | Measured speedup, but output and lifecycle implementation was not release-ready. |
| alignReadsMulti prototype | Archived experiment | None | Measured multi-sample speedup, but resource and failure semantics were not release-ready. |
| Corrected A00 public alignment baseline | Labs evidence | None | OpenMP binding was removed; release medians are 71.35 seconds uncompressed and 72.15 seconds compressed over three deterministic runs each. |
| H01 alignment-affinity recovery | Accepted | blackstar.2 | Recovers a reproduced one-core pthread inheritance failure; ordinary unbound noninferiority and Q01 cumulative qualification passed. |
| A01 touched-window-bin reset | Excluded | None | Correctness passed, but its performance series inherited the one-core affinity failure; the reported effect sizes are invalidated and the source is not stacked. |
| A02 adaptive input chunks | Accepted | blackstar.2 | At 96 threads, three paired full-corpus runs improved median wall time by 13.41% uncompressed and 8.74% through zcat, lowered peak RSS, preserved measured outputs, and passed Q01 cumulative qualification. |
| A02b input producer/consumer queue | Rejected after profiling | None | Post-A02 mapping reached 94.16 equivalent CPUs, input-lock wait consumed only 0.75 worker-equivalents, and the active-worker finish spread was 0.36 seconds; no queue source was stacked. |
| A05 NUMA-aware private genome placement | Accepted | blackstar.2 | Exact commit 91de892 improved the 96-thread uncompressed full-corpus median by 22.51% over A02 with stable replicates, exact outputs, inherited-policy preservation, shared-mode fallback, and Q01 cumulative qualification. |
| A06 transcript recursion copy elision | Accepted | blackstar.2 | Exact commit 6032393 improved the five-pair uncompressed full-corpus median by 2.29% over A05 with a positive paired interval, exact outputs, compressed-input support, a canonical BAM pass, and Q01 cumulative qualification. |
| A09 LTO and PGO toolchain variants | Rejected | None | Independent five-pair tests improved the cumulative A06 median by 1.21% with LTO and 1.20% with PGO. Both preserved exact measured outputs and had positive paired intervals, but neither met the 2% practical gate. |
| Q01 cumulative alignment stack | Release qualification | blackstar.2 | H01+A02+A05+A06 produced median paired improvements of 33.20% uncompressed and 28.54% through zcat across three balanced pairs, lowered RSS, preserved exact outputs, and passed shared-index, BAM, affinity, sanitizer, insertion, SAindex, upstream-compatibility, selector, and reproducible-package gates. |
| Early 41 percent full-index claim | Superseded | None | Replaced by three-pair 49.48 percent release evidence. |
| Early Delta build and runtime figures | Superseded | None | Replaced by hardened package and promotion-gate evidence. |

Historical slide decks are retained outside the source repository. They are not
canonical evidence and should be rebuilt from the figure and claim manifests.

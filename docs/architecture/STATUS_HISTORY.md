# BlackSTAR Status History

This registry is the public boundary between release capability and historical
experimentation. A result remains useful evidence after rejection, but it must
not be described as shipped behavior.

| Area | Status | Release | Decision |
| --- | --- | --- | --- |
| Memory-adaptive parallel suffix-array construction | Accepted | blackstar.1 | Qualified by paired full-CHM13 timing and byte identity. |
| Bounded parallel SAindex construction | Accepted | blackstar.1 | Deterministic reduction and low-memory fallback qualified. |
| Parallel junction index construction | Accepted | blackstar.1 | Retained under byte-identity tests. |
| Persistent Full, Overlay, and Delta sequence insertion | Accepted | blackstar.1 | Qualified with GTF, corruption rejection, mapping, and counts. |
| Atomic package publication and strong base identity | Accepted | blackstar.1 | Required release integrity control. |
| Persistent prefork workers | Rejected | None | Slower in measured configurations and lifecycle semantics were incomplete. |
| libdeflate replacement | Rejected | None | Slower than bundled zlib in the measured workload. |
| Threaded BAM compression prototype | Archived experiment | None | Measured speedup, but output and lifecycle implementation was not release-ready. |
| alignReadsMulti prototype | Archived experiment | None | Measured multi-sample speedup, but resource and failure semantics were not release-ready. |
| Corrected A00 public alignment baseline | Labs evidence | None | OpenMP binding was removed; release medians are 71.35 seconds uncompressed and 72.15 seconds compressed over three deterministic runs each. |
| H01 alignment-affinity recovery | Accepted Labs candidate | None | Recovers a reproduced one-core pthread inheritance failure by restoring 48 places and 96 CPUs; ordinary unbound path passed five-pair noninferiority. |
| A01 touched-window-bin reset | Excluded | None | Correctness passed, but its performance series inherited the one-core affinity failure; the reported effect sizes are invalidated and the source is not stacked. |
| A02 input-dispatch diagnostics | Experimental | None | Corrected full-corpus measurement found 49.52% input-lock occupancy, 71.28 aggregate mapping concurrency, and a 12.93-second finish spread; implementation remains unaccepted. |
| Early 41 percent full-index claim | Superseded | None | Replaced by three-pair 49.48 percent release evidence. |
| Early Delta build and runtime figures | Superseded | None | Replaced by hardened package and promotion-gate evidence. |

Historical slide decks are retained outside the source repository. They are not
canonical evidence and should be rebuilt from the figure and claim manifests.

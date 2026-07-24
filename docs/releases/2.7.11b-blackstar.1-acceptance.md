# BlackSTAR 2.7.11b-blackstar.1 Acceptance Record

## Verdict

BlackSTAR `2.7.11b-blackstar.1` has passed the technical gates for a controlled
x86-64 Linux canary. The full performance and equivalence campaign used commit
`3053d8effb53f015940794ada24155cfb350881a`, binary SHA-256
`6726fea9633f62d7772cde2d00c374a2412cf6a11ab6f03e47d6ab8d07a19117`.
Subsequent release-record and selector commits do not change `source/`.

This is a technical qualification, not a record of production deployment.
Promotion still requires the final GitHub CI run, an explicitly approved
deployment, and normal operational monitoring. The deployable artifact must be
a clean build from the final promotion commit; its checksum is taken from
`build-info.tsv` and published release metadata. Do not infer that checksum
from the benchmark artifact above.

## Supported Boundary

The supported target is x86-64 Linux and includes:

- optimized, deterministic `genomeGenerate` suffix-array, SAindex, and
  junction-index construction;
- `genomeInsert Full`, packaged `Overlay`, and cached `Delta` modes with
  insert-only GTF support;
- virtual-SA no-junction Delta alignment;
- strict package identity, validation, and atomic publication;
- the upstream correctness fixes listed below.

The release excludes `alignReadsMulti`, persistent prefork workers, threaded
BAM-compression prototypes, and default libdeflate changes. Normal STAR
alignment remains the supported runtime interface.

## Closed BlackSTAR Findings

| Finding | Resolution | Acceptance evidence |
|---|---|---|
| Delta accepted a different same-sized base | Bind and validate aggregate identity for `Genome`, `SA`, `SAindex`, metadata, and annotation sidecars | Alternate valid same-dimension base rejected |
| Mutable external Overlay/Delta inputs | Package inserted FASTA/GTF and identify them independently | Source mutation and package relocation do not change results |
| Partial or mixed publication | Same-filesystem staging, sync, completion manifest, and atomic rename | Failed builds leave no published partial artifact |
| Duplicate reference or annotation namespaces | Reject base/insert contig, gene, and transcript collisions | Adversarial collision matrix passes |
| Ambiguous inserted transcript model | Prevent transcript IDs crossing chromosome, strand, or gene contexts | Conflicting transcript fixture rejected |
| Permissive Overlay manifest | Require exact v2 schema, encoded paths, and duplicate/unknown-key rejection | Malformed and path-with-spaces fixtures pass |
| Weak Delta format validation | Fixed little-endian v2 header, dimensions, identities, bounds, exact length, digest, and sorted records | Outer and independently re-signed inner corruption rejected |
| Unbounded SAindex event memory | Live/configured-RAM budget, bounded batches, deterministic reduction, and allocation fallback | Serial, parallel, and low-memory outputs byte-identical |
| Overlay `sjdbOverhang` drift | Apply base-index inheritance and mismatch rules | Inheritance and explicit mismatch tests pass |
| Benchmark helper could accept unresolved validation | Fail closed unless an explicit timing-only override is supplied | Known mismatch, insert-only, and unresolved-SA exit tests pass |

## Inherited Upstream Fixes

These defects predate BlackSTAR but affect paths used by the fork:

- unaligned packed-array word access;
- unbounded inserted-suffix comparison;
- uninitialized optional transcriptome state;
- uninitialized genome-transform quantification-output state;
- misaligned in-memory splice-junction records.

Focused ASan/UBSan tests cover the relevant components. The genome-transform
state defect was independently reproduced in stock STAR under Valgrind; the
fixed Delta path reports no invalid or uninitialized-memory errors when
process-lifetime leak reporting is excluded.

## Automated Gates

- The release branch passed GitHub Actions job `build-and-test` in
  [run 29393821858](https://github.com/justinblethrow-cloud/blackSTAR/actions/runs/29393821858).
- Local gates cover release identity, OpenMP linkage, focused sanitizers,
  genome-insert equivalence and adversarial rejection, SHA-256 identities,
  SAindex strategy equivalence, static analysis, Valgrind, and reproducible
  release packaging.
- The deployment selector fixture covers candidate success, explicit rollback,
  automatic fallback, missing executables, checksum failure, canary failure,
  paths with spaces, concurrent selectors, and preservation of prior state when
  both binaries fail.

The selector test was added after the linked GitHub run and must pass the final
CI run before the branch is protected or tagged.

## Full-Genome Performance

Three randomized/order-balanced pairs built CHM13v2 plus ERCC with the same
annotation, parameters, host, and 96-thread allocation.

| Result | BlackSTAR | Upstream STAR 2.7.11b |
|---|---:|---:|
| Mean wall time | 619.30 s | 1225.78 s |
| Relative result | 49.48% less wall time | control |
| Speedup | 1.98x | 1.00x |

Individual BlackSTAR runs were 577.88, 678.90, and 601.11 seconds. Controls
were 1201.94, 1210.34, and 1265.06 seconds. All 42 comparisons of substantive
index files were byte-identical. The comparison includes `Genome`, `SA`,
`SAindex`, chromosome metadata, splice-junction tables, `sjdbInfo.txt`, and all
gene, transcript, and exon tables.

The benchmark used `genomeSAindexNbases=14`, `genomeChrBinNbits=18`,
`limitGenomeGenerateRAM=300000000000`, and `sjdbOverhang=93`. Full telemetry is
retained under `benchmarks/full_chm13_paired_rc_3053d8e_20260715T0620Z/` in the
audit workspace.

## Delta Promotion Gate

BlackSTAR created a GRCh38 plus GFP/GST Delta package with insert-only GTF
annotations in 50.96 seconds. Peak RSS was 27.98 GiB while loading a 28.05 GiB
base index; the resulting Delta package was 58,130 bytes.

Across 24 real paired-end samples, base and Delta runs had exact
timing-independent mapping metrics, splice-junction output, existing gene
counts, and special count rows. A 2,000-read GFP/GST spike produced exactly
1,000 GFP and 1,000 GST counts without changing an existing gene row. An
independent paired-end GFP fixture produced 100 counted fragments and 200
proper-pair records; the base index mapped none.

The non-synthetic full SAM record multiset was exact after deterministic record
sorting. Raw threaded output order can differ; record content did not.

Delta alignment is not runtime-free. In the 24-sample sweep, mean base-index
alignment time was 62.26 seconds and mean Delta time was 71.48 seconds, a
descriptive 9.22-second or 14.81% increase for these short jobs. All base runs
preceded all Delta runs, so this is not an order-balanced performance estimate.
It replaces earlier informal language that Delta had no meaningful startup
penalty.

Evidence is retained under
`benchmarks/grch38_delta_promotion_rc_3053d8e_20260715/` in the audit workspace.

## Downstream Shadow

A stock run, repeated stock run, BlackSTAR base run, and BlackSTAR Delta run
were compared through coordinate sorting, UMI deduplication, and gene counting.
The normalized non-synthetic STAR record multiset was identical in all four
arms.

The shadow exposed an inherited downstream issue rather than a BlackSTAR
alignment defect: the deployed UMIcollapse `MapQualMerge` path was sensitive to
coordinate-tie input order. Repeated stock STAR alone changed 11 gene rows by
2.52 counts total even though its alignment-record multiset was unchanged.

A separate DUMI hardening commit,
`2ef54b287f83864ee83b3541a7a29236547b72df`, adds stable equal-map-quality and
equal-frequency UMI tie-breakers. With that patch:

- stock, repeated stock, and BlackSTAR base count files were byte-identical;
- all four non-synthetic deduplicated SAM hashes were identical;
- Delta changed only GFP and GST, each by exactly 1,000 counts;
- no existing gene row changed.

The deterministic tie-breakers added 3.14 seconds on average to the UMI step,
or about 1.7% of this representative full pipeline. This companion patch is an
integration requirement when byte-stable downstream results are required; it
is not part of the STAR source tree.

## Canary And Fallback

`extras/scripts/selectBlackSTAR.sh` validates pinned candidate, fallback, and
canary checksums plus executable versions, requires candidate OpenMP linkage, runs an isolated
mapping canary, revalidates the generation-local executable copy, serializes
transitions, and atomically switches a single generation symlink. It also
supports an explicit `fallback-only` rollback policy.

The host gate used an upstream-STAR-built index and deterministic mapped and
unmapped reads. It passed candidate selection, explicit fallback, automatic
fallback on candidate checksum failure, candidate restoration, and rejection
of a both-invalid transition without changing the restored state. The live DGE
installation was not modified. Evidence is retained under
`benchmarks/blackstar_canary_rc_3053d8e_20260715/`.

## Residual Risk

The following are inherited or explicitly out of scope, not unresolved
BlackSTAR regressions:

- broad process-lifetime allocations remain reported as leaks at exit;
- longstanding compiler warnings remain in SAM-header and read-file code;
- bundled HTSlib is old and is not a suitable base for new compression work;
- macOS and non-x86-64 targets have not been recertified;
- Overlay and Delta require `NoSharedMemory`;
- full-index performance claims apply to the measured host, reference, and
  parameters; other systems must be benchmarked independently.

## Remaining Promotion Actions

1. Push only after explicit approval and require a green `build-and-test` check.
2. Build from the final clean commit and publish its exact `build-info.tsv` and
   executable/archive checksums with the release artifact.
3. Deploy through the selector, shadow first, then use a bounded production
   canary with the pinned stock fallback.
4. Promote further only after alignment, junction, count, error, wall-time, and
   memory telemetry remain within the documented acceptance bounds.

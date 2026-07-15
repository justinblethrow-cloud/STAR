# BlackSTAR 2.7.11b-blackstar.1 Acceptance Record

## Verdict

The narrow BlackSTAR release line has passed clean-tree full-size `genomeGenerate` recertification and is ready for pipeline shadow testing. The release-blocking correctness defects found in the July 2026 production-readiness audit are fixed and covered by automated regression or adversarial tests.

This is not yet a production-promotion approval. One clean CHM13 candidate run is recorded below, but comparative CHM13, GRCh38, and real-order timings remain directional until paired randomized repeats are complete. GitHub Actions also remains unexecuted until GitHub changes are explicitly approved.

## Supported Boundary

The supported target is x86-64 Linux and includes:

- optimized, deterministic `genomeGenerate` suffix-array, SAindex, and junction-index construction;
- `genomeInsert Full`, packaged `Overlay`, and cached `Delta` modes with insert-only GTF support;
- the virtual-SA no-junction Delta alignment path;
- the upstream correctness fixes listed below.

The release excludes `alignReadsMulti`, persistent prefork workers, threaded BAM compression prototypes, and default libdeflate changes. Normal STAR alignment remains the runtime interface.

## Closed BlackSTAR Findings

| Finding | Resolution | Acceptance evidence |
| --- | --- | --- |
| Delta accepted a different same-sized base | Strong aggregate identity binds `Genome`, `SA`, `SAindex`, generation/chromosome metadata, and annotation sidecars; loaded content must match | Alternate valid same-dimension base is rejected before insertion |
| Mutable external Overlay/Delta inputs | Inserted FASTA/GTF are packaged and independently identified | Original inputs can be mutated and the package relocated without changing results |
| Partial or mixed output publication | Sibling staging, file and directory sync, completion manifest, and atomic rename | Existing nonempty output is preserved; normal failures clean staging; incomplete output is rejected |
| Duplicate reference and annotation namespaces | Base/insert contig, gene, and transcript collisions are rejected before publication | Adversarial collision matrix passes |
| Ambiguous inserted transcript model | A transcript ID cannot cross chromosome, strand, or gene contexts | Conflicting transcript fixture is rejected |
| Permissive Overlay manifest | Exact v2 key schema, hex paths, required values, and strict unknown/duplicate-key rejection | Unknown-key and path-with-spaces tests pass |
| Weak Delta format and validation | Fixed little-endian v2 header, dimensions, identities, bounds, exact length, payload digest, and sorted-record validation | Outer and independently re-signed inner corruption tests pass |
| Unbounded SAindex event memory | Live/configured-RAM budget, bounded batches, deterministic reducer, and allocation fallback | Parallel and low-memory fallback outputs are byte-identical to serial |
| Overlay `sjdbOverhang` drift | Overlay creation mirrors STAR base-index inheritance and mismatch rules | Default inheritance and explicit mismatch tests pass |
| Benchmark helper could pass failed or unresolved validation | Required files fail closed; a differing SA is inconclusive and exits 2 without an explicit timing-only override | Known mismatch, insert-only, and unresolved-SA exit tests pass |

## Inherited Fixes Now Covered

These defects predate BlackSTAR but affect paths used by the fork:

- unaligned packed-array word access;
- unbounded inserted-suffix comparison;
- uninitialized optional transcriptome state;
- uninitialized genome-transform quantification-output state;
- misaligned in-memory splice-junction records.

Focused ASan/UBSan tests cover each relevant component. The genome-transform flag defect was independently reproduced in stock STAR under Valgrind; after the fix, BlackSTAR Delta alignment reports zero invalid or uninitialized-memory errors when leak reporting is excluded.

## Acceptance Evidence

The local acceptance pass completed successfully with:

- fork version and OpenMP linkage checks;
- SHA-256 known vectors, segmented/file identity, and cross-thread determinism;
- Full and Delta byte reproducibility across thread counts;
- Full rebuild, Full insertion, Overlay, Delta, and no-junction Delta biological equivalence;
- SAM body, splice-junction output, and gene-count comparisons;
- stock STAR 2.7.11b loading and mapping a BlackSTAR Full artifact;
- stale-base, malformed-GTF, collision, destination, relocation, missing/extra file, and corruption rejection;
- serial, bounded-parallel, and low-memory SAindex byte identity;
- focused sanitizer tests and GCC static analysis;
- Valgrind Delta alignment with zero non-leak memory errors;
- two independent release builds with identical binaries and tar archives.

The release builder fixes source date, embedded provenance, build location, locale, timezone, file modes, archive order, ownership, timestamps, and gzip metadata. It records compiler/linkage details and publishes through same-filesystem staging.

### Full-size candidate recertification

The clean candidate at commit `55873468383b94060ad8d33c92e3990e7e410376` was built twice with byte-identical binaries and release archives. The binary used for the run had SHA-256 `f4545d56ac740fc3bdbf1bdf832c95a7b721cb7232b059c9ec0245f1c69f4c30`.

On July 15, 2026, the candidate built the CHM13v2 plus ERCC index with the production annotation using 96 threads, `genomeSAindexNbases 14`, `genomeChrBinNbits 18`, `limitGenomeGenerateRAM 300000000000`, and `sjdbOverhang 93`.

| Result | Value |
| --- | --- |
| Start | `2026-07-15 05:58:05 UTC` |
| Finish | `2026-07-15 06:08:04 UTC` |
| Wall time | 600.39 seconds |
| User/system CPU | 17,208.92 / 1,109.24 seconds |
| Maximum resident set | 80,877,308 KiB |
| SA prefix planning / sorting / packing | 53 / 215 / 73 seconds |
| SAindex construction | 35 seconds |
| Junction insertion and SAi work | 111 seconds |
| Final Genome / SA / SAindex writes | 6 / 44 / 5 seconds |

The bounded SAindex path processed 227,756,190 events with a 256,000,000-byte event budget. `Genome`, `SA`, `SAindex`, all chromosome metadata, both splice-junction lists, `sjdbInfo.txt`, and all gene, transcript, and exon tables were byte-identical to the previously accepted CHM13 candidate. `genomeParameters.txt` differed only in executable and output paths.

The retained input SHA-256 values are:

- CHM13v2 FASTA: `15a4ba1246f6021a89699bf5083da7f2bad3f79c86acd7bc1eb0ca3a13164e85`
- ERCC FASTA: `ab9720a49d9af5463e535fe0c3f6ea2a8c7f9fbf4a4afe29969fb5c1b9e3a2b4`
- production GTF: `e06d8b086c61269d5454d8337845f4c6ba817a8a8728bb5fb44cdfe93e116b63`

The immediately preceding accepted optimized run took 685.29 seconds with the same reference and principal parameters. The observed 84.90-second, 12.4% difference is promising but remains a cross-date comparison rather than a paired randomized performance claim. Command, logs, stage timings, CPU samples, and I/O telemetry are retained under `benchmarks/full_chm13_hardened_rc_5587346_20260715T0557Z/` in the local project workspace.

## Segregated Residual Debt

The following is inherited upstream STAR debt, not a BlackSTAR regression:

- broad process-lifetime allocations remain reported as leaks at program exit;
- longstanding compiler warnings remain in SAM-header and read-file code;
- bundled HTSlib is old and is not a suitable base for new compression work;
- macOS behavior has not been recertified for BlackSTAR-specific code.

These items should be tracked separately. They do not invalidate the passing semantic and non-leak memory checks for the supported release paths.

## Remaining Release Gates

1. Run GitHub Actions after explicit approval and require it on the release branch.
2. Repeat paired randomized CHM13 or GRCh38 full-index benchmarks on a quiet host, preserving system and I/O telemetry.
3. Repeat Delta creation and base-versus-Delta alignment on broad real paired-end data plus synthetic added-reference reads.
4. Compare normalized alignment records, junctions, gene counts, UMI-deduplicated outputs, final matrices, and downstream UI/DGE inputs.
5. Shadow the candidate in the Plasmidsaurus pipeline, then canary with automatic fallback to stock STAR.

The 600.39-second candidate result is newly measured. No cross-date performance comparison should be promoted as a release claim until the paired benchmark gate is complete.

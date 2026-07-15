# BlackSTAR 2.7.11b-blackstar.1 Acceptance Record

## Verdict

The narrow BlackSTAR release line is ready for full-size performance recertification and pipeline shadow testing. The release-blocking correctness defects found in the July 2026 production-readiness audit are fixed and covered by automated regression or adversarial tests.

This is not yet a production-promotion approval. Historical CHM13, GRCh38, and real-order timings remain directional evidence until repeated from a clean tagged candidate. GitHub Actions also remains unexecuted until GitHub changes are explicitly approved.

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

## Segregated Residual Debt

The following is inherited upstream STAR debt, not a BlackSTAR regression:

- broad process-lifetime allocations remain reported as leaks at program exit;
- longstanding compiler warnings remain in SAM-header and read-file code;
- bundled HTSlib is old and is not a suitable base for new compression work;
- macOS behavior has not been recertified for BlackSTAR-specific code.

These items should be tracked separately. They do not invalidate the passing semantic and non-leak memory checks for the supported release paths.

## Remaining Release Gates

1. Commit and build from a clean release-candidate tree; retain generated checksums and build metadata.
2. Run GitHub Actions after explicit approval and require it on the release branch.
3. Repeat paired randomized CHM13 or GRCh38 full-index benchmarks on a quiet host, preserving system and I/O telemetry.
4. Repeat Delta creation and base-versus-Delta alignment on broad real paired-end data plus synthetic added-reference reads.
5. Compare normalized alignment records, junctions, gene counts, UMI-deduplicated outputs, final matrices, and downstream UI/DGE inputs.
6. Shadow the candidate in the Plasmidsaurus pipeline, then canary with automatic fallback to stock STAR.

No historical performance number should be promoted from this record as a newly certified release result.

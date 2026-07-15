# BlackSTAR Release Boundary

BlackSTAR `2.7.11b-blackstar.1` is a focused fork of upstream STAR `2.7.11b`. It preserves the upstream genome compatibility version while adding tested genome-generation performance work and persistent named-sequence insertion.

The `blackstar.1` release target is x86-64 Linux. Upstream macOS source support has not been recertified for the BlackSTAR-specific paths or release builder.

See [the acceptance record](BLACKSTAR_ACCEPTANCE.md) for closed audit findings, segregated upstream debt, local verification, and remaining promotion gates.

## Supported Changes

- Multithreaded and memory-adaptive suffix-array construction for `genomeGenerate`.
- Bounded parallel SAindex event scanning with deterministic serial reduction and a low-memory fallback.
- Parallelized junction indexing paths covered by byte-identity tests.
- `genomeInsert Full`, `Overlay`, and cached `Delta` modes, including insert-only GTF annotations.
- Atomic artifact publication, strict manifests, packaged inserted inputs, strong base identity, and Delta payload validation.
- Fixes for unaligned packed-array access, optional transcriptome initialization, genome-transform output state, bounded inserted-suffix comparison, and in-memory splice-junction record alignment.

## Explicitly Excluded Experiments

The release branch does not contain the experimental `alignReadsMulti` or persistent prefork worker implementation. It also does not contain the experimental `outBAMcompressionThreadN` parameter, bundled-htslib threading changes, or per-bin threaded BAM compression. Those prototypes were archived because their lifecycle, output isolation, or resource semantics were not ready for production.

Normal STAR alignment behavior and parameters remain the supported runtime interface.

## Compatibility

- Existing STAR indexes with `versionGenome 2.7.4a` remain loadable.
- Full genome-insert output uses the conventional STAR index file set. The additional `blackstar.complete.tsv` file is ignored by stock STAR.
- Overlay and Delta directories are BlackSTAR-specific. Delta v2 and overlay manifest v2 are the only supported development formats.
- Overlay and Delta loading requires `NoSharedMemory` and validates the loaded base index against the package identity.

## Release Gates

A release candidate must pass:

1. A clean OpenMP build with a fork-specific `--version` string.
2. Focused ASan/UBSan tests for packed arrays, suffix comparison, transcript initialization, junction records, and SHA-256 identities.
3. Genome-insert equivalence against a full rebuild, including alignments, junctions, and gene counts.
4. Adversarial rejection tests for stale bases, namespace collisions, malformed annotations, package corruption, extra/missing files, and nonempty destinations.
5. Byte identity across serial, bounded-parallel, and RAM-constrained SAindex paths.
6. Full-size benchmark reruns only on a quiet system, with commands, version, timing, memory, I/O snapshots, and validation artifacts retained.

The benchmark scripts are evidence collectors, not substitutes for correctness tests. A nonidentical `SA` is never labeled equivalent without mapping-level validation.

## Deployment Guidance

Deploy the exact tested binary or a reproducible build from a tagged commit. Record `STAR --version`, the commit, compiler version, compile flags, and an executable checksum. Build base indexes and genome-insert packages on local storage when possible, then publish completed artifacts to shared storage. Never deploy a `.tmp.<pid>.<attempt>` staging directory.

From a clean tagged checkout, build a release package with:

```bash
JOBS=16 extras/scripts/buildBlackSTARRelease.sh
```

The builder derives `SOURCE_DATE_EPOCH` from the commit, fixes embedded build provenance, verifies the fork version and OpenMP linkage, and writes a binary, `build-info.tsv`, `ldd.txt`, a deterministic tarball, and SHA-256 checksums under `dist/`.

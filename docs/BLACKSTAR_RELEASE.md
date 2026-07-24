# BlackSTAR Release Boundary

BlackSTAR `2.7.11b-blackstar.2` is a maintained fork of upstream STAR
`2.7.11b`. It preserves the upstream genome compatibility version while
adding qualified genome-generation, persistent named-sequence insertion, and
high-thread alignment improvements.

The `blackstar.2` release target is x86-64 Linux. Upstream macOS source support
has not been recertified for the BlackSTAR-specific paths or release builder.

See [the acceptance record](BLACKSTAR_ACCEPTANCE.md) for closed audit findings,
segregated upstream debt, and measured verification. See
[the promotion runbook](BLACKSTAR_PROMOTION.md) for artifact pinning, canary,
rollback, and staged rollout.
The original `blackstar.1` acceptance record is retained under
[docs/releases](releases/2.7.11b-blackstar.1-acceptance.md).

## Supported Changes

- Multithreaded and memory-adaptive suffix-array construction for `genomeGenerate`.
- Bounded parallel SAindex event scanning with deterministic serial reduction and a low-memory fallback.
- Parallelized junction indexing paths covered by byte-identity tests.
- `genomeInsert Full`, `Overlay`, and cached `Delta` modes, including insert-only GTF annotations.
- Atomic artifact publication, strict manifests, packaged inserted inputs, strong base identity, and Delta payload validation.
- Fixes for unaligned packed-array access, optional transcriptome initialization, genome-transform output state, bounded inserted-suffix comparison, and in-memory splice-junction record alignment.
- Alignment affinity recovery when an explicitly configured OpenMP runtime
  narrows the initial thread before STAR creates pthread workers.
- Adaptive record-safe input chunks at 64 or more mapping threads.
- NUMA-aware placement for eligible high-thread private genome loads, with
  inherited-policy preservation and shared-memory fallback.
- Transcript-recursion copy elision that copies state only for mutating and
  terminal branches.

## Explicitly Excluded Experiments

The release branch does not contain the experimental `alignReadsMulti` or
persistent prefork worker implementation. It also does not contain the
experimental `outBAMcompressionThreadN` parameter, bundled-htslib threading
changes, per-bin threaded BAM compression, A01 touched-bin reset, the rejected
A02b producer/consumer queue, or A09 LTO/PGO variants. Those prototypes were
excluded or archived because their evidence, lifecycle, output isolation,
resource semantics, or practical effect did not meet the release gate.

The normal STAR alignment command and output interface remain supported.
BlackSTAR-specific alignment changes alter scheduling, affinity recovery,
eligible memory placement, and internal transcript-state ownership without
changing measured biological outputs.

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
6. Randomized paired full-size index and alignment benchmarks, with commands,
   binary identities, timing, memory, input identities, and validation
   artifacts retained.
7. Cumulative alignment qualification against `blackstar.1` for uncompressed
   and compressed input, private and shared indexes, canonical BAM records,
   inherited-affinity recovery, and exact timing-independent outputs.
8. Broad real-sample base-versus-Delta comparison plus single-end and paired-end synthetic added-reference controls.
9. Downstream shadow through deduplication and counting, with inherited order-sensitive consumers corrected or isolated.
10. Checksum-pinned candidate selection, executable mapping canary, explicit rollback, automatic fallback, and both-invalid state preservation.

The benchmark scripts are evidence collectors, not substitutes for correctness tests. A nonidentical `SA` is never labeled equivalent without mapping-level validation.

## Deployment Guidance

Deploy the exact tested binary or a reproducible build from a tagged commit. Record `STAR --version`, the commit, compiler version, compile flags, and an executable checksum. Build base indexes and genome-insert packages on local storage when possible, then publish completed artifacts to shared storage. Never deploy a `.tmp.<pid>.<attempt>` staging directory.

Do not describe Delta loading as runtime-free. The `blackstar.1` short-sample sweep
observed a descriptive 9.22-second mean increase, although alignment and count
content remained exact outside the requested added references.

From a clean tagged checkout, build a release package with:

```bash
JOBS=16 extras/scripts/buildBlackSTARRelease.sh
```

The builder derives `SOURCE_DATE_EPOCH` from the commit, fixes embedded build provenance, verifies the fork version and OpenMP linkage, and writes a binary, `build-info.tsv`, `ldd.txt`, a deterministic tarball, and SHA-256 checksums under `dist/`.

Use `extras/scripts/selectBlackSTAR.sh` for deployment selection. It copies and
revalidates a pinned standalone executable into a generation, then atomically
switches `STATE_DIR/current`. Consumers execute `STATE_DIR/selected-star`.
`--selection-policy fallback-only` provides an explicit rollback path.

# BlackSTAR 1.1.0 Acceptance Record

## Verdict

BlackSTAR 1.1.0 is accepted for an x86-64 Linux release after the protected
release commit passes every required GitHub check and the tagged release
workflow reproduces, compares, attests, and publishes both CPU variants.

This release promotes the Q02 compatibility hardening while retaining every
negative and limiting result. It does not claim a single-end speedup, a
STARsolo speedup, or a TranscriptomeSAM speedup.

This is source and release-artifact qualification. It is not authorization for
integration into an external pipeline or production environment.

## Qualification Anchors

| Boundary | Commit or tag | Role |
| --- | --- | --- |
| Official STAR | `2.7.11b` at `b1edc1208d91a53bf40ebae8669f71d50b994851` | Compatibility oracle and inherited core |
| Prior stable BlackSTAR | `v1.0.0` at `cd3adb609539840bcbcdccbcb2635c4edec03ac2` | Independent-project and rollback boundary |
| Final Q02 runtime source | `ef2a2560013293a3cd93403d876f50a3d5ec759c` | Cross-workload implementation and public evidence |
| Exact local release candidate | `b10c14c6515b62f3e730c4e25a3b6dca20caa508` | Dedicated-node 29-gate cumulative qualification |
| Stable release | `v1.1.0` | Protected release commit and immutable artifact identity |

The code and test-harness tree at the exact local candidate was identical to
the Q02 runtime source; intervening changes were documentation. Release
preparation after that candidate is limited to version identity, release
records, generated architecture status, dynamic CI package paths, and required
check policy. The protected branch and tag remain authoritative for the final
commit and executable checksums.

## Supported Additions

BlackSTAR 1.1.0 adds the following to the 1.0.0 contract:

- record-safe SAM-input handling when automatic high-thread chunk sizing is
  active;
- cgroup-aware memory limits for automatic index strategies;
- restoration of inherited NUMA policy after eligible private genome loading;
- stricter named-sequence annotation, namespace, package-identity, relocation,
  and corruption checks;
- isolated short-read and STARlong build state;
- explicit baseline x86-64 and AVX2 release variants with ISA inspection and
  output-equivalence checks; and
- deterministic `TranscriptomeSAM` primary selection from the run seed and
  stable read ordinal while preserving the complete transcript alignment set
  and later inherited random-stream position.

The conventional genome format remains `2.7.4a`. Full indexes retain official
STAR 2.7.11b compatibility. Overlay and Delta remain BlackSTAR-specific and
require `--genomeLoad NoSharedMemory`.

## Exact-Candidate Qualification

The exact candidate ran on dedicated Slurm node CA2 from two independent clean
source paths. Twenty-nine recorded gates passed:

- baseline and AVX2 release products were byte-identical across independent
  absolute source paths;
- release identity, OpenMP linkage, CPU-target metadata, ISA labels, and
  baseline-versus-AVX2 result equivalence passed;
- 12/12 specialized official-STAR differential modes passed;
- 11/11 focused ASan and UBSan scripts passed;
- all 32 GenomeInsert hardening subchecks passed, including stock-STAR
  full-index compatibility, cross-thread idempotence, package relocation,
  corruption rejection, namespace validation, and insert-only GTF contracts;
- serial, bounded-parallel, and constrained-memory SAindex strategies produced
  identical indexes;
- STARlong built independently and passed the high-thread and official-STARlong
  smoke oracle;
- 12,768,316 paired 76-base public reads matched official STAR in every
  timing-independent metric, splice junction, and gene count;
- the real GRCh38 GFP/GST Delta package matched the previously accepted package
  byte-for-byte and matched its conventional full-index mapping oracle,
  including exactly 100 GFP and 100 GST fragments;
- a full CHM13+ERCC build matched all 14 retained substantive index artifacts
  byte-for-byte; and
- all 28 pre-release architecture figures reproduced exactly.

The full-index and public-read timings from this cumulative run were
descriptive single runs, not replacement performance claims.

## Cross-Workload Evidence

Q02 evaluated ten public workload series and a specialized synthetic matrix.
All 36/36 public pair-level output comparisons and 12/12 specialized checks
passed their applicable metrics, junction, count, SAM, BAM, chimeric,
STARsolo, shared-memory, or transformed-genome oracle.

| Workload | Median paired result | Release interpretation |
| --- | ---: | --- |
| Paired 150-base fragmented | 12.67% less wall time | Replicated gain |
| BySJout | 31.45% less wall time | Replicated gain |
| Chimeric detection | 31.76% less wall time | Replicated gain |
| STARlong direct RNA | 19.48% less wall time | Replicated gain under symmetric seed-limit override |
| TranscriptomeSAM | 1.55% point estimate | Noninferior; no speed claim |
| STARsolo 10x v3 Gene | 3.78% point estimate | Noninferior; no speed claim |
| Single-end 150-base | 1.67% point estimate | Correctness passed; variability gate failed; no speed claim |

Paired 76-base, two-pass, and coordinate-sorted BAM also had positive paired
intervals, but their control-arm variability exceeded the stricter 3 percent
preference for a new release speed claim. Their compatibility evidence remains
accepted.

## Compatibility-Visible Correction

Official STAR chooses a `TranscriptomeSAM` primary alignment from worker-local
random state. A controlled 1-versus-96-thread run therefore produced different
primary flags for the same input.

BlackSTAR 1.1.0 selects from `runRNGseed` and the stable input-read ordinal. The
controlled primary digest was exact across 1 and 96 threads. The complete
transcript alignment set after clearing only flag `0x100`, genomic BAM records,
gene counts, junctions, and timing-independent metrics remained exact. One
legacy random draw is retained so later inherited random choices do not shift.

This is an intentional, documented compatibility correction rather than a
claim of byte identity with one particular official-STAR primary flag.

## Required Protected Checks

The tracked and live branch policy requires all of:

1. `build-and-test`;
2. `compiler-gcc`;
3. `compiler-clang`;
4. `starlong-build-and-smoke`;
5. `release-portability`;
6. `codeql-c-cpp`; and
7. `codeql-python`.

The portability job builds baseline and AVX2 packages on Ubuntu 20.04 with
glibc 2.31, verifies the ABI and ISA floors, and compares variant outputs. The
release workflow repeats two clean builds from different absolute paths before
publication.

## Residual Risk

- Release qualification covers x86-64 Linux. Inherited macOS and non-x86
  source paths are not release-qualified.
- Performance evidence covers the recorded CPU family, toolchains, fixtures,
  local storage, and primarily 96-thread runs.
- Full-index acceleration can use substantially more memory than official
  STAR; host-memory assessment remains mandatory.
- Single-end performance is unresolved and must not be described as faster.
- STARsolo post-mapping work is a deferred profile-first opportunity.
- Both official STARlong and BlackSTAR STARlong fail the real direct-RNA
  fixture under the inherited default `seedPerReadNmax`; the accepted
  comparison raises the limit equally in both arms.
- Overlay and Delta packages are not loadable by official STAR.
- Bundled HTSlib remains old and is not an accepted base for new compression
  work.

## Publication Requirements

1. Merge through a pull request with every required check successful.
2. Verify the protected `main` push reruns the same required checks.
3. Create annotated tag `v1.1.0` at that exact protected commit.
4. Run the release workflow from the tag.
5. Require byte-identical independent package builds for baseline and AVX2.
6. Publish the binaries, deterministic archives, SHA-256 sidecars,
   compatibility metadata, build metadata, linkage metadata, SPDX SBOMs,
   license, attribution, and provenance attestations.
7. Verify public asset digests, executable identities, installation smoke,
   prior-release rollback, and repository recovery evidence.

The historical blackstar.2 record is retained at
[2.7.11b-blackstar.2-acceptance.md](releases/2.7.11b-blackstar.2-acceptance.md).

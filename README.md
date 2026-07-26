# BlackSTAR

BlackSTAR is an independently maintained, performance-oriented successor to
the STAR RNA-seq aligner. It is derived from STAR 2.7.11b and retains the
`STAR` executable name and familiar command-line interface for operational
compatibility.

BlackSTAR is not the official STAR project and is not affiliated with or
endorsed by the original STAR authors. The upstream lineage, license, and
scientific citation are preserved in [ATTRIBUTION.md](ATTRIBUTION.md).

## Release and Development Status

`v1.0.0` is the current stable BlackSTAR release. Repository source can be
ahead of that tag: changes listed under `Unreleased` in
[CHANGELOG.md](CHANGELOG.md), including the Q02 hardening candidate, are not
part of the stable release contract until they are deliberately qualified,
versioned, and tagged.

Use release artifacts for stable adoption. When evaluating an unreleased
checkout, record the full Git commit, `STAR --version-json`, and executable
SHA-256; the compatibility-shaped `STAR --version` token alone does not
distinguish every development revision.

## Why BlackSTAR

The current qualified release adds:

- deterministic, memory-adaptive parallel full-index construction;
- persistent Full, Overlay, and cached Delta named-sequence insertion;
- insert-only GTF support for added references;
- improved high-thread alignment scheduling, affinity recovery, and NUMA
  placement;
- correctness fixes separated from inherited upstream behavior; and
- reproducible release packages, checksums, acceptance evidence, and rollback
  tooling.

Direct qualification against official STAR 2.7.11b measured:

| Workload | Official STAR | BlackSTAR | Result |
| --- | ---: | ---: | ---: |
| Full CHM13 index, 96 threads | 1,256.03 s | 607.18 s | 51.4% less wall time |
| Uncompressed alignment, 96 threads | 82.49 s | 55.20 s | 33.1% less wall time |
| Cold GFP/GST named-sequence addition | 1,225.62 s | 39.04 s | 31.5x speedup |

The full-index improvement used more memory: median peak RSS increased from
52.78 GiB to 77.15 GiB. Alignment gains were workload- and thread-dependent;
the 32-thread result was close to upstream while 64- and 96-thread runs showed
the largest gains. See [docs/PERFORMANCE.md](docs/PERFORMANCE.md) for the
complete claims boundary and [docs/BLACKSTAR_ACCEPTANCE.md](docs/BLACKSTAR_ACCEPTANCE.md)
for release gates.

## Compatibility

Conventional BlackSTAR indexes and alignment outputs retain the established
STAR interfaces. Full indexes have been validated with official STAR 2.7.11b.
Overlay and Delta packages are BlackSTAR-specific and require
`--genomeLoad NoSharedMemory`.

Compatibility is a tested contract, not an assumption. See
[docs/COMPATIBILITY.md](docs/COMPATIBILITY.md) before adopting a
BlackSTAR-specific feature.

## Installation

The latest qualified release is available from the
[BlackSTAR releases page](https://github.com/justinblethrow-cloud/blackSTAR/releases).
Current release automation produces separately labeled baseline x86-64 and
AVX2 `STAR` executables. Each variant includes a deterministic archive,
checksums, compatibility and linkage metadata, build provenance, license, and
upstream attribution. Use the baseline artifact unless AVX2 support is known
for every target host.

Build from source on Linux:

```bash
git clone https://github.com/justinblethrow-cloud/blackSTAR.git
cd blackSTAR/source
make -j"$(nproc)" STAR
./STAR --version
```

The release-gated target is 64-bit x86 Linux with GCC and an OpenMP runtime.
The inherited macOS and non-x86 source paths are not currently release
qualified.

## Named-Sequence Addition

`--runMode genomeInsert` adds transgenes, controls, decoys, plasmids, or other
named FASTA records to an existing index. Full mode writes a conventional
index. Overlay and Delta write compact packages that refer to an unchanged base
index; Delta caches the insertion plan for reuse.

See [docs/STARgenomeInsert.md](docs/STARgenomeInsert.md) for commands, GTF
handling, validation, and package constraints.

## Documentation

- [Compatibility contract](docs/COMPATIBILITY.md)
- [Performance evidence and limitations](docs/PERFORMANCE.md)
- [Q02 cross-workload receipts](docs/benchmarks/Q02-cross-workload-20260726/README.md)
- [Architecture Atlas](docs/architecture/README.md)
- [Release boundary](docs/BLACKSTAR_RELEASE.md)
- [Release acceptance](docs/BLACKSTAR_ACCEPTANCE.md)
- [Versioning policy](docs/VERSIONING.md)
- [Migration from STAR](docs/MIGRATING_FROM_STAR.md)
- [Release process](docs/RELEASE_POLICY.md)

The original STAR manual remains the authoritative reference for inherited
STAR behavior. BlackSTAR documentation takes precedence for BlackSTAR-specific
features and qualified differences.

## Project Participation

Use [GitHub Issues](https://github.com/justinblethrow-cloud/blackSTAR/issues)
for reproducible bugs, compatibility reports, and performance regressions.
Use [GitHub Discussions](https://github.com/justinblethrow-cloud/blackSTAR/discussions)
for questions and design exploration.

Read [CONTRIBUTING.md](CONTRIBUTING.md), [SUPPORT.md](SUPPORT.md), and
[SECURITY.md](SECURITY.md) before opening a report. The project is maintained
on a best-effort basis and does not promise support response times.

## License and Citation

BlackSTAR remains available under the MIT License. Retain the upstream
copyright notice when redistributing source or binaries.

Scientific work using the STAR alignment method should continue to cite the
original STAR publication:

> Dobin A, et al. STAR: ultrafast universal RNA-seq aligner.
> Bioinformatics. 2013;29(1):15-21. doi:10.1093/bioinformatics/bts635.

Also record the exact BlackSTAR release tag, executable checksum, genome index
identity, and command line used for reproducibility.

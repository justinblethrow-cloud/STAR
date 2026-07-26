# BlackSTAR Changelog

This changelog records BlackSTAR project releases. The inherited STAR history
remains available in `CHANGES.md` and `RELEASEnotes.md`.

## Unreleased

- Harden SAM-input chunk sizing and genome-insert annotation and identity
  validation.
- Bound automatic index strategies by cgroup-aware available memory.
- Restore inherited NUMA policy after private genome loading.
- Isolate short-read and STARlong build state and add explicit baseline x86-64
  and AVX2 release variants.
- Add differential and paired benchmark coverage for fragmented, single-end,
  two-pass, BySJout, chimeric, sorted-BAM, transcriptome-BAM, STARsolo, and
  STARlong modes.
- Make TranscriptomeSAM primary-alignment flags deterministic across worker
  schedules while preserving the complete alignment set and later inherited
  random-stream position.

These changes remain outside the current release boundary. The Q02 single-end
timing series failed its variability gate, and neither a release nor an external
deployment is authorized by this entry.

## 1.0.0 - 2026-07-24

- Transition project identity from a GitHub fork to an independently maintained
  successor while preserving upstream attribution.
- Establish governance, security, support, compatibility, versioning, and
  release policies.
- Add BlackSTAR-owned issue intake, pull-request requirements, and migration
  documentation.
- Add a recoverable GitHub fork-detachment procedure and evidence archive.
- Make Linux release artifacts independent of the absolute checkout path,
  including bundled HTSlib compilation.

No alignment, indexing, or output behavior is changed by the project-identity
work alone.

## 2.7.11b-blackstar.2 - 2026-07-24

- Added the qualified high-thread alignment stack.
- Preserved all accepted full-index and named-sequence insertion behavior from
  `blackstar.1`.
- Published deterministic Linux x86-64 artifacts and checksums.

See [the release notes](docs/releases/2.7.11b-blackstar.2-release-notes.md).

## 2.7.11b-blackstar.1 - 2026-07-16

- Added deterministic parallel full-index construction.
- Added persistent Full, Overlay, and Delta named-sequence insertion.
- Added insert-only GTF support, strict package identity, and atomic
  publication.
- Added inherited upstream correctness fixes and deployment rollback tooling.

See [the acceptance record](docs/releases/2.7.11b-blackstar.1-acceptance.md).

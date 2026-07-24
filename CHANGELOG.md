# BlackSTAR Changelog

This changelog records BlackSTAR project releases. The inherited STAR history
remains available in `CHANGES.md` and `RELEASEnotes.md`.

## Unreleased

- Transition project identity from a GitHub fork to an independently maintained
  successor while preserving upstream attribution.
- Establish governance, security, support, compatibility, versioning, and
  release policies.
- Add BlackSTAR-owned issue intake, pull-request requirements, and migration
  documentation.
- Add a recoverable GitHub fork-detachment procedure and evidence archive.

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

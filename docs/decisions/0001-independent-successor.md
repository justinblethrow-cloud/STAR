# ADR 0001: Operate BlackSTAR as an Independent Successor

- Status: Accepted for implementation
- Date: 2026-07-24
- Decision owner: BlackSTAR maintainer

## Context

BlackSTAR began as a performance and genome-insert fork of STAR 2.7.11b.
Upstream contribution attempts did not provide a dependable path for
maintaining the qualified BlackSTAR feature set. BlackSTAR now has an
independent release process, compatibility tests, architecture evidence, and
substantial validated behavior beyond the pinned upstream baseline.

Remaining in GitHub's fork network presents BlackSTAR primarily as a patch set
even though releases, support, compatibility decisions, and future development
must be maintained independently.

## Decision

BlackSTAR will operate as an independently maintained, performance-oriented
successor derived from STAR 2.7.11b.

The project will:

- preserve upstream history, license, attribution, and scientific citation;
- state that it is unofficial and not endorsed by upstream;
- retain the `STAR` executable and default interface where compatibility is
  qualified;
- version BlackSTAR releases independently while recording the STAR
  compatibility base and genome format separately;
- detach from the GitHub fork network only after a verified recovery archive;
- use `main` as the protected default branch; and
- classify inherited upstream behavior separately from BlackSTAR regressions.

## Alternatives

Continue as a conventional fork. This minimizes repository administration but
misstates the maintenance and release relationship.

Wait for upstream adoption. This leaves qualified features and fixes dependent
on an uncertain external process.

Rewrite or rename the executable immediately. This would create unnecessary
pipeline compatibility risk and is rejected.

## Consequences

BlackSTAR assumes responsibility for project governance, security intake,
support boundaries, releases, compatibility, and future maintenance. Fork
detachment is permanent and loses GitHub metadata unless it is exported and
recreated. The project must remain precise about lineage and must not imply
official STAR status.

This decision does not authorize integration into an external production
pipeline.

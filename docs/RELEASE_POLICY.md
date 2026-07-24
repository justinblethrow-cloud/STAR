# BlackSTAR Release Policy

Stable BlackSTAR releases are immutable, reproducible, evidence-backed
artifacts from the protected default branch.

## Release Classes

- Patch releases correct compatible behavior, security, packaging,
  documentation, or measured regressions.
- Minor releases add backward-compatible features or material performance work.
- Major releases change a documented compatibility contract.
- Release candidates are explicitly marked prerelease and are not stable
  deployment targets.

## Required Source Gates

Every stable release must:

1. use a clean, reviewed commit on protected `main`;
2. pass required CI, architecture validation, and focused sanitizers;
3. pass genome-insert and SAindex strategy suites;
4. verify the declared CLI, index, and output compatibility surfaces;
5. classify inherited upstream defects separately from BlackSTAR regressions;
6. update the changelog, release boundary, acceptance record, and limitations;
7. record compiler, flags, OpenMP linkage, commit, and source-date epoch; and
8. reproduce every package artifact across two clean builds rooted at different
   absolute source paths.

Performance-affecting releases additionally require seeded, order-balanced
candidate/control pairs on a controlled host. Raw outputs remain outside Git;
bounded receipts, identities, methodology, and correctness results are
committed.

## Published Artifacts

A Linux x86-64 release publishes:

- the `STAR` executable;
- a deterministic archive;
- archive and executable SHA-256 values;
- `build-info.tsv`;
- runtime linkage metadata;
- an SBOM;
- build provenance or attestation;
- license and upstream-attribution notices;
- release notes; and
- an acceptance record.

The release tag points to the exact source commit. Assets are never replaced
silently; a correction requires a new release.

## Branch and Review Policy

Changes enter `main` through pull requests. Required checks use stable names so
branch protection cannot be bypassed by renaming a workflow. Linear history,
conversation resolution, no force pushes, and no branch deletion remain
enforced.

While BlackSTAR has one maintainer, pull requests may be merged after all
required checks pass without an independent approval. A second active
maintainer triggers a governance review and a one-approval CODEOWNERS policy.

## Security Releases

Credible vulnerabilities use GitHub's private advisory workflow. The final
release discloses affected versions and mitigation without exposing private
reporter information. Third-party vulnerabilities are identified separately
from BlackSTAR-authored defects.

## Rollback

The prior stable release and checksums remain available. A release must not be
published until its fallback binary and runtime canary are identified.
Deployment rollback uses `extras/scripts/selectBlackSTAR.sh`; it is separate
from repository rollback.

# Contributing to BlackSTAR

BlackSTAR accepts focused bug fixes, compatibility improvements,
documentation, tests, and performance work. Contributions must preserve
scientific correctness, deterministic output contracts, and the documented
compatibility boundary.

Participation is governed by [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## Before Opening an Issue

Use the latest qualified BlackSTAR release and retain:

- the exact `STAR --version` output and executable checksum;
- the complete STAR command line;
- `Log.out` and `Log.final.out`;
- operating system, compiler or package source, CPU, RAM, and storage details;
- input and index identities that can be shared safely; and
- whether official STAR 2.7.11b reproduces the behavior.

Do not upload private sequencing data, credentials, or customer information.
Build the smallest public or synthetic reproducer that retains the failure.

## Classifying Behavior

Reports and pull requests must distinguish:

- **Inherited upstream behavior**: official STAR 2.7.11b reproduces it.
- **BlackSTAR regression**: official STAR does not reproduce it and BlackSTAR
  violates the compatibility contract.
- **BlackSTAR feature behavior**: the report concerns an explicitly
  BlackSTAR-only interface such as Overlay or Delta.
- **Unresolved provenance**: a reproducer is not yet sufficient to classify the
  origin.

This classification prevents inherited STAR debt from being presented as a
BlackSTAR regression while still allowing BlackSTAR to fix upstream defects.

## Development Setup

Build the default binary:

```bash
make -C source -j"$(nproc)" STAR
source/STAR --version
```

Build the deterministic release package from a clean commit:

```bash
JOBS=16 extras/scripts/buildBlackSTARRelease.sh
```

Run the focused release checks relevant to a change. Common entry points
include:

```bash
extras/tests/scripts/testPackedArray.sh
extras/tests/scripts/testSuffixComparator.sh
extras/tests/scripts/testTranscriptInitialization.sh
extras/tests/scripts/testJunctionAlignment.sh
extras/tests/scripts/testReadChunkConfig.sh
extras/tests/scripts/testGenomeInsertHardening.sh
extras/tests/scripts/testSAindexParallel.sh
extras/tests/scripts/testBenchmarkHarness.sh
```

Run architecture validation after changing diagrams or claims:

```bash
python3 extras/docs/render_architecture.py --check --svg-only
python3 extras/docs/validate_architecture.py
```

## Pull Requests

Every pull request must:

1. State the user-visible behavior and its origin classification.
2. Describe correctness and compatibility risks.
3. Add or update tests that would fail without the change.
4. Record exact commands used for validation.
5. Avoid unrelated refactoring and generated-file churn.
6. Update documentation and the changelog when behavior changes.
7. Include matched, order-balanced evidence for performance claims.

Performance work must compare the candidate with a pinned control binary using
identical inputs, indexes, output modes, storage placement, and thread
allocation. Report wall time, CPU utilization, peak RSS, cache state, and
output-equivalence gates. A faster result with unexplained output differences
is a failed experiment.

Generated architecture SVG and PDF artifacts must be regenerated from their
Mermaid sources. Large raw benchmark outputs should remain outside Git; commit
bounded receipts and the scripts needed to reproduce them.

## Review and Licensing

Maintainers may request a smaller reproducer, additional platform evidence, or
an upstream comparison before review. Acceptance requires passing CI and the
release gates appropriate to the change.

By submitting a contribution, you certify that you have the right to provide
it under the repository's MIT License. No contributor license agreement is
currently required.

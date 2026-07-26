# Q02 Cross-Workload Qualification Receipts

This directory preserves the bounded, machine-readable evidence behind
[Q02](../../experiments/Q02-cross-workload-generalization.md) and the
[Q02 evidence ledger](../../architecture/evidence/alignment-Q02-cross-workload-20260726.tsv).
Q02 compared official STAR 2.7.11b with an unreleased BlackSTAR hardening
candidate across ten public workload series.

## Contents

- `qualification-receipt.tsv` records source commits, executable identities,
  toolchain and host context, functional gates, limiting evidence, and the
  unreleased disposition.
- `results/<mode>/contract.json` records the predeclared benchmark inputs,
  executable and input hashes, run order, resource limits, and tool identities.
- `results/<mode>/pairs.tsv` preserves each measured order-balanced pair.
- `results/<mode>/result.json` preserves the aggregate statistics and gate
  decisions.
- `results/transcriptome-thread-invariance/result.json` preserves the separate
  1-versus-96-thread TranscriptomeSAM determinism check.
- `SHA256SUMS` authenticates every retained receipt other than the manifest
  itself.

## Evidence Boundary

These receipts are sufficient to audit the published aggregate values,
acceptance decisions, negative evidence, and exact identities of the measured
inputs and executables. The recorded absolute paths describe the original
ephemeral benchmark environment and are retained as provenance; they are not
expected to resolve in a fresh clone.

Raw FASTQs, genome indexes, binaries, logs, BAMs, STARsolo output trees, and
other large generated artifacts are intentionally excluded from Git. Their
content identities remain in the contracts. The Q02 harness under
`benchmarks/generalization/` provides the runnable procedure, but reproducing
the measurements requires reacquiring the identified public fixtures and
building the identified source commits.

## Candidate Boundary

Most runtime series used the phase-candidate executable with SHA-256
`fb82a5cf2fb0fdd285f9d211045d3b11ad0d5a91d360bda3874c639dc6848489`.
TranscriptomeSAM and exclusive-node single-end qualification used the later
clean archive-derived executable with SHA-256
`297db5482236d970b2b19fed6016c1f9981d973fdbd6784a1700f1a7bba40635`.
The distinction is preserved in the qualification receipt and individual
contracts.

Q02 did not authorize a release, version bump, GitHub update, or external
deployment. Its results describe the exact measured binaries, not an
unmeasured future release artifact.

# BlackSTAR 1.1.0 Source-Candidate Qualification

This bounded public receipt records the dedicated-node qualification of the
source candidate later prepared for BlackSTAR 1.1.0.

The exact candidate was
`b10c14c6515b62f3e730c4e25a3b6dca20caa508`. Its code and test-harness tree
was exact to Q02 runtime source
`ef2a2560013293a3cd93403d876f50a3d5ec759c`; later files at the candidate were
documentation. Release preparation after this candidate is restricted to
version identity, release records, architecture status, CI package-path
generalization, and required-check policy.

## Verdict

All 29 locally recorded gates passed on a dedicated x86-64 Linux Slurm node.
This source-candidate receipt does not replace protected-branch CI or tagged
release-artifact qualification.

## Covered Gates

- Two independent clean release roots and byte-identical baseline and AVX2
  products.
- Release identity, OpenMP linkage, ISA labels, and cross-variant output
  equivalence.
- 12 specialized official-STAR differential modes.
- 11 focused ASan and UBSan scripts.
- 32 GenomeInsert hardening subchecks, including stock-STAR full-index
  compatibility.
- Serial, bounded-parallel, and constrained-memory SAindex identity.
- STARlong build and official-STARlong smoke comparison.
- Public paired-read correctness, real GFP/GST Delta equivalence, and full
  CHM13+ERCC substantive-index identity.
- Deterministic architecture rendering.

The full-index and paired-read timings were single-run descriptive receipts,
not replacement speed claims. The Q02 package remains authoritative for
replicated cross-workload performance evidence.

## Files

- `qualification-receipt.tsv`: bounded identities and outcomes.
- `SHA256SUMS`: integrity manifest for this public receipt.

Large source checkouts, binaries, indexes, alignments, and raw logs are not
tracked.

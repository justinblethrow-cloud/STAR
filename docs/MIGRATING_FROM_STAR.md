# Migrating from Official STAR

BlackSTAR retains the `STAR` executable and conventional command-line shape so
that evaluation can begin without redesigning an RNA-seq workflow.

## Recommended Evaluation

1. Pin official STAR and BlackSTAR by path, version, commit, and executable
   SHA-256.
2. Reuse the same conventional genome index for the first alignment comparison.
3. Run one representative sample through both binaries with identical
   parameters and resources.
4. Compare timing-independent metrics, junctions, gene counts, and canonical
   BAM records.
5. Measure wall time, CPU, peak RSS, storage behavior, and thread placement.
6. Retain official STAR as an explicit fallback until downstream validation is
   complete.

Prefer an immutable BlackSTAR release tag for adoption. If the evaluation
requires an unreleased candidate, pin its full commit and executable checksum,
treat its `Unreleased` changelog entry as outside the stable support boundary,
and repeat the release gates relevant to the intended workflow.

Do not compare runs that use different reference files, annotations, output
modes, decompression commands, thread counts, storage tiers, or concurrent
system load.

## Full Index Generation

BlackSTAR accepts the established `--runMode genomeGenerate` interface.
Parallel index construction can use substantially more memory. The direct
CHM13 qualification measured 77.15 GiB median peak RSS for BlackSTAR versus
52.78 GiB for official STAR.

Capacity-gate the host before choosing the accelerated path. BlackSTAR retains
a lower-memory strategy internally, but operators should still monitor RSS and
avoid memory overcommit.

## Named-Sequence Addition

Use `--runMode genomeInsert` when adding new named FASTA records such as
transgenes or controls to a prebuilt base.

- Full mode produces a self-contained conventional index.
- Overlay mode stores inserted inputs and reconstructs insertions at startup.
- Delta mode additionally caches the insertion plan.

Insert-only GTF annotations may describe exons on added sequences. Existing
base annotations are retained automatically. See
[STARgenomeInsert.md](STARgenomeInsert.md).

## Operational Rollback

Deploy a standalone executable selected by checksum rather than overwriting an
existing STAR binary in place. The included selector supports a pinned
candidate, pinned fallback, runtime canary, and atomic generation switch:

```bash
extras/scripts/selectBlackSTAR.sh --help
```

Publication of a BlackSTAR release does not authorize modification of an
external pipeline. Each production environment requires its own approval,
shadow run, downstream comparison, and rollback owner.

## Repository Migration

The independently maintained repository uses `main` as its default branch.
Existing clones created while the repository used `master` can update with:

```bash
git fetch origin
git branch -m master main
git branch --set-upstream-to=origin/main main
git remote set-head origin --auto
```

Clones that use another local working branch need only fetch and update the
remote HEAD. Published release tags remain unchanged.

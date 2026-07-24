# Official STAR 2.7.11b versus BlackSTAR blackstar.2

These bounded receipts preserve the direct cumulative comparison summarized in
`docs/PERFORMANCE.md`.

The source benchmark retained full commands, binary and input identities,
timing files, output comparisons, cache-residency receipts, and raw outputs.
Large raw outputs are not committed. The public receipts contain the aggregate
values needed to audit every published performance claim.

## Correctness Gates

- Full index: 42/42 byte comparisons passed.
- Mapping matrix: 24/24 pair comparisons passed.
- Unsorted BAM: canonical records matched.
- Cold-cache entry: 6/6 gates measured zero resident input pages.
- Delta determinism: 57/57 cold-versus-warm artifact checks passed.
- Added-reference alignment: metrics, junctions, counts, and canonical BAM
  records matched; GFP and GST each counted 100 fragments.

## Interpretation

The index result trades higher memory for lower wall time. Alignment benefit is
thresholded, with little change at 32 threads and the largest improvement at
64 and 96 threads. Cold-cache means input file pages were evicted and measured;
it does not mean a power-cycled host.

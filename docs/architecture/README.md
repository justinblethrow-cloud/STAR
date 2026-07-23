# BlackSTAR Architecture Atlas

This directory is the canonical, public explanation of upstream STAR, accepted
BlackSTAR changes, measured results, rejected experiments, and the experimental
roadmap. It is deliberately neutral: organization-specific examples and
customer-derived evidence belong in an untracked internal derivative.

## Reading Order

1. [Upstream STAR workflow](diagrams/svg/F01-upstream-workflow.svg)
2. [Upstream genome generation](diagrams/svg/F02-genome-generate.svg)
3. [Upstream alignment algorithm](diagrams/svg/F03-align-reads.svg)
4. [Index and runtime data model](diagrams/svg/F04-data-model.svg)
5. [Threading and memory map](diagrams/svg/F05-threading-memory.svg)
6. [Accepted BlackSTAR change map](diagrams/svg/F06-change-map.svg)
7. [Full-index benchmark](diagrams/svg/F07-full-index-benchmark.svg)
8. [Full, Overlay, and Delta modes](diagrams/svg/F08-insert-modes.svg)
9. [Delta virtual suffix array](diagrams/svg/F09-delta-virtual-sa.svg)
10. [Atomic publication and validation](diagrams/svg/F10-publication.svg)
11. [Qualified benchmark summary](diagrams/svg/F11-qualified-benchmarks.svg)
12. [Correctness and release boundary](diagrams/svg/F12-release-boundary.svg)
13. [Accepted, rejected, and superseded work](diagrams/svg/F13-history.svg)
14. [Experimental roadmap](diagrams/svg/F14-roadmap.svg)
15. [Rejected A01 touched-window-bin reset](diagrams/svg/F15-touched-window-bins.svg)
16. [A02 input-dispatch attribution](diagrams/svg/F16-input-dispatch-diagnostics.svg)
17. [H01 alignment-affinity recovery](diagrams/svg/F17-alignment-affinity-recovery.svg)
18. [A02 adaptive input chunks](diagrams/svg/F18-adaptive-input-chunks.svg)
19. [A05 NUMA-aware private genome placement](diagrams/svg/F19-numa-placement.svg)
20. [A06 transcript recursion copy elision](diagrams/svg/F20-transcript-recursion.svg)

The editable sources are under `diagrams/src/`. The generated SVG and PDF
exports are presentation-ready but are never the source of truth.
`figures.json` records provenance, maturity, evidence, captions, and alt text.
`claims.tsv` binds quantitative statements to small tracked evidence records.

## Status Vocabulary

- **Upstream**: inherited behavior in STAR 2.7.11b.
- **Accepted**: included in the qualified BlackSTAR release boundary.
- **Accepted Labs candidate**: passed its experiment gates but remains outside
  the qualified release boundary.
- **Experimental**: implemented or proposed in Labs, but not qualified.
- **Rejected**: measured and intentionally excluded.
- **Superseded**: historically useful evidence replaced by stronger evidence.

Color alone never conveys status. Every figure uses explicit text labels and
the validator requires nonempty alt text.

## Rebuilding

Install the pinned documentation dependencies and render:

```bash
npm ci --prefix extras/docs
python3 extras/docs/render_architecture.py
python3 extras/docs/validate_architecture.py
```

If automatic browser discovery selects an unusable system wrapper, point the
renderer at a local Puppeteer JSON file with `PUPPETEER_CONFIG=/path/config`.
The tracked configuration remains the CI default.

CI renders into a temporary directory and compares generated SVG content with
the tracked exports. PDF files are convenience exports and are checked for
presence and provenance, but not byte identity because Chromium embeds
environment-dependent PDF metadata.

## Public and Internal Separation

Public material must use public or synthetic inputs and anonymous workload
descriptions. The validator rejects absolute local paths, known internal order
identifiers, email addresses, and internal branding in this directory.

An internal presentation builder may consume `figures.json`, `claims.tsv`,
and the generated SVG files, then apply the organization design system in a
separate untracked directory. It must not modify the canonical neutral figures.

## Algorithm Attribution

Figure F03 is an original redraw based on the STAR algorithm description in:

Dobin A, et al. STAR: ultrafast universal RNA-seq aligner.
*Bioinformatics*. 2013;29(1):15-21.
<https://doi.org/10.1093/bioinformatics/bts635>

It does not reproduce the paper's artwork.

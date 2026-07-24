# Attribution and Lineage

BlackSTAR is derived from the STAR RNA-seq aligner.

## Upstream Baseline

- Project: STAR
- Upstream repository: https://github.com/alexdobin/STAR
- Baseline release: `2.7.11b`
- Baseline commit: `b1edc1208d91a53bf40ebae8669f71d50b994851`
- Original author: Alexander Dobin
- License: MIT

The original copyright and MIT license are retained in [LICENSE](LICENSE).
BlackSTAR's independent maintenance does not imply endorsement by Alexander
Dobin, Cold Spring Harbor Laboratory, or another upstream contributor or
institution.

## Scientific Citation

Research using the STAR alignment method should cite:

Dobin A, Davis CA, Schlesinger F, Drenkow J, Zaleski C, Jha S, Batut P,
Chaisson M, Gingeras TR. STAR: ultrafast universal RNA-seq aligner.
Bioinformatics. 2013;29(1):15-21. doi:10.1093/bioinformatics/bts635.

BlackSTAR users should additionally record the exact BlackSTAR release, commit
or package checksum, index identity, and command line.

## BlackSTAR Scope

BlackSTAR maintains the inherited alignment core while adding independently
qualified indexing, named-sequence insertion, high-thread runtime, correctness,
packaging, and verification work. The precise boundary is documented in
[docs/BLACKSTAR_RELEASE.md](docs/BLACKSTAR_RELEASE.md) and
[docs/COMPATIBILITY.md](docs/COMPATIBILITY.md).

Upstream-origin and BlackSTAR-origin behavior are tracked separately in issue
triage and release records.

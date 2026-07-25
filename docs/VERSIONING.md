# BlackSTAR Versioning

BlackSTAR versions the maintained project independently while preserving
machine-readable STAR ancestry and genome-format identities.

## Version Surfaces

| Surface | Example | Purpose |
| --- | --- | --- |
| BlackSTAR release | `1.0.0` | Independent API, support, and release boundary |
| STAR compatibility base | `2.7.11b` | Pinned inherited behavior oracle |
| BlackSTAR lineage identity | `2.7.11b-blackstar.3` | Transitional executable identity |
| Genome format | `2.7.4a` | Conventional index loading compatibility |
| CPU target | `baseline` or `avx2` | Release artifact ISA requirement |

These values must not be collapsed into one string. A project release can
change without changing the genome format, and a compatibility-base update can
require a new BlackSTAR major or minor release.

## Semantic Versioning

BlackSTAR release tags use `vMAJOR.MINOR.PATCH`.

- **MAJOR**: a documented incompatible change to BlackSTAR's supported CLI,
  package, index, or output contract.
- **MINOR**: a backward-compatible feature or material performance capability.
- **PATCH**: a compatible correctness, security, documentation, packaging, or
  performance correction.

Prereleases use tags such as `v1.1.0-rc.1`. Published tags are immutable.

## Transition to 1.0

`2.7.11b-blackstar.1` and `.2` remain historical release identities.
`v1.0.0` establishes the independently maintained project contract.

For the 1.x transition, the release package and manifest carry the independent
version while `STAR --version` retains a lineage-shaped value for wrappers that
expect a STAR-like token. A structured version command and `build-info.tsv`
must report the independent, compatibility, genome-format, and CPU-target
identities. Changing the legacy `--version` token requires a separate
compatibility survey and release decision.

## Compatibility-Base Updates

Adopting a later STAR-derived baseline, another fork, or a replacement
implementation requires:

1. a pinned source and license audit;
2. a behavioral and index-format diff;
3. the full BlackSTAR correctness suite;
4. migration documentation; and
5. an explicit versioning decision.

The status of upstream development does not remove these requirements.

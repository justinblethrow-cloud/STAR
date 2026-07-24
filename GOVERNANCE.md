# BlackSTAR Governance

## Project Status

BlackSTAR is an independently maintained open-source successor derived from
STAR 2.7.11b. It is not the official STAR project and does not represent the
original STAR authors or institutions.

The project currently uses a maintainer-led model. The repository owner is the
initial maintainer and has final responsibility for releases, security
decisions, compatibility policy, and repository administration. This model can
be revised when additional maintainers have demonstrated sustained,
high-quality participation.

## Decision Process

Material changes are proposed through GitHub pull requests. Decisions are based
on:

1. biological and output correctness;
2. compatibility with documented STAR behavior;
3. deterministic and reproducible evidence;
4. operational safety and resource semantics;
5. maintainability; and
6. measured benefit relative to complexity.

Performance evidence does not override correctness. A proposal can be rejected
when its effect is too narrow, its lifecycle is unsafe, its outputs differ
without explanation, or its maintenance burden exceeds the demonstrated
benefit.

Substantial interface, index-format, governance, or support-policy changes
require an architecture decision record under `docs/decisions/`. Accepted
decisions identify alternatives, evidence, compatibility impact, and reversal
strategy.

## Maintainer Responsibilities

Maintainers:

- apply the Code of Conduct consistently;
- classify inherited upstream issues separately from BlackSTAR regressions;
- require tests and evidence proportional to the change;
- preserve attribution and licensing;
- publish exact release commits, checksums, provenance, and limitations;
- protect the default branch and release credentials; and
- disclose conflicts of interest relevant to project decisions.

Maintainers do not promise support response times or acceptance of proposed
features.

## Releases

Only commits that pass [docs/RELEASE_POLICY.md](docs/RELEASE_POLICY.md) may be
published as stable releases. The protected default branch is the source of
release tags. Rewriting published history, moving release tags, and replacing
assets without a new release are prohibited.

Security fixes may use an abbreviated private process, but the final release
must still publish provenance, checksums, compatibility impact, and an
appropriate disclosure.

## Changing Governance

Governance changes use the normal pull-request process and require an explicit
maintainer decision. A future multi-maintainer model should define nomination,
review authority, inactivity, removal, and tie-breaking before granting release
or administrative access.

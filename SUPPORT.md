# BlackSTAR Support

BlackSTAR is maintained on a best-effort basis. There is no guaranteed response
time, service-level agreement, or commitment to support every STAR platform,
parameter combination, or downstream wrapper.

## Where to Ask

- Use GitHub Issues for reproducible bugs, compatibility failures, and
  performance regressions.
- Use GitHub Discussions for questions, design exploration, and operational
  experience that is not yet a reproducible defect.
- Use GitHub private vulnerability reporting for security issues.

Do not send BlackSTAR reports to the original STAR authors unless the behavior
has independently been reproduced with official STAR and is relevant to that
project.

## Supported Release Boundary

The latest stable BlackSTAR release is supported on the platform stated in its
release notes. At present, the release-gated target is x86-64 Linux with an
OpenMP runtime. Older BlackSTAR releases receive documentation support only
unless a maintainer explicitly backports a critical correction.

Inherited STAR behavior outside BlackSTAR's qualified platform remains
best-effort. macOS, non-x86 architectures, long-read performance, network
storage, and every downstream workflow are not implied to be qualified merely
because the source compiles.

## Required Diagnostic Information

A useful report includes:

- exact BlackSTAR version and executable SHA-256;
- command line and relevant environment variables;
- `Log.out` and `Log.final.out`;
- CPU, RAM, operating system, filesystem, and storage placement;
- index identity and generation command;
- a minimal public or synthetic reproducer; and
- the result of the same reproducer under official STAR 2.7.11b when feasible.

Remove credentials, customer identifiers, private sequence data, and
presigned URLs before posting.

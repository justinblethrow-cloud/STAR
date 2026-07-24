# BlackSTAR Security Policy

## Supported Versions

Security fixes target the latest stable BlackSTAR release and the current
default branch. Older releases are not guaranteed to receive backports.

## Reporting a Vulnerability

Use GitHub private vulnerability reporting for the BlackSTAR repository. Do
not open a public issue for suspected memory corruption, arbitrary file access,
unsafe archive behavior, command injection, credential exposure, or another
issue that could put users or data at risk.

Include:

- affected release or commit;
- platform and build details;
- minimal reproduction steps;
- expected security impact;
- whether official STAR reproduces the behavior; and
- any proposed embargo constraints.

Do not include real customer data or credentials. Synthetic inputs are
preferred.

The project is maintained on a best-effort basis and cannot promise an
acknowledgement or remediation deadline. Maintainers will avoid unnecessary
public disclosure while validating a credible report and will coordinate a
release and advisory when warranted.

## Release Integrity

Stable releases publish checksums and build provenance. Consumers should pin a
release tag and verify the downloaded artifact checksum. A GitHub release,
container tag, or package name alone is not sufficient provenance.

Security reports about bundled third-party code will be classified separately
from BlackSTAR-authored code and coordinated with the relevant upstream when
practical.

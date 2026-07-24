# BlackSTAR Promotion Runbook

This runbook promotes an already accepted BlackSTAR executable. It does not
authorize deployment and does not replace environment-specific pipeline tests.

## Qualification Anchors

| Field | Value |
|---|---|
| Version | `2.7.11b-blackstar.2` |
| Prior qualified release | `2.7.11b-blackstar.1` at `821457378fa38bfb23b061b8f11ee0a09431dda7` |
| Cumulative qualification record | `9998c445c5b87adacd2a4663bd964ce744aea300` |
| Alignment benchmark source | `7b31a5fe5cb966c9146b99d5ca1ad81ea9c81cdb` |
| Alignment benchmark executable SHA-256 | `6b08a7925022c6a03813660c69d0b0bd1ed98a377ac9c1136dd987e3d4c8f85e` |
| Platform | x86-64 Linux |
| Required linkage | `libgomp` or `libomp` |

The deployable executable is a clean release build from the final promotion
commit. Verify that runtime source changes relative to the qualification
record are limited to the release version, then take the deployment checksum
from that build's `build-info.tsv`. Embedded Git provenance means a
documentation-only commit still changes the executable checksum. Record the
final checksum in release metadata rather than editing it back into the source
commit.

## Prerequisites

- Pin a known-good standalone stock STAR executable by path, version, and
  SHA-256. Do not select a launcher that depends on sibling executables unless
  its resolved standalone target is used.
- Place selector state on a local filesystem that supports `flock`, symbolic
  links, and atomic same-filesystem rename.
- Provide an executable canary script and pin its SHA-256. The selector calls
  it with one argument: the STAR executable to test.
- Make the canary load a known-good stock-built index, align deterministic
  mapped and unmapped reads, and fail on any unexpected record or count.
- Keep the existing stock executable and base indexes unchanged during the
  canary window.

## Select Candidate

```bash
extras/scripts/selectBlackSTAR.sh \
  --candidate /opt/blackstar/STAR \
  --candidate-sha256 CANDIDATE_SHA256 \
  --candidate-version 2.7.11b-blackstar.2 \
  --fallback /opt/star-stock/STAR \
  --fallback-sha256 STOCK_SHA256 \
  --fallback-version 2.7.11b \
  --state-dir /var/lib/blackstar-selector \
  --canary-script /opt/blackstar/runtime-canary.sh \
  --canary-sha256 CANARY_SHA256 \
  --require-candidate-openmp 1
```

The command exits successfully after selecting either a healthy candidate or a
healthy fallback. Read `status.tsv` and require `decision` to be `candidate`
before starting a candidate canary workload. If neither binary is healthy, the
command exits nonzero and leaves the previously published generation unchanged.

Configure the pipeline to execute:

```text
/var/lib/blackstar-selector/selected-star
```

Do not bypass the selector with an unpinned binary path.

## Explicit Rollback

```bash
extras/scripts/selectBlackSTAR.sh \
  --candidate /opt/blackstar/STAR \
  --candidate-sha256 CANDIDATE_SHA256 \
  --candidate-version 2.7.11b-blackstar.2 \
  --fallback /opt/star-stock/STAR \
  --fallback-sha256 STOCK_SHA256 \
  --fallback-version 2.7.11b \
  --state-dir /var/lib/blackstar-selector \
  --canary-script /opt/blackstar/runtime-canary.sh \
  --canary-sha256 CANARY_SHA256 \
  --require-candidate-openmp 1 \
  --selection-policy fallback-only
```

Confirm `decision=fallback` in `status.tsv`, then stop launching new candidate
jobs. Running jobs retain the executable image and files they already opened;
the selector controls subsequent invocations.

## Rollout Stages

1. Shadow one representative sample with stock and BlackSTAR against the same
   base index. Compare timing-independent STAR metrics, normalized full SAM
   records, junctions, existing gene counts, and special rows.
2. Repeat through sorting, UMI deduplication, and final counting. If the
   downstream UMIcollapse path uses `MapQualMerge`, deploy deterministic stable
   tie-breakers or canonicalize input order first.
3. Canary a bounded low-risk batch. Keep stock outputs until comparisons pass.
4. Increase exposure in explicit steps while retaining stock fallback and
   per-stage telemetry.
5. Promote the default only after the observation window passes. Keep the
   accepted generation and its `status.tsv` for audit and rollback.

## Promotion Checks

Require all of the following before each increase in exposure:

- no unexpected exit, fatal STAR log, or selector fallback;
- exact timing-independent mapping metrics for shadowed jobs;
- no unexplained normalized alignment, junction, existing-gene, or special-row
  differences;
- expected GFP/GST or other inserted-reference counts in positive controls;
- wall time and peak memory inside environment-specific bounds;
- no stale-base, incomplete-package, or checksum validation error;
- selector `selected_sha256` equals the accepted artifact checksum.

Delta users must also monitor the measured fixed startup cost. The accepted
24-sample sweep observed 62.26 seconds for base and 71.48 seconds for Delta on
short jobs; this was descriptive, not order-balanced.

## Companion UMI Hardening

The BlackSTAR shadow found inherited order sensitivity in downstream
UMIcollapse, not different STAR alignment content. The companion DUMI commit is
`2ef54b287f83864ee83b3541a7a29236547b72df`; its accepted JAR SHA-256 is
`91b80e924a1200d21f71dcafc0ea5b7ca01ffbb9b3fe78e0751279443647bebc`.

Stock, repeated stock, BlackSTAR base, and BlackSTAR Delta produced identical
non-synthetic deduplicated output after this patch. Delta then changed only the
requested GFP/GST rows. Treat this as a downstream reproducibility control, not
as part of the BlackSTAR executable.

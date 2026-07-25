#!/usr/bin/env python3
"""Compare timing-independent outputs from a BlackSTAR generalization pair."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import tempfile

from compareAlignmentRuns import (
    canonical_bam_digest,
    canonical_sam_digest,
    normalized_text_digest,
    parse_log,
)


MODES = {
    "paired-mapping",
    "single-mapping",
    "two-pass",
    "bysjout",
    "chimeric",
    "sorted-bam",
    "transcriptome-bam",
    "starsolo",
    "starlong",
}


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def add_presence_digest(
    checks: list[dict[str, object]],
    name: str,
    left: Path,
    right: Path,
    digest_function,
) -> None:
    left_exists = left.is_file()
    right_exists = right.is_file()
    check: dict[str, object] = {
        "name": name,
        "passed": left_exists and right_exists,
    }
    if left_exists and right_exists:
        left_digest = digest_function(left)
        right_digest = digest_function(right)
        check.update(
            passed=left_digest == right_digest,
            baseline_sha256=left_digest,
            candidate_sha256=right_digest,
        )
    checks.append(check)


def add_optional_text(
    checks: list[dict[str, object]],
    name: str,
    left: Path,
    right: Path,
    sort_lines: bool,
) -> None:
    if not left.exists() and not right.exists():
        return
    add_presence_digest(
        checks,
        name,
        left,
        right,
        lambda path: normalized_text_digest(path, sort_lines),
    )


def compare_solo_tree(
    checks: list[dict[str, object]], baseline: Path, candidate: Path
) -> None:
    baseline_root = baseline / "star.Solo.out"
    candidate_root = candidate / "star.Solo.out"
    baseline_files = (
        sorted(
            path.relative_to(baseline_root)
            for path in baseline_root.rglob("*")
            if path.is_file()
        )
        if baseline_root.is_dir()
        else []
    )
    candidate_files = (
        sorted(
            path.relative_to(candidate_root)
            for path in candidate_root.rglob("*")
            if path.is_file()
        )
        if candidate_root.is_dir()
        else []
    )
    checks.append(
        {
            "name": "STARsolo output inventory",
            "passed": bool(baseline_files) and baseline_files == candidate_files,
            "baseline_files": [str(path) for path in baseline_files],
            "candidate_files": [str(path) for path in candidate_files],
        }
    )
    if baseline_files != candidate_files:
        return
    for relative in baseline_files:
        add_presence_digest(
            checks,
            f"STARsolo {relative}",
            baseline_root / relative,
            candidate_root / relative,
            digest,
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=sorted(MODES))
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--temp-dir", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    baseline = args.baseline.resolve()
    candidate = args.candidate.resolve()
    checks: list[dict[str, object]] = []

    left_log = baseline / "star.Log.final.out"
    right_log = candidate / "star.Log.final.out"
    if left_log.is_file() and right_log.is_file():
        checks.append(
            {
                "name": "timing-independent Log.final.out fields",
                "passed": parse_log(left_log) == parse_log(right_log),
            }
        )
    else:
        checks.append(
            {
                "name": "Log.final.out presence",
                "passed": False,
            }
        )

    add_optional_text(
        checks,
        "star.SJ.out.tab",
        baseline / "star.SJ.out.tab",
        candidate / "star.SJ.out.tab",
        True,
    )
    add_optional_text(
        checks,
        "star.ReadsPerGene.out.tab",
        baseline / "star.ReadsPerGene.out.tab",
        candidate / "star.ReadsPerGene.out.tab",
        False,
    )

    left_main_sam = baseline / "star.Aligned.out.sam"
    right_main_sam = candidate / "star.Aligned.out.sam"
    if left_main_sam.exists() or right_main_sam.exists():
        add_presence_digest(
            checks,
            "canonical genomic SAM records",
            left_main_sam,
            right_main_sam,
            canonical_sam_digest,
        )

    if args.mode == "chimeric":
        add_presence_digest(
            checks,
            "canonical chimeric SAM records",
            baseline / "star.Chimeric.out.sam",
            candidate / "star.Chimeric.out.sam",
            canonical_sam_digest,
        )
        add_presence_digest(
            checks,
            "chimeric junctions",
            baseline / "star.Chimeric.out.junction",
            candidate / "star.Chimeric.out.junction",
            lambda path: normalized_text_digest(path, True),
        )

    bam_names: list[str] = []
    if args.mode == "sorted-bam":
        bam_names.append("star.Aligned.sortedByCoord.out.bam")
    elif args.mode == "transcriptome-bam":
        bam_names.extend(
            (
                "star.Aligned.out.bam",
                "star.Aligned.toTranscriptome.out.bam",
            )
        )
    if bam_names:
        parent = args.temp_dir or Path(tempfile.gettempdir())
        with tempfile.TemporaryDirectory(
            prefix="blackstar-generalization-bam-", dir=parent
        ) as raw:
            temp_root = Path(raw)
            for name in bam_names:
                add_presence_digest(
                    checks,
                    f"canonical {name} records",
                    baseline / name,
                    candidate / name,
                    lambda path, root=temp_root: canonical_bam_digest(path, root),
                )

    if args.mode == "starsolo":
        compare_solo_tree(checks, baseline, candidate)

    result = {
        "schema": "blackstar-generalization-comparison-v1",
        "mode": args.mode,
        "baseline": str(baseline),
        "candidate": str(candidate),
        "passed": bool(checks) and all(bool(check["passed"]) for check in checks),
        "checks": checks,
    }
    output = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(output, encoding="utf-8")
    else:
        print(output, end="")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

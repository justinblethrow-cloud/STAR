#!/usr/bin/env python3
"""Compare STAR run directories with timing fields excluded from correctness."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


IGNORED_LOG_FIELDS = {
    "Started job on",
    "Started mapping on",
    "Finished on",
    "Mapping speed, Million of reads per hour",
}


def parse_log(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "|" in line:
            key, value = line.split("|", 1)
            key = key.strip()
            if key not in IGNORED_LOG_FIELDS:
                result[key] = value.strip()
    return result


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def normalized_text_digest(path: Path, sort_lines: bool) -> str:
    lines = path.read_text(encoding="utf-8", errors="strict").splitlines()
    if sort_lines:
        lines.sort()
    return hashlib.sha256(("\n".join(lines) + "\n").encode()).hexdigest()


def find_bam(run: Path) -> Path | None:
    candidates = sorted(run.glob("star.*.bam"))
    return candidates[0] if len(candidates) == 1 else None


def canonical_sam_digest(path: Path) -> str:
    headers: list[str] = []
    records: list[str] = []
    for line in path.read_text(encoding="utf-8", errors="strict").splitlines():
        if line.startswith("@PG") or line.startswith("@CO\tuser command line:"):
            continue
        if line.startswith("@"):
            headers.append(line)
        else:
            records.append(line)
    records.sort()
    payload = "\n".join(headers + records) + "\n"
    return hashlib.sha256(payload.encode()).hexdigest()


def canonical_bam_digest(path: Path, temp_root: Path) -> str:
    samtools = shutil.which("samtools")
    sort_bin = shutil.which("sort")
    if not samtools or not sort_bin:
        raise RuntimeError("samtools and GNU sort are required for canonical BAM comparison")
    value = hashlib.sha256()
    env = dict(os.environ, LC_ALL="C")
    view = subprocess.Popen([samtools, "view", str(path)], stdout=subprocess.PIPE)
    sorter = subprocess.Popen(
        [sort_bin, "-T", str(temp_root), "-S", "2G"],
        stdin=view.stdout,
        stdout=subprocess.PIPE,
        env=env,
    )
    assert view.stdout is not None
    view.stdout.close()
    assert sorter.stdout is not None
    for block in iter(lambda: sorter.stdout.read(1024 * 1024), b""):
        value.update(block)
    sorter.stdout.close()
    sort_status = sorter.wait()
    view_status = view.wait()
    if view_status or sort_status:
        raise RuntimeError(f"BAM canonicalization failed: samtools={view_status} sort={sort_status}")
    return value.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--canonical-bam", action="store_true")
    parser.add_argument("--temp-dir", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    baseline = args.baseline.resolve()
    candidate = args.candidate.resolve()
    checks: list[dict[str, object]] = []

    baseline_log = parse_log(baseline / "star.Log.final.out")
    candidate_log = parse_log(candidate / "star.Log.final.out")
    checks.append(
        {
            "name": "timing-independent Log.final.out fields",
            "passed": baseline_log == candidate_log,
        }
    )

    for name, sort_lines in (
        ("star.SJ.out.tab", True),
        ("star.ReadsPerGene.out.tab", False),
    ):
        left = baseline / name
        right = candidate / name
        if left.exists() or right.exists():
            passed = (
                left.is_file()
                and right.is_file()
                and normalized_text_digest(left, sort_lines)
                == normalized_text_digest(right, sort_lines)
            )
            checks.append({"name": name, "passed": passed})

    left_sam = baseline / "star.Aligned.out.sam"
    right_sam = candidate / "star.Aligned.out.sam"
    if left_sam.exists() or right_sam.exists():
        left_hash = canonical_sam_digest(left_sam) if left_sam.is_file() else ""
        right_hash = canonical_sam_digest(right_sam) if right_sam.is_file() else ""
        checks.append(
            {
                "name": "canonical SAM records",
                "passed": bool(left_hash) and left_hash == right_hash,
                "baseline_sha256": left_hash,
                "candidate_sha256": right_hash,
            }
        )

    left_bam = find_bam(baseline)
    right_bam = find_bam(candidate)
    if left_bam or right_bam:
        checks.append(
            {
                "name": "BAM presence",
                "passed": left_bam is not None and right_bam is not None,
            }
        )
        if left_bam and right_bam:
            if args.canonical_bam:
                parent = args.temp_dir or Path(tempfile.gettempdir())
                with tempfile.TemporaryDirectory(prefix="blackstar-bam-sort-", dir=parent) as raw:
                    temp_root = Path(raw)
                    left_hash = canonical_bam_digest(left_bam, temp_root)
                    right_hash = canonical_bam_digest(right_bam, temp_root)
                checks.append(
                    {
                        "name": "canonical BAM records",
                        "passed": left_hash == right_hash,
                        "baseline_sha256": left_hash,
                        "candidate_sha256": right_hash,
                    }
                )
            else:
                left_hash = digest(left_bam)
                right_hash = digest(right_bam)
                checks.append(
                    {
                        "name": "BAM bytes",
                        "passed": left_hash == right_hash,
                        "baseline_sha256": left_hash,
                        "candidate_sha256": right_hash,
                        "qualification": "use --canonical-bam for order-independent records",
                    }
                )

    result = {
        "schema": "blackstar-alignment-comparison-v1",
        "baseline": str(baseline),
        "candidate": str(candidate),
        "passed": all(bool(check["passed"]) for check in checks),
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

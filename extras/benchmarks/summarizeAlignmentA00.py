#!/usr/bin/env python3
"""Summarize one or more A00 alignment runs into a compact TSV."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path
import sys


TIME_FIELDS = {
    "Elapsed (wall clock) time (h:mm:ss or m:ss)": "wall_seconds",
    "User time (seconds)": "user_seconds",
    "System time (seconds)": "system_seconds",
    "Maximum resident set size (kbytes)": "max_rss_kib",
    "File system inputs": "fs_inputs",
    "File system outputs": "fs_outputs",
}
LOG_FIELDS = {
    "Number of input reads": "input_reads",
    "Uniquely mapped reads number": "unique_reads",
    "Uniquely mapped reads %": "unique_percent",
    "Number of reads mapped to multiple loci": "multi_reads",
    "% of reads unmapped: too short": "unmapped_short_percent",
}


def elapsed_seconds(raw: str) -> float:
    parts = [float(part) for part in raw.split(":")]
    total = 0.0
    for part in parts:
        total = total * 60.0 + part
    return total


def parse_colon_file(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    if not path.is_file():
        return result
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if ": " in line:
            key, value = line.strip().rsplit(": ", 1)
            result[key.strip()] = value.strip()
    return result


def parse_provenance(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    with path.open(encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            result[row["field"]] = row["value"]
    return result


def parse_log(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    if not path.is_file():
        return result
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "|" in line:
            key, value = line.split("|", 1)
            result[key.strip()] = value.strip()
    return result


def summarize(run_dir: Path) -> dict[str, str]:
    provenance = parse_provenance(run_dir / "provenance.tsv")
    timing = parse_colon_file(run_dir / "time.txt")
    log = parse_log(run_dir / "star.Log.final.out")
    row = {
        "run_dir": str(run_dir),
        "mode": provenance.get("mode", ""),
        "threads": provenance.get("threads", ""),
        "star_version": provenance.get("star_version", ""),
        "star_sha256": provenance.get("star_sha256", ""),
        "start_utc": provenance.get("start_utc", ""),
        "finish_utc": provenance.get("finish_utc", ""),
    }
    for source, output in TIME_FIELDS.items():
        value = timing.get(source, "")
        row[output] = str(elapsed_seconds(value)) if output == "wall_seconds" and value else value
    for source, output in LOG_FIELDS.items():
        row[output] = log.get(source, "")
    return row


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("run_dir", nargs="+", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    rows = [summarize(path.resolve()) for path in args.run_dir]
    fieldnames = list(rows[0])
    handle = args.output.open("w", encoding="utf-8", newline="") if args.output else sys.stdout
    try:
        writer = csv.DictWriter(
            handle, fieldnames=fieldnames, delimiter="\t", lineterminator="\n"
        )
        writer.writeheader()
        writer.writerows(rows)
    finally:
        if args.output:
            handle.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

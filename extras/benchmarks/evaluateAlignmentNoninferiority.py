#!/usr/bin/env python3
"""Evaluate an existing paired alignment series against a no-regression margin."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
import statistics

from runAlignmentPairs import bootstrap_median_ci, coefficient_of_variation


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("pairs_tsv", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--margin-percent", type=float, default=2.0)
    parser.add_argument("--max-rss-increase-percent", type=float, default=5.0)
    parser.add_argument("--max-cv-percent", type=float, default=3.0)
    parser.add_argument("--minimum-pairs", type=int, default=5)
    parser.add_argument("--seed", type=int, default=20260723)
    return parser.parse_args()


def parse_bool(value: str) -> bool:
    if value == "True":
        return True
    if value == "False":
        return False
    raise ValueError(f"invalid correctness value: {value}")


def main() -> int:
    args = parse_args()
    if args.margin_percent <= 0 or args.max_rss_increase_percent < 0:
        raise SystemExit("margins must be positive")
    if args.max_cv_percent <= 0 or args.minimum_pairs < 1:
        raise SystemExit("maximum CV and minimum pairs must be positive")
    if args.output.exists():
        raise SystemExit(f"output path already exists: {args.output}")

    with args.pairs_tsv.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    if not rows:
        raise SystemExit("paired result table is empty")

    baseline = [float(row["baseline_wall_seconds"]) for row in rows]
    candidate = [float(row["candidate_wall_seconds"]) for row in rows]
    improvements = [float(row["improvement_percent"]) for row in rows]
    baseline_rss = [int(row["baseline_max_rss_kib"]) for row in rows]
    candidate_rss = [int(row["candidate_max_rss_kib"]) for row in rows]
    correctness = [parse_bool(row["correctness_passed"]) for row in rows]

    ci_low, ci_high = bootstrap_median_ci(improvements, args.seed)
    median_baseline_rss = statistics.median(baseline_rss)
    median_candidate_rss = statistics.median(candidate_rss)
    rss_increase = 100.0 * (
        median_candidate_rss - median_baseline_rss
    ) / median_baseline_rss
    baseline_cv = coefficient_of_variation(baseline)
    candidate_cv = coefficient_of_variation(candidate)

    gates = {
        "correctness": all(correctness),
        "pair_count": len(rows) >= args.minimum_pairs,
        "variability": max(baseline_cv, candidate_cv) <= args.max_cv_percent,
        "wall_time_noninferiority": ci_low >= -args.margin_percent,
        "rss": rss_increase <= args.max_rss_increase_percent,
    }
    result = {
        "schema": "blackstar-alignment-noninferiority-v1",
        "accepted": all(gates.values()),
        "gates": gates,
        "pair_count": len(rows),
        "margin_percent": args.margin_percent,
        "max_rss_increase_percent": args.max_rss_increase_percent,
        "max_cv_percent": args.max_cv_percent,
        "median_baseline_wall_seconds": statistics.median(baseline),
        "median_candidate_wall_seconds": statistics.median(candidate),
        "median_improvement_percent": statistics.median(improvements),
        "bootstrap_95_ci_percent": [ci_low, ci_high],
        "baseline_cv_percent": baseline_cv,
        "candidate_cv_percent": candidate_cv,
        "median_rss_increase_percent": rss_increase,
    }
    args.output.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["accepted"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Run order-balanced cross-workload STAR/BlackSTAR noninferiority pairs."""

from __future__ import annotations

import argparse
import csv
import json
import os
from pathlib import Path
import random
import statistics
import subprocess
import sys
import tempfile
import time

from compareAlignmentRuns import canonical_bam_digest
from runAlignmentPairs import (
    bootstrap_median_ci,
    coefficient_of_variation,
    elapsed_seconds,
    make_orders,
    max_rss_kib,
    sha256,
    utc_now,
    validate_affinity_environment,
    write_schedule,
)


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_RUNNER = REPO_ROOT / "extras/benchmarks/runGeneralizationMode.sh"
COMPARATOR = REPO_ROOT / "extras/benchmarks/compareGeneralizationRuns.py"
QUIET_GATE = REPO_ROOT / "extras/benchmarks/quietSystemGate.py"
PAIRED_MODES = {
    "paired-mapping",
    "two-pass",
    "bysjout",
    "chimeric",
    "sorted-bam",
    "transcriptome-bam",
    "starsolo",
}
SINGLE_MODES = {"single-mapping", "starlong"}
MODES = PAIRED_MODES | SINGLE_MODES


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=sorted(MODES), required=True)
    parser.add_argument("--baseline-bin", type=Path, required=True)
    parser.add_argument("--candidate-bin", type=Path, required=True)
    parser.add_argument("--genome-dir", type=Path, required=True)
    parser.add_argument("--read1", type=Path, required=True)
    parser.add_argument("--read2", type=Path)
    parser.add_argument("--warmup-read1", type=Path)
    parser.add_argument("--warmup-read2", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--threads", type=int, default=96)
    parser.add_argument("--pairs", type=int, default=3)
    parser.add_argument("--seed", type=int, default=20260725)
    parser.add_argument("--margin-percent", type=float, default=2.0)
    parser.add_argument("--max-rss-increase-percent", type=float, default=5.0)
    parser.add_argument("--max-cv-percent", type=float, default=5.0)
    parser.add_argument("--quiet-duration", type=float, default=300.0)
    parser.add_argument("--quiet-interval", type=float, default=5.0)
    parser.add_argument("--settle-seconds", type=float, default=5.0)
    parser.add_argument("--runner", type=Path, default=DEFAULT_RUNNER)
    parser.add_argument("--skip-quiet-gate", action="store_true")
    return parser.parse_args()


def validate(args: argparse.Namespace) -> None:
    validate_affinity_environment()
    for path in (
        args.baseline_bin,
        args.candidate_bin,
        args.read1,
        args.runner,
    ):
        if not path.is_file():
            raise SystemExit(f"required file is absent: {path}")
    for path in (args.baseline_bin, args.candidate_bin, args.runner):
        if not os.access(path, os.X_OK):
            raise SystemExit(f"required executable is not executable: {path}")
    for name in ("Genome", "SA", "SAindex", "genomeParameters.txt"):
        if not (args.genome_dir / name).is_file():
            raise SystemExit(f"incomplete genome directory: missing {name}")
    if args.mode in PAIRED_MODES and args.read2 is None:
        raise SystemExit(f"{args.mode} requires --read2")
    if args.mode in SINGLE_MODES and args.read2 is not None:
        raise SystemExit(f"{args.mode} does not accept --read2")
    if args.read2 is not None and not args.read2.is_file():
        raise SystemExit(f"required file is absent: {args.read2}")
    if args.warmup_read1 is None and args.warmup_read2 is not None:
        raise SystemExit("--warmup-read2 requires --warmup-read1")
    if args.warmup_read1 is not None and not args.warmup_read1.is_file():
        raise SystemExit(f"required file is absent: {args.warmup_read1}")
    if args.mode in PAIRED_MODES:
        if (args.warmup_read1 is None) != (args.warmup_read2 is None):
            raise SystemExit("paired-mode warmups require both warmup reads")
    elif args.warmup_read2 is not None:
        raise SystemExit("single-mode warmups do not accept --warmup-read2")
    if args.warmup_read2 is not None and not args.warmup_read2.is_file():
        raise SystemExit(f"required file is absent: {args.warmup_read2}")
    if args.mode == "starsolo":
        whitelist = os.environ.get("STARSOLO_WHITELIST", "")
        if not whitelist or not Path(whitelist).is_file():
            raise SystemExit(
                "STARSOLO_WHITELIST must name an existing file for starsolo"
            )
    if args.output.exists():
        raise SystemExit(f"output path already exists: {args.output}")
    if args.threads < 1 or args.pairs < 3:
        raise SystemExit("threads must be positive and at least three pairs are required")
    if args.margin_percent <= 0 or args.max_rss_increase_percent < 0:
        raise SystemExit("acceptance margins are invalid")
    if args.max_cv_percent <= 0:
        raise SystemExit("--max-cv-percent must be positive")
    if args.quiet_duration <= 0 or args.quiet_interval <= 0:
        raise SystemExit("quiet-system timing must be positive")
    if args.settle_seconds < 0:
        raise SystemExit("--settle-seconds cannot be negative")


def input_value(path: Path | None) -> str:
    return str(path.resolve()) if path is not None else "none"


def input_digest(path: Path | None) -> str:
    return sha256(path) if path is not None else "none"


def write_contract(
    args: argparse.Namespace, orders: list[str], warmup_order: str | None
) -> None:
    whitelist = (
        Path(os.environ["STARSOLO_WHITELIST"]).resolve()
        if args.mode == "starsolo"
        else None
    )
    contract = {
        "schema": "blackstar-generalization-pairs-v1",
        "created_utc": utc_now(),
        "mode": args.mode,
        "threads": args.threads,
        "pairs": args.pairs,
        "orders": orders,
        "seed": args.seed,
        "margin_percent": args.margin_percent,
        "max_rss_increase_percent": args.max_rss_increase_percent,
        "max_cv_percent": args.max_cv_percent,
        "quiet_duration": args.quiet_duration,
        "quiet_gate_skipped": args.skip_quiet_gate,
        "settle_seconds": args.settle_seconds,
        "baseline_bin": str(args.baseline_bin.resolve()),
        "baseline_sha256": sha256(args.baseline_bin),
        "candidate_bin": str(args.candidate_bin.resolve()),
        "candidate_sha256": sha256(args.candidate_bin),
        "genome_dir": str(args.genome_dir.resolve()),
        "genome_parameters_sha256": sha256(
            args.genome_dir / "genomeParameters.txt"
        ),
        "genome_core_sizes": {
            name: (args.genome_dir / name).stat().st_size
            for name in ("Genome", "SA", "SAindex")
        },
        "read1": input_value(args.read1),
        "read1_sha256": input_digest(args.read1),
        "read2": input_value(args.read2),
        "read2_sha256": input_digest(args.read2),
        "warmup_order": warmup_order or "none",
        "warmup_position": "after_quiet_gate",
        "warmup_read1": input_value(args.warmup_read1),
        "warmup_read1_sha256": input_digest(args.warmup_read1),
        "warmup_read2": input_value(args.warmup_read2),
        "warmup_read2_sha256": input_digest(args.warmup_read2),
        "starsolo_whitelist": input_value(whitelist),
        "starsolo_whitelist_sha256": input_digest(whitelist),
        "transcriptome_primary_oracle": (
            "upstream-equivalent records after clearing SAM flag 0x100; "
            "exact candidate primary flags across all candidate runs"
            if args.mode == "transcriptome-bam"
            else "not-applicable"
        ),
        "tools": {
            "pair_driver_sha256": sha256(Path(__file__)),
            "runner_sha256": sha256(args.runner),
            "quiet_gate_sha256": sha256(QUIET_GATE),
            "comparator_sha256": sha256(COMPARATOR),
        },
        "environment": {
            name: os.environ.get(name, "unset")
            for name in (
                "ALLOW_OMP_THREAD_BINDING",
                "BAM_SORT_RAM",
                "GENOME_LOAD_MODE",
                "OMP_DYNAMIC",
                "OMP_PLACES",
                "OMP_PROC_BIND",
                "READ_FILES_COMMAND",
                "STARSOLO_WHITELIST",
                "STAR_EXTRA_ARGS",
            )
        },
    }
    (args.output / "contract.json").write_text(
        json.dumps(contract, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


def run_one(
    args: argparse.Namespace, role: str, read1: Path, read2: Path | None, out: Path
) -> None:
    binary = args.baseline_bin if role == "baseline" else args.candidate_bin
    environment = dict(os.environ)
    environment.update(THREADS=str(args.threads))
    subprocess.run(
        [
            str(args.runner),
            args.mode,
            str(binary),
            str(args.genome_dir),
            str(read1),
            str(read2) if read2 is not None else "none",
            str(out),
        ],
        env=environment,
        check=True,
    )


def compare_pair(
    args: argparse.Namespace, baseline: Path, candidate: Path, output: Path
) -> bool:
    status = subprocess.run(
        [
            sys.executable,
            str(COMPARATOR),
            args.mode,
            str(baseline),
            str(candidate),
            "--temp-dir",
            str(args.output),
            "--output",
            str(output),
        ]
    ).returncode
    result = json.loads(output.read_text(encoding="utf-8"))
    return status == 0 and bool(result["passed"])


def run_quiet_gate(args: argparse.Namespace) -> None:
    if args.skip_quiet_gate:
        return
    subprocess.run(
        [
            sys.executable,
            str(QUIET_GATE),
            "--duration",
            str(args.quiet_duration),
            "--interval",
            str(args.quiet_interval),
            "--path",
            str(args.output),
            "--output",
            str(args.output / "quiet-system.tsv"),
        ],
        check=True,
    )


def main() -> int:
    args = parse_args()
    validate(args)
    args.output.mkdir(parents=True)
    orders = make_orders(args.pairs, args.seed)
    warmup_order = (
        random.Random(args.seed ^ 0xC012).choice(["AB", "BA"])
        if args.warmup_read1 is not None
        else None
    )
    write_contract(args, orders, warmup_order)

    run_quiet_gate(args)

    warmups: dict[str, Path] = {}
    if warmup_order is not None:
        roles = (
            ("baseline", "candidate")
            if warmup_order == "AB"
            else ("candidate", "baseline")
        )
        assert args.warmup_read1 is not None
        for position, role in enumerate(roles, 1):
            output = args.output / f"warmup-{position}-{role}"
            run_one(
                args,
                role,
                args.warmup_read1,
                args.warmup_read2,
                output,
            )
            warmups[role] = output
        if not compare_pair(
            args,
            warmups["baseline"],
            warmups["candidate"],
            args.output / "warmup-comparison.json",
        ):
            raise RuntimeError("warmup correctness comparison failed")

    schedule: list[dict[str, str]] = []
    for pair, order in enumerate(orders, 1):
        roles = (
            ("baseline", "candidate") if order == "AB" else ("candidate", "baseline")
        )
        for position, role in enumerate(roles, 1):
            schedule.append(
                {
                    "pair": str(pair),
                    "order": order,
                    "position": str(position),
                    "role": role,
                    "run_dir": f"pair-{pair:02d}-{position}-{role}",
                    "status": "pending",
                    "started_utc": "",
                    "finished_utc": "",
                }
            )
    write_schedule(args.output / "schedule.tsv", schedule)

    for index, row in enumerate(schedule):
        row["status"] = "running"
        row["started_utc"] = utc_now()
        write_schedule(args.output / "schedule.tsv", schedule)
        try:
            run_one(
                args,
                row["role"],
                args.read1,
                args.read2,
                args.output / row["run_dir"],
            )
        except BaseException:
            row["status"] = "failed"
            row["finished_utc"] = utc_now()
            write_schedule(args.output / "schedule.tsv", schedule)
            raise
        row["status"] = "complete"
        row["finished_utc"] = utc_now()
        write_schedule(args.output / "schedule.tsv", schedule)
        if index + 1 < len(schedule) and args.settle_seconds:
            time.sleep(args.settle_seconds)

    baseline_times: list[float] = []
    candidate_times: list[float] = []
    baseline_rss: list[int] = []
    candidate_rss: list[int] = []
    improvements: list[float] = []
    summary_rows: list[dict[str, object]] = []
    correctness: list[bool] = []
    for pair in range(1, args.pairs + 1):
        pair_rows = [row for row in schedule if int(row["pair"]) == pair]
        runs = {
            row["role"]: args.output / row["run_dir"] for row in pair_rows
        }
        passed = compare_pair(
            args,
            runs["baseline"],
            runs["candidate"],
            args.output / f"pair-{pair:02d}-comparison.json",
        )
        baseline_wall = elapsed_seconds(runs["baseline"] / "time.txt")
        candidate_wall = elapsed_seconds(runs["candidate"] / "time.txt")
        baseline_peak = max_rss_kib(runs["baseline"] / "time.txt")
        candidate_peak = max_rss_kib(runs["candidate"] / "time.txt")
        improvement = 100.0 * (baseline_wall - candidate_wall) / baseline_wall
        baseline_times.append(baseline_wall)
        candidate_times.append(candidate_wall)
        baseline_rss.append(baseline_peak)
        candidate_rss.append(candidate_peak)
        improvements.append(improvement)
        correctness.append(passed)
        summary_rows.append(
            {
                "pair": pair,
                "order": pair_rows[0]["order"],
                "baseline_wall_seconds": baseline_wall,
                "candidate_wall_seconds": candidate_wall,
                "improvement_percent": improvement,
                "baseline_max_rss_kib": baseline_peak,
                "candidate_max_rss_kib": candidate_peak,
                "correctness_passed": passed,
            }
        )

    candidate_primary_digests: list[dict[str, str]] = []
    candidate_primary_deterministic = True
    if args.mode == "transcriptome-bam":
        candidate_runs = [
            args.output / row["run_dir"]
            for row in schedule
            if row["role"] == "candidate"
        ]
        if "candidate" in warmups:
            candidate_runs.insert(0, warmups["candidate"])
        parent = args.output
        with tempfile.TemporaryDirectory(
            prefix="blackstar-transcriptome-primary-", dir=parent
        ) as raw:
            temp_root = Path(raw)
            for run in candidate_runs:
                primary_digest = canonical_bam_digest(
                    run / "star.Aligned.toTranscriptome.out.bam",
                    temp_root,
                )
                candidate_primary_digests.append(
                    {
                        "run": run.name,
                        "sha256": primary_digest,
                    }
                )
        candidate_primary_deterministic = (
            bool(candidate_primary_digests)
            and len(
                {
                    item["sha256"]
                    for item in candidate_primary_digests
                }
            ) == 1
        )

    ci_low, ci_high = bootstrap_median_ci(improvements, args.seed ^ 0x6E)
    baseline_cv = coefficient_of_variation(baseline_times)
    candidate_cv = coefficient_of_variation(candidate_times)
    median_baseline_rss = statistics.median(baseline_rss)
    median_candidate_rss = statistics.median(candidate_rss)
    rss_increase = (
        100.0
        * (median_candidate_rss - median_baseline_rss)
        / median_baseline_rss
    )
    gates = {
        "correctness": all(correctness),
        "pair_count": len(summary_rows) >= 3,
        "variability": max(baseline_cv, candidate_cv) <= args.max_cv_percent,
        "wall_time_noninferiority": ci_low >= -args.margin_percent,
        "rss": rss_increase <= args.max_rss_increase_percent,
    }
    if args.mode == "transcriptome-bam":
        gates["candidate_transcriptome_primary_determinism"] = (
            candidate_primary_deterministic
        )
    result = {
        "schema": "blackstar-generalization-result-v1",
        "mode": args.mode,
        "accepted": all(gates.values()),
        "gates": gates,
        "pair_count": len(summary_rows),
        "margin_percent": args.margin_percent,
        "median_baseline_wall_seconds": statistics.median(baseline_times),
        "median_candidate_wall_seconds": statistics.median(candidate_times),
        "median_improvement_percent": statistics.median(improvements),
        "bootstrap_95_ci_percent": [ci_low, ci_high],
        "baseline_cv_percent": baseline_cv,
        "candidate_cv_percent": candidate_cv,
        "median_rss_increase_percent": rss_increase,
        "median_gain_at_least_2_percent": statistics.median(improvements) >= 2.0,
        "superiority_2_percent": ci_low >= 2.0,
        "candidate_transcriptome_primary_digests": candidate_primary_digests,
    }
    (args.output / "result.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    with (args.output / "pairs.tsv").open(
        "w", encoding="utf-8", newline=""
    ) as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=list(summary_rows[0]),
            delimiter="\t",
            lineterminator="\n",
        )
        writer.writeheader()
        writer.writerows(summary_rows)
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["accepted"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Run order-balanced STAR release/candidate pairs with correctness gates."""

from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import random
import statistics
import subprocess
import sys
import time


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_RUNNER = REPO_ROOT / "extras/benchmarks/runAlignmentA00.sh"
QUIET_GATE = REPO_ROOT / "extras/benchmarks/quietSystemGate.py"
COMPARATOR = REPO_ROOT / "extras/benchmarks/compareAlignmentRuns.py"


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def sha256(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def elapsed_seconds(path: Path) -> float:
    marker = "Elapsed (wall clock) time (h:mm:ss or m:ss): "
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if marker in line:
            total = 0.0
            for part in line.rsplit(": ", 1)[1].split(":"):
                total = total * 60.0 + float(part)
            return total
    raise RuntimeError(f"elapsed time is absent from {path}")


def max_rss_kib(path: Path) -> int:
    marker = "Maximum resident set size (kbytes): "
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if marker in line:
            return int(line.rsplit(": ", 1)[1])
    raise RuntimeError(f"maximum RSS is absent from {path}")


def coefficient_of_variation(values: list[float]) -> float:
    mean = statistics.mean(values)
    if len(values) < 2 or mean == 0:
        return 0.0
    return 100.0 * statistics.stdev(values) / mean


def percentile(sorted_values: list[float], quantile: float) -> float:
    position = quantile * (len(sorted_values) - 1)
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return sorted_values[lower]
    fraction = position - lower
    return sorted_values[lower] * (1.0 - fraction) + sorted_values[upper] * fraction


def bootstrap_median_ci(values: list[float], seed: int) -> tuple[float, float]:
    rng = random.Random(seed)
    medians = sorted(
        statistics.median(rng.choices(values, k=len(values))) for _ in range(10000)
    )
    return percentile(medians, 0.025), percentile(medians, 0.975)


def write_schedule(path: Path, rows: list[dict[str, str]]) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle, fieldnames=list(rows[0]), delimiter="\t", lineterminator="\n"
        )
        writer.writeheader()
        writer.writerows(rows)
    temporary.replace(path)


def make_orders(pair_count: int, seed: int) -> list[str]:
    orders = ["AB", "BA"] * (pair_count // 2)
    if pair_count % 2:
        orders.append(random.Random(seed ^ 0xA00).choice(["AB", "BA"]))
    random.Random(seed).shuffle(orders)
    return orders


def validate_affinity_environment() -> None:
    allow = os.environ.get("ALLOW_OMP_THREAD_BINDING", "0")
    if allow not in ("0", "1"):
        raise SystemExit("ALLOW_OMP_THREAD_BINDING must be 0 or 1")
    proc_bind = os.environ.get("OMP_PROC_BIND", "").strip().lower()
    places = os.environ.get("OMP_PLACES", "").strip()
    binding_requested = bool(places) or proc_bind not in ("", "false")
    if binding_requested and allow != "1":
        raise SystemExit(
            "OpenMP processor binding is unsafe for alignment comparisons: "
            "pthread workers may inherit one OpenMP place. Unset OMP_PROC_BIND "
            "and OMP_PLACES, or set ALLOW_OMP_THREAD_BINDING=1 only for a "
            "deliberate affinity test."
        )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-bin", type=Path, required=True)
    parser.add_argument("--candidate-bin", type=Path, required=True)
    parser.add_argument("--genome-dir", type=Path, required=True)
    parser.add_argument("--read1", type=Path, required=True)
    parser.add_argument("--read2", type=Path, required=True)
    parser.add_argument("--warmup-read1", type=Path)
    parser.add_argument("--warmup-read2", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--mode",
        choices=("mapping-only", "unsorted-bam", "sorted-bam"),
        default="mapping-only",
    )
    parser.add_argument("--threads", type=int, default=16)
    parser.add_argument("--pairs", type=int, default=3)
    parser.add_argument("--seed", type=int, default=20260722)
    parser.add_argument("--expected-effect-percent", type=float, default=5.0)
    parser.add_argument("--quiet-duration", type=float, default=300.0)
    parser.add_argument("--quiet-interval", type=float, default=5.0)
    parser.add_argument("--settle-seconds", type=float, default=5.0)
    parser.add_argument("--runner", type=Path, default=DEFAULT_RUNNER)
    parser.add_argument("--skip-quiet-gate", action="store_true", help=argparse.SUPPRESS)
    return parser.parse_args()


def validate(args: argparse.Namespace) -> None:
    validate_affinity_environment()
    for path in (
        args.baseline_bin,
        args.candidate_bin,
        args.read1,
        args.read2,
        args.runner,
    ):
        if not path.is_file():
            raise SystemExit(f"required file is absent: {path}")
    if (args.warmup_read1 is None) != (args.warmup_read2 is None):
        raise SystemExit("warmup read1 and read2 must be supplied together")
    for path in (args.warmup_read1, args.warmup_read2):
        if path is not None and not path.is_file():
            raise SystemExit(f"warmup file is absent: {path}")
    for path in (args.baseline_bin, args.candidate_bin, args.runner):
        if not os.access(path, os.X_OK):
            raise SystemExit(f"required executable is not executable: {path}")
    for name in ("Genome", "SA", "SAindex", "genomeParameters.txt"):
        if not (args.genome_dir / name).is_file():
            raise SystemExit(f"incomplete genome directory: missing {name}")
    if args.output.exists():
        raise SystemExit(f"output path already exists: {args.output}")
    if args.threads < 1 or args.pairs < 1:
        raise SystemExit("threads and pairs must be positive")
    if args.quiet_duration <= 0 or args.quiet_interval <= 0:
        raise SystemExit("quiet duration and interval must be positive")
    if args.settle_seconds < 0:
        raise SystemExit("settle seconds cannot be negative")
    if args.expected_effect_percent <= 0:
        raise SystemExit("expected effect percent must be positive")


def write_contract(
    args: argparse.Namespace, orders: list[str], warmup_order: str | None
) -> None:
    contract = {
        "schema": "blackstar-alignment-pairs-v1",
        "created_utc": utc_now(),
        "mode": args.mode,
        "threads": args.threads,
        "pairs": args.pairs,
        "orders": orders,
        "seed": args.seed,
        "expected_effect_percent": args.expected_effect_percent,
        "quiet_duration": args.quiet_duration,
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
        "read1": str(args.read1.resolve()),
        "read1_sha256": sha256(args.read1),
        "read2": str(args.read2.resolve()),
        "read2_sha256": sha256(args.read2),
        "warmup_order": warmup_order or "none",
        "warmup_read1": str(args.warmup_read1.resolve())
        if args.warmup_read1
        else "none",
        "warmup_read1_sha256": sha256(args.warmup_read1)
        if args.warmup_read1
        else "none",
        "warmup_read2": str(args.warmup_read2.resolve())
        if args.warmup_read2
        else "none",
        "warmup_read2_sha256": sha256(args.warmup_read2)
        if args.warmup_read2
        else "none",
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
                "OMP_DYNAMIC",
                "OMP_PLACES",
                "OMP_PROC_BIND",
                "QUANT_MODE",
                "READ_FILES_COMMAND",
                "STAR_EXTRA_ARGS",
            )
        },
    }
    (args.output / "contract.json").write_text(
        json.dumps(contract, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


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


def execute_warmups(args: argparse.Namespace, order: str | None) -> None:
    if order is None or args.warmup_read1 is None or args.warmup_read2 is None:
        return
    environment = dict(os.environ)
    environment.update(THREADS=str(args.threads), PERF_MODE="none")
    roles = ("baseline", "candidate") if order == "AB" else ("candidate", "baseline")
    run_by_role: dict[str, Path] = {}
    for position, role in enumerate(roles, 1):
        binary = args.baseline_bin if role == "baseline" else args.candidate_bin
        run_dir = args.output / f"warmup-{position}-{role}"
        subprocess.run(
            [
                str(args.runner),
                "mapping-only",
                str(binary),
                str(args.genome_dir),
                str(args.warmup_read1),
                str(args.warmup_read2),
                str(run_dir),
            ],
            env=environment,
            check=True,
        )
        run_by_role[role] = run_dir
    comparison_path = args.output / "warmup-comparison.json"
    status = subprocess.run(
        [
            sys.executable,
            str(COMPARATOR),
            str(run_by_role["baseline"]),
            str(run_by_role["candidate"]),
            "--output",
            str(comparison_path),
        ]
    ).returncode
    if status:
        raise RuntimeError("warmup release/candidate correctness comparison failed")


def execute_runs(args: argparse.Namespace, rows: list[dict[str, str]]) -> None:
    schedule_path = args.output / "schedule.tsv"
    environment = dict(os.environ)
    environment.update(THREADS=str(args.threads), PERF_MODE="none")
    for index, row in enumerate(rows):
        binary = args.baseline_bin if row["role"] == "baseline" else args.candidate_bin
        run_dir = args.output / row["run_dir"]
        row["status"] = "running"
        row["started_utc"] = utc_now()
        write_schedule(schedule_path, rows)
        try:
            subprocess.run(
                [
                    str(args.runner),
                    args.mode,
                    str(binary),
                    str(args.genome_dir),
                    str(args.read1),
                    str(args.read2),
                    str(run_dir),
                ],
                env=environment,
                check=True,
            )
        except BaseException:
            row["status"] = "failed"
            row["finished_utc"] = utc_now()
            write_schedule(schedule_path, rows)
            raise
        row["status"] = "complete"
        row["finished_utc"] = utc_now()
        write_schedule(schedule_path, rows)
        if index + 1 < len(rows) and args.settle_seconds:
            time.sleep(args.settle_seconds)


def analyze_pairs(
    args: argparse.Namespace, rows: list[dict[str, str]]
) -> tuple[dict[str, object], list[dict[str, object]]]:
    comparisons: list[dict[str, object]] = []
    baseline_times: list[float] = []
    candidate_times: list[float] = []
    baseline_rss: list[int] = []
    candidate_rss: list[int] = []
    improvements: list[float] = []
    summary_rows: list[dict[str, object]] = []
    for pair in range(1, args.pairs + 1):
        pair_rows = [row for row in rows if int(row["pair"]) == pair]
        run_by_role = {
            row["role"]: args.output / row["run_dir"] for row in pair_rows
        }
        comparison_path = args.output / f"pair-{pair:02d}-comparison.json"
        command = [
            sys.executable,
            str(COMPARATOR),
            str(run_by_role["baseline"]),
            str(run_by_role["candidate"]),
            "--output",
            str(comparison_path),
        ]
        if args.mode != "mapping-only":
            command.append("--canonical-bam")
        comparison_status = subprocess.run(command).returncode
        comparison = json.loads(comparison_path.read_text(encoding="utf-8"))
        comparisons.append(comparison)
        baseline_wall = elapsed_seconds(run_by_role["baseline"] / "time.txt")
        candidate_wall = elapsed_seconds(run_by_role["candidate"] / "time.txt")
        baseline_peak = max_rss_kib(run_by_role["baseline"] / "time.txt")
        candidate_peak = max_rss_kib(run_by_role["candidate"] / "time.txt")
        improvement = 100.0 * (baseline_wall - candidate_wall) / baseline_wall
        baseline_times.append(baseline_wall)
        candidate_times.append(candidate_wall)
        baseline_rss.append(baseline_peak)
        candidate_rss.append(candidate_peak)
        improvements.append(improvement)
        summary_rows.append(
            {
                "pair": pair,
                "order": pair_rows[0]["order"],
                "baseline_wall_seconds": baseline_wall,
                "candidate_wall_seconds": candidate_wall,
                "improvement_percent": improvement,
                "baseline_max_rss_kib": baseline_peak,
                "candidate_max_rss_kib": candidate_peak,
                "correctness_passed": comparison_status == 0
                and bool(comparison["passed"]),
            }
        )

    ci_low, ci_high = bootstrap_median_ci(improvements, args.seed ^ 0xB007)
    median_improvement = statistics.median(improvements)
    median_baseline_rss = statistics.median(baseline_rss)
    median_candidate_rss = statistics.median(candidate_rss)
    rss_increase = (
        100.0
        * (median_candidate_rss - median_baseline_rss)
        / median_baseline_rss
    )
    max_cv = max(
        coefficient_of_variation(baseline_times),
        coefficient_of_variation(candidate_times),
    )
    replicate_gate = (
        args.pairs >= 3 and max_cv <= 3.0
        if args.expected_effect_percent >= 5.0
        else args.pairs >= 5 and ci_low > 0.0
    )
    result: dict[str, object] = {
        "schema": "blackstar-alignment-paired-result-v1",
        "correctness_passed": all(bool(item["passed"]) for item in comparisons),
        "pair_count": args.pairs,
        "median_baseline_wall_seconds": statistics.median(baseline_times),
        "median_candidate_wall_seconds": statistics.median(candidate_times),
        "median_improvement_percent": median_improvement,
        "bootstrap_95_ci_percent": [ci_low, ci_high],
        "baseline_cv_percent": coefficient_of_variation(baseline_times),
        "candidate_cv_percent": coefficient_of_variation(candidate_times),
        "median_rss_increase_percent": rss_increase,
        "performance_gate_passed": median_improvement >= 2.0,
        "rss_gate_passed": rss_increase <= 5.0,
        "replicate_gate_passed": replicate_gate,
    }
    result["accepted"] = all(
        bool(result[key])
        for key in (
            "correctness_passed",
            "performance_gate_passed",
            "rss_gate_passed",
            "replicate_gate_passed",
        )
    )
    return result, summary_rows


def main() -> int:
    args = parse_args()
    validate(args)
    args.output.mkdir(parents=True)
    orders = make_orders(args.pairs, args.seed)
    rows: list[dict[str, str]] = []
    for pair, order in enumerate(orders, 1):
        roles = ("baseline", "candidate") if order == "AB" else ("candidate", "baseline")
        for position, role in enumerate(roles, 1):
            rows.append(
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
    write_schedule(args.output / "schedule.tsv", rows)
    warmup_order = (
        random.Random(args.seed ^ 0xCA2).choice(["AB", "BA"])
        if args.warmup_read1
        else None
    )
    write_contract(args, orders, warmup_order)
    execute_warmups(args, warmup_order)
    run_quiet_gate(args)
    execute_runs(args, rows)
    result, summary_rows = analyze_pairs(args, rows)
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
    return 0 if result["correctness_passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

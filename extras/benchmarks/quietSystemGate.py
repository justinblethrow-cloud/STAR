#!/usr/bin/env python3
"""Require sustained CPU and storage quiescence before a performance run."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import os
from pathlib import Path
import re
import time


@dataclass(frozen=True)
class CpuSample:
    total: int
    idle: int
    iowait: int


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--duration", type=float, default=300.0)
    parser.add_argument("--interval", type=float, default=5.0)
    parser.add_argument("--min-idle", type=float, default=90.0)
    parser.add_argument("--max-iowait", type=float, default=2.0)
    parser.add_argument("--max-storage-util", type=float, default=10.0)
    parser.add_argument("--device", action="append", default=[])
    parser.add_argument("--path", type=Path, action="append", default=[])
    parser.add_argument("--competing-regex")
    parser.add_argument("--output", type=Path)
    return parser.parse_args()


def read_cpu() -> CpuSample:
    fields = Path("/proc/stat").read_text(encoding="ascii").splitlines()[0].split()
    values = [int(value) for value in fields[1:]]
    while len(values) < 8:
        values.append(0)
    return CpuSample(sum(values), values[3], values[4])


def read_disk_ticks() -> dict[str, int]:
    result: dict[str, int] = {}
    for line in Path("/proc/diskstats").read_text(encoding="ascii").splitlines():
        fields = line.split()
        if len(fields) >= 13:
            result[fields[2]] = int(fields[12])
    return result


def device_for_path(path: Path) -> str | None:
    stat = path.resolve().stat()
    uevent = Path(f"/sys/dev/block/{os.major(stat.st_dev)}:{os.minor(stat.st_dev)}/uevent")
    if not uevent.is_file():
        return None
    for line in uevent.read_text(encoding="ascii").splitlines():
        if line.startswith("DEVNAME="):
            return Path(line.split("=", 1)[1]).name
    return None


def competing_processes(pattern: re.Pattern[str] | None) -> list[tuple[int, str]]:
    if pattern is None:
        return []
    matches: list[tuple[int, str]] = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit() or int(entry.name) == os.getpid():
            continue
        try:
            command = (entry / "cmdline").read_bytes().replace(b"\0", b" ").decode(
                "utf-8", errors="replace"
            )
        except (FileNotFoundError, PermissionError, ProcessLookupError):
            continue
        if command and pattern.search(command):
            matches.append((int(entry.name), command.strip()))
    return matches


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def main() -> int:
    args = parse_args()
    if args.duration <= 0 or args.interval <= 0 or args.interval > args.duration:
        raise SystemExit("duration and interval must be positive, with interval <= duration")

    devices = set(args.device)
    for path in args.path:
        device = device_for_path(path)
        if device:
            devices.add(device)
    process_pattern = re.compile(args.competing_regex) if args.competing_regex else None
    samples: list[tuple[str, float, float, float, str]] = []
    failures: list[str] = []

    previous_cpu = read_cpu()
    previous_disks = read_disk_ticks()
    previous_time = time.monotonic()
    deadline = previous_time + args.duration
    while time.monotonic() < deadline:
        time.sleep(min(args.interval, max(0.0, deadline - time.monotonic())))
        current_time = time.monotonic()
        current_cpu = read_cpu()
        current_disks = read_disk_ticks()
        total = current_cpu.total - previous_cpu.total
        idle = 100.0 * (current_cpu.idle - previous_cpu.idle) / total if total else 0.0
        iowait = 100.0 * (current_cpu.iowait - previous_cpu.iowait) / total if total else 0.0
        elapsed = current_time - previous_time
        disk_utils = [
            100.0
            * (current_disks.get(device, 0) - previous_disks.get(device, 0))
            / (elapsed * 1000.0)
            for device in devices
        ]
        storage_util = max(disk_utils, default=0.0)
        competitors = competing_processes(process_pattern)
        competitor_text = "; ".join(f"{pid}:{command}" for pid, command in competitors)
        samples.append((utc_now(), idle, iowait, storage_util, competitor_text))

        if idle < args.min_idle:
            failures.append(f"cpu idle {idle:.2f}% < {args.min_idle:.2f}%")
        if iowait > args.max_iowait:
            failures.append(f"iowait {iowait:.2f}% > {args.max_iowait:.2f}%")
        if storage_util > args.max_storage_util:
            failures.append(
                f"storage util {storage_util:.2f}% > {args.max_storage_util:.2f}%"
            )
        if competitors:
            failures.append(f"competing processes: {competitor_text}")
        previous_cpu = current_cpu
        previous_disks = current_disks
        previous_time = current_time

    lines = ["utc\tcpu_idle_percent\tiowait_percent\tstorage_util_percent\tcompetitors"]
    lines.extend(
        f"{stamp}\t{idle:.3f}\t{iowait:.3f}\t{storage:.3f}\t{competitors}"
        for stamp, idle, iowait, storage, competitors in samples
    )
    output = "\n".join(lines) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8")
    else:
        print(output, end="")

    if failures:
        print("quiet-system gate: FAIL")
        for failure in dict.fromkeys(failures):
            print(f"  {failure}")
        return 1
    print(
        f"quiet-system gate: PASS for {args.duration:.0f}s "
        f"on devices {','.join(sorted(devices)) or 'not sampled'}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

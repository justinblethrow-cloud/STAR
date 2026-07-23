#!/usr/bin/env python3
"""Create a deterministic, validated first-N subset of paired gzip FASTQ files."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import os
from pathlib import Path
import tempfile


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--read1", type=Path, required=True)
    parser.add_argument("--read2", type=Path, required=True)
    parser.add_argument("--records", type=int, required=True)
    parser.add_argument("--output1", type=Path, required=True)
    parser.add_argument("--output2", type=Path, required=True)
    return parser.parse_args()


def read_record(handle: gzip.GzipFile, label: str, index: int) -> list[bytes] | None:
    lines = [handle.readline() for _ in range(4)]
    if lines[0] == b"":
        if any(lines[1:]):
            raise ValueError(f"{label}: truncated record after EOF at record {index}")
        return None
    if any(line == b"" for line in lines):
        raise ValueError(f"{label}: truncated FASTQ record {index}")
    if not lines[0].startswith(b"@") or not lines[2].startswith(b"+"):
        raise ValueError(f"{label}: malformed FASTQ record {index}")
    sequence = lines[1].rstrip(b"\r\n")
    quality = lines[3].rstrip(b"\r\n")
    if len(sequence) != len(quality):
        raise ValueError(f"{label}: sequence/quality length mismatch at record {index}")
    return lines


def canonical_name(header: bytes) -> bytes:
    name = header[1:].split(None, 1)[0].rstrip(b"\r\n")
    if name.endswith((b"/1", b"/2")):
        return name[:-2]
    return name


def open_deterministic_gzip(path: Path) -> tuple[Path, gzip.GzipFile]:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, raw = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    os.close(fd)
    temporary = Path(raw)
    binary = temporary.open("wb")
    return temporary, gzip.GzipFile(filename="", mode="wb", fileobj=binary, mtime=0)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    args = parse_args()
    if args.records <= 0:
        raise SystemExit("--records must be positive")
    if args.output1.resolve() == args.output2.resolve():
        raise SystemExit("mate outputs must be distinct")

    temp1, output1 = open_deterministic_gzip(args.output1)
    temp2, output2 = open_deterministic_gzip(args.output2)
    completed = False
    try:
        with gzip.open(args.read1, "rb") as read1, gzip.open(args.read2, "rb") as read2:
            for index in range(1, args.records + 1):
                record1 = read_record(read1, "read1", index)
                record2 = read_record(read2, "read2", index)
                if record1 is None or record2 is None:
                    raise ValueError(f"input ended before requested record {index}")
                if canonical_name(record1[0]) != canonical_name(record2[0]):
                    raise ValueError(f"mate names differ at record {index}")
                output1.writelines(record1)
                output2.writelines(record2)
        completed = True
    finally:
        output1.close()
        output2.close()
        if not completed:
            temp1.unlink(missing_ok=True)
            temp2.unlink(missing_ok=True)

    os.replace(temp1, args.output1)
    os.replace(temp2, args.output2)
    print(
        f"wrote {args.records} paired records; "
        f"read1_sha256={sha256(args.output1)} read2_sha256={sha256(args.output2)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

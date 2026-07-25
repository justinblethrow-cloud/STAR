#!/usr/bin/env python3
"""Create a deterministic, validated first-N subset of one FASTQ file."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import gzip
import hashlib
import os
from pathlib import Path
import tempfile
from typing import BinaryIO, Iterator


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--records", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def read_record(handle: BinaryIO, index: int) -> list[bytes] | None:
    lines = [handle.readline() for _ in range(4)]
    if lines[0] == b"":
        if any(lines[1:]):
            raise ValueError(f"truncated record after EOF at record {index}")
        return None
    if any(line == b"" for line in lines):
        raise ValueError(f"truncated FASTQ record {index}")
    if not lines[0].startswith(b"@") or not lines[2].startswith(b"+"):
        raise ValueError(f"malformed FASTQ record {index}")
    sequence = lines[1].rstrip(b"\r\n")
    quality = lines[3].rstrip(b"\r\n")
    if len(sequence) != len(quality):
        raise ValueError(f"sequence/quality length mismatch at record {index}")
    return lines


def sha256(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


@contextmanager
def open_input(path: Path) -> Iterator[BinaryIO]:
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rb") as handle:
        yield handle


def open_output(path: Path) -> tuple[Path, BinaryIO]:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, raw = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    os.close(descriptor)
    temporary = Path(raw)
    binary = temporary.open("wb")
    if path.suffix == ".gz":
        return temporary, gzip.GzipFile(
            filename="", mode="wb", fileobj=binary, mtime=0
        )
    return temporary, binary


def main() -> int:
    args = parse_args()
    if args.records <= 0:
        raise SystemExit("--records must be positive")
    if not args.input.is_file():
        raise SystemExit(f"input file is absent: {args.input}")
    if args.input.resolve() == args.output.resolve():
        raise SystemExit("input and output must be distinct")

    temporary, output = open_output(args.output)
    completed = False
    try:
        with open_input(args.input) as source:
            for index in range(1, args.records + 1):
                record = read_record(source, index)
                if record is None:
                    raise ValueError(f"input ended before requested record {index}")
                output.writelines(record)
        completed = True
    finally:
        output.close()
        if not completed:
            temporary.unlink(missing_ok=True)

    os.replace(temporary, args.output)
    print(
        f"wrote {args.records} records; output_sha256={sha256(args.output)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

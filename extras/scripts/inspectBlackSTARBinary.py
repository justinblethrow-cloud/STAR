#!/usr/bin/env python3
"""Record and validate the runtime compatibility boundary of a release ELF."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess


VERSION_FAMILIES = ("GLIBC", "GLIBCXX", "CXXABI", "GOMP")


def run(*args: str) -> str:
    return subprocess.run(
        args,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    ).stdout


def version_key(value: str) -> tuple[int, ...]:
    return tuple(int(component) for component in value.split("."))


def highest_symbol_version(symbols: str, family: str) -> str:
    pattern = re.compile(rf"\b{re.escape(family)}_(\d+(?:\.\d+)*)\b")
    versions = {match.group(1) for match in pattern.finditer(symbols)}
    return max(versions, key=version_key) if versions else "none"


def elf_field(header: str, name: str) -> str:
    pattern = re.compile(rf"^\s*{re.escape(name)}:\s*(.+?)\s*$", re.MULTILINE)
    match = pattern.search(header)
    if not match:
        raise ValueError(f"readelf did not report {name}")
    return match.group(1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument(
        "--cpu-target",
        choices=("baseline", "avx2"),
        required=True,
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    binary = args.binary.resolve()
    header = run("readelf", "-h", str(binary))
    symbols = run("objdump", "-T", str(binary))
    disassembly = run("objdump", "-d", str(binary))
    notes = run("readelf", "-n", str(binary))

    has_ymm = re.search(r"%ymm\d+\b", disassembly) is not None
    if args.cpu_target == "baseline" and has_ymm:
        raise SystemExit(
            "baseline compatibility check failed: binary contains YMM instructions"
        )
    if args.cpu_target == "avx2" and not has_ymm:
        raise SystemExit(
            "AVX2 compatibility check failed: binary contains no YMM instructions"
        )

    isa_property = "not-declared"
    for line in notes.splitlines():
        if "x86 ISA needed:" in line:
            isa_property = line.split("x86 ISA needed:", 1)[1].strip()
            break

    values = [
        ("binary_format", elf_field(header, "Class")),
        ("machine", elf_field(header, "Machine")),
        ("cpu_target", args.cpu_target),
        ("ymm_instructions", "present" if has_ymm else "absent"),
        ("gnu_x86_isa_needed", isa_property),
    ]
    values.extend(
        (f"minimum_{family.lower()}", highest_symbol_version(symbols, family))
        for family in VERSION_FAMILIES
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="\n") as output:
        output.write("key\tvalue\n")
        for key, value in values:
            output.write(f"{key}\t{value}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

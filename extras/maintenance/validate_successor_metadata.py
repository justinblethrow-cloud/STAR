#!/usr/bin/env python3
"""Validate BlackSTAR successor metadata and bounded benchmark receipts."""

from __future__ import annotations

import csv
import json
from pathlib import Path
import re
import statistics
import sys

try:
    import yaml
except ImportError as error:
    raise SystemExit("PyYAML is required to validate GitHub metadata") from error


def macro(path: Path, name: str) -> str:
    pattern = re.compile(rf'^#define {re.escape(name)} "([^"]+)"$')
    for line in path.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match:
            return match.group(1)
    raise ValueError(f"missing {name} in {path}")


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def assert_close(actual: float, expected: float, label: str) -> None:
    if abs(actual - expected) > 1e-9:
        raise ValueError(f"{label}: {actual} != {expected}")


def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    required = [
        "README.md",
        "ATTRIBUTION.md",
        "GOVERNANCE.md",
        "SECURITY.md",
        "SUPPORT.md",
        "CONTRIBUTING.md",
        "CHANGELOG.md",
        "docs/COMPATIBILITY.md",
        "docs/BLACKSTAR_RELEASE.md",
        "docs/PERFORMANCE.md",
        "docs/MIGRATING_FROM_STAR.md",
        "docs/VERSIONING.md",
        "docs/RELEASE_POLICY.md",
        "docs/INDEPENDENCE_TRANSITION.md",
        "docs/releases/1.0.0-release-notes.md",
        ".github/repository-settings.json",
    ]
    for relative in required:
        if not (repo / relative).is_file():
            raise ValueError(f"required successor document missing: {relative}")

    version_file = repo / "source" / "VERSION"
    blackstar_version = macro(version_file, "BLACKSTAR_VERSION")
    executable_version = macro(version_file, "STAR_VERSION")
    compatibility_version = macro(version_file, "STAR_COMPATIBILITY_VERSION")
    genome_format_version = macro(
        version_file, "BLACKSTAR_GENOME_FORMAT_VERSION"
    )
    if not re.fullmatch(r"\d+\.\d+\.\d+", blackstar_version):
        raise ValueError(f"BlackSTAR version is not stable SemVer: {blackstar_version}")
    if "blackstar" not in executable_version:
        raise ValueError("executable version does not preserve BlackSTAR identity")
    if compatibility_version != "2.7.11b":
        raise ValueError("unexpected STAR compatibility base")

    release_notes = repo / "docs" / "releases" / (
        f"{blackstar_version}-release-notes.md"
    )
    if not release_notes.is_file():
        raise ValueError(
            f"release notes missing for BlackSTAR {blackstar_version}"
        )

    defaults = (repo / "source" / "parametersDefault").read_text(encoding="utf-8")
    default_match = re.search(r"^versionGenome\s+(\S+)", defaults, re.MULTILINE)
    if not default_match or default_match.group(1) != genome_format_version:
        raise ValueError("genome-format macro differs from parametersDefault")

    for path in sorted((repo / ".github").rglob("*.yml")):
        with path.open(encoding="utf-8") as stream:
            yaml.safe_load(stream)
    for path in sorted((repo / ".github").rglob("*.yaml")):
        with path.open(encoding="utf-8") as stream:
            yaml.safe_load(stream)
    for path in sorted((repo / "docs").rglob("*.json")) + sorted(
        (repo / ".github").rglob("*.json")
    ):
        with path.open(encoding="utf-8") as stream:
            json.load(stream)

    readme = (repo / "README.md").read_text(encoding="utf-8")
    contributing = (repo / "CONTRIBUTING.md").read_text(encoding="utf-8")
    compatibility = (repo / "docs" / "COMPATIBILITY.md").read_text(
        encoding="utf-8"
    )
    changelog = (repo / "CHANGELOG.md").read_text(encoding="utf-8")
    release_boundary = (repo / "docs" / "BLACKSTAR_RELEASE.md").read_text(
        encoding="utf-8"
    )
    versioning = (repo / "docs" / "VERSIONING.md").read_text(encoding="utf-8")
    forbidden = [
        "github.com/alexdobin/STAR/issues",
        "github.com/alexdobin/STAR/releases",
        "BlackSTAR 2.7.11b-blackstar.1",
    ]
    for value in forbidden:
        if value in readme or value in contributing:
            raise ValueError(f"stale upstream project routing remains: {value}")
    for value, label in (
        (blackstar_version, "BlackSTAR release"),
        (executable_version, "executable identity"),
        (compatibility_version, "STAR compatibility base"),
        (genome_format_version, "genome format"),
    ):
        if value not in compatibility:
            raise ValueError(f"{label} missing from compatibility contract")
    if executable_version not in versioning:
        raise ValueError("current executable identity missing from versioning policy")
    for text, label in (
        (readme, "README"),
        (changelog, "changelog"),
        (release_boundary, "release boundary"),
        (release_notes.read_text(encoding="utf-8"), "release notes"),
    ):
        if blackstar_version not in text:
            raise ValueError(
                f"BlackSTAR release {blackstar_version} missing from {label}"
            )

    settings = json.loads(
        (repo / ".github" / "repository-settings.json").read_text(
            encoding="utf-8"
        )
    )
    if settings.get("schema_version") != 1:
        raise ValueError("unsupported repository-settings schema")
    repository_settings = settings["repository"]
    if repository_settings["default_branch"] != "main":
        raise ValueError("successor default branch is not main")
    if repository_settings["features"] != {
        "discussions": True,
        "issues": True,
        "wiki": False,
    }:
        raise ValueError("unexpected successor repository feature policy")
    contexts = settings["branch_protection"]["required_status_checks"]["contexts"]
    expected_contexts = [
        "build-and-test",
        "compiler-gcc",
        "compiler-clang",
        "starlong-build-and-smoke",
        "release-portability",
        "codeql-c-cpp",
        "codeql-python",
    ]
    if contexts != expected_contexts:
        raise ValueError("required branch-protection checks do not match policy")

    for relative in required:
        text = (repo / relative).read_text(encoding="utf-8")
        if "/mnt/datavault/" in text:
            raise ValueError(f"private local path in public document: {relative}")

    evidence = (
        repo
        / "docs"
        / "benchmarks"
        / "official-star-2.7.11b-vs-blackstar.2"
    )
    summary = json.loads((evidence / "summary.json").read_text(encoding="utf-8"))

    index_rows = read_tsv(evidence / "index-pairs.tsv")
    assert_close(
        statistics.median(float(row["upstream_wall_seconds"]) for row in index_rows),
        summary["full_chm13_index_96_threads"]["upstream_median_wall_seconds"],
        "upstream index median",
    )
    assert_close(
        statistics.median(float(row["blackstar_wall_seconds"]) for row in index_rows),
        summary["full_chm13_index_96_threads"]["blackstar_median_wall_seconds"],
        "BlackSTAR index median",
    )

    alignment_rows = read_tsv(evidence / "alignment-scaling.tsv")
    for input_mode in ("uncompressed", "zcat"):
        row = next(
            value
            for value in alignment_rows
            if value["input"] == input_mode and value["threads"] == "96"
        )
        values = summary["alignment"][f"{input_mode}_96_threads"]
        assert_close(
            float(row["blackstar_median_wall_seconds"]),
            values["blackstar_median_wall_seconds"],
            f"{input_mode} BlackSTAR median",
        )
        if row["correctness_passed"] != "True":
            raise ValueError(f"{input_mode} correctness receipt failed")

    delta_rows = read_tsv(evidence / "delta-cold-pairs.tsv")
    cold = summary["named_sequence_addition"]["cold_input_file_cache"]
    assert_close(
        statistics.median(
            float(row["blackstar_delta_wall_seconds"]) for row in delta_rows
        ),
        cold["blackstar_delta_median_wall_seconds"],
        "Delta cold median",
    )

    print(
        "BlackSTAR successor metadata: PASS "
        f"(release={blackstar_version}, executable={executable_version})"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, json.JSONDecodeError, yaml.YAMLError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)

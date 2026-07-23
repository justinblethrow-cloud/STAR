#!/usr/bin/env python3
"""Validate BlackSTAR architecture provenance, claims, and public hygiene."""

from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path
import re
import sys


REPO_ROOT = Path(__file__).resolve().parents[2]
ARCH_ROOT = REPO_ROOT / "docs/architecture"
MANIFEST_PATH = ARCH_ROOT / "figures.json"
CLAIMS_PATH = ARCH_ROOT / "claims.tsv"
SOURCE_MARKER = "blackstar-source-sha256"
ALLOWED_MATURITY = {"upstream", "accepted", "experimental", "rejected", "superseded", "roadmap"}
REQUIRED_FIGURE_FIELDS = {
    "id",
    "title",
    "maturity",
    "visibility",
    "code_commit",
    "source",
    "evidence",
    "caption",
    "alt_text",
    "outputs",
}
REQUIRED_CLAIM_FIELDS = {
    "claim_id",
    "statement",
    "value",
    "unit",
    "scope",
    "evidence_path",
    "evidence_sha256",
    "code_commit",
    "qualification",
}
PUBLIC_FORBIDDEN = {
    "/mnt/": "absolute internal mount path",
    "/home/": "absolute home path",
    "9ZJN42": "internal order identifier",
    "Plasmidsaurus": "internal brand belongs only in the private derivative",
    "justinblethrow": "personal account identifier",
}
EMAIL_PATTERN = re.compile(r"\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b")
COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative_path(raw: str, errors: list[str], context: str) -> Path:
    path = Path(raw)
    if path.is_absolute() or ".." in path.parts:
        errors.append(f"{context}: path must be repository-relative: {raw}")
    return REPO_ROOT / path


def validate_figures(errors: list[str]) -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1:
        errors.append("figures.json: unsupported schema_version")
    if manifest.get("canonical_visibility") != "public":
        errors.append("figures.json: canonical visibility must be public")
    if manifest.get("renderers", {}).get("mermaid_cli") != "11.16.0":
        errors.append("figures.json: Mermaid CLI must remain pinned to 11.16.0")

    figures = manifest.get("figures", [])
    if len(figures) < 14:
        errors.append(f"figures.json: expected at least 14 canonical figures, got {len(figures)}")
    expected_ids = [f"F{index:02d}" for index in range(1, len(figures) + 1)]
    actual_ids = [figure.get("id") for figure in figures]
    if actual_ids != expected_ids:
        errors.append(f"figures.json: expected ordered IDs {expected_ids}, got {actual_ids}")

    for figure in figures:
        figure_id = figure.get("id", "<missing>")
        missing = REQUIRED_FIGURE_FIELDS - set(figure)
        if missing:
            errors.append(f"{figure_id}: missing fields {sorted(missing)}")
            continue
        if figure["maturity"] not in ALLOWED_MATURITY:
            errors.append(f"{figure_id}: invalid maturity {figure['maturity']}")
        if figure["visibility"] != "public":
            errors.append(f"{figure_id}: canonical figure must be public")
        if len(figure["caption"].strip()) < 40:
            errors.append(f"{figure_id}: caption is too short")
        if len(figure["alt_text"].strip()) < 60:
            errors.append(f"{figure_id}: alt text is too short")

        source = relative_path(figure["source"], errors, figure_id)
        if not source.is_file():
            errors.append(f"{figure_id}: missing source {figure['source']}")
            continue
        if source.suffix != ".mmd":
            errors.append(f"{figure_id}: source must be Mermaid .mmd")

        for evidence_raw in figure["evidence"]:
            evidence = relative_path(evidence_raw, errors, figure_id)
            if not evidence.exists():
                errors.append(f"{figure_id}: missing evidence {evidence_raw}")

        suffixes: set[str] = set()
        for output_raw in figure["outputs"]:
            output = relative_path(output_raw, errors, figure_id)
            suffixes.add(output.suffix)
            if not output.is_file():
                errors.append(f"{figure_id}: missing generated output {output_raw}")
                continue
            if output.suffix == ".svg":
                marker = f"{SOURCE_MARKER}: {sha256(source)}"
                if marker not in output.read_text(encoding="utf-8"):
                    errors.append(f"{figure_id}: SVG source digest is stale")
        if suffixes != {".svg", ".pdf"}:
            errors.append(f"{figure_id}: outputs must contain one SVG and one PDF")


def validate_claims(errors: list[str]) -> None:
    with CLAIMS_PATH.open(encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if set(reader.fieldnames or []) != REQUIRED_CLAIM_FIELDS:
            errors.append(
                f"claims.tsv: expected columns {sorted(REQUIRED_CLAIM_FIELDS)}, "
                f"got {reader.fieldnames}"
            )
            return
        rows = list(reader)

    seen: set[str] = set()
    evidence_text_by_path: dict[Path, str] = {}
    for row in rows:
        claim_id = row["claim_id"]
        if claim_id in seen:
            errors.append(f"claims.tsv: duplicate claim_id {claim_id}")
        seen.add(claim_id)
        if not row["statement"].strip() or not row["qualification"].strip():
            errors.append(f"{claim_id}: statement and qualification are required")
        if not COMMIT_PATTERN.match(row["code_commit"]):
            errors.append(f"{claim_id}: code_commit must be a full SHA-1")

        evidence = relative_path(row["evidence_path"], errors, claim_id)
        if not evidence.is_file():
            errors.append(f"{claim_id}: missing evidence {row['evidence_path']}")
            continue
        digest = sha256(evidence)
        if row["evidence_sha256"] != digest:
            errors.append(
                f"{claim_id}: evidence digest mismatch; expected {digest}"
            )
        text = evidence_text_by_path.setdefault(
            evidence, evidence.read_text(encoding="utf-8")
        )
        if claim_id not in text:
            errors.append(f"{claim_id}: claim ID absent from evidence record")


def validate_public_hygiene(errors: list[str]) -> None:
    text_suffixes = {".md", ".json", ".tsv", ".mmd", ".svg"}
    for path in sorted(ARCH_ROOT.rglob("*")):
        if not path.is_file() or path.suffix not in text_suffixes:
            continue
        text = path.read_text(encoding="utf-8")
        relative = path.relative_to(REPO_ROOT)
        for token, meaning in PUBLIC_FORBIDDEN.items():
            if token in text:
                errors.append(f"{relative}: contains {meaning}: {token}")
        email = EMAIL_PATTERN.search(text)
        if email:
            errors.append(f"{relative}: contains email address {email.group(0)}")


def validate_experiment_contract(errors: list[str]) -> None:
    required = [
        "docs/experiments/README.md",
        "docs/experiments/EXPERIMENT_TEMPLATE.md",
        "docs/experiments/ROADMAP.md",
        "docs/experiments/A00-public-alignment-baseline.md",
        "docs/experiments/A01-touched-window-bins.md",
        "docs/architecture/STATUS_HISTORY.md",
    ]
    for raw in required:
        if not (REPO_ROOT / raw).is_file():
            errors.append(f"missing governance artifact {raw}")


def main() -> int:
    errors: list[str] = []
    validate_figures(errors)
    validate_claims(errors)
    validate_public_hygiene(errors)
    validate_experiment_contract(errors)
    if errors:
        print("architecture validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    figure_count = len(json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))["figures"])
    print(f"architecture validation passed: {figure_count} figures and claim provenance are complete")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

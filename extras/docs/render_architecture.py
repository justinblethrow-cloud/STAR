#!/usr/bin/env python3
"""Render the canonical BlackSTAR architecture atlas from pinned Mermaid sources."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = REPO_ROOT / "docs/architecture/figures.json"
DEFAULT_MMDC = REPO_ROOT / "extras/docs/node_modules/.bin/mmdc"
MERMAID_CONFIG = REPO_ROOT / "extras/docs/mermaid.config.json"
PUPPETEER_CONFIG = Path(
    os.environ.get(
        "PUPPETEER_CONFIG",
        REPO_ROOT / "extras/docs/puppeteer.config.json",
    )
)
SOURCE_MARKER = "blackstar-source-sha256"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument(
        "--check",
        action="store_true",
        help="render SVGs to a temporary directory and compare with tracked files",
    )
    parser.add_argument(
        "--svg-only",
        action="store_true",
        help="skip PDF convenience exports",
    )
    parser.add_argument(
        "--figure",
        action="append",
        help="render only this figure ID; may be repeated",
    )
    return parser.parse_args()


def locate_mmdc() -> Path:
    override = os.environ.get("MMDC")
    if override:
        path = Path(override)
    elif DEFAULT_MMDC.exists():
        path = DEFAULT_MMDC
    else:
        found = shutil.which("mmdc")
        path = Path(found) if found else DEFAULT_MMDC
    if not path.exists():
        raise SystemExit(
            "mmdc was not found. Run 'npm ci --prefix extras/docs' or set MMDC."
        )
    return path


def source_digest(source: Path) -> str:
    return hashlib.sha256(source.read_bytes()).hexdigest()


def add_svg_provenance(svg_path: Path, digest: str) -> None:
    text = svg_path.read_text(encoding="utf-8")
    lines = text.splitlines()
    lines = [line for line in lines if SOURCE_MARKER not in line]
    marker = f"<!-- {SOURCE_MARKER}: {digest} -->"
    if lines and lines[0].startswith("<?xml"):
        lines.insert(1, marker)
    else:
        lines.insert(0, marker)
    svg_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def render_one(mmdc: Path, source: Path, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(mmdc),
        "--input",
        str(source),
        "--output",
        str(output),
        "--configFile",
        str(MERMAID_CONFIG),
        "--puppeteerConfigFile",
        str(PUPPETEER_CONFIG),
        "--backgroundColor",
        "white",
        "--quiet",
    ]
    if output.suffix == ".pdf":
        command.append("--pdfFit")
    subprocess.run(command, cwd=REPO_ROOT, check=True)
    if output.suffix == ".svg":
        add_svg_provenance(output, source_digest(source))


def compare_files(expected: Path, actual: Path) -> bool:
    return expected.read_bytes() == actual.read_bytes()


def main() -> int:
    args = parse_args()
    if not PUPPETEER_CONFIG.is_file():
        raise SystemExit(f"Puppeteer config was not found: {PUPPETEER_CONFIG}")
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    mmdc = locate_mmdc()
    failures: list[str] = []
    figures = manifest["figures"]
    if args.figure:
        requested = set(args.figure)
        figures = [figure for figure in figures if figure["id"] in requested]
        missing = requested - {figure["id"] for figure in figures}
        if missing:
            raise SystemExit(f"unknown figure IDs: {', '.join(sorted(missing))}")

    with tempfile.TemporaryDirectory(prefix="blackstar-diagrams-") as tmp:
        tmp_root = Path(tmp)
        for figure in figures:
            source = REPO_ROOT / figure["source"]
            outputs = [REPO_ROOT / item for item in figure["outputs"]]
            svg = next(path for path in outputs if path.suffix == ".svg")

            if args.check:
                rendered_svg = tmp_root / svg.name
                render_one(mmdc, source, rendered_svg)
                if not svg.exists() or not compare_files(svg, rendered_svg):
                    failures.append(f"{figure['id']}: stale SVG {svg.relative_to(REPO_ROOT)}")
                continue

            render_one(mmdc, source, svg)
            if not args.svg_only:
                pdf = next(path for path in outputs if path.suffix == ".pdf")
                render_one(mmdc, source, pdf)

    if failures:
        print("\n".join(failures), file=sys.stderr)
        print(
            "Run python3 extras/docs/render_architecture.py and review the diff.",
            file=sys.stderr,
        )
        return 1
    print(f"validated {len(figures)} rendered figures")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

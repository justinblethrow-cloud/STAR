#!/usr/bin/env python3
"""Verify a BlackSTAR transition backup and perform a local restore drill."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def checksum(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run(args: list[str]) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        args,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        raise RuntimeError(f"{' '.join(args)} failed:\n{result.stderr.strip()}")
    return result


def refs(git_dir: Path) -> str:
    return run(
        [
            "git",
            f"--git-dir={git_dir}",
            "for-each-ref",
            "--format=%(refname)\t%(objectname)\t%(objecttype)",
        ]
    ).stdout


def verify_checksums(backup: Path) -> int:
    checked = 0
    for line in (backup / "checksums.sha256").read_text(encoding="utf-8").splitlines():
        expected, relative = line.split("  ", 1)
        path = backup / relative
        if not path.is_file():
            raise RuntimeError(f"missing backup file: {relative}")
        actual = checksum(path)
        if actual != expected:
            raise RuntimeError(
                f"checksum mismatch for {relative}: {actual} != {expected}"
            )
        checked += 1
    return checked


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--backup", type=Path, required=True)
    parser.add_argument("--receipt", type=Path)
    args = parser.parse_args()

    backup = args.backup.resolve()
    checked = verify_checksums(backup)
    bundle = backup / "git" / "local-all.bundle"
    run(["git", "bundle", "verify", str(bundle)])

    with tempfile.TemporaryDirectory(prefix="blackstar-restore-drill.") as temp:
        temp_root = Path(temp)
        restored_bundle = temp_root / "bundle.git"
        restored_remote = temp_root / "remote.git"
        run(["git", "clone", "--mirror", str(bundle), str(restored_bundle)])
        run(
            [
                "git",
                "clone",
                "--mirror",
                str(backup / "git" / "remote-mirror.git"),
                str(restored_remote),
            ]
        )

        expected_local = (backup / "git" / "local-refs.tsv").read_text(
            encoding="utf-8"
        )
        expected_remote = (backup / "git" / "remote-refs.tsv").read_text(
            encoding="utf-8"
        )
        if refs(restored_bundle) != expected_local:
            raise RuntimeError("bundle restore reference inventory differs")
        if refs(restored_remote) != expected_remote:
            raise RuntimeError("remote mirror restore reference inventory differs")
        run(["git", f"--git-dir={restored_bundle}", "fsck", "--full"])
        run(["git", f"--git-dir={restored_remote}", "fsck", "--full"])

    snapshot = json.loads((backup / "snapshot.json").read_text(encoding="utf-8"))
    receipt = {
        "status": "PASS",
        "checked_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "backup": str(backup),
        "checksum_manifest_sha256": checksum(backup / "checksums.sha256"),
        "repository": snapshot["repository"],
        "default_branch": snapshot["default_branch"],
        "default_branch_commit": snapshot["default_branch_commit"],
        "files_verified": checked,
        "bundle_restore": "PASS",
        "remote_mirror_restore": "PASS",
    }
    rendered = json.dumps(receipt, indent=2, sort_keys=True) + "\n"
    if args.receipt:
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except RuntimeError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)

#!/usr/bin/env python3
"""Check the live and archived gates immediately before fork detachment."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def gh_api(endpoint: str) -> dict:
    result = subprocess.run(
        ["gh", "api", endpoint],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip())
    return json.loads(result.stdout)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def check_runs(repo: str, commit: str, name: str) -> list[dict]:
    values = gh_api(f"repos/{repo}/commits/{commit}/check-runs?per_page=100")[
        "check_runs"
    ]
    return [value for value in values if value["name"] == name]


def check_run_receipts(values: list[dict]) -> list[dict]:
    return [
        {
            "id": value["id"],
            "status": value["status"],
            "conclusion": value["conclusion"],
            "html_url": value["html_url"],
        }
        for value in values
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True)
    parser.add_argument("--backup", type=Path, required=True)
    parser.add_argument("--restore-receipt", type=Path, required=True)
    parser.add_argument("--expected-sha", required=True)
    parser.add_argument("--expected-parent", default="alexdobin/STAR")
    parser.add_argument("--required-check", default="build-and-test")
    parser.add_argument("--transition-branch")
    parser.add_argument("--transition-sha")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if bool(args.transition_branch) != bool(args.transition_sha):
        raise RuntimeError(
            "--transition-branch and --transition-sha must be provided together"
        )

    backup = args.backup.resolve()
    snapshot = json.loads((backup / "snapshot.json").read_text(encoding="utf-8"))
    restore = json.loads(args.restore_receipt.read_text(encoding="utf-8"))
    repository = gh_api(f"repos/{args.repo}")
    default_branch = repository["default_branch"]
    branch = gh_api(f"repos/{args.repo}/branches/{default_branch}")

    failures = []
    if restore.get("status") != "PASS":
        failures.append("restore drill did not pass")
    if restore.get("backup") != str(backup):
        failures.append("restore receipt names a different backup")
    manifest = backup / "checksums.sha256"
    if restore.get("checksum_manifest_sha256") != sha256(manifest):
        failures.append("restore receipt does not match the checksum manifest")
    if restore.get("repository") != args.repo:
        failures.append("restore receipt names a different repository")
    if restore.get("default_branch_commit") != args.expected_sha:
        failures.append("restore receipt default-branch commit differs")
    if snapshot["repository"] != args.repo:
        failures.append("archive names a different repository")
    if snapshot["default_branch_commit"] != args.expected_sha:
        failures.append("archived default-branch commit differs")
    if branch["commit"]["sha"] != args.expected_sha:
        failures.append("live default-branch commit differs")
    if default_branch != "master":
        failures.append("pre-detachment default branch is not master")
    if not repository["fork"]:
        failures.append("repository is already detached")
    elif repository.get("parent", {}).get("full_name") != args.expected_parent:
        failures.append("repository has an unexpected fork-network parent")
    if repository["size"] >= 1_000_000:
        failures.append("repository is not below the native 1 GB detachment limit")
    if repository["forks_count"] != 0:
        failures.append("repository has child forks")

    archived_releases = json.loads(
        (backup / "github" / "releases.json").read_text(encoding="utf-8")
    )
    live_releases = gh_api(f"repos/{args.repo}/releases?per_page=100")
    archived_tags = sorted(value["tag_name"] for value in archived_releases)
    live_tags = sorted(value["tag_name"] for value in live_releases)
    if archived_tags != live_tags:
        failures.append("live release tags differ from the archive")

    archived_git_tags = {
        value["name"]: value["commit"]["sha"]
        for value in json.loads(
            (backup / "github" / "tags.json").read_text(encoding="utf-8")
        )
    }
    live_git_tags = {
        value["name"]: value["commit"]["sha"]
        for value in gh_api(f"repos/{args.repo}/tags?per_page=100")
    }
    if archived_git_tags != live_git_tags:
        failures.append("live Git tag inventory differs from the archive")

    matching_checks = check_runs(
        args.repo, args.expected_sha, args.required_check
    )
    if not any(
        value["status"] == "completed" and value["conclusion"] == "success"
        for value in matching_checks
    ):
        failures.append(f"required check is not successful: {args.required_check}")

    transition = None
    if args.transition_branch:
        transition_branch = gh_api(
            f"repos/{args.repo}/branches/{args.transition_branch}"
        )
        transition_checks = check_runs(
            args.repo, args.transition_sha, args.required_check
        )
        archived_branches = {
            value["name"]: value["commit"]["sha"]
            for value in json.loads(
                (backup / "github" / "branches.json").read_text(encoding="utf-8")
            )
        }
        evidence_commits = snapshot.get("workflow_evidence_commits", [])
        if transition_branch["commit"]["sha"] != args.transition_sha:
            failures.append("live transition branch commit differs")
        if archived_branches.get(args.transition_branch) != args.transition_sha:
            failures.append("archived transition branch commit differs")
        if args.transition_sha not in evidence_commits:
            failures.append("transition SHA is absent from archived workflow evidence")
        if not any(
            value["status"] == "completed" and value["conclusion"] == "success"
            for value in transition_checks
        ):
            failures.append(
                f"transition check is not successful: {args.required_check}"
            )
        transition = {
            "branch": args.transition_branch,
            "commit": args.transition_sha,
            "required_check_runs": check_run_receipts(transition_checks),
        }

    receipt = {
        "checked_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "repository": args.repo,
        "ready": not failures,
        "irreversible_operation": "leave GitHub fork network",
        "live_default_branch": default_branch,
        "live_default_branch_commit": branch["commit"]["sha"],
        "repository_size_kb": repository["size"],
        "child_forks": repository["forks_count"],
        "fork_parent": repository.get("parent", {}).get("full_name"),
        "archived_release_tags": archived_tags,
        "archived_git_tags": archived_git_tags,
        "required_check": args.required_check,
        "required_check_runs": check_run_receipts(matching_checks),
        "transition": transition,
        "restore_drill": restore.get("status"),
        "failures": failures,
    }
    rendered = json.dumps(receipt, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0 if not failures else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, OSError, KeyError, json.JSONDecodeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)

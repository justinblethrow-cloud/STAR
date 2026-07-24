#!/usr/bin/env python3
"""Idempotently configure BlackSTAR after GitHub fork detachment."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Any
from urllib.parse import quote


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SETTINGS = REPO_ROOT / ".github" / "repository-settings.json"


def run(
    args: list[str],
    *,
    input_text: str | None = None,
    check: bool = True,
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        args,
        input=input_text,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if check and result.returncode != 0:
        raise RuntimeError(f"{' '.join(args)} failed:\n{result.stderr.strip()}")
    return result


def gh_api(
    endpoint: str,
    *,
    method: str = "GET",
    payload: Any | None = None,
    check: bool = True,
) -> Any:
    args = ["gh", "api", "-X", method, endpoint]
    input_text = None
    if payload is not None:
        args.extend(["--input", "-"])
        input_text = json.dumps(payload)
    result = run(args, input_text=input_text, check=check)
    if result.returncode != 0:
        return None
    if not result.stdout.strip():
        return {}
    return json.loads(result.stdout)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def download_release_assets(repo: str, tag: str, output: Path) -> bool:
    result = run(
        [
            "gh",
            "release",
            "download",
            tag,
            "--repo",
            repo,
            "--dir",
            str(output),
        ],
        check=False,
    )
    return result.returncode == 0


def restore_releases(repo: str, backup: Path) -> list[dict[str, Any]]:
    release_root = backup / "github" / "releases"
    results = []
    for directory in sorted(path for path in release_root.iterdir() if path.is_dir()):
        release = json.loads(
            (directory / "release.json").read_text(encoding="utf-8")
        )
        tag = release["tagName"]
        current = run(
            [
                "gh",
                "release",
                "view",
                tag,
                "--repo",
                repo,
                "--json",
                "body,isDraft,isPrerelease,name,tagName",
            ],
            check=False,
        )
        expected = {
            path.name: file_sha256(path)
            for path in (directory / "assets").iterdir()
            if path.is_file()
        }
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", delete=False
        ) as notes:
            notes.write(release["body"])
            notes_path = Path(notes.name)
        try:
            if current.returncode != 0:
                command = [
                    "gh",
                    "release",
                    "create",
                    tag,
                    "--repo",
                    repo,
                    "--verify-tag",
                    "--title",
                    release["name"],
                    "--notes-file",
                    str(notes_path),
                ]
                if release["isPrerelease"]:
                    command.append("--prerelease")
                if release["isDraft"]:
                    command.append("--draft")
                command.extend(
                    str(path) for path in sorted((directory / "assets").iterdir())
                )
                run(command)
            else:
                run(
                    [
                        "gh",
                        "release",
                        "edit",
                        tag,
                        "--repo",
                        repo,
                        "--title",
                        release["name"],
                        "--notes-file",
                        str(notes_path),
                        f"--draft={str(release['isDraft']).lower()}",
                        f"--prerelease={str(release['isPrerelease']).lower()}",
                    ]
                )
        finally:
            notes_path.unlink(missing_ok=True)

        with tempfile.TemporaryDirectory(prefix=f"blackstar-release-{tag}.") as temp:
            downloaded = Path(temp)
            download_release_assets(repo, tag, downloaded)
            actual = {
                path.name: file_sha256(path)
                for path in downloaded.iterdir()
                if path.is_file()
            }
            if actual != expected:
                assets = [
                    str(path)
                    for path in sorted((directory / "assets").iterdir())
                    if path.is_file()
                ]
                if assets:
                    run(
                        [
                            "gh",
                            "release",
                            "upload",
                            tag,
                            "--repo",
                            repo,
                            "--clobber",
                            *assets,
                        ]
                    )
                for path in downloaded.iterdir():
                    if path.is_file():
                        path.unlink()
                download_release_assets(repo, tag, downloaded)
                actual = {
                    path.name: file_sha256(path)
                    for path in downloaded.iterdir()
                    if path.is_file()
                }
                if actual != expected:
                    raise RuntimeError(f"release asset digest mismatch for {tag}")
        results.append({"tag": tag, "asset_count": len(expected), "status": "PASS"})
    return results


def configure_label(repo: str, name: str, color: str, description: str) -> None:
    endpoint = f"repos/{repo}/labels/{quote(name, safe='')}"
    existing = gh_api(endpoint, check=False)
    payload = {"name": name, "color": color, "description": description}
    if existing is None:
        gh_api(f"repos/{repo}/labels", method="POST", payload=payload)
    else:
        gh_api(endpoint, method="PATCH", payload=payload)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True)
    parser.add_argument("--backup", type=Path, required=True)
    parser.add_argument("--settings", type=Path, default=DEFAULT_SETTINGS)
    parser.add_argument("--readiness-receipt", type=Path)
    parser.add_argument("--readiness-max-age-hours", type=float, default=24.0)
    parser.add_argument("--expected-sha", required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument(
        "--confirmation",
        help="Required with --apply: DETACHMENT-APPROVED",
    )
    args = parser.parse_args()

    settings = json.loads(args.settings.read_text(encoding="utf-8"))
    if settings.get("schema_version") != 1:
        raise RuntimeError("unsupported repository-settings schema")
    repository_settings = settings["repository"]
    target_branch = repository_settings["default_branch"]

    repository = gh_api(f"repos/{args.repo}")
    if args.apply and repository["fork"]:
        raise RuntimeError("repository remains in a fork network")
    if args.apply and args.confirmation != "DETACHMENT-APPROVED":
        raise RuntimeError("explicit post-detachment confirmation is missing")
    readiness = None
    if args.readiness_receipt:
        readiness = json.loads(args.readiness_receipt.read_text(encoding="utf-8"))
        checked = dt.datetime.fromisoformat(readiness["checked_utc"])
        age = dt.datetime.now(dt.timezone.utc) - checked
        if age.total_seconds() < -300:
            raise RuntimeError("readiness receipt timestamp is in the future")
        if age > dt.timedelta(hours=args.readiness_max_age_hours):
            raise RuntimeError("readiness receipt is stale")
        if not readiness.get("ready"):
            raise RuntimeError("readiness receipt did not pass")
        if readiness.get("repository") != args.repo:
            raise RuntimeError("readiness receipt names a different repository")
        if readiness.get("live_default_branch_commit") != args.expected_sha:
            raise RuntimeError("readiness receipt names a different commit")
        if args.apply and readiness.get("transition") is None:
            raise RuntimeError("readiness receipt does not verify a transition branch")
    elif args.apply:
        raise RuntimeError("apply requires a fresh detachment-readiness receipt")

    branches = {
        value["name"]: value
        for value in gh_api(f"repos/{args.repo}/branches?per_page=100")
    }
    source_branch = "main" if "main" in branches else "master"
    if source_branch not in branches:
        raise RuntimeError("neither main nor master exists")
    if branches[source_branch]["commit"]["sha"] != args.expected_sha:
        raise RuntimeError("default source branch does not match expected SHA")

    planned = {
        "current_is_fork": repository["fork"],
        "apply_blocked_until_detached": repository["fork"],
        "readiness_receipt": (
            {
                "path": str(args.readiness_receipt.resolve()),
                "checked_utc": readiness["checked_utc"],
                "ready": readiness["ready"],
            }
            if readiness is not None
            else None
        ),
        "rename": source_branch != target_branch,
        "default_branch": target_branch,
        "repository": repository_settings,
        "branch_protection": settings["branch_protection"],
        "labels": settings["labels"],
        "pages": settings["pages"],
        "release_tags": sorted(
            path.name
            for path in (args.backup / "github" / "releases").iterdir()
            if path.is_dir()
        ),
    }
    if not args.apply:
        print(json.dumps({"mode": "dry-run", "planned": planned}, indent=2))
        return 0

    if source_branch != target_branch:
        gh_api(
            f"repos/{args.repo}/branches/{source_branch}/rename",
            method="POST",
            payload={"new_name": target_branch},
        )

    features = repository_settings["features"]
    gh_api(
        f"repos/{args.repo}",
        method="PATCH",
        payload={
            "default_branch": target_branch,
            "description": repository_settings["description"],
            "homepage": repository_settings["homepage"],
            "has_issues": features["issues"],
            "has_discussions": features["discussions"],
            "has_wiki": features["wiki"],
        },
    )
    gh_api(
        f"repos/{args.repo}/topics",
        method="PUT",
        payload={"names": repository_settings["topics"]},
    )
    if repository_settings["private_vulnerability_reporting"]:
        gh_api(
            f"repos/{args.repo}/private-vulnerability-reporting",
            method="PUT",
        )

    gh_api(
        f"repos/{args.repo}/branches/{target_branch}/protection",
        method="PUT",
        payload=settings["branch_protection"],
    )
    for name, label in settings["labels"].items():
        configure_label(args.repo, name, label["color"], label["description"])

    pages_settings = settings["pages"]
    pages_payload = {
        "source": {
            "branch": pages_settings["branch"],
            "path": pages_settings["path"],
        }
    }
    pages = gh_api(f"repos/{args.repo}/pages", check=False)
    if pages is None:
        gh_api(
            f"repos/{args.repo}/pages",
            method="POST",
            payload=pages_payload,
        )
    else:
        gh_api(
            f"repos/{args.repo}/pages",
            method="PUT",
            payload=pages_payload,
        )

    releases = restore_releases(args.repo, args.backup.resolve())
    final_repository = gh_api(f"repos/{args.repo}")
    final_main = gh_api(f"repos/{args.repo}/branches/{target_branch}")
    final_protection = gh_api(
        f"repos/{args.repo}/branches/{target_branch}/protection"
    )
    if final_repository["fork"]:
        raise RuntimeError("repository unexpectedly reports fork status")
    if final_repository["default_branch"] != target_branch:
        raise RuntimeError(f"default branch is not {target_branch}")
    if final_main["commit"]["sha"] != args.expected_sha:
        raise RuntimeError("main SHA changed during configuration")

    receipt = {
        "status": "PASS",
        "completed_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "repository": args.repo,
        "is_fork": final_repository["fork"],
        "default_branch": final_repository["default_branch"],
        "default_branch_commit": final_main["commit"]["sha"],
        "required_status_checks": final_protection["required_status_checks"],
        "releases": releases,
        "settings": planned,
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(
        json.dumps(receipt, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(receipt, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, OSError, KeyError, json.JSONDecodeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)

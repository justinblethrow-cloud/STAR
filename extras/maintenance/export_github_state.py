#!/usr/bin/env python3
"""Export the Git and GitHub state needed before an irreversible migration."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Any


SCRIPT_VERSION = "2"


def run(
    args: list[str],
    *,
    cwd: Path | None = None,
    check: bool = True,
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        args,
        cwd=cwd,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if check and result.returncode != 0:
        command = " ".join(args)
        raise RuntimeError(f"{command} failed:\n{result.stderr.strip()}")
    return result


def write_text(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8")


def write_json(path: Path, value: Any) -> None:
    write_text(path, json.dumps(value, indent=2, sort_keys=True) + "\n")


def gh_api(endpoint: str, *, paginate: bool = False) -> Any:
    args = ["gh", "api"]
    if paginate:
        args.append("--paginate")
    args.append(endpoint)
    result = run(args)
    if not paginate:
        return json.loads(result.stdout)

    decoder = json.JSONDecoder()
    offset = 0
    flattened: list[Any] = []
    while offset < len(result.stdout):
        while offset < len(result.stdout) and result.stdout[offset].isspace():
            offset += 1
        if offset == len(result.stdout):
            break
        page, offset = decoder.raw_decode(result.stdout, offset)
        if isinstance(page, list):
            flattened.extend(page)
        else:
            flattened.append(page)
    return flattened


def optional_gh_api(endpoint: str, *, paginate: bool = False) -> Any:
    try:
        return gh_api(endpoint, paginate=paginate)
    except (RuntimeError, json.JSONDecodeError) as error:
        return {"unavailable": True, "reason": str(error)}


def ref_inventory(git_dir: Path | None, repo_root: Path | None = None) -> str:
    args = ["git"]
    if git_dir is not None:
        args.extend([f"--git-dir={git_dir}"])
    args.extend(
        [
            "for-each-ref",
            "--format=%(refname)\t%(objectname)\t%(objecttype)",
        ]
    )
    return run(args, cwd=repo_root).stdout


def checksum(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_checksums(root: Path) -> None:
    checksum_path = root / "checksums.sha256"
    rows = []
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path == checksum_path:
            continue
        rows.append(f"{checksum(path)}  {path.relative_to(root)}")
    write_text(checksum_path, "\n".join(rows) + "\n")


def export_pull_request(repo: str, number: int, output: Path) -> None:
    pull_root = output / "github" / "pulls" / str(number)
    endpoints = {
        "pull.json": f"repos/{repo}/pulls/{number}",
        "issue-comments.json": f"repos/{repo}/issues/{number}/comments?per_page=100",
        "review-comments.json": f"repos/{repo}/pulls/{number}/comments?per_page=100",
        "reviews.json": f"repos/{repo}/pulls/{number}/reviews?per_page=100",
        "commits.json": f"repos/{repo}/pulls/{number}/commits?per_page=100",
        "files.json": f"repos/{repo}/pulls/{number}/files?per_page=100",
    }
    for filename, endpoint in endpoints.items():
        write_json(
            pull_root / filename,
            optional_gh_api(endpoint, paginate=endpoint.endswith("per_page=100")),
        )
    diff = run(
        ["gh", "pr", "diff", str(number), "--repo", repo],
        check=False,
    )
    write_text(pull_root / "diff.patch", diff.stdout)
    if diff.returncode != 0:
        write_text(pull_root / "diff.error.txt", diff.stderr)


def export_release(repo: str, tag: str, output: Path) -> None:
    release_root = output / "github" / "releases" / tag
    release_root.mkdir(parents=True, exist_ok=True)
    fields = (
        "name,tagName,isDraft,isPrerelease,publishedAt,targetCommitish,url,"
        "assets,body,author,createdAt"
    )
    result = run(
        [
            "gh",
            "release",
            "view",
            tag,
            "--repo",
            repo,
            "--json",
            fields,
        ]
    )
    write_json(release_root / "release.json", json.loads(result.stdout))
    assets = release_root / "assets"
    assets.mkdir(exist_ok=True)
    download = run(
        [
            "gh",
            "release",
            "download",
            tag,
            "--repo",
            repo,
            "--dir",
            str(assets),
        ],
        check=False,
    )
    if download.returncode != 0:
        write_text(release_root / "download.error.txt", download.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True, help="GitHub OWNER/REPO")
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument(
        "--remote",
        default="fork",
        help="Local remote that tracks the GitHub repository (default: fork)",
    )
    parser.add_argument(
        "--evidence-sha",
        action="append",
        default=[],
        help="Additional workflow head SHA to archive; may be repeated",
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    os.umask(0o077)
    repo_root = args.repo_root.resolve()
    output = args.output.resolve()
    if output.exists():
        raise SystemExit(f"output already exists: {output}")
    output.mkdir(parents=True)

    for executable in ("git", "gh"):
        if shutil.which(executable) is None:
            raise SystemExit(f"required executable not found: {executable}")

    status = run(["git", "status", "--porcelain"], cwd=repo_root).stdout
    if status:
        raise SystemExit("repository must be clean before export")

    repo_api = f"repos/{args.repo}"
    repository = gh_api(repo_api)
    default_branch = repository["default_branch"]
    live_branch = gh_api(f"{repo_api}/branches/{default_branch}")
    head_commit = live_branch["commit"]["sha"]
    local_remote_ref = f"{args.remote}/{default_branch}"
    local_remote_commit = run(
        ["git", "rev-parse", local_remote_ref],
        cwd=repo_root,
    ).stdout.strip()
    if local_remote_commit != head_commit:
        raise SystemExit(
            f"{local_remote_ref} is stale: {local_remote_commit} != {head_commit}"
        )

    local_head = run(["git", "rev-parse", "HEAD"], cwd=repo_root).stdout.strip()
    evidence_commits = sorted({head_commit, local_head, *args.evidence_sha})
    metadata = {
        "schema_version": SCRIPT_VERSION,
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "repository": args.repo,
        "repository_url": repository["html_url"],
        "default_branch": default_branch,
        "default_branch_commit": head_commit,
        "local_remote": args.remote,
        "local_remote_commit": local_remote_commit,
        "local_head": local_head,
        "workflow_evidence_commits": evidence_commits,
        "git_version": run(["git", "--version"]).stdout.strip(),
        "gh_version": run(["gh", "--version"]).stdout.splitlines()[0],
    }
    write_json(output / "snapshot.json", metadata)

    github_root = output / "github"
    endpoints = {
        "repository.json": repo_api,
        "pages.json": f"{repo_api}/pages",
        "rulesets.json": f"{repo_api}/rulesets",
        "actions-permissions.json": f"{repo_api}/actions/permissions",
        "environments.json": f"{repo_api}/environments",
        "community-profile.json": f"{repo_api}/community/profile",
        "private-vulnerability-reporting.json": (
            f"{repo_api}/private-vulnerability-reporting"
        ),
        "workflows.json": f"{repo_api}/actions/workflows?per_page=100",
        "workflow-runs.json": f"{repo_api}/actions/runs?per_page=100",
    }
    for filename, endpoint in endpoints.items():
        write_json(github_root / filename, optional_gh_api(endpoint))

    paginated = {
        "branches.json": f"{repo_api}/branches?per_page=100",
        "tags.json": f"{repo_api}/tags?per_page=100",
        "releases.json": f"{repo_api}/releases?per_page=100",
        "pulls.json": f"{repo_api}/pulls?state=all&per_page=100",
        "labels.json": f"{repo_api}/labels?per_page=100",
        "collaborators.json": f"{repo_api}/collaborators?per_page=100",
    }
    exported: dict[str, Any] = {}
    for filename, endpoint in paginated.items():
        value = optional_gh_api(endpoint, paginate=True)
        exported[filename] = value
        write_json(github_root / filename, value)

    branches = exported["branches.json"]
    if isinstance(branches, list):
        for branch in branches:
            if not branch.get("protected"):
                continue
            name = branch["name"]
            protection = optional_gh_api(f"{repo_api}/branches/{name}/protection")
            write_json(github_root / "branch-protection" / f"{name}.json", protection)

    pulls = exported["pulls.json"]
    if isinstance(pulls, list):
        for pull in pulls:
            export_pull_request(args.repo, int(pull["number"]), output)

    releases = exported["releases.json"]
    if isinstance(releases, list):
        for release in releases:
            export_release(args.repo, release["tag_name"], output)

    workflow_runs = optional_gh_api(f"{repo_api}/actions/runs?per_page=100")
    if isinstance(workflow_runs, dict):
        for workflow_run in workflow_runs.get("workflow_runs", []):
            if workflow_run.get("head_sha") not in evidence_commits:
                continue
            run_id = str(workflow_run["id"])
            run_root = github_root / "workflow-run-evidence" / run_id
            run_root.mkdir(parents=True, exist_ok=True)
            log_result = run(
                ["gh", "run", "view", run_id, "--repo", args.repo, "--log"],
                check=False,
            )
            write_text(run_root / "log.txt", log_result.stdout)
            if log_result.returncode != 0:
                write_text(run_root / "log.error.txt", log_result.stderr)
            artifact_result = run(
                [
                    "gh",
                    "run",
                    "download",
                    run_id,
                    "--repo",
                    args.repo,
                    "--dir",
                    str(run_root / "artifacts"),
                ],
                check=False,
            )
            if artifact_result.returncode != 0:
                write_text(
                    run_root / "artifact-download.error.txt",
                    artifact_result.stderr,
                )

    git_root = output / "git"
    git_root.mkdir()
    write_text(git_root / "local-refs.tsv", ref_inventory(None, repo_root))
    write_text(
        git_root / "local-remotes.txt",
        run(["git", "remote", "-v"], cwd=repo_root).stdout,
    )
    write_text(
        git_root / "local-fsck.txt",
        run(["git", "fsck", "--full"], cwd=repo_root).stdout,
    )
    run(
        ["git", "bundle", "create", str(git_root / "local-all.bundle"), "--all"],
        cwd=repo_root,
    )

    mirror = git_root / "remote-mirror.git"
    run(["git", "clone", "--mirror", repository["clone_url"], str(mirror)])
    write_text(git_root / "remote-refs.tsv", ref_inventory(mirror))
    write_text(
        git_root / "remote-fsck.txt",
        run(["git", f"--git-dir={mirror}", "fsck", "--full"]).stdout,
    )

    wiki = git_root / "wiki.git"
    wiki_result = run(
        [
            "git",
            "clone",
            "--mirror",
            repository["clone_url"].removesuffix(".git") + ".wiki.git",
            str(wiki),
        ],
        check=False,
    )
    if wiki_result.returncode != 0:
        write_text(git_root / "wiki-unavailable.txt", wiki_result.stderr)

    write_checksums(output)
    print(output)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except RuntimeError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)

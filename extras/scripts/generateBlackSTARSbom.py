#!/usr/bin/env python3
"""Generate a deterministic SPDX 2.3 SBOM for a BlackSTAR release binary."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import re


def macro(path: Path, name: str) -> str:
    pattern = re.compile(rf'^#define {re.escape(name)} "([^"]+)"$')
    for line in path.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match:
            return match.group(1)
    raise ValueError(f"missing {name} in {path}")


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--artifact-variant", required=True)
    parser.add_argument("--source-date-epoch", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    binary = args.binary.resolve()
    version_file = repo_root / "source" / "VERSION"
    blackstar_version = macro(version_file, "BLACKSTAR_VERSION")
    executable_version = macro(version_file, "STAR_VERSION")
    compatibility_version = macro(version_file, "STAR_COMPATIBILITY_VERSION")

    created = dt.datetime.fromtimestamp(
        args.source_date_epoch, tz=dt.timezone.utc
    ).strftime("%Y-%m-%dT%H:%M:%SZ")
    namespace = (
        "https://github.com/justinblethrow-cloud/blackSTAR/"
        f"sbom/{blackstar_version}/{args.commit}/{args.artifact_variant}"
    )
    document = {
        "SPDXID": "SPDXRef-DOCUMENT",
        "creationInfo": {
            "created": created,
            "creators": ["Tool: BlackSTAR-generateBlackSTARSbom.py-1"],
        },
        "dataLicense": "CC0-1.0",
        "documentNamespace": namespace,
        "name": f"BlackSTAR-{blackstar_version}-{args.artifact_variant}",
        "packages": [
            {
                "SPDXID": "SPDXRef-Package-BlackSTAR",
                "checksums": [
                    {
                        "algorithm": "SHA256",
                        "checksumValue": file_sha256(binary),
                    }
                ],
                "copyrightText": (
                    "Copyright (c) 2019 Alexander Dobin; "
                    "BlackSTAR modifications copyright their contributors"
                ),
                "downloadLocation": (
                    "https://github.com/justinblethrow-cloud/blackSTAR"
                ),
                "filesAnalyzed": False,
                "licenseConcluded": "MIT",
                "licenseDeclared": "MIT",
                "name": "BlackSTAR",
                "sourceInfo": (
                    f"Git commit {args.commit}; executable identity "
                    f"{executable_version}; artifact variant "
                    f"{args.artifact_variant}"
                ),
                "versionInfo": blackstar_version,
            },
            {
                "SPDXID": "SPDXRef-Package-STAR-Upstream",
                "copyrightText": "Copyright (c) 2019 Alexander Dobin",
                "downloadLocation": "https://github.com/alexdobin/STAR",
                "filesAnalyzed": False,
                "licenseConcluded": "MIT",
                "licenseDeclared": "MIT",
                "name": "STAR",
                "versionInfo": compatibility_version,
            },
            {
                "SPDXID": "SPDXRef-Package-HTSlib-Bundled",
                "copyrightText": "NOASSERTION",
                "downloadLocation": "NOASSERTION",
                "filesAnalyzed": False,
                "licenseConcluded": "NOASSERTION",
                "licenseDeclared": "NOASSERTION",
                "name": "HTSlib (bundled STAR snapshot)",
                "versionInfo": "0.0.1",
            },
            {
                "SPDXID": "SPDXRef-Package-Opal-Bundled",
                "copyrightText": "Copyright (c) 2014 Martin Sosic",
                "downloadLocation": "NOASSERTION",
                "filesAnalyzed": False,
                "licenseConcluded": "MIT",
                "licenseDeclared": "MIT",
                "name": "Opal (bundled)",
                "versionInfo": "NOASSERTION",
            },
        ],
        "relationships": [
            {
                "relatedSpdxElement": "SPDXRef-Package-BlackSTAR",
                "relationshipType": "DESCRIBES",
                "spdxElementId": "SPDXRef-DOCUMENT",
            },
            {
                "relatedSpdxElement": "SPDXRef-Package-STAR-Upstream",
                "relationshipType": "VARIANT_OF",
                "spdxElementId": "SPDXRef-Package-BlackSTAR",
            },
            {
                "relatedSpdxElement": "SPDXRef-Package-HTSlib-Bundled",
                "relationshipType": "CONTAINS",
                "spdxElementId": "SPDXRef-Package-BlackSTAR",
            },
            {
                "relatedSpdxElement": "SPDXRef-Package-Opal-Bundled",
                "relationshipType": "CONTAINS",
                "spdxElementId": "SPDXRef-Package-BlackSTAR",
            },
        ],
        "spdxVersion": "SPDX-2.3",
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

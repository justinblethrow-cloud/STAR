# BlackSTAR Repository Maintenance

`export_github_state.py` captures the Git and GitHub state required before an
irreversible repository migration. It exports repository settings, branch
protection, pull-request evidence, releases and assets, current workflow
evidence, Pages state, a remote mirror, and a bundle of all local refs.

The export intentionally does not retrieve Actions secrets, credentials, or
other secret values. Treat the resulting directory as private operational
evidence because it can still contain collaborator names and repository
metadata.

Run the exporter only from a clean checkout:

```bash
python3 extras/maintenance/export_github_state.py \
  --repo OWNER/REPOSITORY \
  --repo-root "$PWD" \
  --remote fork \
  --output /secure/path/transition-backup
```

Workflow evidence is captured for both the live default-branch commit and the
clean checkout's local `HEAD`. Use repeated `--evidence-sha` arguments only when
additional commits must be preserved.

Verify every checksum and exercise both Git restoration paths:

```bash
python3 extras/maintenance/verify_transition_backup.py \
  --backup /secure/path/transition-backup \
  --receipt /secure/path/restore-receipt.json
```

Fork-network detachment remains blocked until the verification receipt reports
`PASS`.

The intended post-detachment repository state is declared in
`.github/repository-settings.json`. Review that file together with the dry-run
output before approving the transaction.

Immediately before detachment, compare the live repository with the accepted
archive:

```bash
python3 extras/maintenance/check_detachment_readiness.py \
  --repo OWNER/REPOSITORY \
  --backup /secure/path/transition-backup \
  --restore-receipt /secure/path/restore-receipt.json \
  --expected-sha EXPECTED_COMMIT \
  --transition-branch SUCCESSOR_BRANCH \
  --transition-sha SUCCESSOR_COMMIT \
  --output /secure/path/detachment-readiness.json
```

The post-detachment configurator can be dry-run while the repository is still a
fork. It reports that apply is blocked until GitHub confirms detachment:

```bash
python3 extras/maintenance/finalize_successor_repo.py \
  --repo OWNER/REPOSITORY \
  --backup /secure/path/transition-backup \
  --readiness-receipt /secure/path/detachment-readiness.json \
  --expected-sha EXPECTED_COMMIT \
  --receipt /secure/path/post-detachment.json
```

Apply it only after reviewing the dry run:

```bash
python3 extras/maintenance/finalize_successor_repo.py \
  --repo OWNER/REPOSITORY \
  --backup /secure/path/transition-backup \
  --readiness-receipt /secure/path/detachment-readiness.json \
  --expected-sha EXPECTED_COMMIT \
  --receipt /secure/path/post-detachment.json \
  --apply --confirmation DETACHMENT-APPROVED
```

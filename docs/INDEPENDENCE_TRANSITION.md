# BlackSTAR Independence Transition

This runbook converts the GitHub fork into a standalone repository and changes
the protected default branch from `master` to `main`.

Fork-network detachment is permanent. It is blocked until the repository owner
approves the exact transaction after the recovery receipt passes.

## Preconditions

- Released `master` is clean and pinned by commit and tag.
- A remote mirror and `git bundle --all` exist.
- Release assets, PR evidence, Actions evidence, Pages state, repository
  settings, and branch protections are exported.
- Every exported file passes SHA-256 verification.
- Local restore drills reproduce both Git reference inventories exactly.
- The successor transition branch passes CI.
- The final recovery archive contains the transition branch and its passing
  check evidence.
- No repository transfer, branch deletion, or production integration is
  combined with the detachment transaction.

## Transaction

1. Freeze pushes and record UTC start time.
2. Requery repository size, child forks, default-branch SHA, releases, and
   required checks.
3. Obtain explicit owner approval.
4. Use GitHub's native **Leave fork network** operation.
5. Wait for GitHub to report a standalone repository.
6. Verify `isFork=false`, no parent, exact branch/tag SHAs, and release state.
7. Recreate lost releases and assets from the checksummed archive.
8. Restore Pages, Actions permissions, private vulnerability reporting, and
   repository metadata.
9. Rename `master` to `main`.
10. Reapply strict `main` protection and verify required checks.
11. Enable Issues and Discussions, install labels, and disable the wiki.
12. Push the transition branch, open its PR against `main`, and require CI.
13. Record UTC finish time and a machine-readable post-transition receipt.

## Verification

- Fresh clone selects `main`.
- `main` equals the expected pre-transition commit before successor changes.
- Historical tags resolve to the original commits.
- Historical release assets match archived SHA-256 values.
- GitHub Pages returns successfully.
- Issue and pull-request templates render.
- A test PR cannot merge without required checks.
- The release binary downloads and verifies.
- Old `master` URLs redirect to `main`.

## Recovery Boundary

Before detachment, aborting leaves the GitHub repository unchanged. After
detachment, Git code and assets can be restored from the archive, but the
repository cannot be rejoined automatically to the original fork network.

Branch renaming and repository settings are reversible. Fork detachment is not.
The archive and transaction receipts must therefore be retained permanently.

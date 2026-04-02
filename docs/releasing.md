# Releasing dingo

Use the `release` GitHub Actions workflow.

## Rules

- New release lines start from `master` and use `x.y.0`.
- Patch releases start from `releases/x.y` and use `x.y.z` where `z > 0`.
- Tags are `vX.Y.Z`.
- Run `dry_run=true` first.

Examples:

- `0.3.0` -> source `master` -> branch `releases/0.3`
- `0.3.1` -> source `releases/0.3`

## Inputs

- `version` (required): semantic version in `x.y.z` form
- `source_ref` (required): `master` for a new line, `releases/x.y` for a patch
- `release_line` (optional): usually leave empty; derived from `version`
- `dry_run` (optional): when `true`, validates and verifies without pushing refs
  or creating a GitHub Release

## What the workflow does

The workflow:

- validates `version`, `source_ref`, and `release_line`
- creates `releases/x.y` for a new release line if needed
- updates `CMakeLists.txt` to the release version
- creates the release commit if the version changed
- verifies Markdown formatting and the release build before publishing
- pushes the release branch and tag
- creates the GitHub Release

The workflow does not:

- backport fixes
- publish packages
- generate changelogs
- manage support policy

## Before you run it

Make sure:

1. the code to release is already merged
2. the target branch already contains the fixes you want
3. you know whether this is a new release line or a patch release
4. you are starting with `dry_run=true`

## New release line

Example: `0.3.0`

Use:

- `version=0.3.0`
- `source_ref=master`
- `dry_run=true`

Process:

1. Open GitHub Actions.
2. Run the `release` workflow with the inputs above.
3. If the dry run passes, run it again with `dry_run=false`.

Result:

- `releases/0.3` is created from `master` if it does not already exist
- `CMakeLists.txt` is updated on `releases/0.3`
- `releases/0.3` points to the release commit
- tag `v0.3.0` exists
- GitHub Release `v0.3.0` is created
- `master` is not modified by the workflow

## Patch release

Example: `0.3.1`

Use:

- `version=0.3.1`
- `source_ref=releases/0.3`
- `dry_run=true`

Process:

1. Make sure `releases/0.3` already contains the fixes you want.
2. Run the `release` workflow with the inputs above.
3. If the dry run passes, run it again with `dry_run=false`.

Result:

- `CMakeLists.txt` is updated on `releases/0.3`
- `releases/0.3` points to the release commit
- tag `v0.3.1` exists
- GitHub Release `v0.3.1` is created

## Preparing a patch branch

Backports are manual.

If a fix lands on `master` and should also ship in `0.3.x`:

1. switch to `releases/0.3`
2. cherry-pick or reapply the fix
3. push `releases/0.3`
4. run the release workflow for `0.3.1`

The workflow makes releases. You decide what belongs on each maintained release
branch.

## Dry runs and reruns

Use `dry_run=true` when:

- creating a new release line
- releasing from a branch you just updated
- checking that the version and branch match
- retrying after a partial failure

Reruns are safe if the existing remote branch and tag already point to the
expected release commit. The workflow retries transient failures when pushing
the release branch or tag, and when creating the GitHub Release. It does not
rewrite unexpected remote state.

## Quick reference

- New line: `version=0.3.0`, `source_ref=master`
- Patch: `version=0.3.1`, `source_ref=releases/0.3`
- Development branch: `master`
- Maintenance branch: `releases/x.y`
- Tag: `vX.Y.Z`

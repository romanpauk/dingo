# Releasing dingo

Use the `release` GitHub Actions workflow.

## Rules

- A new release line starts from `master` and uses `x.y.0`.
- A patch release starts from `releases/x.y` and uses `x.y.z` with `z > 0`.
- A tag is `vX.Y.Z`.
- Run with `dry_run=true` first.

Examples:

- `0.3.0` -> source `master` -> branch `releases/0.3`
- `0.3.1` -> source `releases/0.3`

## Inputs

- `version` (required): semantic version in `x.y.z` form.
- `source_ref` (required, default `master`): `master` for a new line,
  `releases/x.y` for a patch.
- `release_line` (optional): leave it empty. The workflow derives it from
  `version`.
- `dry_run` (default `true`): when `true`, the workflow validates and verifies,
  but it does not push refs or create a GitHub Release.

## What the workflow does

The workflow:

1. Validates `version`, `source_ref` and `release_line` with
   `tools/release_flow.py`, run through `uv` in the locked environment.
2. Switches to `releases/x.y`. For a new release line, it creates the branch
   from `master` if the branch does not exist.
3. Sets the release version in `CMakeLists.txt`.
4. Creates the release commit if the version changed.
5. Runs the `check` target (`check-base`) and the CI build matrix (`build-base`)
   on the release source.
6. Pushes the release branch and the tag.
7. Creates the GitHub Release.

A dry run stops after step 5.

The workflow does not:

- backport fixes
- publish packages
- generate changelogs
- manage support policy

## Before Running It

Make sure that:

1. the code to release is merged
2. the target branch contains the intended fixes
3. you know the release type: new release line or patch release
4. you start with `dry_run=true`

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

- `releases/0.3` exists, created from `master` if it was missing.
- `CMakeLists.txt` on `releases/0.3` contains the release version.
- `releases/0.3` points to the release commit.
- Tag `v0.3.0` exists.
- GitHub Release `v0.3.0` exists.
- The workflow does not change `master`.

## Patch release

Example: `0.3.1`

Use:

- `version=0.3.1`
- `source_ref=releases/0.3`
- `dry_run=true`

Process:

1. Make sure that `releases/0.3` contains the intended fixes.
2. Run the `release` workflow with the inputs above.
3. If the dry run passes, run it again with `dry_run=false`.

Result:

- `CMakeLists.txt` on `releases/0.3` contains the release version.
- `releases/0.3` points to the release commit.
- Tag `v0.3.1` exists.
- GitHub Release `v0.3.1` exists.

## Preparing a patch branch

Backports are manual. To ship a fix from `master` in `0.3.x`:

1. Switch to `releases/0.3`.
2. Cherry-pick or reapply the fix.
3. Push `releases/0.3`.
4. Run the release workflow for `0.3.1`.

The workflow makes releases. The contents of each maintained release branch are
chosen separately.

## Dry runs and reruns

Use `dry_run=true` when you:

- create a new release line
- release from a recently updated branch
- check that the version and the branch match
- retry after a partial failure

A rerun is safe if the remote branch and tag already point to the expected
release commit. The workflow retries transient failures of the branch push, the
tag push and the GitHub Release creation. If the remote branch or tag points to
a different commit, the workflow fails and does not rewrite it.

## Quick reference

- New line: `version=0.3.0`, `source_ref=master`
- Patch: `version=0.3.1`, `source_ref=releases/0.3`
- Development branch: `master`
- Maintenance branch: `releases/x.y`
- Tag: `vX.Y.Z`

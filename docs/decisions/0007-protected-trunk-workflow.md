# Protected trunk workflow

## Status

Accepted — 2026-08-22

## Context

The repository is public and currently maintained by one developer. `main` had no protection,
three merge methods were enabled, merged branches were retained, and a mixed-scope PR remained
open while later work was merged. A previous attempt to skip CI for documentation used workflow
`paths-ignore`; that conflicts with required status checks because GitHub leaves a required check
Pending when its whole workflow is skipped.

The workflow must keep `main` releasable without requiring a second person to approve the sole
maintainer's own changes. It must also avoid spending the full C++ build matrix on Markdown-only
updates.

## Decision

- Use GitHub Flow with `main` as the only long-lived integration branch.
- Require every change, including maintainer changes, to enter `main` through a Pull Request.
- Allow only squash merge. Require Conventional Commit PR titles and linear `main` history.
- Protect `main` from force pushes and deletion, include administrators, require resolved review
  conversations, and require the branch to be current with `main`.
- Require a single unique status context named `required`. It aggregates the conditional CI jobs
  with `if: always()` and fails when any applicable dependency fails or is cancelled.
- Always run lightweight change classification and the aggregate gate. Markdown, `docs/**`, and
  license-only changes skip the C++ matrix; any other path runs format, GCC/Clang tests,
  ASan/UBSan, and clang-tidy.
- Require zero approvals while there is only one maintainer. External contributions still receive
  maintainer review. Raise the protected-branch approval count when another maintainer is active.
- Automatically delete merged head branches. Close superseded PRs with a replacement link.
- Create tags and Releases for deliberate releases, not for every merged PR.

The exact contributor-facing lifecycle and merge rules live in `CONTRIBUTING.md`. Repository
settings are part of this decision and must be checked after changing CI job names.

## Alternatives considered

### Long-lived `develop` branch

Rejected. It would add a second integration point without providing value for one maintainer and
would allow `main` and the actual development state to drift.

### Require one approving review

Rejected for now. A Pull Request author cannot approve their own change, so this would make normal
solo maintenance depend on a bypass. It becomes the preferred rule when a second maintainer is
available.

### Skip the whole workflow for documentation paths

Rejected. A workflow skipped by a path filter does not report a required check, which can block a
protected branch indefinitely. Conditional jobs report a skipped result, while the aggregate
`required` job still reports a definitive result.

### Keep merge commit, rebase merge, and squash merge enabled

Rejected. Multiple merge modes make history and release notes inconsistent. Squash merge maps one
reviewed PR to one Conventional Commit while retaining the detailed PR audit trail.

## Consequences

- Direct pushes to `main` stop working, including for the repository administrator.
- A code PR must be current with `main` and may need its CI rerun after another PR merges.
- Documentation PRs still create two lightweight jobs so branch protection receives a result, but
  they do not compile or test C++.
- Topic branches are disposable after merge; permanent history lives in `main`, PRs, tags, and
  Releases.
- Changing the `required` job name requires updating branch protection in the same maintenance
  window.

## References

- [About protected branches](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches)
- [Troubleshooting required status checks](https://docs.github.com/en/pull-requests/how-tos/merge-and-close-pull-requests/troubleshooting-required-status-checks)
- [Configuring commit squashing](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/configuring-pull-request-merges/configuring-commit-squashing-for-pull-requests)
- [Managing automatic branch deletion](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/configuring-pull-request-merges/managing-the-automatic-deletion-of-branches)

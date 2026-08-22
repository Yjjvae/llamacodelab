# Repository governance adoption checklist

## Prepare

- [ ] Confirm the default branch and active maintainer count.
- [ ] Inventory open PRs and branches; classify each as merge, split, Draft, supersede, or close.
- [ ] Record current merge methods, branch deletion setting, and branch protection/rulesets.
- [ ] Identify the real code quality command and documentation-only paths.
- [ ] Choose the solo or team protection profile.

## Implement through a PR

- [ ] Copy the contribution guide, PR template, CI template, and ADR.
- [ ] Replace every template token; the placeholder `rg` command returns no matches in active files.
- [ ] Keep the workflow permissions read-only unless a documented job needs more.
- [ ] Ensure each job has a timeout and stale workflow runs use concurrency cancellation.
- [ ] Ensure every conditional job is included in `required.needs` and result evaluation.
- [ ] Run the project's local quality gate.
- [ ] Open the governance PR and confirm `required` succeeds before selecting it in protection.

## Configure GitHub

- [ ] Require Pull Requests for the default branch.
- [ ] Require the unique `required` context and a current branch tip.
- [ ] Require resolved conversations and linear history.
- [ ] Include administrators; forbid force pushes and deletion.
- [ ] Set approvals to zero only for a sole maintainer; otherwise require at least one.
- [ ] Enable squash merge only.
- [ ] Enable automatic deletion of merged head branches.
- [ ] Keep automatic merging disabled unless separately approved.

## Verify behavior

- [ ] Merge the governance PR through the configured protection rule.
- [ ] A documentation-only PR runs `changes` and `required` but skips heavy jobs.
- [ ] A code/configuration PR runs the full quality path and `required` aggregates it.
- [ ] A failing dependency makes `required` fail.
- [ ] Direct push, force push, and default-branch deletion are blocked.
- [ ] A merged branch is deleted automatically.
- [ ] Open PRs have one concern; conflicting or superseded PRs have an explicit resolution.

## Maintain

- [ ] Audit branch protection after renaming CI jobs.
- [ ] Review inactive Draft PRs and obsolete branches on the documented schedule.
- [ ] Raise the approval requirement when a second maintainer becomes active.
- [ ] Create tags and Releases only for planned delivery events.
- [ ] Record governance changes in an ADR and migration note.

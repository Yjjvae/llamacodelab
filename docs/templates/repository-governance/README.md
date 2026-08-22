# Reusable repository governance template

Template format: 1

This directory packages a protected-trunk workflow for public repositories maintained by one
person or a small team. It separates reusable Git/GitHub policy from language-specific build and
test commands.

Do not copy the current LlamaCodeLab workflow blindly. Start from these templates, replace every
placeholder, and make the target repository's own CI the source of truth.

## Included files

| File | Copy to | Purpose |
|---|---|---|
| `CONTRIBUTING.md.template` | `CONTRIBUTING.md` | Branch, commit, PR, merge, stale-work, and release rules |
| `.github/pull_request_template.md` | `.github/pull_request_template.md` | Evidence-oriented PR description |
| `.github/workflows/ci.yml.template` | `.github/workflows/ci.yml` | Always-emitted `required` gate with conditional heavy CI |
| `branch-protection.solo.json` | Temporary local input | Protected `main` with zero approvals |
| `branch-protection.team.json` | Temporary local input | Protected `main` with one independent approval |
| `GOVERNANCE_ADR.md.template` | Next project ADR path | Records why the repository chose this workflow |
| `ADOPTION_CHECKLIST.md` | Keep or copy into an issue | Safe rollout and verification checklist |

## Required substitutions

Search for placeholders before opening the adoption PR:

```bash
rg -n '\{\{[A-Z0-9_]+\}\}' \
  CONTRIBUTING.md \
  .github/pull_request_template.md \
  .github/workflows/ci.yml \
  docs/decisions
```

| Placeholder | Example |
|---|---|
| `{{PROJECT_NAME}}` | `LlamaCodeLab` |
| `{{DEFAULT_BRANCH}}` | `main` |
| `{{QUALITY_COMMAND}}` | `./scripts/quality.sh --preset dev` |
| `{{DOCS_COMMAND}}` | `git diff --check` |
| `{{WIP_LIMIT}}` | `2` |
| `{{DRAFT_REVIEW_DAYS}}` | `14` |
| `{{DRAFT_CLOSE_DAYS}}` | `30` |
| `{{DATE}}` | ADR acceptance date in `YYYY-MM-DD` form |
| `{{MAINTAINER_COUNT}}` | `1` or the current active count |
| `{{REQUIRED_APPROVALS}}` | `0` for solo, normally `1` for a team |
| `{{RUNNER}}` | `ubuntu-24.04` |
| `{{CHECKOUT_REF}}` | Reviewed checkout tag or full commit SHA |
| `{{CHECKOUT_SUBMODULES}}` | `false` or `recursive` |
| `{{OWNER}}` | GitHub owner or organization |
| `{{REPOSITORY}}` | Repository name |

The workflow's documentation-only patterns default to Markdown, `docs/**`, and license files.
Review them for every project. Workflow files, dependency manifests, scripts, build configuration,
tests, and source files must take the full quality path.

## Adoption order

1. Start a focused governance branch from current `main`.
2. Copy the templates and replace all placeholders.
3. Replace the single generic `quality` job with the target project's real job or matrix when
   necessary; add every conditional job to the final `required.needs` list and result check.
4. Open a PR before enabling a required status context. Confirm that `required` has completed
   successfully in the repository.
5. Apply either the solo or team branch-protection profile.
6. Keep only squash merging and enable automatic deletion of merged head branches.
7. Merge the governance PR through the new rule.
8. Open one documentation-only test PR and one code test PR. Confirm that documentation skips the
   heavy jobs while code runs them.
9. Record the final settings in an ADR and remove obsolete remote topic branches only after their
   PR state and recoverability are confirmed.

The ordering matters: GitHub requires a selected status check to have run recently, and a workflow
skipped by a path filter may never report a required context. This template always runs `changes`
and `required`, then skips only individual heavy jobs.

## Apply GitHub settings

After replacing `{{OWNER}}` and `{{REPOSITORY}}`, choose exactly one protection profile:

```bash
gh api \
  --method PUT \
  repos/{{OWNER}}/{{REPOSITORY}}/branches/{{DEFAULT_BRANCH}}/protection \
  --input branch-protection.solo.json
```

For a team, use `branch-protection.team.json` instead. Then configure repository merge methods:

```bash
gh api \
  --method PATCH \
  repos/{{OWNER}}/{{REPOSITORY}} \
  -F allow_squash_merge=true \
  -F allow_merge_commit=false \
  -F allow_rebase_merge=false \
  -F delete_branch_on_merge=true \
  -f squash_merge_commit_title=PR_TITLE \
  -f squash_merge_commit_message=PR_BODY
```

Do not enable automatic merging as part of the template. It changes when external state is merged
and should be a separate, explicit repository decision.

## Profiles

Use the solo profile only while there is one active maintainer. It requires PRs and applies all
other protection to administrators, but sets approvals to zero because authors cannot approve
their own PRs.

Switch to the team profile when at least two maintainers can review each other. It requires one
approval, dismisses stale approvals after new commits, and requires approval of the latest push.
Add CODEOWNERS review only after ownership boundaries and reviewer availability are real.

The supplied JSON uses classic branch protection because it also works for a public repository
owned by one person. An organization may choose a repository ruleset instead; preserve the same PR,
status, history, force-push, deletion, and bypass semantics, and record any bypass actors in its ADR.

## Maintenance

- Keep `required` unique across workflows and stable once branch protection refers to it.
- Review repository settings whenever CI job names change.
- Keep project-specific commands out of this template unless they illustrate a reusable interface.
- Update the template format only for a behavioral or compatibility change; record migration notes.
- Test the template in a disposable repository before using it for an established project with
  required checks or external contributors.

## References

- [About protected branches](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches)
- [Troubleshooting required status checks](https://docs.github.com/en/pull-requests/how-tos/merge-and-close-pull-requests/troubleshooting-required-status-checks)
- [Configuring commit squashing](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/configuring-pull-request-merges/configuring-commit-squashing-for-pull-requests)
- [Managing automatic branch deletion](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/configuring-pull-request-merges/managing-the-automatic-deletion-of-branches)

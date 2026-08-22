# Contributing to LlamaCodeLab

## Before you start

- Create work from an up-to-date `main` branch; never develop or push directly on `main`.
- Keep one focused concern in each branch and Pull Request.
- Do not commit GGUF models, build output, indexes, credentials, or generated dependency sources.
- Initialize the llama.cpp submodule before building:

  ```bash
  git submodule update --init --recursive
  ```

## Branch names

Use a lowercase category and a concise kebab-case description:

```text
feat/m5-vector-search
fix/scan-symlink-boundary
docs/update-cuda-benchmark
perf/reduce-chunk-allocation
```

Use `feat`, `fix`, `docs`, `test`, `refactor`, `perf`, `build`, `ci`, or `chore` as the normal
prefix. Reserve `experiment` and `spike` for work that is not yet supported or release-ready.

## Branch lifecycle

This repository uses a protected-trunk GitHub Flow. `main` is the only long-lived integration
branch and must remain releasable. There is no permanent `develop` branch.

Multiple branches are useful when they represent independent work; they are not a reason to keep
finished work unmerged. Use these lifecycles:

| Branch kind | Lifetime | Integration rule |
|---|---|---|
| `feat/*`, `fix/*`, `perf/*`, `refactor/*` | One coherent capability or correction | Pull Request into `main`; delete after squash merge |
| `docs/*`, `test/*`, `ci/*`, `build/*`, `chore/*` | One focused maintenance change | Pull Request into `main`; normally no release by itself |
| `experiment/*`, `spike/*` | Time-boxed exploration | Draft PR or local branch; convert supported work into a normal branch before merging |
| `release/*` | Short release preparation only | Pull Request after release verification; tag the resulting `main` commit |

As the sole maintainer, keep at most two maintainer-owned implementation topics actively in
progress. A Ready PR that is green and conflict-free should be merged promptly; a blocked or
incomplete change stays Draft. Close a superseded PR immediately with a link to its replacement.
Review Draft PRs after 14 days of inactivity and close them after 30 days unless there is a
recorded reason to keep them.

Start every branch from current `main`:

```bash
git switch main
git pull --ff-only
git switch -c feat/concise-topic
```

Bring a stale topic branch up to date before marking its PR Ready. Rebase only commits that belong
to you and have not become a shared base; if a rewritten topic branch must be pushed, use
`--force-with-lease`, never plain `--force`. Force pushes to `main` are forbidden.

For dependent work, prefer separate sequential PRs. If a stacked PR is necessary, target its
immediate parent branch, state the dependency in both PRs, merge bottom-up, then retarget and update
the remaining PR. Do not combine completed milestones merely to reduce the number of merges.

## Commit messages

Use the Conventional Commits form:

```text
<type>(<optional-scope>): <imperative summary>
```

The summary is written in English, starts with an imperative verb, uses lowercase, has no trailing
period, and stays short enough to scan in `git log` (aim for 72 characters or fewer).

Common types:

| Type | Use for |
|---|---|
| `feat` | A user-visible capability |
| `fix` | A correctness or safety bug fix |
| `docs` | Documentation only |
| `test` | Adding or correcting tests only |
| `refactor` | Internal restructuring with unchanged behavior |
| `perf` | A measurable performance improvement |
| `build` | CMake, dependencies, toolchain, or packaging |
| `ci` | Continuous-integration configuration |
| `chore` | Repository maintenance |

Use a scope when it identifies the affected module. Preferred scopes are `domain`, `support`,
`filesystem`, `llama`, `application`, `cli`, `config`, `tests`, `docs`, and `cmake`.

Examples:

```text
feat(filesystem): add stable line-based code chunks
fix(llama): stop decoding after cancellation request
test(filesystem): cover symlink escape rejection
docs(contributing): document commit conventions
build(cmake): enable cuda release preset
```

For incompatible changes, append `!` after the type or scope and explain the migration in the
commit body:

```text
feat(domain)!: replace chunk offsets with source ranges

BREAKING CHANGE: callers must construct SourceRange with 1-based inclusive lines.
```

## Build and test

For code, build, test, dependency, script, CMake, or workflow changes, run the local quality gate
before marking a PR Ready. It checks formatting, configures and builds the selected preset, runs
its tests, then runs `clang-tidy` for project `.cpp` files:

```bash
./scripts/quality.sh --preset dev
```

`src/adapters/clang/` is intentionally excluded from the default gate because it is compiled only
with `LLCL_ENABLE_CLANG=ON`. When changing that adapter, configure a Clang-enabled build and run
its parser tests in addition to the default gate:

```bash
ctest --test-dir build/clang -R ClangCodeParser --output-on-failure
```

Use `./scripts/format.sh` to apply formatting, or `./scripts/format.sh --check` in a non-mutating
check. After changing compiler flags or a preset, run `cmake --preset dev` before
`./scripts/tidy.sh dev` so that its compilation database is fresh.

For changes that affect CUDA, llama.cpp integration, or the CLI executable, also build the CUDA
Release preset. Run model-labelled tests only after setting `LLCL_TEST_MODEL` to a local GGUF.

```bash
cmake --build --preset release-cuda
LLCL_TEST_MODEL="$PWD/models/qwen2.5-coder-1.5b-instruct-q4_k_m.gguf" \
  LLCL_TEST_GPU_LAYERS=-1 \
  ctest --test-dir build/release-cuda -L model --output-on-failure
```

For a documentation-only change, run `git diff --check` and verify every new local link. GitHub
still emits the lightweight `changes` and `required` checks so protected-branch requirements are
satisfied, but it skips the C++ format, compiler, sanitizer, and clang-tidy matrix.

## Pull Requests

Open an incomplete change as Draft. Mark it Ready only when its scope is complete, its branch is
current enough to merge cleanly, and local verification has passed. Every PR should include:

1. A Conventional Commit-style title.
2. A concise description of behavior and design choices.
3. Test commands and their results.
4. Documentation and worklog updates when user-facing behavior or a milestone changes.
5. Any model, GPU, benchmark, or compatibility assumptions needed to reproduce the result.
6. An explicit release impact: `none`, `patch`, `minor`, or `major`.

Do not use a PR as a container for unrelated completed work. Follow-up fixes discovered during
review may remain in the same PR only when they are necessary to make its stated change correct.
Otherwise, open a linked issue or a separate PR.

All review conversations must be resolved and the `required` status check must pass before merge.
External contributions need maintainer review. This single-maintainer repository does not require
an approval on the maintainer's own PR, because GitHub does not allow authors to approve their own
changes; the PR, CI evidence, and public review surface remain mandatory.

## Merge policy

- Squash merge is the only enabled merge method. The PR title becomes the Conventional Commit on
  `main`.
- Merge when a focused PR is complete and green; do not batch unrelated PRs for an arbitrary date.
- Never merge a Draft, a conflicted PR, a failing PR, or a PR with unresolved conversations.
- Delete the topic branch after merge. The PR and squash commit retain the audit trail.
- Revert a faulty merged change with a new PR; do not rewrite published `main` history.

Frequent small merges are healthy when every PR is independently reviewable and keeps `main`
releasable. The failure mode to avoid is frequent incomplete merging or long-lived mixed-scope PRs,
not merge frequency itself.

## Protected `main`

Repository settings enforce the following rule for `main`:

- changes must arrive through a Pull Request, including maintainer changes;
- the branch must be up to date and the unique `required` CI check must pass;
- all review conversations must be resolved;
- linear history is required;
- force pushes and branch deletion are disabled;
- administrators are included in the rule;
- zero approving reviews are required while the repository has one maintainer.

If additional maintainers become active, raise the approval requirement to one and enable stale
review dismissal. Do not add a required check whose entire workflow can be skipped by a path
filter: GitHub leaves such checks Pending. The always-emitted `required` job aggregates conditional
jobs and is the only branch-protection status context.

## Releases

Merging a PR does not automatically create a tag or GitHub Release. Release only when a
user-visible capability, a compatible set of fixes, a completed milestone, or a planned
maintenance batch is ready. Documentation-only and CI-only PRs normally have release impact
`none`.

Create annotated tags from the verified `main` commit and publish release notes describing behavior
changes, verification, compatibility, and upgrade considerations. Model weights are never attached.

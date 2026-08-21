# Contributing to LlamaCodeLab

## Before you start

- Create work from an up-to-date `main` branch.
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

## Branch lifecycle and integration

`main` is the protected integration branch: it must remain buildable, tested, and suitable for a
release at all times. Do not develop directly on it.

Keep multiple branches when they represent independent work. A branch should cover one coherent
feature, bug fix, experiment, or maintenance topic; it can contain several commits and stay open
while other branches merge. Rebase or merge `main` into a long-lived branch before its PR is ready
when needed to resolve drift.

Use these lifecycles:

| Branch kind | Typical lifetime | Integration rule |
|---|---|---|
| `feat/*`, `fix/*`, `perf/*`, `refactor/*` | Until one coherent capability is reviewed | PR into `main`; delete after merge unless intentionally long-lived |
| `docs/*`, `test/*`, `ci/*`, `build/*`, `chore/*` | Small, focused maintenance work | PR into `main`; normally no release by itself |
| `experiment/*`, `spike/*` | Exploratory or comparative work | Keep while useful; merge only after converting it into a supported change |
| `release/*` | Release preparation | Short-lived; merge only after release checks and notes are ready |

Merge frequency follows integration readiness, not commit frequency. It is normal to keep several
open feature branches and merge only work that is independently reviewable and green in CI. Delete
merged short-lived remote branches to keep the repository navigable; GitHub retains the PR, commits,
and merge history. Preserve explicitly named long-lived experiment or maintenance branches.

## Releases

Merging a PR does not automatically create a version, tag, or GitHub Release. Release only when a
user-visible capability, a compatible group of fixes, a milestone, or a planned maintenance batch
is ready. Documentation-only and CI-only PRs normally merge without a release. Create the tag from
the final `main` commit, then publish notes describing behavior changes, verification, and upgrade
considerations.

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

Run the local quality gate before opening a PR. It checks formatting, configures and builds the
selected preset, runs its tests, then runs `clang-tidy` for project `.cpp` files:

```bash
./scripts/quality.sh --preset dev
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

## Pull Requests

Every PR should include:

1. A Conventional Commit-style title.
2. A concise description of behavior and design choices.
3. Test commands and their results.
4. Documentation and worklog updates when user-facing behavior or a milestone changes.
5. Any model, GPU, benchmark, or compatibility assumptions needed to reproduce the result.

Prefer squash merges into `main`. A release is tagged on `main` only when it meets the release
criteria above; it may receive a GitHub Release containing source archives and release notes. Model
weights are never attached.

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
feature/m5-vector-search
fix/scan-symlink-boundary
docs/update-cuda-benchmark
perf/reduce-chunk-allocation
```

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

Prefer squash merges into `main`. A completed milestone is tagged on `main` and may receive a
GitHub Release containing source archives and release notes; model weights are never attached.

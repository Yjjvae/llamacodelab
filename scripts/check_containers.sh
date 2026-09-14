#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

docker compose --profile cpu config --quiet
docker compose --profile cuda config --quiet
docker build --check --file docker/Dockerfile.cpu .
docker build --check --file docker/Dockerfile.cuda .

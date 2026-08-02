#!/usr/bin/env bash
set -euo pipefail

command -v clang-format >/dev/null 2>&1 || {
  echo "clang-format is required" >&2
  exit 1
}

mapfile -d '' files < <(
  find apps include src tests -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0
)

if ((${#files[@]} > 0)); then
  clang-format -i "${files[@]}"
fi

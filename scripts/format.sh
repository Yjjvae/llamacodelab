#!/usr/bin/env bash
set -euo pipefail

command -v clang-format >/dev/null 2>&1 || {
  echo "clang-format is required" >&2
  exit 1
}

check_only=false
if (($# == 1)) && [[ "$1" == "--check" ]]; then
  check_only=true
elif (($# != 0)); then
  echo "usage: $0 [--check]" >&2
  exit 2
fi

mapfile -d '' files < <(
  find apps include src tests -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0
)

if ((${#files[@]} > 0)); then
  if [[ "$check_only" == true ]]; then
    clang-format --dry-run --Werror "${files[@]}"
  else
    clang-format -i "${files[@]}"
  fi
fi

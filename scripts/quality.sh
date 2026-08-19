#!/usr/bin/env bash
set -euo pipefail

preset="dev"
while (($# > 0)); do
  case "$1" in
    --preset)
      [[ $# -ge 2 ]] || { echo "--preset requires a value" >&2; exit 2; }
      preset="$2"
      shift 2
      ;;
    *)
      echo "usage: $0 [--preset configure-preset]" >&2
      exit 2
      ;;
  esac
done

"$(dirname "$0")/format.sh" --check
cmake --preset "$preset"
cmake --build --preset "$preset"
ctest --preset "$preset"
"$(dirname "$0")/tidy.sh" "$preset"

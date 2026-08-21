#!/usr/bin/env bash
set -euo pipefail

preset="${1:-dev}"
if (($# > 1)); then
  echo "usage: $0 [configure-preset]" >&2
  exit 2
fi

command -v clang-tidy >/dev/null 2>&1 || {
  echo "clang-tidy is required" >&2
  exit 1
}
command -v g++ >/dev/null 2>&1 || {
  echo "g++ is required so clang-tidy can locate the C++ standard library" >&2
  exit 1
}

build_dir="build/${preset}"
compile_commands="${build_dir}/compile_commands.json"
if [[ ! -f "$compile_commands" ]]; then
  echo "missing ${compile_commands}; run: cmake --preset ${preset}" >&2
  exit 1
fi

mapfile -d '' files < <(
  find apps src \
    -path 'src/adapters/clang' -prune -o \
    -type f -name '*.cpp' -print0
)

gcc_version="$(g++ -dumpversion)"
gcc_target="$(g++ -dumpmachine)"
system_include_args=()
for directory in "/usr/include/c++/${gcc_version}" \
                 "/usr/include/${gcc_target}/c++/${gcc_version}"; do
  if [[ -d "$directory" ]]; then
    system_include_args+=("--extra-arg=-isystem${directory}")
  fi
done

if ((${#files[@]} > 0)); then
  printf '%s\0' "${files[@]}" | xargs --null --no-run-if-empty --max-args=1 --max-procs=4 \
    clang-tidy --quiet -p "$build_dir" "${system_include_args[@]}"
fi

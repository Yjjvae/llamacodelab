#!/usr/bin/env bash
set -euo pipefail

build_dir="build/bench"
output_dir="benchmarks/results/$(date -u +%Y%m%dT%H%M%SZ)"
seed="${LLCL_BENCH_SEED:-42}"
records="${LLCL_BENCH_RECORDS:-10000}"
queries="${LLCL_BENCH_QUERIES:-200}"
runs="${LLCL_BENCH_RUNS:-10}"
gpu_info_tool=""
if command -v nvidia-smi >/dev/null; then
  gpu_info_tool="$(command -v nvidia-smi)"
elif [[ -x /usr/lib/wsl/lib/nvidia-smi ]]; then
  gpu_info_tool="/usr/lib/wsl/lib/nvidia-smi"
fi

cmake -S . -B "$build_dir" -G Ninja -DLLCL_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --target llcl_chunker_benchmark llcl_retrieval_benchmark llcl_inference_benchmark
mkdir -p "$output_dir"

{
  echo "commit_sha=$(git rev-parse HEAD)"
  echo "llama_cpp_commit=$(git -C third_party/llama.cpp rev-parse HEAD)"
  echo "kernel=$(uname -srmo)"
  echo "compiler=$(c++ --version | head -n 1)"
  echo "cmake=$(cmake --version | head -n 1)"
  command -v nvcc >/dev/null && nvcc --version | tail -n 1 || true
  [[ -n "$gpu_info_tool" ]] && "$gpu_info_tool" --query-gpu=name,driver_version,memory.total --format=csv,noheader || true
} > "$output_dir/environment.txt"

"$build_dir/benchmarks/llcl_chunker_benchmark" > "$output_dir/chunker.json"
"$build_dir/benchmarks/llcl_retrieval_benchmark" "$records" "$queries" "$seed" > "$output_dir/retrieval.jsonl"

if [[ -n "${LLCL_BENCH_MODEL:-}" ]]; then
  "$build_dir/benchmarks/llcl_inference_benchmark" \
    --model "$LLCL_BENCH_MODEL" --runs "$runs" --seed "$seed" \
    --gpu-layers "${LLCL_BENCH_GPU_LAYERS:--1}" > "$output_dir/inference.json"
else
  echo "LLCL_BENCH_MODEL is unset; inference benchmark was intentionally skipped." >&2
fi

echo "Benchmark artifacts written to $output_dir"

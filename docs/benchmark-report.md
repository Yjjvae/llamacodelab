# Benchmark Report

This template is deliberately empty: never substitute an unmeasured value with a claim. Generate
raw artifacts with `scripts/run_benchmarks.sh`, preserve them under `benchmarks/results/`, then
copy their values and environment file into a dated report.

## Environment

| Item | Value |
|---|---|
| Project commit | |
| llama.cpp commit | |
| OS / kernel | |
| Compiler / build type | |
| GPU | |
| Driver / CUDA | |
| Model / SHA-256 / quantization | |
| Embedding model | |
| Context / batch / GPU layers / flash attention | |
| Seed / warmups / measured runs | |

## Inference

Use the identical prompt, output-token limit, seed and run count for each row. Report p50 and p95,
not the best run. Record peak VRAM separately with `nvidia-smi` or an equivalent supported tool.

| Config | TTFT p50/p95 ms | Decode p50/p95 tok/s | E2E p50/p95 ms | Peak RSS MiB | Peak VRAM MiB |
|---|---:|---:|---:|---:|---:|
| CPU baseline | | | | | |
| CUDA all layers | | | | | |
| CUDA KV variant | | | | | |

## Retrieval

| Index | Dataset / seed | Recall@10 | MRR@10 | nDCG@10 | p50 ms | p95 ms | Size |
|---|---|---:|---:|---:|---:|---:|---:|
| Brute | | | | | | | |
| HNSW | | | | | | | |
| Hybrid | | | | | | | |

## Conclusion

State only conclusions supported by the rows above. Diagnose TTFT and decode throughput separately:
prefill is primarily affected by prompt length and batch behaviour, while decode is sensitive to
per-token scheduling, KV cache and memory bandwidth.

#!/usr/bin/env bash
set -euo pipefail

if ! grep -qi microsoft /proc/sys/kernel/osrelease; then
  echo "error: this installer is intended for WSL2" >&2
  exit 2
fi

keyring_url='https://developer.download.nvidia.com/compute/cuda/repos/wsl-ubuntu/x86_64/cuda-keyring_1.1-1_all.deb'
keyring_deb="$(mktemp --suffix=.deb)"

cleanup() {
  rm -f "${keyring_deb}"
}
trap cleanup EXIT

curl --fail --location --retry 3 --output "${keyring_deb}" "${keyring_url}"
sudo dpkg -i "${keyring_deb}"
sudo apt-get update

# These are the compiler/runtime/development components required by llama.cpp.
# Deliberately do not install the `cuda`, `cuda-13-3`, or `cuda-drivers`
# packages: the Windows NVIDIA driver is exposed to WSL by the host.
sudo apt-get install -y \
  cuda-nvcc-13-3 \
  cuda-cudart-dev-13-3 \
  libcublas-dev-13-3 \
  ninja-build \
  ccache \
  clang-format \
  clang-tidy

/usr/local/cuda-13.3/bin/nvcc --version
echo "CUDA 13.3 build tools are ready; no Linux NVIDIA driver was installed."

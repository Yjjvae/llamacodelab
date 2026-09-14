# Changelog

All notable user-facing changes are recorded here from `v0.12.0` onward. Earlier delivery evidence remains available in
the [Worklog](WORKLOG.md) and GitHub Releases.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.12.0] - 2026-09-14

### Added

- Multi-stage CPU and CUDA runtime images that build `llcl-cli` and `llcl-server` from pinned base images.
- Docker Compose `cpu` and `cuda` profiles with health checks, loopback-only port publishing, read-only model/source
  mounts, and a persistent index volume.
- Container configuration examples, environment template, entrypoint, smoke test, and manifest validation script.
- Install rules for the CLI and server binaries, plus configurable server bind address support.

### Changed

- Project version advanced to `0.12.0`.
- Container runtime processes use UID/GID `10001`, a read-only root filesystem, dropped Linux capabilities, and
  `no-new-privileges`.
- Loopback smoke tests bypass proxy environment variables explicitly.

### Compatibility

- The CUDA image uses CUDA 13.1.1 on Ubuntu 24.04 and was verified with an RTX 4060 Laptop GPU and Windows driver
  591.44. Native WSL source builds continue to use the documented CUDA 13.3 toolchain on Ubuntu 26.04.
- Model weights, analyzed repositories, and generated indexes are not included in the images.
- This release publishes reproducible Dockerfiles and Compose manifests; prebuilt registry images and binary packages
  are not yet distributed.

[Unreleased]: https://github.com/Yjjvae/llamacodelab/compare/v0.12.0...HEAD
[0.12.0]: https://github.com/Yjjvae/llamacodelab/compare/v0.11.0...v0.12.0

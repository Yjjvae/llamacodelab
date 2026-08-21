#!/usr/bin/env bash
set -euo pipefail

endpoint="${1:-http://127.0.0.1:8080}"
curl --fail --silent --show-error "${endpoint}/healthz"
curl --fail --silent --show-error "${endpoint}/readyz"
curl --fail --silent --show-error "${endpoint}/v1/models"

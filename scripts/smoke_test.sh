#!/usr/bin/env bash
set -euo pipefail

endpoint="${1:-http://127.0.0.1:8080}"
curl_args=(--fail --silent --show-error --noproxy '*')

curl "${curl_args[@]}" "${endpoint}/healthz"
curl "${curl_args[@]}" "${endpoint}/readyz"
curl "${curl_args[@]}" "${endpoint}/v1/models"

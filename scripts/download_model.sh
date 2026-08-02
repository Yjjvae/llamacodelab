#!/usr/bin/env bash
set -euo pipefail

: "${MODEL_URL:?set MODEL_URL}"
: "${MODEL_FILE:?set MODEL_FILE}"
: "${MODEL_SHA256:?set MODEL_SHA256}"

case "${MODEL_FILE}" in
  ""|"."|".."|*"/"*)
    echo "MODEL_FILE must be a plain filename" >&2
    exit 2
    ;;
esac

mkdir -p models
partial_path="models/.${MODEL_FILE}.part"
final_path="models/${MODEL_FILE}"

cleanup() {
  rm -f "${partial_path}"
}
trap cleanup ERR INT TERM

curl --fail --location --retry 3 --output "${partial_path}" "${MODEL_URL}"

printf '%s  %s\n' "${MODEL_SHA256}" "${partial_path}" | sha256sum --check -

mv "${partial_path}" "${final_path}"
trap - ERR INT TERM
echo "saved ${final_path}"

#!/bin/sh
set -eu

if [ "$#" -eq 0 ]; then
  echo "usage: install-packages.sh PACKAGE..." >&2
  exit 2
fi

# The default Docker apt hook deletes downloaded archives even after a mirror failure. Keeping the
# cache during this build step lets a retry fetch only missing packages.
rm -f /etc/apt/apt.conf.d/docker-clean

attempt=1
max_attempts=5
while [ "$attempt" -le "$max_attempts" ]; do
  if apt-get -o Acquire::Retries=3 -o Acquire::http::Timeout=30 update \
    && apt-get -o Acquire::Retries=3 -o Acquire::http::Timeout=30 \
      install --yes --no-install-recommends "$@"; then
    rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/*.deb
    exit 0
  fi

  if [ "$attempt" -eq "$max_attempts" ]; then
    echo "apt package installation failed after ${max_attempts} attempts" >&2
    exit 1
  fi

  delay=$((attempt * 5))
  echo "apt package installation failed; retrying in ${delay}s" >&2
  sleep "$delay"
  attempt=$((attempt + 1))
done

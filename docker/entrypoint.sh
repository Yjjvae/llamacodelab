#!/bin/sh
set -eu

if [ "$#" -eq 0 ]; then
  set -- llcl-server \
    --host 0.0.0.0 \
    --port 8080 \
    --config /config/default.json \
    --repo /workspace/repo
elif [ "${1#-}" != "$1" ]; then
  set -- llcl-server "$@"
fi

exec "$@"

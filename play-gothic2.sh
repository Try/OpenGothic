#!/bin/sh
# Launch OpenGothic with your Steam Gothic II Gold data.
DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$DIR/build/opengothic/Gothic2Notr" -g "$HOME/Games/Gothic2" "$@"

#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
export LD_LIBRARY_PATH="$DIR/lib:$LD_LIBRARY_PATH"
if [[ $DEBUGGER != "" ]]; then
  exec $DEBUGGER --args "$DIR/bin/Gothic2Notr" "$@"
else
  exec "$DIR/bin/Gothic2Notr" "$@"
fi

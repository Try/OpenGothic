#!/bin/bash

function cdroot()
{
  cd ..; 
  echo ${PWD}
}

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"


export LD_LIBRARY_PATH=(cdroot)+"/lib:$LD_LIBRARY_PATH"
if [[ $DEBUGGER != "" ]]; then
  exec $DEBUGGER --args "$DIR/Gothic2Notr" "$@"
else
  exec "$DIR/Gothic2Notr" "$@"
fi

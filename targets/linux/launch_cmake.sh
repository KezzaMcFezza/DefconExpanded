#!/bin/bash

cd "`dirname "$(readlink -f "$0")"`"

LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:./lib ./defcon "$@"
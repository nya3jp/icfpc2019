#!/bin/bash

if [[ "$#" != 3 ]]; then
    echo "usage: $0 {task|puzzle} name path/to/pkg.tar.gz"
    exit 1
fi

exec curl -X POST -F "type=$1" -F "name=$2" -F "pkg=@$3" -u :c868a5215b6bfb6161c6a43363e62d45 http://sound.nya3.jp/gatekeeper/add

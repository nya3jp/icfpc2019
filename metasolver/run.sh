#!/bin/bash -ex

if [[ -z "$1" ]]; then
    echo "usage: $0 balance"
    exit 1
fi

cd "$(dirname "$0")"
mkdir -p tmp
g++ -O2 -std=c++11 -o tmp/calc_index calc_index.cpp
wget -q -O tmp/solutions.csv http://sound.nya3.jp/dashboard/csv
tmp/calc_index tmp/solutions.csv ./problemsize.txt "$1" 0 tmp/value_debug.txt /dev/stdout

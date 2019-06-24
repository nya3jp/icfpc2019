#!/bin/sh -e

cargo build --release

OPTS="--increase-mop --aggressive-item --aggressive-teleport --spawn-delegate --change-clone-dir --use-drill=60 --vects-shuffle=1 --num-try=100"

target/release/tanakh-solver solve $OPTS -b $2 $1

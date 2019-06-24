#!/bin/sh -e

cargo build --release

rm -rf tmp
mkdir tmp

# OPTS="--increase-mop --change-clone-dir --aggressive-item --aggressive-teleport --use-drill --spawn-delegate --aggressive-fast --mop-direction"

OPTS="--increase-mop --aggressive-item --aggressive-teleport --spawn-delegate"

seq=$(seq $1 300)
shift

for i in $seq; do
    input=$(printf "data/prob-%03d.desc" $i)
    printf "Solving $input ...\n"
    target/release/tanakh-solver solve $OPTS $@ $input
done

# cd tmp
# zip solutions.zip *.sol
# cd ..
# mv tmp/solutions.zip .

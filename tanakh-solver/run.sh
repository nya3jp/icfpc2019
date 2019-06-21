#!/bin/sh -e

cargo build --release

rm -rf tmp
mkdir tmp

for i in $(seq 1 150); do
    input=$(printf "data/prob-%03d.desc" $i)
    printf "Solving $input ...\n"
    target/release/tanakh-solver $input
done

# cd tmp
# zip solutions.zip *.sol
# cd ..
# mv tmp/solutions.zip .

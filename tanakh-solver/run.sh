#!/bin/sh

cargo build --release

for i in $(seq 1 150); do
    target/release/tanakh-solver \
        $(printf "data/prob-%03d.desc" $i) \
        > $(printf "data/prob-%03d.sol" $i)
done

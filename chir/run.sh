#!/bin/sh -e

for i in $(seq 1 220); do
    input=$(printf "../official/part-1-initial/prob-%03d.desc" $i)
    num=$(printf "%03d" $i)
    mkdir -p ./results/$num
    ../bazel-bin/chir/simple --input $input >./results/$num/tmp.sol
    score=$(wc -c ./results/${num}/tmp.sol | awk '{print $1}')
    echo ${score}
    mv ./results/${num}/tmp.sol ./results/${num}/${score}.sol
done

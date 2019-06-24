#!/bin/sh -e

prob="$1"
coins="B" "BF" "C" "CC" "CCC" "CCCC" "FC" "FCC" "F" "L" "FCCC"
if [[ -n "$2" ]];
   coins="$2"
default_opts="--increase-mop --aggressive-item --aggressive-teleport --spawn-delegate --change-clone-dir --use-drill=60 "
input=$(printf "data/prob-%03d.desc" $prob)
printf "Solving $input ...\n"

for coin in "" $coins; do
    for shuffler in 1 2 5 10; do
	echo "trying prob:$prob coin:'$coin' shuf:$shuffler"
	sem -j+0 target/release/tanakh-solver solve $default_opts --vects-shuffle=$shuffler --num-try=50 -b "$coin" $input
    done
done

sem --wait

# cd tmp
# zip solutions.zip *.sol
# cd ..
# mv tmp/solutions.zip .

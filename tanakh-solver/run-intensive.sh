#!/bin/bash -e

prob="$1"
coins="B BF C CC CCC CCCC CF CCF F L CCCF"
if [[ -n "$2" ]]; then
   coins="$2"
fi
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

for coin in "" $coins; do
    echo "submitting prob:$prob coin:'$coin'"
    probname="$(printf "prob-%03d" $prob)"
    resdir="$(printf "results/%03d%s" $prob "$coin")"
    result="$(for I in ${resdir}/*.sol; do basename $I; done | sort -n | head -n 1)"
    sem -j+0 ../submit.py --problem ${probname} --solver tanakh-mob-mojako --solution ${resdir}/${result} --purchase "$coin"
done

sem --wait

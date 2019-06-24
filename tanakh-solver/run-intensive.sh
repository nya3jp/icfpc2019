#!/bin/bash -e

probrange="$1"
trials="50"
if [[ -n "$2" ]]; then
    trials="$2"
fi
coins="B BF C CC CCC CCCC CF CCF F L CCCF"
if [[ -n "$3" ]]; then
    coins="$3"
fi
default_opts="--increase-mop --aggressive-item --aggressive-teleport --spawn-delegate --change-clone-dir --use-drill=60 "
for prob in $(seq $probrange); do
    input=$(printf "data/prob-%03d.desc" $prob)
    printf "Solving $input ...\n"

    for coin in "" $coins; do
	for shuffler in 1 2 5 10; do
	    echo "trying prob:$prob coin:'$coin' shuf:$shuffler"
	    purchase_opt=""
	    if [[ -n "$coin" ]]; then
		purchase_opt="-b $coin"
	    fi
	    sem -j+0 target/release/tanakh-solver solve $default_opts --vects-shuffle=$shuffler --num-try=$trials $purchase_opt $input
	done
    done
done

sem --wait

for prob in $(seq $probrange); do
    input=$(printf "data/prob-%03d.desc" $prob)
    for coin in "" $coins; do
	echo "submitting prob:$prob coin:'$coin'"
	probname="$(printf "prob-%03d" $prob)"
	resdir="$(printf "results/%03d%s" $prob "$coin")"
	result="$(for I in ${resdir}/*.sol; do basename $I; done | sort -n | head -n 1)"
	purchase_opt=""
	if [[ -n "$coin" ]]; then
	    purchase_opt="--purchase $coin"
	fi
	sem -j+0 ../submit.py --problem ${probname} --solver tanakh-mob-mojako --solution ${resdir}/${result} $purchase_opt
	sleep 1
    done
done

sem --wait

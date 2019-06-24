#!/bin/bash -e

solver="tanakh-noname"
if [[ -n "$1" ]]; then
    solver="$1"
fi
for prob in $(seq 300); do
    result_path="$(printf "results/%03d" $prob)"
    for d in $result_path*; do
	coin="${d#$result_path}"
	echo "submitting prob:$prob coin:'$coin'"
	probname="$(printf "prob-%03d" $prob)"
	resdir="${result_path}${coin}"
	result="$(for I in ${resdir}/*.sol; do basename $I; done | sort -n | head -n 1)"
	purchase_opt=""
	if [[ -n "$coin" ]]; then
	    purchase_opt="--purchase $coin"
	fi
	../submit.py --problem ${probname} --solver "$solver" --solution ${resdir}/${result} $purchase_opt &
	sleep 1
    done
done

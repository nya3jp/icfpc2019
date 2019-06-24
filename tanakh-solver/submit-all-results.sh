#!/bin/bash -e

prob="$1"
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
    sem -j+0 ../submit.py --problem ${probname} --solver tanakh-mob-mojako --solution ${resdir}/${result} $purchase_opt
    sleep 1
done

sem --wait
